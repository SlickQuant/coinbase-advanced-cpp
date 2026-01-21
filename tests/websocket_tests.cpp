#include <gtest/gtest.h>
#include <atomic>
#include <cstdio>
#include <filesystem>

#include <slick/logger.hpp>

#ifdef ENABLE_SLICK_LOGGER
    #ifndef INIT_LOGGER
    #define INIT_LOGGER
    namespace {
        auto *s_logger = []() -> slick::logger::Logger* {
            auto &logger = slick::logger::Logger::instance();
            logger.clear_sinks();
            logger.add_console_sink();
            // logger.set_level(slick::logger::LogLevel::L_DEBUG);
            logger.init(1048576, 16777216);
            return &logger;
        }();
    }
    #endif
#endif

#include <coinbase/websocket.hpp>


namespace coinbase::tests {
    template<typename CallbacksType>
    class WebSocketT : public ::testing::Test, public CallbacksType {
    protected:
        std::unique_ptr<WebSocketClient> client_;
        std::atomic_uint_fast64_t snapshot_received_ = 0;
        std::atomic_uint_fast64_t update_received_count_ = 0;
        std::atomic_uint_fast64_t md_gap_count_ = 0;

        void SetUp() override {
            client_ = std::make_unique<WebSocketClient>(this);
            snapshot_received_ = 0;
            update_received_count_ = 0;
        }

        void TearDown() override {
            client_.reset();
            std::remove("coinbase.log");
        }

        void onLevel2Snapshot(const Level2UpdateBatch& snapshot) override {
            EXPECT_TRUE(snapshot.product_id == "BTC-USD" || snapshot.product_id == "ETH-USD");
            EXPECT_GT(snapshot.updates.size(), 0);
            if (!snapshot.updates.empty()) {
                EXPECT_EQ(snapshot.updates[0].side, Side::BUY);
                EXPECT_GT(snapshot.updates[0].price_level, 0);
                EXPECT_GT(snapshot.updates[0].new_quantity, 0);
                auto last_index = snapshot.updates.size() - 1;
                EXPECT_EQ(snapshot.updates[last_index].side, Side::SELL);
                EXPECT_GT(snapshot.updates[last_index].price_level, 0);
                EXPECT_GT(snapshot.updates[last_index].new_quantity, 0);
                EXPECT_GT(snapshot.updates[last_index].price_level, snapshot.updates[0].price_level);
            }
            ++snapshot_received_;
        }
        void onLevel2Updates(const Level2UpdateBatch& updates) override {
            EXPECT_TRUE(updates.product_id == "BTC-USD" || updates.product_id == "ETH-USD");
            EXPECT_GT(updates.updates.size(), 0);
            if (!updates.updates.empty()) {
                EXPECT_GT(updates.updates[0].price_level, 0);
                EXPECT_GE(updates.updates[0].new_quantity, 0);
            }
            ++update_received_count_;
        }
        void onMarketTradesSnapshot(const std::vector<MarketTrade>& snapshots) override {
            EXPECT_GT(snapshots.size(), 0);
            if (!snapshots.empty()) {
                EXPECT_EQ(snapshots[0].product_id, "BTC-USD");
                EXPECT_GT(snapshots[0].price, 0);
                EXPECT_GT(snapshots[0].size, 0);
            }
            ++snapshot_received_;
        }
        void onMarketTrades(const std::vector<MarketTrade>& trades) override {
            EXPECT_GT(trades.size(), 0);
            if (!trades.empty()) {
                EXPECT_EQ(trades[0].product_id, "BTC-USD");
                EXPECT_GT(trades[0].price, 0);
                EXPECT_GT(trades[0].size, 0);
            }
            ++update_received_count_;
        }
        void onTickerSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) override {
            EXPECT_GT(tickers.size(), 0);
            if (!tickers.empty()) {
                EXPECT_EQ(tickers[0].product_id, "BTC-USD");
                EXPECT_GT(tickers[0].price, 0);
                EXPECT_GT(tickers[0].volume_24_h, 0);
                EXPECT_GT(tickers[0].low_24_h, 0);
                EXPECT_GT(tickers[0].high_24_h, 0);
                EXPECT_GT(tickers[0].low_52_w, 0);
                EXPECT_GT(tickers[0].high_52_w, 0);
                EXPECT_GT(tickers[0].best_bid, 0);
                EXPECT_GT(tickers[0].best_bid_quantity, 0);
                EXPECT_GT(tickers[0].best_ask, 0);
                EXPECT_GT(tickers[0].best_ask_quantity, 0);
                EXPECT_GT(tickers[0].price_percent_chg_24_h, -100);
            }
            ++snapshot_received_;
        }
        void onTickers(uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) override {
            EXPECT_GT(tickers.size(), 0);
            if (!tickers.empty()) {
                EXPECT_EQ(tickers[0].product_id, "BTC-USD");
                EXPECT_GT(tickers[0].price, 0);
                EXPECT_GT(tickers[0].volume_24_h, 0);
                EXPECT_GT(tickers[0].low_24_h, 0);
                EXPECT_GT(tickers[0].high_24_h, 0);
                EXPECT_GT(tickers[0].low_52_w, 0);
                EXPECT_GT(tickers[0].high_52_w, 0);
                EXPECT_GT(tickers[0].best_bid, 0);
                EXPECT_GT(tickers[0].best_bid_quantity, 0);
                EXPECT_GT(tickers[0].best_ask, 0);
                EXPECT_GT(tickers[0].best_ask_quantity, 0);
                EXPECT_GT(tickers[0].price_percent_chg_24_h, -100);
            }
            ++update_received_count_;
        }
        void onCandlesSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) override {
            EXPECT_GT(candles.size(), 0);
            if (!candles.empty()) {
                EXPECT_EQ(candles[0].product_id, "BTC-USD");
                EXPECT_GT(candles[0].open, 0);
                EXPECT_GT(candles[0].high, 0);
                EXPECT_GT(candles[0].low, 0);
                EXPECT_GT(candles[0].close, 0);
                EXPECT_GT(candles[0].volume, 0);
            }
            ++snapshot_received_;
        }
        void onCandles(uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) override {
            EXPECT_GT(candles.size(), 0);
            if (!candles.empty()) {
                EXPECT_EQ(candles[0].product_id, "BTC-USD");
                EXPECT_GT(candles[0].open, 0);
                EXPECT_GT(candles[0].high, 0);
                EXPECT_GT(candles[0].low, 0);
                EXPECT_GT(candles[0].close, 0);
                EXPECT_GT(candles[0].volume, 0);
            }
            ++update_received_count_;
        }
        void onStatusSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) override {
            EXPECT_GT(status.size(), 0);
            if (!status.empty()) {
                EXPECT_EQ(status[0].id, "BTC-USD");
                EXPECT_EQ(status[0].product_type, ProductType::SPOT);
                EXPECT_EQ(status[0].base_currency, "BTC");
                EXPECT_EQ(status[0].quote_currency, "USD");
                EXPECT_EQ(status[0].display_name, "BTC/USD");
                EXPECT_GE(status[0].base_increment, 0);
                EXPECT_GE(status[0].quote_increment, 0);
            }
            ++snapshot_received_;
        }
        void onStatus(uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) override {
            LOG_INFO("Status");
            EXPECT_GT(status.size(), 0);
            if (!status.empty()) {
                EXPECT_EQ(status[0].id, "BTC-USD");
                EXPECT_EQ(status[0].product_type, ProductType::SPOT);
                EXPECT_EQ(status[0].base_currency, "BTC");
                EXPECT_EQ(status[0].quote_currency, "USD");
                EXPECT_EQ(status[0].display_name, "BTC/USD");
                EXPECT_GE(status[0].base_increment, 0);
                EXPECT_GE(status[0].quote_increment, 0);
            }
            ++update_received_count_;
        }
        void onMarketDataGap() override {
            LOG_INFO("MarketDataGap");
            ++md_gap_count_;
        }
        void onUserDataGap() override {
            LOG_INFO("UserDataGap");
        }
        void onUserDataSnapshot(uint64_t seq_num, const std::vector<Order>& orders, const std::vector<PerpetualFuturePosition>& perpetual_future_positions, const std::vector<ExpiringFuturePosition>& expiring_future_positions) override {
            LOG_INFO("UserDataSnapshot");
            LOG_INFO("SeqNum: {}", seq_num);
            LOG_INFO("Orders: {}", orders.size());
            LOG_INFO("PerpetualFuturePositions: {}", perpetual_future_positions.size());
            LOG_INFO("ExpiringFuturePositions: {}", expiring_future_positions.size());
            for (auto& order : orders) {
                LOG_INFO("Order: {}", order.order_id);
                LOG_INFO("ProductID: {}", order.product_id);
                LOG_INFO("Side: {}", to_string(order.side));
                LOG_INFO("Type: {}", to_string(order.order_type));
                LOG_INFO("Status: {}", to_string(order.status));
            }
            for (auto& position : perpetual_future_positions) {
                LOG_INFO("PerpetualFuturePosition:");
                LOG_INFO("ProductID: {}", position.product_id);
                LOG_INFO("Side: {}", to_string(position.position_side));
                LOG_INFO("net_size: {}", position.net_size);
            }
            for (auto& position : expiring_future_positions) {
                LOG_INFO("ExpiringFuturePosition:");
                LOG_INFO("ProductID: {}", position.product_id);
                LOG_INFO("realized_pnl: {}", position.realized_pnl);
                LOG_INFO("unrealized_pnl: {}", position.unrealized_pnl);
                LOG_INFO("entry_price: {}", position.entry_price);
            }
            ++snapshot_received_;
        }
        void onOrderUpdates(uint64_t seq_num, const std::vector<Order>& orders) override {
            LOG_INFO("OrderUpdates");
        }
        void onMarketDataError(std::string err) override {
            LOG_INFO("MarketDataError");
        }
        void onUserDataError(std::string err) override {
            LOG_INFO("UserDataError");
        }
    };

    using WebSocketTests = WebSocketT<WebsocketCallbacks>;
    using UserThreadWebSocketTests = WebSocketT<UserThreadWebsocketCallbacks>;

    TEST_F(WebSocketTests, UserChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::USER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, Level2Channel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::LEVEL2});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 10) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, MarketTradesChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::MARKET_TRADES});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 10) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, CandlesChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::CANDLES});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, TickerChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::TICKER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, StatusChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::STATUS});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, UserChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::USER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, Level2Channel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::LEVEL2});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 10) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, MarketTradesChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::MARKET_TRADES});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 10) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, CandlesChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::CANDLES});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, TickerChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::TICKER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(UserThreadWebSocketTests, StatusChannel) {
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::STATUS});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    TEST_F(WebSocketTests, DataLogger) {
        std::remove("coinbase.log");
        EXPECT_FALSE(std::filesystem::exists("coinbase.log"));

        client_->logData("coinbase.log");
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::LEVEL2, WebSocketChannel::USER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        EXPECT_TRUE(std::filesystem::exists("coinbase.log"));
        std::ifstream f("coinbase.log");
        std::string line;
        EXPECT_TRUE(std::getline(f, line));
        EXPECT_FALSE(line.empty());
    }

    TEST_F(UserThreadWebSocketTests, DataLogger) {
        std::remove("coinbase.log");
        EXPECT_FALSE(std::filesystem::exists("coinbase.log"));

        client_->logData("coinbase.log");
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::LEVEL2, WebSocketChannel::USER});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        EXPECT_TRUE(std::filesystem::exists("coinbase.log"));
        std::ifstream f("coinbase.log");
        std::string line;
        EXPECT_TRUE(std::getline(f, line));
        EXPECT_FALSE(line.empty());
    }

    TEST_F(UserThreadWebSocketTests, MultipleClient) {
        auto client2 = std::make_unique<WebSocketClient>(this);
        client_->subscribe({"BTC-USD"}, {WebSocketChannel::LEVEL2});
        client2->subscribe({"ETH-USD"}, {WebSocketChannel::LEVEL2});
        while (!snapshot_received_.load(std::memory_order_relaxed)) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 5) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (!snapshot_received_.load(std::memory_order_relaxed) < 2) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (update_received_count_.load(std::memory_order_relaxed) < 10) {
            processData();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        EXPECT_EQ(md_gap_count_.load(std::memory_order_relaxed), 0u);
    }
}
