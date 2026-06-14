package order

import (
	"errors"
	"fmt"

	flatbuffers "github.com/google/flatbuffers/go"

	"github.com/hossein1376/bourse/go/internal/schema"
)

type Side uint8

const (
	SideBuy  Side = 0
	SideSell Side = 1
)

func (s Side) String() string {
	switch s {
	case SideBuy:
		return "buy"
	case SideSell:
		return "sell"
	default:
		return "unknown"
	}
}

func (s *Side) UnmarshalText(text []byte) error {
	switch string(text) {
	case "buy":
		*s = SideBuy
		return nil
	case "sell":
		*s = SideSell
		return nil
	default:
		return fmt.Errorf("invalid side: %q", text)
	}
}

func ParseSide(s string) (Side, error) {
	var v Side
	return v, v.UnmarshalText([]byte(s))
}

type Type uint8

const (
	TypeLimit  Type = 0
	TypeMarket Type = 1
	TypeIOC    Type = 2
	TypeFOK    Type = 3
)

func (t Type) String() string {
	switch t {
	case TypeLimit:
		return "limit"
	case TypeMarket:
		return "market"
	case TypeIOC:
		return "ioc"
	case TypeFOK:
		return "fok"
	default:
		return "unknown"
	}
}

func (t *Type) UnmarshalText(text []byte) error {
	switch string(text) {
	case "limit":
		*t = TypeLimit
		return nil
	case "market":
		*t = TypeMarket
		return nil
	case "ioc":
		*t = TypeIOC
		return nil
	case "fok":
		*t = TypeFOK
		return nil
	default:
		return fmt.Errorf("invalid order type: %q", text)
	}
}

func ParseType(s string) (Type, error) {
	var v Type
	return v, v.UnmarshalText([]byte(s))
}

type Order struct {
	OrderID  uint64 `json:"order_id"`
	Side     Side   `json:"side"`
	Type     Type   `json:"type"`
	Price    uint64 `json:"price"`
	Quantity uint64 `json:"quantity"`
}

func (o *Order) Validate() error {
	if o.Price == 0 && o.Type == TypeLimit {
		return errors.New("limit order must have a positive price")
	}
	if o.Quantity == 0 {
		return errors.New("quantity must be positive")
	}
	return nil
}

func (o Order) MarshalBinary() ([]byte, error) {
	b := flatbuffers.NewBuilder(128)
	schema.OrderStart(b)
	schema.OrderAddOrderId(b, o.OrderID)
	schema.OrderAddSide(b, byte(o.Side))
	schema.OrderAddType(b, byte(o.Type))
	schema.OrderAddPrice(b, o.Price)
	schema.OrderAddQuantity(b, o.Quantity)
	off := schema.OrderEnd(b)
	schema.FinishOrderBuffer(b, off)
	return b.FinishedBytes(), nil
}

func (o *Order) UnmarshalBinary(data []byte) error {
	fb := schema.GetRootAsOrder(data, 0)
	o.OrderID = fb.OrderId()
	o.Side = Side(fb.Side())
	o.Type = Type(fb.Type())
	o.Price = fb.Price()
	o.Quantity = fb.Quantity()
	return nil
}

type Response struct {
	Status   string `json:"status"`
	OrderID  uint64 `json:"order_id,omitempty"`
	Sequence uint64 `json:"sequence,omitempty"`
	Reason   string `json:"reason,omitempty"`
}

func Accepted(id uint64) Response {
	return Response{Status: "accepted", OrderID: id}
}

func Rejected(reason string) Response {
	return Response{Status: "rejected", Reason: reason}
}
