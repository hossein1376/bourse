#include "consumer.hpp"
#include "engine/matching_engine.hpp"
#include "producer.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

static engine::KafkaConsumer *global_consumer = nullptr;
static std::atomic<bool> running = true;

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  signal(SIGINT, [](int) { running = false; });

  std::string brokers = "localhost:9092";
  std::string topic = "orders-eq";

  engine::KafkaProducer trade_producer(brokers, "trades-eq");
  engine::KafkaProducer book_producer(brokers, "book-updates-eq");

  engine::MatchingEngine engine(
      [&](const engine::TradeEvent &t) { trade_producer.produce_trade(t); },
      [&](const engine::OrderBook &b) { book_producer.produce_book(b); });

  engine::KafkaConsumer consumer(brokers, topic);
  global_consumer = &consumer;

  consumer.start(engine);
  std::cerr << "kafka_engine started. brokers=" << brokers << " topic=" << topic
            << std::endl;

  while (running)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "\nShutting down...\n";
  consumer.stop();
}
