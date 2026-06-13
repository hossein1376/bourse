#pragma once

#include "engine/order_book.hpp"
#include "engine/types.hpp"
#include <string>

namespace engine {

struct CLIArgs {
  int num_orders = 1000;
  unsigned seed = 0;
  Price mid_price = 10000;
  Price spread = 10;
  Price volatility = 5;
  Quantity avg_volume = 10;
  double arrival = 0; // 0 = no sleep
  int limit_pct = 60;
  int market_pct = 20;
  int ioc_pct = 10;
  int fok_pct = 10;
  std::string output_path;
};

struct Stats {
  int orders_submitted = 0;
  int trades_emitted = 0;
  Quantity total_trade_qty = 0;
  double total_price_qty = 0;
};

CLIArgs parse_args(int argc, char *argv[]);
void print_summary(const CLIArgs &args, const Stats &stats,
                   const OrderBook &book, unsigned seed);

} // namespace engine
