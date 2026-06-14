#include "consumer.hpp"
#include "order_generated.h"

namespace engine {

KafkaConsumer::KafkaConsumer(const std::string &brokers,
                             const std::string &topic)
    : topic_(topic) {
  std::string errstr;
  auto *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  conf->set("group.id", "kafka_engine", errstr);
  conf->set("enable.auto.commit", "false", errstr);
  conf->set("auto.offset.reset", "earliest", errstr);
  consumer_ = RdKafka::KafkaConsumer::create(conf, errstr);
  delete conf;
}

KafkaConsumer::~KafkaConsumer() {
  stop();
  delete consumer_;
}

void KafkaConsumer::start(MatchingEngine &engine) {
  running_ = true;
  consumer_->assign({RdKafka::TopicPartition::create(topic_, 0)});

  thread_ = std::thread([this, &engine] {
    while (running_) {
      auto *msg = consumer_->consume(1000);
      if (!msg || msg->err() == RdKafka::ERR__TIMED_OUT) {
        delete msg;
        continue;
      }
      if (msg->err()) {
        delete msg;
        continue;
      }

      auto *fbs = engine::schema::GetOrder(msg->payload());
      engine.process_order(Order{
          .order_id = fbs->order_id(),
          .side = static_cast<Side>(fbs->side()),
          .type = static_cast<OrderType>(fbs->type()),
          .price = fbs->price(),
          .quantity = fbs->quantity(),
      });

      consumer_->commitSync(msg);
      delete msg;
    }
  });
}

void KafkaConsumer::stop() {
  if (!running_.exchange(false))
    return;
  if (thread_.joinable())
    thread_.join();
  consumer_->close();
}

} // namespace engine
