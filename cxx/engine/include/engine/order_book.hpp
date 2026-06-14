#pragma once

#include "engine/types.hpp"
#include "util/linked_list.hpp"
#include <optional>
#include <unordered_map>

namespace engine {

class OrderBook {
public:
  OrderBook();
  bool add_order(Order order);
  bool cancel_order(OrderID order_id);
  std::optional<Order> best_bid() const;
  std::optional<Order> best_ask() const;
  Order &front_order(Side side);
  void pop_front(Side side);
  bool empty(Side side) const;

  template <typename F>
  void for_each_level(Side side, Price limit, F &&fn) const {
    if (side == Side::Buy) {
      for (const auto &[price, level] : bids_) {
        if (price < limit)
          break;
        fn(price, level);
      }
    } else {
      for (const auto &[price, level] : asks_) {
        if (price > limit)
          break;
        fn(price, level);
      }
    }
  }

private:
  DoublyLinkedList<PriceLevel> bids_;
  DoublyLinkedList<PriceLevel> asks_;
  std::unordered_map<OrderID, std::list<Order>::iterator> order_index_;
  std::unordered_map<Price, DoublyLinkedList<PriceLevel>::Node *> level_index_;
};

} // namespace engine
