package kafka

import (
	"context"
	"fmt"

	"github.com/twmb/franz-go/pkg/kgo"

	"github.com/hossein1376/bourse/go/internal/gate"
	"github.com/hossein1376/bourse/go/internal/order"
)

type Producer struct {
	client *kgo.Client
	topic  string
}

func NewProducer(brokers []string, topic string) (*Producer, error) {
	client, err := kgo.NewClient(
		kgo.SeedBrokers(brokers...),
		kgo.DefaultProduceTopic(topic),
	)
	if err != nil {
		return nil, fmt.Errorf("kafka client: %w", err)
	}
	return &Producer{client: client, topic: topic}, nil
}

func (p *Producer) Produce(
	ctx context.Context, o order.Order, d *gate.Deduper,
) (int64, error) {
	buf, err := o.MarshalBinary()
	if err != nil {
		return 0, fmt.Errorf("marshal: %w", err)
	}
	rec := &kgo.Record{Topic: p.topic, Value: buf}
	pres := p.client.ProduceSync(ctx, rec)
	if err := pres.FirstErr(); err != nil {
		return 0, fmt.Errorf("kafka produce: %w", err)
	}
	offset := pres[0].Record.Offset
	if o.OrderID != 0 {
		d.Store(o.OrderID, uint64(offset))
	}
	return offset, nil
}

func (p *Producer) Close() {
	p.client.Close()
}
