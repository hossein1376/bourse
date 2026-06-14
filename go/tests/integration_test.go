//go:build integration

package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"testing"
	"time"

	"github.com/coder/websocket"
	"github.com/twmb/franz-go/pkg/kgo"

	"github.com/hossein1376/bourse/go/internal/gate"
	"github.com/hossein1376/bourse/go/internal/kafka"
	"github.com/hossein1376/bourse/go/internal/order"
	"github.com/hossein1376/bourse/go/internal/ws"
)

func TestGatewayE2E(t *testing.T) {
	brokers := []string{"localhost:9092"}
	topic := "orders-eq"

	// — produce an order via the gateway —
	acks := runGateway(t, brokers, topic)

	// — verify we got an ACK —
	select {
	case ack := <-acks:
		if ack.Status != "accepted" {
			t.Fatalf("expected accepted, got %+v", ack)
		}
		if ack.OrderID == 0 {
			t.Fatalf("expected positive order_id, got 0")
		}
		t.Logf("ACK: order_id=%d", ack.OrderID)
	case <-time.After(5 * time.Second):
		t.Fatal("timed out waiting for ACK")
	}

	// — verify the message landed in Kafka —
	verifyKafkaMessage(t, brokers, topic)
}

func runGateway(t *testing.T, brokers []string, topic string,
) chan order.Response {
	t.Helper()

	producer, err := kafka.NewProducer(brokers, topic)
	if err != nil {
		t.Fatalf("producer: %v", err)
	}
	t.Cleanup(producer.Close)

	rl := gate.NewRateLimiter(100, 50)
	handler := ws.NewHandler(producer, rl)

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	addr := ln.Addr().String()

	srv := &http.Server{Handler: handler}
	go srv.Serve(ln)
	t.Cleanup(func() {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		srv.Shutdown(ctx)
	})

	conn, _, err := websocket.Dial(context.Background(), "ws://"+addr, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	t.Cleanup(func() {
		conn.Close(websocket.StatusNormalClosure, "")
	})

	payload, _ := json.Marshal(map[string]any{
		"side":     "buy",
		"type":     "limit",
		"price":    10000,
		"quantity": 10,
	})
	if err := conn.Write(
		context.Background(), websocket.MessageText, payload,
	); err != nil {
		t.Fatalf("write: %v", err)
	}

	acks := make(chan order.Response, 1)
	go func() {
		defer close(acks)
		_, msg, err := conn.Read(context.Background())
		if err != nil {
			t.Logf("read ACK: %v", err)
			return
		}
		var ack order.Response
		if err := json.Unmarshal(msg, &ack); err != nil {
			t.Logf("unmarshal ACK: %v", err)
			return
		}
		acks <- ack
	}()

	return acks
}

func verifyKafkaMessage(t *testing.T, brokers []string,
	topic string) {
	t.Helper()

	cl, err := kgo.NewClient(
		kgo.SeedBrokers(brokers...),
		kgo.ConsumeTopics(topic),
		kgo.ConsumerGroup("gateway-integration-test"),
	)
	if err != nil {
		t.Fatalf("kgo: %v", err)
	}
	t.Cleanup(cl.Close)

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	for {
		fetches := cl.PollFetches(ctx)
		if fetches.IsClientClosed() {
			break
		}
		iter := fetches.RecordIter()
		for !iter.Done() {
			rec := iter.Next()
			t.Logf("Kafka message on %s/%d offset %d (%d bytes)",
				rec.Topic, rec.Partition, rec.Offset, len(rec.Value))
			// We just verify the message exists — schema check is optional
			return
		}
		if ctx.Err() != nil {
			t.Fatal("timed out waiting for Kafka message")
		}
	}
}

func TestMain(m *testing.M) {
	// quick connectivity check
	cl, err := kgo.NewClient(kgo.SeedBrokers("localhost:9092"))
	if err == nil {
		cl.Close()
	} else {
		fmt.Fprintf(os.Stderr,
			"Kafka not available, skipping integration tests: %v\n", err)
		os.Exit(0)
	}
	os.Exit(m.Run())
}
