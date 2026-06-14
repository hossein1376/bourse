use serde::Serialize;

#[derive(Clone, Debug, Serialize)]
pub struct Level {
    pub price: u64,
    pub quantity: u64,
}

#[derive(Clone, Debug, Serialize)]
pub struct Trade {
    pub price: u64,
    pub quantity: u64,
    pub sequence: u64,
}

#[derive(Clone, Debug, Serialize)]
pub struct L1Snapshot {
    pub best_bid: Option<Level>,
    pub best_ask: Option<Level>,
    pub last_trade: Option<Trade>,
}

#[derive(Clone, Debug, Serialize)]
pub struct L2Snapshot {
    pub bids: Vec<Level>,
    pub asks: Vec<Level>,
    pub last_trade: Option<Trade>,
}

pub struct OrderBookMirror {
    pub bids: Vec<Level>,
    pub asks: Vec<Level>,
    pub last_trade: Option<Trade>,
}

impl OrderBookMirror {
    pub fn new() -> Self {
        todo!()
    }

    pub fn apply_book(&mut self, _update: &crate::schema::BookUpdate) {
        todo!()
    }

    pub fn apply_trade(&mut self, _trade: &crate::schema::TradeEvent) {
        todo!()
    }

    pub fn l1(&self) -> L1Snapshot {
        todo!()
    }

    pub fn l2(&self) -> L2Snapshot {
        todo!()
    }
}
