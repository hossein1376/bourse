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

struct BookTestHarness {
  std::vector<TradeEvent> trades;
  std::vector<const OrderBook *> books;
  MatchingEngine engine;

  BookTestHarness()
      : engine([this](const TradeEvent &t) { trades.push_back(t); },
               [this](const OrderBook &b) { books.push_back(&b); }) {}
};

TEST(MatchingEngineTest, BookCallbackFiresOnEachOrder) {
  BookTestHarness h;

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

  EXPECT_EQ(h.books.size(), 2);
  ASSERT_FALSE(h.books[0]->empty(Side::Sell));
  EXPECT_EQ(h.books[0]->best_ask()->price, 10000);

  ASSERT_FALSE(h.books[1]->empty(Side::Sell));
  EXPECT_EQ(h.books[1]->best_ask()->quantity, 5);
  ASSERT_TRUE(h.books[1]->empty(Side::Buy));
}

TEST(MatchingEngineTest, DeterministicReplay) {
  std::vector<Order> orders = {
      {.order_id = 1,
       .side = Side::Sell,
       .type = OrderType::Limit,
       .price = 10000,
       .quantity = 10},
      {.order_id = 2,
       .side = Side::Buy,
       .type = OrderType::Market,
       .price = 0,
       .quantity = 5},
      {.order_id = 3,
       .side = Side::Buy,
       .type = OrderType::Limit,
       .price = 9900,
       .quantity = 5},
      {.order_id = 4,
       .side = Side::Sell,
       .type = OrderType::Limit,
       .price = 10100,
       .quantity = 10},
      {.order_id = 5,
       .side = Side::Buy,
       .type = OrderType::IOC,
       .price = 10200,
       .quantity = 3},
      {.order_id = 6,
       .side = Side::Buy,
       .type = OrderType::FOK,
       .price = 10300,
       .quantity = 7},
      {.order_id = 7,
       .side = Side::Buy,
       .type = OrderType::Limit,
       .price = 10100,
       .quantity = 12},
      {.order_id = 8,
       .side = Side::Sell,
       .type = OrderType::Limit,
       .price = 9900,
       .quantity = 20},
  };

  auto run = [&](std::vector<TradeEvent> &out) {
    MatchingEngine eng([&](const TradeEvent &t) { out.push_back(t); });
    for (const auto &o : orders)
      eng.process_order(o);
  };

  std::vector<TradeEvent> run1, run2;
  run(run1);
  run(run2);

  ASSERT_EQ(run1.size(), run2.size());
  for (size_t i = 0; i < run1.size(); ++i) {
    EXPECT_EQ(run1[i].taker_order_id, run2[i].taker_order_id);
    EXPECT_EQ(run1[i].maker_order_id, run2[i].maker_order_id);
    EXPECT_EQ(run1[i].taker_side, run2[i].taker_side);
    EXPECT_EQ(run1[i].price, run2[i].price);
    EXPECT_EQ(run1[i].quantity, run2[i].quantity);
    EXPECT_EQ(run1[i].sequence, run2[i].sequence);
  }
}
