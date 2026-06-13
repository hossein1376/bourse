#include "engine/matching_engine.hpp"
#include "engine/types.hpp"

namespace engine {

MatchingEngine::MatchingEngine(TradeCallback on_trade)
    : book_(), on_trade_(std::move(on_trade)) {}

void MatchingEngine::process_order(const Order &order) {
  Order incoming = order;
  match_against_book(incoming);
}

void MatchingEngine::match_against_book(Order &incoming) {
  Side otherside = incoming.side == Side::Buy ? Side::Sell : Side::Buy;
  switch (incoming.type) {
  case OrderType::Limit:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if ((incoming.side == Side::Buy && incoming.price >= resting.price) ||
          (incoming.side == Side::Sell && incoming.price <= resting.price)) {
        if (incoming.quantity >= resting.quantity) {
          on_trade_(TradeEvent{
              .taker_order_id = incoming.order_id,
              .maker_order_id = resting.order_id,
              .taker_side = incoming.side,
              .price = resting.price,
              .quantity = resting.quantity,
          });
          incoming.quantity -= resting.quantity;
          incoming.filled += resting.quantity;
          book_.pop_front(otherside);
          continue;
        }
        on_trade_(TradeEvent{
            .taker_order_id = incoming.order_id,
            .maker_order_id = resting.order_id,
            .taker_side = incoming.side,
            .price = resting.price,
            .quantity = incoming.quantity,
        });
        incoming.filled += incoming.quantity;
        resting.filled += incoming.quantity;
        resting.quantity = resting.quantity - incoming.quantity;
        incoming.quantity = 0;
      }
      break;
    }

    if (incoming.quantity > 0) {
      book_.add_order(incoming);
    }
    break;

  case OrderType::Market:
    while (incoming.quantity > 0 && !book_.empty(otherside)) {
      Order &resting = book_.front_order(otherside);
      if (incoming.quantity >= resting.quantity) {
        on_trade_(TradeEvent{
            .taker_order_id = incoming.order_id,
            .maker_order_id = resting.order_id,
            .taker_side = incoming.side,
            .price = resting.price,
            .quantity = resting.quantity,
        });
        incoming.quantity -= resting.quantity;
        incoming.filled += resting.quantity;
        book_.pop_front(otherside);
        continue;
      }
      on_trade_(TradeEvent{
          .taker_order_id = incoming.order_id,
          .maker_order_id = resting.order_id,
          .taker_side = incoming.side,
          .price = resting.price,
          .quantity = incoming.quantity,
      });
      incoming.filled += incoming.quantity;
      resting.filled += incoming.quantity;
      resting.quantity = resting.quantity - incoming.quantity;
      incoming.quantity = 0;
    }

    break;

  case OrderType::IOC:
  case OrderType::FOK:
    break;
  }
}

bool MatchingEngine::can_fully_fill(const Order &order) const {
  (void)order;
  return false;
}

} // namespace engine
