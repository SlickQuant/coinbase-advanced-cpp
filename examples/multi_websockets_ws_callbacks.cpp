// SPDX-License-Identifier: MIT
// Example 3: Receive market data for multiple symbols using a shared
// stream_buffer_multiplexer with WebsocketCallbacks.
//
// Each symbol gets its own WebsocketCallbacks instance and WebSocketClient.
// Callbacks run directly on the WebSocket I/O thread, so handlers must
// return quickly without blocking.
//
// Sharing a stream_buffer_multiplexer across clients unifies their receive
// ring buffers under one allocation and one shared fan-in queue.
//
// The mux queue and MD_DATA producer buffers are placed in named shared
// memory so a second process (multi_websockets_ws_callbacks_reader) can
// attach and log the same raw JSON stream without an extra network connection.
//
// No API credentials are required for public market-data channels.

#include <chrono>
#include <format>
#include <iostream>
#include <thread>

#include <slick/net/logging.hpp>
#include <slick/stream_buffer_multiplexer.hpp>

#include <coinbase/websocket.hpp>

static constexpr const char* MD_URL = "wss://advanced-trade-ws.coinbase.com";

// Named shared-memory identifiers — must match multi_websockets_ws_callbacks_reader.cpp.
static constexpr const char* SHM_QUEUE_NAME = "coinbase_md_mux_queue";
static constexpr const char* SHM_BTC_BUF    = "coinbase_btc_md_buf";
static constexpr const char* SHM_ETH_BUF    = "coinbase_eth_md_buf";

// ---------------------------------------------------------------------------
// Callback implementation — one instance per symbol
// ---------------------------------------------------------------------------

class SymbolCallbacks : public coinbase::WebsocketCallbacks {
public:
    explicit SymbolCallbacks(std::string symbol) : symbol_(std::move(symbol)) {}

    // --- connection lifecycle ---

    void onMarketDataConnected(coinbase::WebSocketClient*) override {
        LOG_INFO("[{}] connected", symbol_);
    }
    void onMarketDataDisconnected(coinbase::WebSocketClient*) override {
        LOG_INFO("[{}] disconnected", symbol_);
    }
    void onUserDataConnected(coinbase::WebSocketClient*) override {}
    void onUserDataDisconnected(coinbase::WebSocketClient*) override {}

    // --- level 2 order book ---

    void onLevel2Snapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& s) override {
        LOG_INFO("[{}] L2 snapshot seq={} entries={}", symbol_, seq_num, s.updates.size());
    }
    void onLevel2Updates(coinbase::WebSocketClient*, uint64_t seq_num,
                         const coinbase::Level2UpdateBatch& u) override {
        LOG_DEBUG("[{}] L2 update seq={} changes={}", symbol_, seq_num, u.updates.size());
    }

    // --- market trades ---

    void onMarketTradesSnapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                                const std::vector<coinbase::MarketTrade>& trades) override {
        LOG_INFO("[{}] trades snapshot seq={} count={}", symbol_, seq_num, trades.size());
    }
    void onMarketTrades(coinbase::WebSocketClient*, uint64_t seq_num,
                        const std::vector<coinbase::MarketTrade>& trades) override {
        for (const auto& t : trades) {
            LOG_INFO("[{}] trade seq={} price={} size={} side={}", symbol_, seq_num,
                     t.price, t.size, t.side == coinbase::Side::BUY ? "buy" : "sell");
        }
    }

    // --- ticker ---

    void onTickerSnapshot(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                          const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[{}] ticker snapshot seq={} price={} bid={} ask={}",
                     symbol_, seq_num, tk.price, tk.best_bid, tk.best_ask);
        }
    }
    void onTickers(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                   const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[{}] ticker seq={} price={} bid={} ask={}",
                     symbol_, seq_num, tk.price, tk.best_bid, tk.best_ask);
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

    void onMarketDataGap(coinbase::WebSocketClient*) override {
        LOG_WARN("[{}] sequence gap detected", symbol_);
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

    void onMarketDataError(coinbase::WebSocketClient*, std::string&& err) override {
        LOG_ERROR("[{}] error: {}", symbol_, err);
    }
    void onUserDataError(coinbase::WebSocketClient*, std::string&&) override {}

private:
    std::string symbol_;
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

    LOG_INFO("=== Multi-symbol WebsocketCallbacks (shared mux, cross-process IPC) ===");
    LOG_INFO("Callbacks run on WebSocket I/O threads. Press Ctrl-C to exit.");
    LOG_INFO("Run multi_websockets_ws_callbacks_reader in a second process to observe");
    LOG_INFO("the same raw JSON stream from shared memory.");

    // Shared-memory fan-in queue — a second process can attach to this queue
    // and read the same records that slick-net publishes for each websocket frame.
    slick::stream_buffer_multiplexer mux(1u << 17, SHM_QUEUE_NAME);

    SymbolCallbacks btc_cb("BTC-USD");
    SymbolCallbacks eth_cb("ETH-USD");

    // No user-data URL ("") — market data only.
    // producer_offset separates the two clients' producer IDs within the mux.
    // md_read_buffer_shm_name puts the MD_DATA ring buffer in named shared memory
    // so multi_websockets_ws_callbacks_reader can open it in-process.
    coinbase::WebSocketClient ws_btc(
        &btc_cb, mux, MD_URL, "",
        0,
        1u << 26, 1u << 16, SHM_BTC_BUF   // MD_DATA buffer in shared memory
    );
    coinbase::WebSocketClient ws_eth(
        &eth_cb, mux, MD_URL, "",
        coinbase::ProducerType::_PRODUCER_TYPE_COUNT_,
        1u << 26, 1u << 16, SHM_ETH_BUF   // MD_DATA buffer in shared memory
    );

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
    while (coinbase::Websocket::is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("Shutting down...");
    ws_btc.stop();
    ws_eth.stop();
}
