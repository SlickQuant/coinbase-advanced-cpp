// SPDX-License-Identifier: MIT
// Example 4: Receive market data for multiple symbols using a shared
// stream_buffer_multiplexer with UserThreadWebsocketCallbacks.
//
// A single UserThreadWebsocketCallbacks instance handles both symbols.
// The WebSocket I/O thread writes data into per-symbol producer buffers;
// a single processData() call on the user thread drains all symbols at once.
// This makes it safe to maintain per-symbol order books without locks.
//
// No API credentials are required for public market-data channels.

#include <chrono>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <thread>
#include <unordered_map>

#include <slick/net/logging.hpp>
#include <slick/stream_buffer_multiplexer.hpp>

#include <coinbase/websocket.hpp>

static constexpr const char* MD_URL = "wss://advanced-trade-ws.coinbase.com";

// ---------------------------------------------------------------------------
// Per-symbol order book
// ---------------------------------------------------------------------------

struct OrderBook {
    std::map<double, double, std::greater<double>> bids;  // best (highest) first
    std::map<double, double>                        asks;  // best (lowest) first

    void applySnapshot(const coinbase::Level2UpdateBatch& snapshot) {
        bids.clear();
        asks.clear();
        for (const auto& u : snapshot.updates) {
            if (u.side == coinbase::Side::BUY) bids[u.price_level] = u.new_quantity;
            else                               asks[u.price_level] = u.new_quantity;
        }
    }

    void applyUpdate(const coinbase::Level2UpdateBatch& update) {
        for (const auto& u : update.updates) {
            if (u.side == coinbase::Side::BUY) {
                if (u.new_quantity == 0.0) bids.erase(u.price_level);
                else                       bids[u.price_level] = u.new_quantity;
            } else {
                if (u.new_quantity == 0.0) asks.erase(u.price_level);
                else                       asks[u.price_level] = u.new_quantity;
            }
        }
    }

    void print(const std::string& symbol) const {
        LOG_INFO("--- {} Order Book (top 5) ---", symbol);
        int n = 0;
        for (const auto& [price, qty] : bids) {
            LOG_INFO("  bid  {}  qty={}", price, qty);
            if (++n >= 5) break;
        }
        n = 0;
        for (const auto& [price, qty] : asks) {
            LOG_INFO("  ask  {}  qty={}", price, qty);
            if (++n >= 5) break;
        }
        LOG_INFO("-----------------------------");
    }
};

// ---------------------------------------------------------------------------
// Callback implementation — one instance shared by both symbols
// ---------------------------------------------------------------------------

class MultiSymbolCallbacks : public coinbase::UserThreadWebsocketCallbacks {
public:
    // Register a client-to-symbol mapping before subscribe().
    void registerClient(coinbase::WebSocketClient* ws, std::string symbol) {
        symbol_map_[ws] = symbol;
        book_map_[symbol];  // default-construct OrderBook
    }

    // --- connection lifecycle ---

    void onMarketDataConnected(coinbase::WebSocketClient* ws) override {
        LOG_INFO("[{}] connected", sym(ws));
    }
    void onMarketDataDisconnected(coinbase::WebSocketClient* ws) override {
        LOG_INFO("[{}] disconnected", sym(ws));
    }
    void onUserDataConnected(coinbase::WebSocketClient*) override {}
    void onUserDataDisconnected(coinbase::WebSocketClient*) override {}

    // --- level 2 order book ---

    void onLevel2Snapshot(coinbase::WebSocketClient* ws, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& snapshot) override {
        auto& s = sym(ws);
        book_map_[s].applySnapshot(snapshot);
        LOG_INFO("[{}] L2 snapshot seq={} bids={} asks={}", s, seq_num,
                 book_map_[s].bids.size(), book_map_[s].asks.size());
    }
    void onLevel2Updates(coinbase::WebSocketClient* ws, uint64_t /*seq_num*/,
                         const coinbase::Level2UpdateBatch& update) override {
        book_map_[sym(ws)].applyUpdate(update);
    }

    // --- market trades ---

    void onMarketTradesSnapshot(coinbase::WebSocketClient* ws, uint64_t seq_num,
                                const std::vector<coinbase::MarketTrade>& trades) override {
        LOG_INFO("[{}] trades snapshot seq={} count={}", sym(ws), seq_num, trades.size());
    }
    void onMarketTrades(coinbase::WebSocketClient* ws, uint64_t seq_num,
                        const std::vector<coinbase::MarketTrade>& trades) override {
        for (const auto& t : trades) {
            LOG_INFO("[{}] trade seq={} price={} size={} side={}", sym(ws), seq_num,
                     t.price, t.size, t.side == coinbase::Side::BUY ? "buy" : "sell");
        }
    }

    // --- ticker ---

    void onTickerSnapshot(coinbase::WebSocketClient* ws, uint64_t seq_num, uint64_t,
                          const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[{}] ticker snapshot seq={} price={} bid={} ask={}",
                     sym(ws), seq_num, tk.price, tk.best_bid, tk.best_ask);
        }
    }
    void onTickers(coinbase::WebSocketClient* ws, uint64_t seq_num, uint64_t,
                   const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[{}] ticker seq={} price={} bid={} ask={}",
                     sym(ws), seq_num, tk.price, tk.best_bid, tk.best_ask);
        }
    }

    // --- candles / status (not subscribed, no-ops) ---

    void onCandlesSnapshot(coinbase::WebSocketClient*, uint64_t, uint64_t,
                           const std::vector<coinbase::Candle>&) override {}
    void onCandles(coinbase::WebSocketClient*, uint64_t, uint64_t,
                   const std::vector<coinbase::Candle>&) override {}
    void onStatusSnapshot(coinbase::WebSocketClient*, uint64_t, uint64_t,
                          const std::vector<coinbase::Status>&) override {}
    void onStatus(coinbase::WebSocketClient*, uint64_t, uint64_t,
                  const std::vector<coinbase::Status>&) override {}

    // --- sequence gap ---

    void onMarketDataGap(coinbase::WebSocketClient* ws) override {
        LOG_WARN("[{}] sequence gap detected", sym(ws));
    }
    void onUserDataGap(coinbase::WebSocketClient*) override {}

    // --- user data (not subscribed, no-ops) ---

    void onUserDataSnapshot(coinbase::WebSocketClient*, uint64_t,
                            const std::vector<coinbase::Order>&,
                            const std::vector<coinbase::PerpetualFuturePosition>&,
                            const std::vector<coinbase::ExpiringFuturePosition>&) override {}
    void onOrderUpdates(coinbase::WebSocketClient*, uint64_t,
                        const std::vector<coinbase::Order>&) override {}

    // --- errors ---

    void onMarketDataError(coinbase::WebSocketClient* ws, std::string&& err) override {
        LOG_ERROR("[{}] error: {}", sym(ws), err);
    }
    void onUserDataError(coinbase::WebSocketClient*, std::string&&) override {}

    // --- periodic display (called from user thread) ---

    void printAllOrderBooks() {
        for (const auto& [symbol, book] : book_map_) {
            book.print(symbol);
        }
    }

private:
    const std::string& sym(coinbase::WebSocketClient* ws) const {
        static const std::string unknown = "?";
        auto it = symbol_map_.find(ws);
        return it != symbol_map_.end() ? it->second : unknown;
    }

    std::unordered_map<coinbase::WebSocketClient*, std::string> symbol_map_;
    std::unordered_map<std::string, OrderBook> book_map_;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {

    slick::net::set_log_handler(
        [](slick::net::LogLevel level, const char* fmt, std::format_args args) {
            const char* prefix = "?????";
            switch (level) {
                case slick::net::LogLevel::Trace: prefix = "TRACE"; break;
                case slick::net::LogLevel::Debug: prefix = "DEBUG"; break;
                case slick::net::LogLevel::Info:  prefix = "INFO "; break;
                case slick::net::LogLevel::Warn:  prefix = "WARN "; break;
                case slick::net::LogLevel::Error: prefix = "ERROR"; break;
                case slick::net::LogLevel::Fatal: prefix = "FATAL"; break;
                default: break;
            }
            std::cout << '[' << prefix << "] " << std::vformat(fmt, args) << '\n';
        },
        []() { return slick::net::LogLevel::Debug; }
    );

    LOG_INFO("=== Multi-symbol UserThreadWebsocketCallbacks (shared mux) ===");
    LOG_INFO("Single processData() drains both symbols. Order books printed every 5s.");
    LOG_INFO("Press Ctrl-C to exit.");

    // Shared multiplexer — both clients write into the same fan-in queue.
    // Queue size doubled relative to the single-client default.
    slick::stream_buffer_multiplexer mux(1u << 17);

    MultiSymbolCallbacks callbacks;

    // No user-data URL ("") — market data only.
    // producer_offset assigns non-overlapping producer IDs in the shared mux.
    coinbase::WebSocketClient ws_btc(&callbacks, mux, MD_URL, "", 0);
    coinbase::WebSocketClient ws_eth(&callbacks, mux, MD_URL, "",
                                     coinbase::ProducerType::_PRODUCER_TYPE_COUNT_);

    // Register before subscribe so the symbol map is ready when callbacks fire.
    callbacks.registerClient(&ws_btc, "BTC-USD");
    callbacks.registerClient(&ws_eth, "ETH-USD");

    ws_btc.subscribe({"BTC-USD"}, {
        coinbase::WebSocketChannel::TICKER,
        coinbase::WebSocketChannel::LEVEL2,
        coinbase::WebSocketChannel::MARKET_TRADES,
    });
    ws_eth.subscribe({"ETH-USD"}, {
        coinbase::WebSocketChannel::TICKER,
        coinbase::WebSocketChannel::LEVEL2,
        coinbase::WebSocketChannel::MARKET_TRADES,
    });

    // Rely on slick-net's SIGINT handler to set is_running() to false.
    auto last_print = std::chrono::steady_clock::now();
    while (coinbase::Websocket::is_running()) {
        // One call drains messages from both symbols.
        callbacks.processData(200);

        auto now = std::chrono::steady_clock::now();
        if (now - last_print >= std::chrono::seconds(5)) {
            callbacks.printAllOrderBooks();
            last_print = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_INFO("Shutting down...");
    ws_btc.stop();
    ws_eth.stop();
}
