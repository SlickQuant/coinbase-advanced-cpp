// SPDX-License-Identifier: MIT
// Example 1: Receive market data with WebsocketCallbacks.
//
// Callbacks are invoked directly on the WebSocket I/O thread, so each
// handler must return quickly without blocking.
//
// No API credentials are required for public market-data channels
// (TICKER, LEVEL2, MARKET_TRADES).

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <coinbase/rest.hpp>
#include <coinbase/websocket.hpp>

static std::atomic_bool g_running{true};

// ---------------------------------------------------------------------------
// Callback implementation
// ---------------------------------------------------------------------------

class MarketDataCallbacks : public coinbase::WebsocketCallbacks {
public:
    // --- connection lifecycle ---

    void onMarketDataConnected(coinbase::WebSocketClient*) override {
        std::cout << "[connected] market data\n";
    }
    void onMarketDataDisconnected(coinbase::WebSocketClient*) override {
        std::cout << "[disconnected] market data\n";
    }
    void onUserDataConnected(coinbase::WebSocketClient*) override {}
    void onUserDataDisconnected(coinbase::WebSocketClient*) override {}

    // --- level 2 order book ---

    void onLevel2Snapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& snapshot) override {
        std::cout << "[L2 snapshot] " << snapshot.product_id
                  << " seq=" << seq_num
                  << " entries=" << snapshot.updates.size() << '\n';
        if (!snapshot.updates.empty()) {
            const auto& top = snapshot.updates.front();
            std::cout << "  top bid  price=" << top.price_level
                      << " qty=" << top.new_quantity << '\n';
        }
    }

    void onLevel2Updates(coinbase::WebSocketClient*, uint64_t seq_num,
                         const coinbase::Level2UpdateBatch& updates) override {
        std::cout << "[L2 update] " << updates.product_id
                  << " seq=" << seq_num
                  << " changes=" << updates.updates.size() << '\n';
    }

    // --- market trades ---

    void onMarketTradesSnapshot(coinbase::WebSocketClient*, uint64_t seq_num,
                                const std::vector<coinbase::MarketTrade>& trades) override {
        std::cout << "[trades snapshot] seq=" << seq_num
                  << " count=" << trades.size() << '\n';
    }

    void onMarketTrades(coinbase::WebSocketClient*, uint64_t seq_num,
                        const std::vector<coinbase::MarketTrade>& trades) override {
        for (const auto& t : trades) {
            std::cout << "[trade] seq=" << seq_num
                      << ' ' << t.product_id
                      << " price=" << t.price
                      << " size=" << t.size
                      << " side=" << (t.side == coinbase::Side::BUY ? "buy" : "sell")
                      << '\n';
        }
    }

    // --- ticker ---

    void onTickerSnapshot(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                          const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            std::cout << "[ticker snapshot] seq=" << seq_num
                      << ' ' << tk.product_id
                      << " price=" << tk.price
                      << " bid=" << tk.best_bid
                      << " ask=" << tk.best_ask << '\n';
        }
    }

    void onTickers(coinbase::WebSocketClient*, uint64_t seq_num, uint64_t,
                   const std::vector<coinbase::Ticker>& tickers) override {
        for (const auto& tk : tickers) {
            std::cout << "[ticker] seq=" << seq_num
                      << ' ' << tk.product_id
                      << " price=" << tk.price
                      << " bid=" << tk.best_bid
                      << " ask=" << tk.best_ask << '\n';
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
        std::cerr << "[WARNING] market data sequence gap detected\n";
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
        std::cerr << "[error] market data: " << err << '\n';
    }
    void onUserDataError(coinbase::WebSocketClient*, std::string&& err) override {
        std::cerr << "[error] user data: " << err << '\n';
    }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {

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
    std::cout << "\n=== WebSocket (WebsocketCallbacks) ===\n"
              << "Callbacks run on the WebSocket I/O thread.\n"
              << "Press Ctrl-C to exit.\n\n";

    MarketDataCallbacks callbacks;
    coinbase::WebSocketClient ws(&callbacks);

    ws.subscribe({"BTC-USD"}, {
        coinbase::WebSocketChannel::TICKER,
        coinbase::WebSocketChannel::LEVEL2,
        coinbase::WebSocketChannel::MARKET_TRADES,
    });

    // Install AFTER subscribe() — subscribe() triggers slick-net's install_signal_handlers()
    // which overwrites any handler registered before it. We install here so our handler wins.
    // Or rely on slick-net's signal handler, please referer to market_data_user_thread_callback example.
    std::signal(SIGINT, [](int) { g_running = false; });

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down...\n";
    ws.stop();
    // slick-net's WebsocketServiceTerminater global destructor calls Websocket<>::shutdown()
    // on exit, which stops the io_context and joins the service thread cleanly.
}
