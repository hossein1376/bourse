#include "engine/matching_engine.hpp"
#include "engine/types.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace engine;

// Helper: create a MatchingEngine that records all trade events
struct TestHarness {
  std::vector<TradeEvent> trades;
  MatchingEngine engine;

  TestHarness()
      : engine([this](const TradeEvent &t) { trades.push_back(t); }) {}
};

TEST(MatchingEngineTest, MarketBuyFillsLimitSell) {
  // Add a sell limit order at 10000, qty 10
  // Process a market buy for qty 5
  // Assert: 1 trade event, correct price/qty/sides
}

TEST(MatchingEngineTest, MarketBuyEmptyBookNoCrash) {
  // Process a market buy with no orders in book
  // Assert: no trade events, no crash
}

TEST(MatchingEngineTest, LimitBuyCrossesSpreadPartialFill) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10100,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
  EXPECT_EQ(h.trades[0].taker_side, Side::Buy);
  EXPECT_EQ(h.trades[0].taker_order_id, 2);
  EXPECT_EQ(h.trades[0].maker_order_id, 1);

  EXPECT_FALSE(h.engine.book().empty(Side::Sell));
  EXPECT_EQ(h.engine.book().best_ask().price, 10000);
  EXPECT_EQ(h.engine.book().best_ask().quantity, 5);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, LimitBuyCrossesSpreadPartialFillWithRest) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10100,
      .quantity = 15,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 10);
  EXPECT_EQ(h.trades[0].taker_side, Side::Buy);

  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid().price, 10100);
  EXPECT_EQ(h.engine.book().best_bid().quantity, 5);
}

TEST(MatchingEngineTest, LimitBuyDoesNotCrossSpread) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 9900,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid().price, 9900);
  EXPECT_EQ(h.engine.book().best_bid().quantity, 5);
}

TEST(MatchingEngineTest, LimitSellCrossesSpread) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 9900,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
  EXPECT_EQ(h.trades[0].taker_side, Side::Sell);
  EXPECT_EQ(h.trades[0].taker_order_id, 2);
  EXPECT_EQ(h.trades[0].maker_order_id, 1);

  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid().price, 10000);
  EXPECT_EQ(h.engine.book().best_bid().quantity, 5);
  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
}

TEST(MatchingEngineTest, LimitBuyExactlyConsumesResting) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10100,
      .quantity = 10,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 10);

  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, LimitBuyCrossesMultiLevel) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 5,
  });
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10100,
      .quantity = 5,
  });
  h.engine.process_order(Order{
      .order_id = 3,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10200,
      .quantity = 8,
  });

  ASSERT_EQ(h.trades.size(), 2);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
  EXPECT_EQ(h.trades[0].maker_order_id, 1);
  EXPECT_EQ(h.trades[1].price, 10100);
  EXPECT_EQ(h.trades[1].quantity, 3);
  EXPECT_EQ(h.trades[1].maker_order_id, 2);

  EXPECT_FALSE(h.engine.book().empty(Side::Sell));
  EXPECT_EQ(h.engine.book().best_ask().price, 10100);
  EXPECT_EQ(h.engine.book().best_ask().quantity, 2);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, IocPartialFill) {
  // Add a sell limit order at 10000, qty 10
  // Process a buy IOC at 10000, qty 20
  // Assert: trade event with qty=10
  // Assert: no resting buy order (remainder was cancelled)
}

TEST(MatchingEngineTest, IocNoLiquidity) {
  // Process a buy IOC at 10000, qty 5 on empty book
  // Assert: no trade events
}

TEST(MatchingEngineTest, FokFillsEntirely) {
  // Add a sell limit order at 10000, qty 10
  // Process a buy FOK at 10000, qty 10
  // Assert: trade event with qty=10
  // Assert: sell order consumed
}

TEST(MatchingEngineTest, FokInsufficientLiquidity) {
  // Add a sell limit order at 10000, qty 5
  // Process a buy FOK at 10000, qty 10
  // Assert: no trade events (order entirely rejected)
  // Assert: sell order still in book
}
