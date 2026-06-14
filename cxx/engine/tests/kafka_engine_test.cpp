#include "book_update_generated.h"
#include "order_generated.h"
#include "trade_event_generated.h"
#include "engine/types.hpp"
#include <flatbuffers/flatbuffers.h>
#include <gtest/gtest.h>

using namespace engine;

TEST(KafkaEngineTest, OrderRoundTrip) {
  Order original{
      .order_id = 42,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 50,
  };

  flatbuffers::FlatBufferBuilder fb(256);
  auto fbs = engine::schema::CreateOrder(
      fb, original.order_id, static_cast<uint8_t>(original.side),
      static_cast<uint8_t>(original.type), original.price, original.quantity);
  fb.Finish(fbs);

  auto *result = engine::schema::GetOrder(fb.GetBufferPointer());
  EXPECT_EQ(result->order_id(), 42);
  EXPECT_EQ(result->side(), 0);
  EXPECT_EQ(result->type(), 0);
  EXPECT_EQ(result->price(), 10000);
  EXPECT_EQ(result->quantity(), 50);
}

TEST(KafkaEngineTest, TradeEventRoundTrip) {
  TradeEvent original{
      .taker_order_id = 42,
      .maker_order_id = 7,
      .taker_side = Side::Buy,
      .price = 10000,
      .quantity = 50,
      .sequence = 1,
  };

  flatbuffers::FlatBufferBuilder fb(256);
  auto fbs = engine::schema::CreateTradeEvent(
      fb, original.taker_order_id, original.maker_order_id,
      static_cast<uint8_t>(original.taker_side), original.price,
      original.quantity, original.sequence);
  fb.Finish(fbs);

  auto *result = engine::schema::GetTradeEvent(fb.GetBufferPointer());
  EXPECT_EQ(result->taker_order_id(), 42);
  EXPECT_EQ(result->maker_order_id(), 7);
  EXPECT_EQ(result->taker_side(), 0);
  EXPECT_EQ(result->price(), 10000);
  EXPECT_EQ(result->quantity(), 50);
  EXPECT_EQ(result->sequence(), 1);
}

TEST(KafkaEngineTest, BookUpdateRoundTrip) {
  flatbuffers::FlatBufferBuilder fb(256);

  auto bid1 = engine::schema::CreateLevel(fb, 10000, 10);
  auto bid2 = engine::schema::CreateLevel(fb, 9900, 5);
  auto ask1 = engine::schema::CreateLevel(fb, 10100, 8);

  auto update = engine::schema::CreateBookUpdate(
      fb, fb.CreateVector(std::vector({bid1, bid2})),
      fb.CreateVector(std::vector({ask1})));
  fb.Finish(update);

  auto *result = engine::schema::GetBookUpdate(fb.GetBufferPointer());
  ASSERT_EQ(result->bid_levels()->size(), 2);
  EXPECT_EQ(result->bid_levels()->Get(0)->price(), 10000);
  EXPECT_EQ(result->bid_levels()->Get(0)->quantity(), 10);
  EXPECT_EQ(result->bid_levels()->Get(1)->price(), 9900);
  EXPECT_EQ(result->bid_levels()->Get(1)->quantity(), 5);

  ASSERT_EQ(result->ask_levels()->size(), 1);
  EXPECT_EQ(result->ask_levels()->Get(0)->price(), 10100);
  EXPECT_EQ(result->ask_levels()->Get(0)->quantity(), 8);
}
