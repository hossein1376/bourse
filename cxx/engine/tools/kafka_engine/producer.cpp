#include "producer.hpp"
#include "book_update_generated.h"
#include "trade_event_generated.h"
#include <rdkafkacpp.h>

namespace engine {

KafkaProducer::KafkaProducer(const std::string &brokers,
                             const std::string &topic, int l2_depth)
    : topic_(topic), l2_depth_(l2_depth) {
  std::string errstr;
  auto *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  conf->set("client.id", "kafka_engine", errstr);
  producer_ = RdKafka::Producer::create(conf, errstr);
  delete conf;
}

KafkaProducer::~KafkaProducer() {
  producer_->flush(5000);
  delete producer_;
}

void KafkaProducer::produce_trade(const TradeEvent &event) {
  flatbuffers::FlatBufferBuilder fb(256);
  auto fbs = engine::schema::CreateTradeEvent(
      fb, event.taker_order_id, event.maker_order_id,
      static_cast<uint8_t>(event.taker_side), event.price, event.quantity,
      event.sequence);
  fb.Finish(fbs);

  producer_->produce(topic_, RdKafka::Topic::PARTITION_UA,
                     RdKafka::Producer::RK_MSG_COPY,
                     const_cast<void *>((const void *)fb.GetBufferPointer()),
                     fb.GetSize(), nullptr, 0, 0, nullptr);
  producer_->poll(0);
}
void KafkaProducer::produce_book(const OrderBook &book) {
  flatbuffers::FlatBufferBuilder fb(256);

  std::vector<flatbuffers::Offset<engine::schema::Level>> bid_levels;
  int count = 0;
  book.for_each_level(Side::Buy, std::numeric_limits<Price>::max(),
                      [&](Price price, const auto &orders) {
                        if (++count > l2_depth_)
                          return;
                        Quantity total = 0;
                        for (const auto &o : orders)
                          total += o.quantity;
                        bid_levels.push_back(
                            engine::schema::CreateLevel(fb, price, total));
                      });

  count = 0;
  std::vector<flatbuffers::Offset<engine::schema::Level>> ask_levels;
  book.for_each_level(Side::Sell, std::numeric_limits<Price>::max(),
                      [&](Price price, const auto &orders) {
                        if (++count > l2_depth_)
                          return;
                        Quantity total = 0;
                        for (const auto &o : orders)
                          total += o.quantity;
                        ask_levels.push_back(
                            engine::schema::CreateLevel(fb, price, total));
                      });

  // Build BookUpdate
  auto update = engine::schema::CreateBookUpdate(
      fb, fb.CreateVector(bid_levels), fb.CreateVector(ask_levels));
  fb.Finish(update);

  // Produce to Kafka (same pattern as produce_trade)
  producer_->produce(topic_, RdKafka::Topic::PARTITION_UA,
                     RdKafka::Producer::RK_MSG_COPY,
                     const_cast<void *>((const void *)fb.GetBufferPointer()),
                     fb.GetSize(), nullptr, 0, 0, nullptr);
  producer_->poll(0);
}

} // namespace engine
