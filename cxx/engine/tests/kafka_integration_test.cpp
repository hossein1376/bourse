#include "book_update_generated.h"
#include "order_generated.h"
#include "trade_event_generated.h"
#include "engine/matching_engine.hpp"
#include "engine/types.hpp"
#include <flatbuffers/flatbuffers.h>
#include <gtest/gtest.h>
#include <rdkafkacpp.h>
#include <thread>

using namespace engine;

static std::string brokers = "localhost:9092";

static RdKafka::Producer *create_producer() {
  std::string errstr;
  auto *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  auto *p = RdKafka::Producer::create(conf, errstr);
  delete conf;
  return p;
}

static RdKafka::KafkaConsumer *create_consumer(const std::string &topic) {
  std::string errstr;
  auto *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  conf->set("group.id", "integ-test", errstr);
  conf->set("auto.offset.reset", "earliest", errstr);
  auto *c = RdKafka::KafkaConsumer::create(conf, errstr);
  delete conf;
  c->assign({RdKafka::TopicPartition::create(topic, 0)});
  return c;
}

static void publish_order(RdKafka::Producer *p, OrderID id, Side side,
                          OrderType type, Price price, Quantity qty) {
  flatbuffers::FlatBufferBuilder fb(256);
  auto fbs = engine::schema::CreateOrder(
      fb, id, static_cast<uint8_t>(side), static_cast<uint8_t>(type), price,
      qty);
  fb.Finish(fbs);

  p->produce("orders-eq", RdKafka::Topic::PARTITION_UA,
             RdKafka::Producer::RK_MSG_COPY,
             const_cast<void *>((const void *)fb.GetBufferPointer()),
             fb.GetSize(), nullptr, 0, 0, nullptr);
  p->poll(0);
}

TEST(KafkaIntegration, RoundTrip) {
  auto *producer = create_producer();

  // Probe RedPanda
  if (!producer) {
    GTEST_SKIP() << "RedPanda not available at " << brokers;
  }
  producer->produce("orders-eq", RdKafka::Topic::PARTITION_UA,
                    RdKafka::Producer::RK_MSG_COPY, (void *)"probe", 5,
                    nullptr, 0, 0, nullptr);
  if (producer->flush(2000) != RdKafka::ERR_NO_ERROR) {
    delete producer;
    GTEST_SKIP() << "RedPanda not available at " << brokers;
  }

  std::vector<TradeEvent> trades;
  MatchingEngine engine([&](const TradeEvent &t) { trades.push_back(t); });

  // Publish and process orders
  publish_order(producer, 1, Side::Sell, OrderType::Limit, 10000, 10);
  publish_order(producer, 2, Side::Buy, OrderType::Market, 0, 5);
  publish_order(producer, 3, Side::Buy, OrderType::Limit, 9900, 5);

  engine.process_order(Order{
      .order_id = 1, .side = Side::Sell, .type = OrderType::Limit,
      .price = 10000, .quantity = 10});
  engine.process_order(Order{
      .order_id = 2, .side = Side::Buy, .type = OrderType::Market,
      .price = 0, .quantity = 5});
  engine.process_order(Order{
      .order_id = 3, .side = Side::Buy, .type = OrderType::Limit,
      .price = 9900, .quantity = 5});

  producer->flush(5000);

  // Produce trades to RedPanda
  for (const auto &t : trades) {
    flatbuffers::FlatBufferBuilder fb(256);
    auto fbs = engine::schema::CreateTradeEvent(
        fb, t.taker_order_id, t.maker_order_id,
        static_cast<uint8_t>(t.taker_side), t.price, t.quantity, t.sequence);
    fb.Finish(fbs);
    producer->produce(
        "trades-eq", RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<void *>((const void *)fb.GetBufferPointer()), fb.GetSize(),
        nullptr, 0, 0, nullptr);
    producer->poll(0);
  }
  producer->flush(5000);

  // Consume trades back from RedPanda
  auto *consumer = create_consumer("trades-eq");
  std::vector<TradeEvent> consumed;
  for (int i = 0; i < 10; ++i) {
    auto *msg = consumer->consume(500);
    if (msg && msg->err() == RdKafka::ERR_NO_ERROR) {
      auto *fbs = engine::schema::GetTradeEvent(msg->payload());
      consumed.push_back(TradeEvent{
          .taker_order_id = fbs->taker_order_id(),
          .maker_order_id = fbs->maker_order_id(),
          .taker_side = static_cast<Side>(fbs->taker_side()),
          .price = fbs->price(),
          .quantity = fbs->quantity(),
          .sequence = fbs->sequence(),
      });
    }
    delete msg;
    if (consumed.size() >= trades.size()) break;
  }
  delete consumer;
  delete producer;

  ASSERT_EQ(consumed.size(), trades.size());
  for (size_t i = 0; i < consumed.size(); ++i) {
    EXPECT_EQ(consumed[i].taker_order_id, trades[i].taker_order_id);
    EXPECT_EQ(consumed[i].price, trades[i].price);
    EXPECT_EQ(consumed[i].quantity, trades[i].quantity);
    EXPECT_EQ(consumed[i].sequence, trades[i].sequence);
  }
}
