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

  Order best_bid = book.best_bid();
  EXPECT_EQ(best_bid.order_id, 1);
  EXPECT_EQ(best_bid.price, 1000);
  EXPECT_EQ(best_bid.quantity, 10);
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

  Order best_ask = book.best_ask();
  EXPECT_EQ(best_ask.order_id, 1);
  EXPECT_EQ(best_ask.price, 1000);
  EXPECT_EQ(best_ask.quantity, 10);
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

  Order best_bid = book.best_bid();
  EXPECT_EQ(best_bid.order_id, 2);
  EXPECT_EQ(best_bid.price, 10000);
  EXPECT_EQ(best_bid.quantity, 5);
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

  Order best_ask = book.best_ask();
  EXPECT_EQ(best_ask.order_id, 2);
  EXPECT_EQ(best_ask.price, 10000);
  EXPECT_EQ(best_ask.quantity, 5);
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
  Order best_bid = book.best_bid();
  EXPECT_EQ(best_bid.order_id, 1);
  EXPECT_EQ(best_bid.price, 10000);
  EXPECT_EQ(best_bid.quantity, 5);

  // Cancel order 1
  book.cancel_order(best_bid.order_id);

  // Assert best_bid() throws "No bids"
  EXPECT_THROW(book.best_bid(), std::runtime_error);
}

TEST(OrderBookTest, CancelNonExistentOrderThrows) {
  OrderBook book;

  EXPECT_THROW(book.cancel_order(999), std::runtime_error);
}
