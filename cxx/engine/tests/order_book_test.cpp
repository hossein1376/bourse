#include "engine/order_book.hpp"
#include "engine/types.hpp"
#include <gtest/gtest.h>

using namespace engine;

TEST(OrderBookTest, BestBidAfterAdd) {
  OrderBook book;
  book.add_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 1000,
      .quantity = 10,
  });

  auto best_bid = book.best_bid();
  ASSERT_TRUE(best_bid.has_value());
  EXPECT_EQ(best_bid->order_id, 1);
  EXPECT_EQ(best_bid->price, 1000);
  EXPECT_EQ(best_bid->quantity, 10);
}

TEST(OrderBookTest, BestAskAfterAdd) {
  OrderBook book;
  book.add_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 1000,
      .quantity = 10,
  });

  auto best_ask = book.best_ask();
  ASSERT_TRUE(best_ask.has_value());
  EXPECT_EQ(best_ask->order_id, 1);
  EXPECT_EQ(best_ask->price, 1000);
  EXPECT_EQ(best_ask->quantity, 10);
}

TEST(OrderBookTest, BestBidReturnsHighestPrice) {
  OrderBook book;

  book.add_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 9900,
      .quantity = 10,
  });
  book.add_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 5,
  });

  auto best_bid = book.best_bid();
  ASSERT_TRUE(best_bid.has_value());
  EXPECT_EQ(best_bid->order_id, 2);
  EXPECT_EQ(best_bid->price, 10000);
  EXPECT_EQ(best_bid->quantity, 5);
}

TEST(OrderBookTest, BestAskReturnsLowestPrice) {
  OrderBook book;

  book.add_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10100,
      .quantity = 10,
  });
  book.add_order(Order{
      .order_id = 2,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 5,
  });

  auto best_ask = book.best_ask();
  ASSERT_TRUE(best_ask.has_value());
  EXPECT_EQ(best_ask->order_id, 2);
  EXPECT_EQ(best_ask->price, 10000);
  EXPECT_EQ(best_ask->quantity, 5);
}

TEST(OrderBookTest, CancelOrderRemovesItFromBestBid) {
  OrderBook book;

  // Add a buy order at 10000 (id=1)
  // Assert best_bid().order_id == 1
  book.add_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 5,
  });
  auto best_bid = book.best_bid();
  ASSERT_TRUE(best_bid.has_value());
  EXPECT_EQ(best_bid->order_id, 1);
  EXPECT_EQ(best_bid->price, 10000);
  EXPECT_EQ(best_bid->quantity, 5);

  // Cancel order 1
  book.cancel_order(best_bid->order_id);

  // Assert best_bid() returns empty
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, CancelNonExistentOrderReturnsFalse) {
  OrderBook book;

  EXPECT_FALSE(book.cancel_order(999));
}
