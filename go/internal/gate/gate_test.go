package gate

import (
	"testing"
	"time"
)

func TestRateLimiterAllows(t *testing.T) {
	rl := NewRateLimiter(100, 10)
	if !rl.Allow("client1") {
		t.Error("expected first request to be allowed")
	}
}

func TestRateLimiterExhaustsBurst(t *testing.T) {
	rl := NewRateLimiter(1000, 5)
	for range 5 {
		if !rl.Allow("client2") {
			t.Fatal("expected burst to allow 5 requests")
		}
	}
	if rl.Allow("client2") {
		t.Error("expected 6th request to be denied")
	}
}

func TestRateLimiterSeparateClients(t *testing.T) {
	rl := NewRateLimiter(1000, 1)
	if !rl.Allow("a") {
		t.Error("expected a to be allowed")
	}
	if rl.Allow("a") {
		t.Error("expected a to be denied")
	}
	if !rl.Allow("b") {
		t.Error("expected b to be allowed (separate bucket)")
	}
}

func TestDeduperMiss(t *testing.T) {
	d := NewDeduper(time.Minute)
	if _, ok := d.Check(42); ok {
		t.Error("expected miss for unseen id")
	}
}

func TestDeduperHit(t *testing.T) {
	d := NewDeduper(time.Minute)
	d.Store(42, 100)
	offset, ok := d.Check(42)
	if !ok {
		t.Fatal("expected hit after store")
	}
	if offset != 100 {
		t.Fatalf("expected offset=100, got %d", offset)
	}
}

func TestDeduperExpiry(t *testing.T) {
	d := NewDeduper(time.Millisecond)
	d.Store(42, 100)
	time.Sleep(2 * time.Millisecond)
	if _, ok := d.Check(42); ok {
		t.Error("expected miss after TTL expiry")
	}
}
