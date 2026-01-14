#include <gtest/gtest.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <nlohmann/json.hpp>

// uncomment to see debug logging
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

#include <coinbase/rest.hpp>

namespace coinbase::tests {

// Test fixture for HTTP tests
class CoinbaseAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    // Helper to wait for async operations
    template<typename Predicate>
    bool wait_for_condition(Predicate pred, std::chrono::milliseconds timeout) {
        auto start = std::chrono::high_resolution_clock::now();
        while (!pred() &&
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::high_resolution_clock::now() - start) < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return pred();
    }

    CoinbaseRestClient client_;
};

TEST_F(CoinbaseAdvancedTest, GetServerTimeTest) {
    auto timestamp = client_.get_server_time();
    EXPECT_TRUE(timestamp > 0);
}

TEST_F(CoinbaseAdvancedTest, ListAccountsGetAccountTest) {
    auto accounts = client_.list_accounts();
    EXPECT_FALSE(accounts.empty());

    if (!accounts.empty()) {
        auto account = client_.get_account(accounts[0].uuid);
        EXPECT_FALSE(account.uuid.empty());
        EXPECT_EQ(account.name, accounts[0].name);
    }
}

TEST_F(CoinbaseAdvancedTest, ListPublicProductsGetPublicProductTest) {
    auto products = client_.list_public_products();
    EXPECT_FALSE(products.empty());

    if (!products.empty()) {
        auto product = client_.get_public_product(products[0].product_id);
        EXPECT_FALSE(product.product_id.empty());
        EXPECT_EQ(product.product_id, products[0].product_id);
    }
}


TEST_F(CoinbaseAdvancedTest, ListProductsGetProductTest) {
    auto products = client_.list_products();
    EXPECT_FALSE(products.empty());

    if (!products.empty()) {
        auto product = client_.get_product(products[0].product_id, true);
        EXPECT_FALSE(product.product_id.empty());
        EXPECT_EQ(product.product_id, products[0].product_id);
    }
}

TEST_F(CoinbaseAdvancedTest, GetBestBidAsk) {
    auto pricebooks = client_.get_best_bid_ask({"BTC-USD", "ETH-USD"});
    EXPECT_EQ(pricebooks.size(), 2);
    EXPECT_TRUE(pricebooks[0].product_id == "BTC-USD" || pricebooks[0].product_id == "ETH-USD");
    EXPECT_EQ(pricebooks[0].bids.size(), 1);
    EXPECT_GT(pricebooks[0].bids[0].price, 0.);
    EXPECT_GT(pricebooks[0].bids[0].size, 0.);
    EXPECT_EQ(pricebooks[0].asks.size(), 1);
    EXPECT_GT(pricebooks[0].asks[0].price, 0.);
    EXPECT_GT(pricebooks[0].asks[0].size, 0.);
    EXPECT_TRUE(pricebooks[1].product_id == "BTC-USD" || pricebooks[1].product_id == "ETH-USD");
    EXPECT_EQ(pricebooks[1].bids.size(), 1);
    EXPECT_GT(pricebooks[1].bids[0].price, 0.);
    EXPECT_GT(pricebooks[1].bids[0].size, 0.);
    EXPECT_EQ(pricebooks[1].asks.size(), 1);
    EXPECT_GT(pricebooks[1].asks[0].price, 0.);
    EXPECT_GT(pricebooks[1].asks[0].size, 0.);
}

TEST_F(CoinbaseAdvancedTest, GetPriceBook) {
    PriceBookQueryParams params;
    params.product_id = "BTC-USD";
    auto pb_response = client_.get_product_book(params);
    EXPECT_EQ(pb_response.pricebook.product_id, "BTC-USD");
    EXPECT_GT(pb_response.pricebook.bids.size(), 0);
    EXPECT_GT(pb_response.pricebook.asks.size(), 0);
}

TEST_F(CoinbaseAdvancedTest, GetMarketTrades) {
    auto market_trades = client_.get_market_trades("BTC-USD", {10});
    EXPECT_EQ(market_trades.trades.size(), 10);
    EXPECT_GT(market_trades.best_bid, 0);
    EXPECT_GT(market_trades.best_ask, 0);
    EXPECT_GT(market_trades.best_ask, market_trades.best_bid);
}

TEST_F(CoinbaseAdvancedTest, GetProductCandles) {
    ProductCandlesQueryParams params;
    params.start = to_milliseconds("2025-10-01T00:00:00Z") / 1000;
    params.end = to_milliseconds("2025-10-31T11:59:59Z") / 1000;
    params.granularity = Granularity::ONE_DAY;
    auto candles = client_.get_product_candles("BTC-USD", params);
    LOG_DEBUG("num: {}", candles.size());
    EXPECT_GT(candles.size(), 0);
}

TEST_F(CoinbaseAdvancedTest, LimitOrderTests) {
    auto pricebook = client_.get_best_bid_ask({"BTC-USD"});
    if (!pricebook.empty()) {
        auto rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BTC-USD",
            Side::BUY,
            OrderType::LIMIT,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            0.0005,
            pricebook[0].asks[0].price + 10000.0,
            true
        );
        // Should fail because post_only
        EXPECT_FALSE(rsp.success);

        auto price = pricebook[0].asks[0].price - 10000.0;
        rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BTC-USD",
            Side::BUY,
            OrderType::LIMIT,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            0.0003,
            price,
            true
        );
        EXPECT_TRUE(rsp.success);

        if (rsp.success) {
            auto order = client_.get_order(rsp.success_response.order_id);
            EXPECT_EQ(order.order_id, rsp.success_response.order_id);
            EXPECT_EQ(order.side, Side::BUY);
            EXPECT_EQ(order.status, OrderStatus::OPEN);

            price -= 10000.0;
            auto modify_rsp = client_.modify_order(
                order.order_id,
                "BTC-USD",
                price,
                0.0005
            );
            EXPECT_TRUE(modify_rsp.success);

            if (modify_rsp.success) {
                auto order = client_.get_order(rsp.success_response.order_id);
                EXPECT_EQ(order.order_id, rsp.success_response.order_id);
                EXPECT_EQ(order.side, Side::BUY);
                EXPECT_EQ(order.status, OrderStatus::OPEN);
                EXPECT_DOUBLE_EQ(order.order_configuration.limit_limit_gtc.value().limit_price, price);
            }

            auto cancel_rsp = client_.cancel_orders({order.order_id});
            EXPECT_EQ(cancel_rsp.size(), 1);
            EXPECT_TRUE(cancel_rsp[0].success);
        }
    }
}

TEST_F(CoinbaseAdvancedTest, LimitBracketOrderTests) {
    {
        auto pricebook = client_.get_best_bid_ask({"BTC-USD"});
        if (!pricebook.empty()) {
            auto price = pricebook[0].asks[0].price + 10000.0;
            auto rsp = client_.create_order(
                std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                "BTC-USD",
                Side::SELL,
                OrderType::LIMIT,
                TimeInForce::GOOD_UNTIL_CANCELLED,
                0.0005,
                price,
                true,
                false,
                price + 10000.0,
                price - 10000.0
            );
            // SPOT Bracket order cannot be placed as BUY side
            EXPECT_FALSE(rsp.success);
        }
    }

    auto pricebook = client_.get_best_bid_ask({"BIP-20DEC30-CDE"});
    if (!pricebook.empty()) {
        auto price = pricebook[0].bids[0].price - 10000.0;
        auto rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BIP-20DEC30-CDE",
            Side::SELL,
            OrderType::LIMIT,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            1,
            price,
            true,
            false,
            price + 10000.0,
            price - 10000.0
        );
        // Should fail because post_only
        EXPECT_FALSE(rsp.success);

        price = pricebook[0].asks[0].price + 10000.0;
        rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BIP-20DEC30-CDE",
            Side::SELL,
            OrderType::LIMIT,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            1,
            price,
            true,
            false,
            price + 5000.0,
            price - 10000.0
        );
        EXPECT_TRUE(rsp.success);

        if (rsp.success) {
            auto order = client_.get_order(rsp.success_response.order_id);
            EXPECT_EQ(order.order_id, rsp.success_response.order_id);
            EXPECT_EQ(order.side, Side::SELL);
            EXPECT_EQ(order.status, OrderStatus::OPEN);
            EXPECT_TRUE(order.attached_order_configuration.trigger_bracket_gtc.has_value());
            EXPECT_DOUBLE_EQ(order.attached_order_configuration.trigger_bracket_gtc->limit_price, price - 10000.0);
            EXPECT_DOUBLE_EQ(order.attached_order_configuration.trigger_bracket_gtc->stop_trigger_price, price + 5000.0);

            auto modify_rsp = client_.modify_order(
                order.order_id,
                "BIP-20DEC30-CDE",
                price,
                1,
                price + 10000.0,
                price - 5000.0
            );
            EXPECT_TRUE(modify_rsp.success);

            if (modify_rsp.success) {
                auto order = client_.get_order(rsp.success_response.order_id);
                EXPECT_EQ(order.order_id, rsp.success_response.order_id);
                EXPECT_EQ(order.side, Side::SELL);
                EXPECT_EQ(order.status, OrderStatus::OPEN);
                EXPECT_DOUBLE_EQ(order.order_configuration.limit_limit_gtc.value().limit_price, price);
                EXPECT_TRUE(order.attached_order_configuration.trigger_bracket_gtc.has_value());
                EXPECT_DOUBLE_EQ(order.attached_order_configuration.trigger_bracket_gtc->stop_trigger_price, price + 10000.0);
                EXPECT_DOUBLE_EQ(order.attached_order_configuration.trigger_bracket_gtc->limit_price, price - 5000.0);
            }

            auto cancel_rsp = client_.cancel_orders({order.order_id});
            EXPECT_EQ(cancel_rsp.size(), 1);
            EXPECT_TRUE(cancel_rsp[0].success);
        }
    }
}

TEST_F(CoinbaseAdvancedTest, BracketOrderTests) {
    {
        auto pricebook = client_.get_best_bid_ask({"BTC-USD"});
        if (!pricebook.empty()) {
            auto price = pricebook[0].bids[0].price - 10000.0;
            auto rsp = client_.create_order(
                std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                "BTC-USD",
                Side::BUY,
                OrderType::BRACKET,
                TimeInForce::GOOD_UNTIL_CANCELLED,
                0.0005,
                price,
                true,
                false,
                price - 5000.0,
                price
            );
            // SPOT Bracket order cannot be placed as BUY side
            EXPECT_FALSE(rsp.success);

            price = pricebook[0].bids[0].price - 10000.0;
            rsp = client_.create_order(
                std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
                "BTC-USD",
                Side::SELL,
                OrderType::BRACKET,
                TimeInForce::GOOD_UNTIL_CANCELLED,
                0.0005,
                NAN,
                true,
                false,
                price,
                price + 20000.0
            );
            // SPOT Bracket order cannot be placed as BUY side
            EXPECT_TRUE(rsp.success);

            if (rsp.success) {
                auto order = client_.get_order(rsp.success_response.order_id);
                EXPECT_EQ(order.order_id, rsp.success_response.order_id);
                EXPECT_EQ(order.side, Side::SELL);
                EXPECT_EQ(order.status, OrderStatus::OPEN);
                EXPECT_TRUE(order.order_configuration.trigger_bracket_gtc.has_value());
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->stop_trigger_price, price);
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->limit_price, price + 20000.0);

                auto cancel_rsp = client_.cancel_orders({order.order_id});
                EXPECT_EQ(cancel_rsp.size(), 1);
                EXPECT_TRUE(cancel_rsp[0].success);
            }

        }        
    }

    // Bracket order are available for both BUY and SELL on derivatives products
    auto pricebook = client_.get_best_bid_ask({"BIP-20DEC30-CDE"});
    if (!pricebook.empty()) {
        auto price = pricebook[0].asks[0].price + 10000.0;
        auto rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BIP-20DEC30-CDE",
            Side::SELL,
            OrderType::BRACKET,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            1,
            price,
            true,
            false,
            price + 5000.0,
            price - 6000.0
        );
        EXPECT_FALSE(rsp.success);

        if (rsp.success) {
            auto order = client_.get_order(rsp.success_response.order_id);
            EXPECT_EQ(order.order_id, rsp.success_response.order_id);
            EXPECT_EQ(order.side, Side::SELL);
            EXPECT_EQ(order.status, OrderStatus::OPEN);
            EXPECT_TRUE(order.order_configuration.trigger_bracket_gtc.has_value());
            EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->stop_trigger_price, price + 5000.0);
            EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->limit_price, price - 6000.0);

            auto modify_rsp = client_.modify_order(
                order.order_id,
                "BIP-20DEC30-CDE",
                price,
                1,
                price + 10000.0,
                price - 5000.0
            );
            EXPECT_TRUE(modify_rsp.success);

            if (modify_rsp.success) {
                auto order = client_.get_order(rsp.success_response.order_id);
                EXPECT_EQ(order.order_id, rsp.success_response.order_id);
                EXPECT_EQ(order.side, Side::SELL);
                EXPECT_EQ(order.status, OrderStatus::OPEN);
                EXPECT_DOUBLE_EQ(order.order_configuration.limit_limit_gtc.value().limit_price, price);
                EXPECT_TRUE(order.order_configuration.trigger_bracket_gtc.has_value());
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->stop_trigger_price, price + 10000.0);
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->limit_price, price - 5000.0);
            }

            auto cancel_rsp = client_.cancel_orders({order.order_id});
            EXPECT_EQ(cancel_rsp.size(), 1);
            EXPECT_TRUE(cancel_rsp[0].success);
        }

        price = pricebook[0].bids[0].price - 10000.0;
        rsp = client_.create_order(
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
            "BIP-20DEC30-CDE",
            Side::BUY,
            OrderType::BRACKET,
            TimeInForce::GOOD_UNTIL_CANCELLED,
            1,
            price,
            true,
            false,
            price - 5000.0,
            price + 6000.0
        );
        EXPECT_FALSE(rsp.success);

        if (rsp.success) {
            auto order = client_.get_order(rsp.success_response.order_id);
            EXPECT_EQ(order.order_id, rsp.success_response.order_id);
            EXPECT_EQ(order.side, Side::BUY);
            EXPECT_EQ(order.status, OrderStatus::OPEN);
            EXPECT_TRUE(order.order_configuration.trigger_bracket_gtc.has_value());
            EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->stop_trigger_price, price - 5000.0);
            EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->limit_price, price + 6000.0);

            auto modify_rsp = client_.modify_order(
                order.order_id,
                "BIP-20DEC30-CDE",
                price,
                1,
                price - 10000.0,
                price + 5000.0
            );
            EXPECT_TRUE(modify_rsp.success);

            if (modify_rsp.success) {
                auto order = client_.get_order(rsp.success_response.order_id);
                EXPECT_EQ(order.order_id, rsp.success_response.order_id);
                EXPECT_EQ(order.side, Side::BUY);
                EXPECT_EQ(order.status, OrderStatus::OPEN);
                EXPECT_DOUBLE_EQ(order.order_configuration.limit_limit_gtc.value().limit_price, price);
                EXPECT_TRUE(order.order_configuration.trigger_bracket_gtc.has_value());
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->stop_trigger_price, price - 50000.0);
                EXPECT_DOUBLE_EQ(order.order_configuration.trigger_bracket_gtc->limit_price, price + 6000.0);
            }

            auto cancel_rsp = client_.cancel_orders({order.order_id});
            EXPECT_EQ(cancel_rsp.size(), 1);
            EXPECT_TRUE(cancel_rsp[0].success);
        }
    }
}

TEST_F(CoinbaseAdvancedTest, ListOrdersGetOrderTest) {
    auto orders = client_.list_orders();
    EXPECT_FALSE(orders.empty());

    if (!orders.empty()) {
        auto order = client_.get_order(orders[0].order_id);
        EXPECT_FALSE(order.order_id.empty());
        EXPECT_EQ(order.order_id, orders[0].order_id);
    }

    OrderQueryParams params;
    params.order_status = {OrderStatus::OPEN};
    orders = client_.list_orders(params);
    EXPECT_FALSE(orders.empty());
}

TEST_F(CoinbaseAdvancedTest, ListFillsTest) {
    auto fills = client_.list_fills();
    EXPECT_FALSE(fills.empty());
    
    if (!fills.empty()) {
        auto size = fills.size();
        auto oid = fills[0].order_id;
        FillQueryParams params;
        params.order_ids = { fills[0].order_id };
        fills = client_.list_fills(params);
        EXPECT_FALSE(fills.empty());
        EXPECT_LE(fills.size(), size);
        EXPECT_EQ(fills[0].order_id, oid);
    }
}

} // namespace slick::net
