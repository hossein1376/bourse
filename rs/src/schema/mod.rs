#![allow(dead_code, unused_imports, clippy::extra_unused_lifetimes)]

pub mod book_update_generated;
pub mod trade_event_generated;

pub use book_update_generated::engine::schema::{BookUpdate, Level, root_as_book_update};
pub use trade_event_generated::engine::schema::{TradeEvent, root_as_trade_event};
