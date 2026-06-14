use std::sync::Arc;

use tokio::sync::RwLock;
use tokio::sync::broadcast;

use crate::book::OrderBookMirror;
use crate::ws::MarketEvent;

pub async fn run_consumer(
    _brokers: &str,
    _symbol: &str,
    _group_id: &str,
    _mirror: Arc<RwLock<OrderBookMirror>>,
    _tx: broadcast::Sender<MarketEvent>,
) {
    todo!()
}
