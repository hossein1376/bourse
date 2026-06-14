package order

import (
	"encoding/json"
	"testing"
)

func TestSideUnmarshalText(t *testing.T) {
	tests := []struct {
		input string
		want  Side
		err   bool
	}{
		{"buy", SideBuy, false},
		{"sell", SideSell, false},
		{"BUY", 0, true},
		{"", 0, true},
	}
	for _, tc := range tests {
		var got Side
		err := got.UnmarshalText([]byte(tc.input))
		if tc.err && err == nil {
			t.Errorf("UnmarshalText(%q) expected error", tc.input)
		}
		if !tc.err && got != tc.want {
			t.Errorf("UnmarshalText(%q) = %d, want %d", tc.input, got, tc.want)
		}
	}
}

func TestTypeUnmarshalText(t *testing.T) {
	tests := []struct {
		input string
		want  Type
		err   bool
	}{
		{"limit", TypeLimit, false},
		{"market", TypeMarket, false},
		{"ioc", TypeIOC, false},
		{"fok", TypeFOK, false},
		{"MARKET", 0, true},
		{"", 0, true},
	}
	for _, tc := range tests {
		var got Type
		err := got.UnmarshalText([]byte(tc.input))
		if tc.err && err == nil {
			t.Errorf("UnmarshalText(%q) expected error", tc.input)
		}
		if !tc.err && got != tc.want {
			t.Errorf("UnmarshalText(%q) = %d, want %d", tc.input, got, tc.want)
		}
	}
}

func TestUnmarshalJSON(t *testing.T) {
	data := `{"order_id":1,"side":"buy","type":"limit"` +
		`,"price":10000,"quantity":10}`
	var o Order
	if err := json.Unmarshal([]byte(data), &o); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}
	if o.OrderID != 1 {
		t.Errorf("expected OrderID preserved (=1), got %d", o.OrderID)
	}
	if o.Side != SideBuy || o.Type != TypeLimit || o.Price != 10000 ||
		o.Quantity != 10 {
		t.Errorf("unexpected Order: %+v", o)
	}
}

func TestUnmarshalJSON_NoOrderID(t *testing.T) {
	data := `{"side":"sell","type":"market","price":0,"quantity":5}`
	var o Order
	if err := json.Unmarshal([]byte(data), &o); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}
	if o.OrderID != 0 {
		t.Errorf("expected OrderID default (=0), got %d", o.OrderID)
	}
	if o.Side != SideSell || o.Type != TypeMarket || o.Quantity != 5 {
		t.Errorf("unexpected Order: %+v", o)
	}
}

func TestUnmarshalJSON_InvalidSide(t *testing.T) {
	data := `{"side":"foo","type":"limit","price":100,"quantity":1}`
	var o Order
	err := json.Unmarshal([]byte(data), &o)
	if err == nil {
		t.Fatal("expected error for invalid side")
	}
}

func TestMarshalBinaryRoundTrip(t *testing.T) {
	o := Order{
		OrderID:  42,
		Side:     SideBuy,
		Type:     TypeLimit,
		Price:    10000,
		Quantity: 10,
	}
	buf, err := o.MarshalBinary()
	if err != nil {
		t.Fatalf("MarshalBinary: %v", err)
	}
	var got Order
	if err := got.UnmarshalBinary(buf); err != nil {
		t.Fatalf("UnmarshalBinary: %v", err)
	}
	if got.OrderID != 42 || got.Side != SideBuy || got.Type != TypeLimit ||
		got.Price != 10000 || got.Quantity != 10 {
		t.Errorf("round-trip mismatch: %+v", got)
	}
}

func TestValidate(t *testing.T) {
	tests := []struct {
		name string
		o    Order
		err  bool
	}{
		{"limit", Order{Side: SideBuy, Type: TypeLimit, Price: 100, Quantity: 10}, false},
		{"market", Order{Side: SideSell, Type: TypeMarket, Quantity: 10}, false},
		{"zero qty", Order{Price: 100}, true},
		{"zero price", Order{Side: SideBuy, Type: TypeLimit, Quantity: 10}, true},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.o.Validate()
			if tc.err && err == nil {
				t.Error("expected error")
			}
			if !tc.err && err != nil {
				t.Errorf("unexpected error: %v", err)
			}
		})
	}
}

func TestAcceptedResponse(t *testing.T) {
	r := Accepted(42)
	if r.Status != "accepted" || r.OrderID != 42 {
		t.Errorf("unexpected response: %+v", r)
	}
}

func TestRejectedResponse(t *testing.T) {
	r := Rejected("bad order")
	if r.Status != "rejected" || r.Reason != "bad order" {
		t.Errorf("unexpected response: %+v", r)
	}
}
