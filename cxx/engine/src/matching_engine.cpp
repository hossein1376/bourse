#include "engine/matching_engine.hpp"
#include "engine/types.hpp"

namespace engine {

namespace {
bool crosses(const Order &incoming, const Order &resting) {
  return (incoming.side == Side::Buy && incoming.price >= resting.price) ||
         (incoming.side == Side::Sell && incoming.price <= resting.price);
}
} // namespace

MatchingEngine::MatchingEngine(TradeCallback on_trade, BookCallback on_book)
    : book_(), on_trade_(std::move(on_trade)), on_book_(std::move(on_book)) {}

void MatchingEngine::process_order(const Order &order) {
  if (order.quantity == 0)
    return;
  if (order.type == OrderType::Limit && order.price == 0)
    return;
  Order incoming = order;
  if (incoming.type == OrderType::FOK && !can_fully_fill(incoming))
    return;
  match_against_book(incoming);
  if (on_book_)
    on_book_(book_);
}

void MatchingEngine::match_against_book(Order &incoming) {
  Side otherside = incoming.side == Side::Buy ? Side::Sell : Side::Buy;
  switch (incoming.type) {
  case OrderType::Limit:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if (crosses(incoming, resting)) {
        if (fill_level(incoming, resting)) {
          book_.pop_front(otherside);
        }
      } else {
        break;
      }
    }
    if (incoming.quantity > 0)
      book_.add_order(incoming);
    break;

  case OrderType::Market:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if (fill_level(incoming, resting)) {
        book_.pop_front(otherside);
      }
    }
    break;

  case OrderType::IOC:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if (crosses(incoming, resting)) {
        if (fill_level(incoming, resting)) {
          book_.pop_front(otherside);
        }
      } else {
        break;
      }
    }
    break;

  case OrderType::FOK:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if (crosses(incoming, resting)) {
        if (fill_level(incoming, resting)) {
          book_.pop_front(otherside);
        }
      } else {
        break;
      }
    }
    break;
  }
}

bool MatchingEngine::fill_level(Order &incoming, Order &resting) {
  if (incoming.quantity >= resting.quantity) {
    on_trade_(TradeEvent{
        .taker_order_id = incoming.order_id,
        .maker_order_id = resting.order_id,
        .taker_side = incoming.side,
        .price = resting.price,
        .quantity = resting.quantity,
        .sequence = next_sequence(),
    });
    incoming.quantity -= resting.quantity;
    incoming.filled += resting.quantity;
    return true;
  }
  on_trade_(TradeEvent{
      .taker_order_id = incoming.order_id,
      .maker_order_id = resting.order_id,
      .taker_side = incoming.side,
      .price = resting.price,
      .quantity = incoming.quantity,
      .sequence = next_sequence(),
  });
  incoming.filled += incoming.quantity;
  resting.filled += incoming.quantity;
  resting.quantity -= incoming.quantity;
  incoming.quantity = 0;
  return false;
}

bool MatchingEngine::can_fully_fill(const Order &order) const {
  Side opposite = order.side == Side::Buy ? Side::Sell : Side::Buy;
  Quantity total = 0;
  book_.for_each_level(opposite, order.price, [&](Price, const auto &orders) {
    for (const auto &o : orders) {
      total += o.quantity;
    }
  });
  return total >= order.quantity;
}

bool MatchingEngine::cancel_order(OrderID order_id) {
  return book_.cancel_order(order_id);
}

bool MatchingEngine::amend_order(OrderID old_id, const Order &new_order) {
  if (!cancel_order(old_id))
    return false;
  process_order(new_order);
  return true;
}

} // namespace engine
