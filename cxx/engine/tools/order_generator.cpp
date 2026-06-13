#include "order_generator.hpp"
#include "engine/matching_engine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

namespace engine {

CLIArgs parse_args(int argc, char *argv[]) {
  CLIArgs args;
  for (int i = 1; i < argc; ++i) {
    std::string flag = argv[i];

    auto next = [&] {
      return ++i < argc ? std::string(argv[i]) : std::string();
    };

    if (flag == "--orders" || flag == "-n")
      args.num_orders = std::stoi(next());
    else if (flag == "--seed" || flag == "-s")
      args.seed = std::stoul(next());
    else if (flag == "--mid-price" || flag == "-m")
      args.mid_price = std::stoull(next());
    else if (flag == "--spread" || flag == "-d")
      args.spread = std::stoull(next());
    else if (flag == "--volatility" || flag == "-v")
      args.volatility = std::stoull(next());
    else if (flag == "--volume")
      args.avg_volume = std::stoull(next());
    else if (flag == "--arrival")
      args.arrival = std::stod(next());
    else if (flag == "--limit-pct")
      args.limit_pct = std::stoi(next());
    else if (flag == "--market-pct")
      args.market_pct = std::stoi(next());
    else if (flag == "--ioc-pct")
      args.ioc_pct = std::stoi(next());
    else if (flag == "--fok-pct")
      args.fok_pct = std::stoi(next());
    else if (flag == "--output")
      args.output_path = next();
  }
  return args;
}

void print_summary(const CLIArgs &, const Stats &stats, const OrderBook &book,
                   unsigned seed) {
  std::cout << "\n=== Generator Summary ===\n"
            << "Seed:      " << seed << "\n"
            << "Orders:    " << stats.orders_submitted << "\n"
            << "Trades:    " << stats.trades_emitted << "\n"
            << "Avg qty:   "
            << (stats.trades_emitted > 0
                    ? stats.total_trade_qty / stats.trades_emitted
                    : 0)
            << "\n"
            << "VWAP:      "
            << (stats.total_trade_qty > 0
                    ? stats.total_price_qty / stats.total_trade_qty
                    : 0)
            << "\n";

  std::cout << "\nFinal book:\n";
  if (!book.empty(Side::Buy))
    std::cout << "  Best bid: " << book.best_bid().price << " @ "
              << book.best_bid().quantity << "\n";
  else
    std::cout << "  Best bid: none\n";

  if (!book.empty(Side::Sell))
    std::cout << "  Best ask: " << book.best_ask().price << " @ "
              << book.best_ask().quantity << "\n";
  else
    std::cout << "  Best ask: none\n";
}

} // namespace engine

int main(int argc, char *argv[]) {
  using namespace engine;

  auto args = parse_args(argc, argv);
  if (args.seed == 0)
    args.seed = std::random_device{}();

  unsigned seed = args.seed;
  std::mt19937 rng(seed);

  // Order type distribution
  std::discrete_distribution<int> type_dist(
      {static_cast<double>(args.limit_pct),
       static_cast<double>(args.market_pct), static_cast<double>(args.ioc_pct),
       static_cast<double>(args.fok_pct)});
  auto order_types = {engine::OrderType::Limit, engine::OrderType::Market,
                      engine::OrderType::IOC, engine::OrderType::FOK};

  // Side distribution
  std::uniform_int_distribution<int> side_dist(0, 1);

  // Price movement
  std::normal_distribution<double> price_drift(0.0, args.volatility);
  std::uniform_int_distribution<Price> jitter(0, args.spread);

  // Volume
  std::poisson_distribution<Quantity> volume_dist(args.avg_volume);

  // Inter-arrival
  std::exponential_distribution<double> arrival_dist(args.arrival);

  engine::Stats stats;

  // CSV output
  std::ofstream csv;
  if (!args.output_path.empty()) {
    csv.open(args.output_path);
    csv << "taker_order_id,maker_order_id,taker_side,price,quantity\n";
  }

  engine::MatchingEngine engine([&](const engine::TradeEvent &t) {
    stats.trades_emitted++;
    stats.total_trade_qty += t.quantity;
    stats.total_price_qty += t.price * t.quantity;

    if (csv.is_open()) {
      csv << t.taker_order_id << "," << t.maker_order_id << ","
          << (t.taker_side == engine::Side::Buy ? "Buy" : "Sell") << ","
          << t.price << "," << t.quantity << "\n";
    }
  });

  Price mid = args.mid_price;
  engine::OrderID next_id = 1;

  for (int i = 0; i < args.num_orders; ++i, ++next_id) {
    if (args.arrival > 0) {
      double secs = arrival_dist(rng);
      std::this_thread::sleep_for(std::chrono::duration<double>(secs));
    }

    // Drift mid price
    mid = static_cast<Price>(
        std::max(0.0, std::round(static_cast<double>(mid) + price_drift(rng))));
    if (mid < args.spread)
      mid = args.spread;

    // Pick side
    engine::Side side =
        side_dist(rng) == 0 ? engine::Side::Buy : engine::Side::Sell;

    // Pick type
    engine::OrderType type = *std::next(order_types.begin(), type_dist(rng));

    // Generate price
    Price price;
    Price half_spread = args.spread / 2;
    Price j = jitter(rng);
    if (side == engine::Side::Buy) {
      Price base = mid > half_spread ? mid - half_spread : 1;
      price = base > j ? base - j : 1;
    } else {
      price = mid + half_spread + j;
    }

    // Generate quantity
    engine::Quantity qty = std::max<engine::Quantity>(1, volume_dist(rng));

    engine.process_order(engine::Order{
        .order_id = next_id,
        .side = side,
        .type = type,
        .price = price,
        .quantity = qty,
    });

    stats.orders_submitted++;
  }

  engine::print_summary(args, stats, engine.book(), seed);
}
