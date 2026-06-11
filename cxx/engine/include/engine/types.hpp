#pragma once

#include <cstdint>
#include <list>
#include <map>

namespace engine {

// All prices are in cents (e.g. 10000 = $100.00)
using Price = std::uint64_t;
using Quantity = std::uint64_t;
using OrderID = std::uint64_t;

enum class Side : std::uint8_t { Buy, Sell };

enum class OrderType : std::uint8_t {
  Limit,
  Market,
  IOC, // Immediate-or-Cancel
  FOK  // Fill-or-Kill
};

struct Order {
  OrderID order_id;
  Side side;
  OrderType type;
  Price price;
  Quantity quantity;
  Quantity filled = 0;
};

struct TradeEvent {
  OrderID taker_order_id;
  OrderID maker_order_id;
  Side taker_side;
  Price price;
  Quantity quantity;
};

struct PriceLevel {
  Price price;
  std::list<Order> orders;
};

} // namespace engine
