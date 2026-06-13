#pragma once

#include "engine/order_book.hpp"
#include "engine/types.hpp"

namespace engine {

class MatchingEngine {
public:
  using TradeCallback = std::function<void(const TradeEvent &)>;

  explicit MatchingEngine(TradeCallback on_trade);
  const OrderBook &book() const { return book_; }
  void process_order(const Order &order);
  bool cancel_order(OrderID order_id);
  bool amend_order(OrderID old_id, const Order &new_order);
  std::uint64_t next_sequence() { return next_trade_seq_++; }

private:
  void match_against_book(Order &incoming);
  bool can_fully_fill(const Order &order) const;
  bool fill_level(Order &incoming, Order &resting);

  OrderBook book_;
  TradeCallback on_trade_;
  std::uint64_t next_trade_seq_ = 1;
};

} // namespace engine
