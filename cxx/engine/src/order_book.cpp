#include "engine/order_book.hpp"
#include "engine/types.hpp"
#include <optional>
#include <stdexcept>

namespace engine {
OrderBook::OrderBook() : bids(), asks(), index() {}

bool OrderBook::add_order(Order order) {
  if (index.find(order.order_id) != index.end())
    return false;
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
  return true;
}

bool OrderBook::cancel_order(OrderID order_id) {
  auto it = index.find(order_id);
  if (it == index.end())
    return false;

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
  return true;
}

std::optional<Order> OrderBook::best_bid() const {
  if (bids.empty())
    return std::nullopt;
  return bids.begin()->second.orders.front();
}

std::optional<Order> OrderBook::best_ask() const {
  if (asks.empty())
    return std::nullopt;
  return asks.begin()->second.orders.front();
}

Order &OrderBook::front_order(Side side) {
  if (side == Side::Buy) {
    if (bids.empty()) {
      throw std::runtime_error("No bids");
    }
    return bids.begin()->second.orders.front();
  } else {
    if (asks.empty()) {
      throw std::runtime_error("No asks");
    }
    return asks.begin()->second.orders.front();
  }
}

void OrderBook::pop_front(Side side) {
  if (side == Side::Buy) {
    if (bids.empty()) {
      throw std::runtime_error("No bids");
    }
    OrderID order_id = bids.begin()->second.orders.front().order_id;
    bids.begin()->second.orders.pop_front();
    if (bids.begin()->second.orders.empty()) {
      bids.erase(bids.begin());
    }
    index.erase(order_id);
  } else {
    if (asks.empty()) {
      throw std::runtime_error("No asks");
    }
    OrderID order_id = asks.begin()->second.orders.front().order_id;
    asks.begin()->second.orders.pop_front();
    if (asks.begin()->second.orders.empty()) {
      asks.erase(asks.begin());
    }
    index.erase(order_id);
  }
}

bool OrderBook::empty(Side side) const {
  if (side == Side::Buy) {
    return bids.empty();
  } else {
    return asks.empty();
  }
}

} // namespace engine
