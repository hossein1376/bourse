#pragma once

#include "engine/types.hpp"
#include <unordered_map>
#include <functional>

namespace engine {

class OrderBook {
public:
  OrderBook();
  void add_order(Order order);
  void cancel_order(OrderID order_id);
  Order best_bid();
  Order best_ask();

private:
  std::map<Price, PriceLevel, std::greater<Price>> bids;
  std::map<Price, PriceLevel, std::less<Price>> asks;
  std::unordered_map<OrderID, std::list<Order>::iterator> index;
};

} // namespace engine
