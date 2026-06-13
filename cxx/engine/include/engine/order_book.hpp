#pragma once

#include "engine/types.hpp"
#include <unordered_map>

namespace engine {

class OrderBook {
public:
  OrderBook();
  void add_order(Order order);
  void cancel_order(OrderID order_id);
  Order best_bid() const;
  Order best_ask() const;
  Order &front_order(Side side);
  void pop_front(Side side);
  bool empty(Side side) const;

  template <typename F>
  void for_each_level(Side side, Price limit, F &&fn) const {
    if (side == Side::Buy) {
      for (const auto &[price, level] : bids) {
        if (price < limit)
          break;
        fn(price, level.orders);
      }
    } else {
      for (const auto &[price, level] : asks) {
        if (price > limit)
          break;
        fn(price, level.orders);
      }
    }
  }

private:
  // TODO: Replace map-of-lists with a custom doubly-linked list per side
  //       for O(1) price level lookup and better cache locality (PLAN.md §192).
  //       Keep std::map for now — correctness first, optimization later.
  std::map<Price, PriceLevel, std::greater<Price>> bids;
  std::map<Price, PriceLevel, std::less<Price>> asks;
  std::unordered_map<OrderID, std::list<Order>::iterator> index;
};

} // namespace engine
