package main

import (
	"errors"
	"flag"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/hossein1376/bourse/go/internal/gate"
	"github.com/hossein1376/bourse/go/internal/kafka"
	"github.com/hossein1376/bourse/go/internal/ws"
)

func main() {
	addr := flag.String("addr", ":8080", "WebSocket listen address")
	brokers := flag.String(
		"brokers", "localhost:9092", "Kafka brokers (csv)",
	)
	topic := flag.String("topic", "orders-eq", "Kafka topic for orders")
	rateLimit := flag.Float64("rate-limit", 10, "max orders/sec per client")
	burst := flag.Float64("burst", 20, "max burst per client")
	dedupTTL := flag.Duration("dedup-ttl", 5*time.Minute, "dedup TTL")
	flag.Parse()

	producer, err := kafka.NewProducer(strings.Split(*brokers, ","), *topic)
	if err != nil {
		slog.Error("kafka producer", "error", err)
		os.Exit(1)
	}
	defer producer.Close()

	rl := gate.NewRateLimiter(*rateLimit, *burst)
	deduper := gate.NewDeduper(*dedupTTL)
	handler := ws.NewHandler(producer, rl, deduper)

	mux := http.NewServeMux()
	mux.Handle("/ws", handler)

	server := &http.Server{Addr: *addr, Handler: mux}

	go func() {
		slog.Info("listening", "addr", *addr, "topic", *topic)
		err := server.ListenAndServe()
		if err != nil && !errors.Is(err, http.ErrServerClosed) {
			slog.Error("serve", "error", err)
			os.Exit(1)
		}
	}()

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig
	slog.Info("shutting down")
	server.Close()
}
