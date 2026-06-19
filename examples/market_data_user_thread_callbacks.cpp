// SPDX-License-Identifier: MIT
// Example 2: Receive market data with UserThreadWebsocketCallbacks.
//
// The WebSocket I/O thread enqueues data into a lock-free buffer.
// The user calls processData() from their own thread to drain the buffer
// and invoke callbacks — no synchronisation needed inside the handlers.
// This makes it safe to do heavier work (e.g. maintaining an order book).
//
// No API credentials are required for public market-data channels
// (TICKER, LEVEL2, MARKET_TRADES).

#include <atomic>
#include <chrono>
#include <csignal>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <thread>

#include <slick/net/logging.hpp>

#include <coinbase/rest.hpp>
#include <coinbase/websocket.hpp>

// ---------------------------------------------------------------------------
// Callback implementation
// ---------------------------------------------------------------------------

class UserThreadCallbacks : public coinbase::UserThreadWebsocketCallbacks {
public:
    // --- connection lifecycle ---

    void onMarketDataConnected(coinbase::WebSocketClient*) override {
        LOG_INFO("[connected] market data");
    }
    void onMarketDataDisconnected(coinbase::WebSocketClient*) override {
        LOG_INFO("[disconnected] market data");
    }
    void onUserDataConnected(coinbase::WebSocketClient*) override {}
    void onUserDataDisconnected(coinbase::WebSocketClient*) override {}

    // --- level 2 order book (maintained on the user thread) ---

    void onLevel2Snapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& snapshot) override {
        bids_.clear();
        asks_.clear();
        for (const auto& u : snapshot.updates) {
            if (u.side == coinbase::Side::BUY) {
                bids_[u.price_level] = u.new_quantity;
            } else {
                asks_[u.price_level] = u.new_quantity;
            }
        }
        LOG_INFO("[L2 snapshot] {} seq={} bids={} asks={}", snapshot.product_id, seq_num, bids_.size(), asks_.size());
    }

    void onLevel2Updates(coinbase::WebSocketClient*, uint64_t /*seq_num*/,
                         const coinbase::Level2UpdateBatch& updates) override {
        for (const auto& u : updates.updates) {
            if (u.side == coinbase::Side::BUY) {
                if (u.new_quantity == 0.0) bids_.erase(u.price_level);
                else bids_[u.price_level] = u.new_quantity;
            } else {
                if (u.new_quantity == 0.0) asks_.erase(u.price_level);
                else asks_[u.price_level] = u.new_quantity;
            }
        }
    }

    // --- market trades ---

    void onMarketTradesSnapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                                const std::vector<coinbase::MarketTrade>& trades) override {
        LOG_INFO("[trades snapshot] seq={} count={}", seq_num, trades.size());
    }

    void onMarketTrades(coinbase::WebSocketClient*, uint64_t seq_num,
                        const std::vector<coinbase::MarketTrade>& trades) override {
        for (const auto& t : trades) {
            LOG_INFO("[trade] seq={} {} price={} size={} side={}", seq_num, t.product_id, t.price, t.size,
                     t.side == coinbase::Side::BUY ? "buy" : "sell");
        }
    }

    // --- ticker ---

    void onTickerSnapshot(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                          const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[ticker snapshot] seq={} {} price={} bid={} ask={}", seq_num, tk.product_id, tk.price, tk.best_bid, tk.best_ask);
        }
    }

    void onTickers(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                   const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            LOG_INFO("[ticker] seq={} {} price={} bid={} ask={}", seq_num, tk.product_id, tk.price, tk.best_bid, tk.best_ask);
        }
    }

    // --- candles / status (not subscribed in this example, no-ops) ---

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
        LOG_WARN("market data sequence gap detected");
    }
    void onUserDataGap(coinbase::WebSocketClient*) override {}

    // --- user data (not subscribed in this example, no-ops) ---

    void onUserDataSnapshot(coinbase::WebSocketClient*, uint64_t,
                            const std::vector<coinbase::Order>&,
                            const std::vector<coinbase::PerpetualFuturePosition>&,
                            const std::vector<coinbase::ExpiringFuturePosition>&) override {}
    void onOrderUpdates(coinbase::WebSocketClient*, uint64_t,
                        const std::vector<coinbase::Order>&) override {}

    // --- errors ---

    void onMarketDataError(coinbase::WebSocketClient*, std::string&& err) override {
        LOG_ERROR("market data error: {}", err);
    }
    void onUserDataError(coinbase::WebSocketClient*, std::string&& err) override {
        LOG_ERROR("user data error: {}", err);
    }

    // --- order book display (called from user thread) ---

    void printOrderBook() const {
        LOG_INFO("--- Order Book (top 5) ---");
        int n = 0;
        for (const auto& [price, qty] : bids_) {
            LOG_INFO("  bid  {}  qty={}", price, qty);
            if (++n >= 5) break;
        }
        n = 0;
        for (const auto& [price, qty] : asks_) {
            LOG_INFO("  ask  {}  qty={}", price, qty);
            if (++n >= 5) break;
        }
        LOG_INFO("--------------------------");
    }

private:
    // bids sorted best (highest) first, asks sorted best (lowest) first
    std::map<double, double, std::greater<double>> bids_;
    std::map<double, double>                       asks_;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {

    // Route slick-net log messages to stdout.
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

    // --- REST client: fetch public market info (no credentials required) ---
    coinbase::CoinbaseRestClient rest;

    std::cout << "=== REST: BTC-USD (public endpoints) ===\n";
    auto product = rest.get_public_product("BTC-USD");
    std::cout << "  price:      " << product.price << '\n'
              << "  volume_24h: " << product.volume_24h << '\n'
              << "  status:     " << product.status << '\n';

    auto server_time = rest.get_server_time();
    std::cout << "  server time (ns): " << server_time << '\n';

    auto market_trades = rest.get_market_trades("BTC-USD", {.limit = 5});
    std::cout << "  recent trades"
              << "  best_bid=" << market_trades.best_bid
              << "  best_ask=" << market_trades.best_ask << '\n';
    for (const auto& t : market_trades.trades) {
        std::cout << "    price=" << t.price << " size=" << t.size << '\n';
    }

    // --- WebSocket: subscribe to live market data ---
    std::cout << "\n=== WebSocket (UserThreadWebsocketCallbacks) ===\n"
              << "Callbacks run on the main thread via processData().\n"
              << "Order book is printed every 5 seconds.\n"
              << "Press Ctrl-C to exit.\n\n";

    UserThreadCallbacks callbacks;
    coinbase::WebSocketClient ws(&callbacks);

    ws.subscribe({"BTC-USD", "ETH-USD"}, {
        coinbase::WebSocketChannel::TICKER,
        coinbase::WebSocketChannel::LEVEL2,
        coinbase::WebSocketChannel::MARKET_TRADES,
    });

    // This example relies on slick-net's signal handler to exit the example.

    auto last_print = std::chrono::steady_clock::now();
    while (coinbase::Websocket::is_running()) {
        // Drain the lock-free queue and dispatch callbacks on this thread.
        callbacks.processData(100);

        auto now = std::chrono::steady_clock::now();
        if (now - last_print >= std::chrono::seconds(5)) {
            callbacks.printOrderBook();
            last_print = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nShutting down...\n";
    ws.stop();
    // slick-net's WebsocketServiceTerminater global destructor calls Websocket<>::shutdown()
    // on exit, which stops the io_context and joins the service thread cleanly.
}
