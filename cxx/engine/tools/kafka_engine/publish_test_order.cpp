#include "order_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <rdkafkacpp.h>
#include <iostream>

int main() {
  flatbuffers::FlatBufferBuilder fb(256);
  auto fbs = engine::schema::CreateOrder(fb, 1, 0, 0, 10000, 10);
  fb.Finish(fbs);

  std::string errstr;
  auto *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", "localhost:9092", errstr);
  auto *producer = RdKafka::Producer::create(conf, errstr);
  delete conf;

  producer->produce("orders-eq", RdKafka::Topic::PARTITION_UA,
      RdKafka::Producer::RK_MSG_COPY,
      const_cast<void *>((const void *)fb.GetBufferPointer()),
      fb.GetSize(), nullptr, 0, 0, nullptr);
  producer->flush(5000);

  std::cerr << "Test order published to orders-eq\n";
  delete producer;
}
