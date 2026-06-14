package gate

import (
	"sync"
	"time"
)

// Deduper detects duplicate order submissions using the client-provided
// order_id as a dedup key. If the same order_id arrives within the TTL
// window, the cached Kafka offset is returned and no message is produced.
//
// Stage 3 limitations:
//   - The map is in-memory only. On restart, all entries are lost.
//     A retry within the TTL window after a restart will produce a
//     duplicate order in Kafka. The matching engine handles this
//     gracefully — a duplicate order with a different order_id will
//     either rest in the book or match against existing liquidity.
//   - Expired entries are only cleaned up via AfterFunc goroutines.
//     Under sustained load the map grows linearly with unique order_ids.
//     A future version should use a priority queue or ring buffer
//     for O(1) eviction.
//   - The Store-after-Produce gap is minimized by calling Store
//     inside Producer.Produce before returning. The remaining gap is
//     a few lines of Go statements between Store and the return to
//     the handler.
//   - Stage 5 (Settlement) can own persistent dedup state via its
//     database. The Settlement service processes TradeEvents from
//     Kafka and could maintain a seen-ids table, making the gateway's
//     in-memory deduper a best-effort frontend rather than the source
//     of truth. A TradeEvent with a duplicate order_id would be
//     rejected at settlement with an idempotency check.
type Deduper struct {
	mu   sync.Mutex
	seen map[uint64]uint64
	ttl  time.Duration
}

func NewDeduper(ttl time.Duration) *Deduper {
	return &Deduper{
		seen: make(map[uint64]uint64),
		ttl:  ttl,
	}
}

// Check returns the cached Kafka offset if id was seen within the TTL.
func (d *Deduper) Check(id uint64) (uint64, bool) {
	d.mu.Lock()
	defer d.mu.Unlock()

	offset, ok := d.seen[id]
	if !ok {
		return 0, false
	}
	return offset, true
}

// Store records the id → offset mapping with the current TTL.
func (d *Deduper) Store(id uint64, offset uint64) {
	d.mu.Lock()
	defer d.mu.Unlock()

	d.seen[id] = offset

	// Schedule eviction after TTL. A background goroutine would be
	// cleaner but adds lifecycle complexity. This fires a goroutine
	// per unique id, which is acceptable for Stage 3 volumes.
	// For higher throughput, switch to a timewheel or periodic sweep.
	time.AfterFunc(d.ttl, func() {
		d.mu.Lock()
		delete(d.seen, id)
		d.mu.Unlock()
	})
}
