#pragma once

#include "engine/order_book.hpp"
#include "engine/types.hpp"
#include <rdkafkacpp.h>
#include <string>

namespace engine {

class KafkaProducer {
public:
  explicit KafkaProducer(const std::string &brokers, const std::string &topic,
                         int l2_depth_ = 10);
  ~KafkaProducer();

  void produce_trade(const TradeEvent &event);
  void produce_book(const OrderBook &book);

private:
  RdKafka::Producer *producer_;
  std::string topic_;
  int l2_depth_;
};

} // namespace engine
