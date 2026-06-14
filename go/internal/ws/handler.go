package ws

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"sync"
	"time"

	"github.com/coder/websocket"

	"github.com/hossein1376/bourse/go/internal/gate"
	"github.com/hossein1376/bourse/go/internal/kafka"
	"github.com/hossein1376/bourse/go/internal/order"
)

type Handler struct {
	producer *kafka.Producer
	rateLim  *gate.RateLimiter
	deduper  *gate.Deduper
	mu       sync.Mutex
	clients  map[*client]struct{}
}

func NewHandler(
	producer *kafka.Producer, rl *gate.RateLimiter, d *gate.Deduper,
) *Handler {
	return &Handler{
		producer: producer,
		rateLim:  rl,
		deduper:  d,
		clients:  make(map[*client]struct{}),
	}
}

func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	conn, err := websocket.Accept(w, r, nil)
	if err != nil {
		slog.Error("ws accept", "error", err)
		return
	}

	ctx, cancel := context.WithCancel(context.Background())
	c := &client{
		conn:       conn,
		cancel:     cancel,
		send:       make(chan []byte, 64),
		done:       make(chan struct{}),
		remoteAddr: r.RemoteAddr,
	}

	h.mu.Lock()
	h.clients[c] = struct{}{}
	h.mu.Unlock()

	go c.writePump(ctx)

	go func() {
		defer func() {
			h.mu.Lock()
			delete(h.clients, c)
			h.mu.Unlock()
			c.close()
		}()
		h.readLoop(c)
	}()
}

func (h *Handler) readLoop(c *client) {
	for {
		_, msg, err := c.conn.Read(context.Background())
		if err != nil {
			if _, ok := errors.AsType[websocket.CloseError](err); !ok {
				slog.Error("ws read", "error", err)
			}
			return
		}

		var o order.Order
		if err := json.Unmarshal(msg, &o); err != nil {
			slog.Warn("ws parse", "error", err)
			c.sendMsg(order.Rejected("invalid JSON: " + err.Error()))
			continue
		}

		if err := o.Validate(); err != nil {
			slog.Warn("ws validation", "error", err)
			c.sendMsg(order.Rejected(err.Error()))
			continue
		}

		remote := c.remoteAddr
		if !h.rateLim.Allow(remote) {
			slog.Warn("ws rate limited", "remote", remote)
			c.sendMsg(order.Rejected("rate_limited"))
			continue
		}

		if o.OrderID != 0 {
			if offset, ok := h.deduper.Check(o.OrderID); ok {
				c.sendMsg(order.Accepted(offset))
				continue
			}
		}

		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		offset, err := h.producer.Produce(ctx, o, h.deduper)
		cancel()
		if err != nil {
			slog.Error("ws produce", "error", err)
			c.sendMsg(order.Rejected("internal_error"))
			continue
		}

		c.sendMsg(order.Accepted(uint64(offset)))
	}
}

type client struct {
	conn       *websocket.Conn
	cancel     context.CancelFunc
	send       chan []byte
	done       chan struct{}
	remoteAddr string
}

func (c *client) sendMsg(resp order.Response) {
	data, _ := json.Marshal(resp)
	select {
	case c.send <- data:
	default:
		slog.Warn("ws send buffer full")
	}
}

func (c *client) writePump(ctx context.Context) {
	defer c.conn.Close(websocket.StatusNormalClosure, "")
	for {
		select {
		case msg := <-c.send:
			err := c.conn.Write(ctx, websocket.MessageText, msg)
			if err != nil {
				slog.Error("ws write", "error", err)
				return
			}
		case <-c.done:
			return
		}
	}
}

func (c *client) close() {
	close(c.done)
	c.cancel()
	c.conn.Close(websocket.StatusNormalClosure, "")
}
