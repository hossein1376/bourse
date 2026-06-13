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
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
  EXPECT_EQ(h.trades[0].taker_side, Side::Buy);
  EXPECT_EQ(h.trades[0].maker_order_id, 1);

  EXPECT_FALSE(h.engine.book().empty(Side::Sell));
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 5);
  EXPECT_EQ(h.engine.book().best_ask()->filled, 5);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, MarketBuyEmptyBookNoCrash) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
}

TEST(MatchingEngineTest, MarketSellFillsLimitBuy) {
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
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
  EXPECT_EQ(h.trades[0].taker_side, Side::Sell);
  EXPECT_EQ(h.trades[0].maker_order_id, 1);

  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid()->quantity, 5);
  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
}

TEST(MatchingEngineTest, MarketBuyPartialFill) {
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
      .type = OrderType::Market,
      .price = 0,
      .quantity = 15,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 10);

  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, MarketBuyCrossesMultiLevel) {
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
      .type = OrderType::Market,
      .price = 0,
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
  EXPECT_EQ(h.engine.book().best_ask()->price, 10100);
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 2);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
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
  EXPECT_EQ(h.engine.book().best_ask()->price, 10000);
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 5);
  EXPECT_EQ(h.engine.book().best_ask()->filled, 5);
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
  EXPECT_EQ(h.engine.book().best_bid()->price, 10100);
  EXPECT_EQ(h.engine.book().best_bid()->quantity, 5);
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
  EXPECT_EQ(h.engine.book().best_bid()->price, 9900);
  EXPECT_EQ(h.engine.book().best_bid()->quantity, 5);
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
  EXPECT_EQ(h.engine.book().best_bid()->price, 10000);
  EXPECT_EQ(h.engine.book().best_bid()->quantity, 5);
  EXPECT_EQ(h.engine.book().best_bid()->filled, 5);
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
  EXPECT_EQ(h.engine.book().best_ask()->price, 10100);
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 2);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, IocPartialFill) {
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
      .type = OrderType::IOC,
      .price = 10000,
      .quantity = 20,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 10);

  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, IocNoLiquidity) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::IOC,
      .price = 10000,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
}

TEST(MatchingEngineTest, IocSellNoLiquidity) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::IOC,
      .price = 10000,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
}

TEST(MatchingEngineTest, IocPriceNotMet) {
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
      .type = OrderType::IOC,
      .price = 9900,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
  EXPECT_FALSE(h.engine.book().empty(Side::Sell));
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 10);
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, FokFillsEntirely) {
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
      .type = OrderType::FOK,
      .price = 10000,
      .quantity = 10,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 10);

  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, FokInsufficientLiquidity) {
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
      .side = Side::Buy,
      .type = OrderType::FOK,
      .price = 10000,
      .quantity = 10,
  });

  EXPECT_TRUE(h.trades.empty());
  EXPECT_FALSE(h.engine.book().empty(Side::Sell));
  EXPECT_EQ(h.engine.book().best_ask()->quantity, 5);
}

TEST(MatchingEngineTest, FokNoLiquidity) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::FOK,
      .price = 10000,
      .quantity = 5,
  });

  EXPECT_TRUE(h.trades.empty());
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, FokSellFillsEntirely) {
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
      .type = OrderType::FOK,
      .price = 9900,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);

  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid()->quantity, 5);
  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
}

TEST(MatchingEngineTest, CancelRestingLimitOrder) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });

  EXPECT_TRUE(h.engine.cancel_order(1));
  EXPECT_TRUE(h.engine.book().empty(Side::Buy));
}

TEST(MatchingEngineTest, CancelOrderFromAskSide) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });

  EXPECT_TRUE(h.engine.cancel_order(1));
  EXPECT_TRUE(h.engine.book().empty(Side::Sell));
}

TEST(MatchingEngineTest, CancelNonExistentOrder) {
  TestHarness h;
  EXPECT_FALSE(h.engine.cancel_order(999));
}

TEST(MatchingEngineTest, AmendOrderChangesPrice) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Buy,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });

  EXPECT_TRUE(h.engine.amend_order(1, Order{
                                          .order_id = 2,
                                          .side = Side::Buy,
                                          .type = OrderType::Limit,
                                          .price = 10100,
                                          .quantity = 10,
                                      }));

  EXPECT_FALSE(h.engine.book().empty(Side::Buy));
  EXPECT_EQ(h.engine.book().best_bid()->price, 10100);
}

TEST(MatchingEngineTest, AmendNonExistentOrder) {
  TestHarness h;
  EXPECT_FALSE(h.engine.amend_order(999, Order{
                                             .order_id = 1,
                                             .side = Side::Buy,
                                             .type = OrderType::Limit,
                                             .price = 10000,
                                             .quantity = 10,
                                         }));
}

TEST(MatchingEngineTest, AmendOrderCrossesSpread) {
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

  EXPECT_TRUE(h.engine.amend_order(2, Order{
                                          .order_id = 3,
                                          .side = Side::Buy,
                                          .type = OrderType::Limit,
                                          .price = 10100,
                                          .quantity = 5,
                                      }));

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].price, 10000);
  EXPECT_EQ(h.trades[0].quantity, 5);
}

TEST(MatchingEngineTest, SequenceStartsAtOne) {
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
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].sequence, 1);
}

TEST(MatchingEngineTest, SequenceIncrementsAcrossOrders) {
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
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });
  h.engine.process_order(Order{
      .order_id = 3,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 10,
  });
  h.engine.process_order(Order{
      .order_id = 4,
      .side = Side::Buy,
      .type = OrderType::Market,
      .price = 0,
      .quantity = 3,
  });

  ASSERT_EQ(h.trades.size(), 2);
  EXPECT_EQ(h.trades[0].sequence, 1);
  EXPECT_EQ(h.trades[1].sequence, 2);
}

TEST(MatchingEngineTest, SequenceSkipsOnRejectedFok) {
  TestHarness h;
  h.engine.process_order(Order{
      .order_id = 1,
      .side = Side::Sell,
      .type = OrderType::Limit,
      .price = 10000,
      .quantity = 5,
  });
  // FOK buy 10 — rejected, no trades
  h.engine.process_order(Order{
      .order_id = 2,
      .side = Side::Buy,
      .type = OrderType::FOK,
      .price = 10000,
      .quantity = 10,
  });
  // Market buy 5 — fills against remaining 5
  h.engine.process_order(Order{
      .order_id = 3,
      .side = Side::Buy,
      .type = OrderType::Market,
      .price = 0,
      .quantity = 5,
  });

  ASSERT_EQ(h.trades.size(), 1);
  EXPECT_EQ(h.trades[0].sequence, 1);
}
