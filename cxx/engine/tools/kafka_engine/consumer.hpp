#pragma once

#include "engine/matching_engine.hpp"
#include <atomic>
#include <rdkafkacpp.h>
#include <string>
#include <thread>

namespace engine {

class KafkaConsumer {
public:
  explicit KafkaConsumer(const std::string &brokers, const std::string &topic);
  ~KafkaConsumer();

  void start(MatchingEngine &engine);
  void stop();

private:
  RdKafka::KafkaConsumer *consumer_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

} // namespace engine
