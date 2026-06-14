#include "engine/order_book.hpp"
#include "engine/types.hpp"
#include <iterator>
#include <optional>
#include <stdexcept>

namespace engine {
OrderBook::OrderBook() : bids_(), asks_(), level_index_() {}

bool OrderBook::add_order(Order order) {
  if (order_index_.find(order.order_id) != order_index_.end())
    return false;
  if (order.side == Side::Buy) {
    auto *level = level_index_[order.price];
    if (level)
      level->data.orders.push_back(order);
    else {
      auto *cur = bids_.head();
      while (cur && cur->data.price > order.price)
        cur = cur->next;
      level = bids_.insert_before(cur, PriceLevel{order.price, {order}});
      level_index_[order.price] = level;
    }
    order_index_[order.order_id] = std::prev(level->data.orders.end());
  } else {
    auto *level = level_index_[order.price];
    if (level)
      level->data.orders.push_back(order);
    else {
      auto *cur = asks_.head();
      while (cur && cur->data.price < order.price)
        cur = cur->next;
      level = asks_.insert_before(cur, PriceLevel{order.price, {order}});
      level_index_[order.price] = level;
    }
    order_index_[order.order_id] = std::prev(level->data.orders.end());
  }
  return true;
}

bool OrderBook::cancel_order(OrderID order_id) {
  auto it = order_index_.find(order_id);
  if (it == order_index_.end())
    return false;

  Price price = it->second->price;
  Side side = it->second->side;

  auto *level = level_index_[price];
  level->data.orders.erase(it->second);
  order_index_.erase(order_id);

  if (level->data.orders.empty()) {
    if (side == Side::Buy)
      bids_.erase(level);
    else
      asks_.erase(level);
    level_index_.erase(price);
  }

  return true;
}

std::optional<Order> OrderBook::best_bid() const {
  if (bids_.empty())
    return std::nullopt;
  return bids_.head()->data.orders.front();
}

std::optional<Order> OrderBook::best_ask() const {
  if (asks_.empty())
    return std::nullopt;
  return asks_.head()->data.orders.front();
}

Order &OrderBook::front_order(Side side) {
  if (side == Side::Buy) {
    if (bids_.empty()) {
      throw std::runtime_error("No bids");
    }
    return bids_.head()->data.orders.front();
  } else {
    if (asks_.empty()) {
      throw std::runtime_error("No asks");
    }
    return asks_.head()->data.orders.front();
  }
}

void OrderBook::pop_front(Side side) {
  if (side == Side::Buy) {
    if (bids_.empty()) {
      throw std::runtime_error("No bids");
    }
    auto *node = bids_.head();
    Price price = node->data.price;
    OrderID order_id = node->data.orders.front().order_id;

    node->data.orders.pop_front();
    order_index_.erase(order_id);

    if (node->data.orders.empty()) {
      bids_.erase(node);
      level_index_.erase(price);
    }
  } else {
    if (asks_.empty()) {
      throw std::runtime_error("No asks");
    }
    auto *node = asks_.head();
    Price price = node->data.price;
    OrderID order_id = node->data.orders.front().order_id;

    node->data.orders.pop_front();
    order_index_.erase(order_id);

    if (node->data.orders.empty()) {
      asks_.erase(node);
      level_index_.erase(price);
    }
  }
}

bool OrderBook::empty(Side side) const {
  if (side == Side::Buy) {
    return bids_.empty();
  } else {
    return asks_.empty();
  }
}

} // namespace engine
