#include "engine/order_book.hpp"
#include "engine/types.hpp"

namespace engine {
OrderBook::OrderBook() : bids(), asks(), index() {}

void OrderBook::add_order(Order order) {
  if (order.side == Side::Buy) {
    auto p = bids.try_emplace(order.price, PriceLevel{order.price, {order}});
    if (!p.second) {
      p.first->second.orders.push_back(order);
    }
    index.insert({order.order_id, std::prev(p.first->second.orders.end())});
  } else {
    auto p = asks.try_emplace(order.price, PriceLevel{order.price, {order}});
    if (!p.second) {
      p.first->second.orders.push_back(order);
    }
    index.insert({order.order_id, std::prev(p.first->second.orders.end())});
  }
}

void OrderBook::cancel_order(OrderID order_id) {
  auto it = index.find(order_id);
  if (it == index.end()) {
    throw std::runtime_error("Order not found");
  }

  if (it->second->side == Side::Buy) {
    auto p = bids.find(it->second->price);
    if (p != bids.end()) {
      p->second.orders.erase(it->second);
      if (p->second.orders.empty()) {
        bids.erase(p);
      }
      index.erase(order_id);
    }
  } else {
    auto p = asks.find(it->second->price);
    if (p != asks.end()) {
      p->second.orders.erase(it->second);
      if (p->second.orders.empty()) {
        asks.erase(p);
      }
      index.erase(order_id);
    }
  }
}

Order OrderBook::best_bid() {
  if (bids.empty()) {
    throw std::runtime_error("No bids");
  }
  return bids.begin()->second.orders.front();
}

Order OrderBook::best_ask() {
  if (asks.empty()) {
    throw std::runtime_error("No asks");
  }
  return asks.begin()->second.orders.front();
}

} // namespace engine
