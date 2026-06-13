#pragma once

#include "engine/order_book.hpp"
#include "engine/types.hpp"

namespace engine {

class MatchingEngine {
public:
  using TradeCallback = std::function<void(const TradeEvent &)>;

  explicit MatchingEngine(TradeCallback on_trade);

  void process_order(const Order &order);

  const OrderBook &book() const { return book_; }

private:
  void match_against_book(Order &incoming);
  bool can_fully_fill(const Order &order) const;
  bool fill_level(Order &incoming, Order &resting);

  OrderBook book_;
  TradeCallback on_trade_;
};

} // namespace engine
