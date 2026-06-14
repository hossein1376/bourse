use std::sync::Arc;

use tokio::sync::RwLock;
use tokio::sync::broadcast;

use crate::book::{L1Snapshot, L2Snapshot, OrderBookMirror};

#[derive(Clone, Debug)]
pub enum MarketEvent {
    L1(L1Snapshot),
    L2(L2Snapshot),
}

pub async fn run_server(
    _addr: &str,
    _mirror: Arc<RwLock<OrderBookMirror>>,
    _tx: broadcast::Sender<MarketEvent>,
) {
    todo!()
}
