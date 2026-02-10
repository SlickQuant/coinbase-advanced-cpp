#include <gtest/gtest.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <nlohmann/json.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <slick/logger.hpp>

#define ENABLE_SLICK_LOGGER
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

#include <coinbase/rest_awaitable.hpp>

namespace asio = boost::asio;
using namespace coinbase;

namespace coinbase::tests {

// Test fixture for awaitable REST API tests
class CoinbaseAwaitableTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<asio::io_context>();
    }

    void TearDown() override {
        if (HasFatalFailure() || HasNonfatalFailure()) {
            if (!order_.order_id.empty()) {
                // Cleanup - cancel order if test failed
                run_async([this]() -> asio::awaitable<void> {
                    co_await client_.cancel_orders({order_.order_id});
                    co_return;
                });
            }
        }
    }

    // Helper to run an async coroutine and wait for result
    template<typename Func>
    auto run_async(Func&& coro) {
        using awaitable_type = std::invoke_result_t<Func>;
        using result_type = typename awaitable_type::value_type;

        if constexpr (std::is_void_v<result_type>) {
            std::exception_ptr eptr;

            asio::co_spawn(*io_context_,
                [&, coro = std::forward<Func>(coro)]() mutable -> asio::awaitable<void> {
                    try {
                        co_await coro();
                    } catch (...) {
                        eptr = std::current_exception();
                    }
                    co_return;
                },
                asio::detached);

            io_context_->run();
            io_context_->restart();

            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } else {
            std::optional<result_type> result;
            std::exception_ptr eptr;

            asio::co_spawn(*io_context_,
                [&, coro = std::forward<Func>(coro)]() mutable -> asio::awaitable<void> {
                    try {
                        result = co_await coro();
                    } catch (...) {
                        eptr = std::current_exception();
                    }
                    co_return;
                },
                asio::detached);

            io_context_->run();
            io_context_->restart();

            if (eptr) {
                std::rethrow_exception(eptr);
            }

            return std::move(*result);
        }
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

    std::unique_ptr<asio::io_context> io_context_;
    coinbase::Order order_;
    CoinbaseAwaitableRestClient client_;
};

// ============================================================================
// Server Time Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, GetServerTimeTest) {
    auto timestamp = run_async([this]() -> asio::awaitable<uint64_t> {
        co_return co_await client_.get_server_time();
    });

    EXPECT_GT(timestamp, 0);
    LOG_INFO("Server timestamp: {}", timestamp);
}

// ============================================================================
// Account Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, ListAccountsTest) {
    auto accounts = run_async([this]() -> asio::awaitable<std::vector<Account>> {
        co_return co_await client_.list_accounts();
    });

    EXPECT_FALSE(accounts.empty());
    LOG_INFO("Found {} accounts", accounts.size());

    if (!accounts.empty()) {
        EXPECT_FALSE(accounts[0].uuid.empty());
        EXPECT_FALSE(accounts[0].currency.empty());
        LOG_INFO("First account: {} ({})", accounts[0].name, accounts[0].currency);
    }
}

TEST_F(CoinbaseAwaitableTest, GetAccountTest) {
    auto accounts = run_async([this]() -> asio::awaitable<std::vector<Account>> {
        co_return co_await client_.list_accounts();
    });

    ASSERT_FALSE(accounts.empty());

    auto account = run_async([this, &accounts]() -> asio::awaitable<Account> {
        co_return co_await client_.get_account(accounts[0].uuid);
    });

    EXPECT_FALSE(account.uuid.empty());
    EXPECT_EQ(account.uuid, accounts[0].uuid);
    EXPECT_EQ(account.name, accounts[0].name);
    LOG_INFO("Retrieved account: {} with balance: {}", account.name, account.available_balance.value);
}

TEST_F(CoinbaseAwaitableTest, ListAccountsWithPaginationTest) {
    AccountQueryParams params;
    params.limit = 5;

    auto accounts = run_async([this, &params]() -> asio::awaitable<std::vector<Account>> {
        co_return co_await client_.list_accounts(params);
    });

    EXPECT_FALSE(accounts.empty());
    EXPECT_LE(accounts.size(), 5);
    LOG_INFO("Retrieved {} accounts with limit=5", accounts.size());
}

// ============================================================================
// Product Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, ListPublicProductsTest) {
    auto products = run_async([this]() -> asio::awaitable<std::vector<Product>> {
        co_return co_await client_.list_public_products();
    });

    EXPECT_FALSE(products.empty());
    LOG_INFO("Found {} public products", products.size());

    if (!products.empty()) {
        EXPECT_FALSE(products[0].product_id.empty());
        EXPECT_FALSE(products[0].base_currency_id.empty());
        EXPECT_FALSE(products[0].quote_currency_id.empty());
    }
}

TEST_F(CoinbaseAwaitableTest, GetPublicProductTest) {
    auto product = run_async([this]() -> asio::awaitable<Product> {
        co_return co_await client_.get_public_product("BTC-USD");
    });

    EXPECT_EQ(product.product_id, "BTC-USD");
    EXPECT_EQ(product.base_currency_id, "BTC");
    EXPECT_EQ(product.quote_currency_id, "USD");
    LOG_INFO("BTC-USD status: {}", product.status);
}

TEST_F(CoinbaseAwaitableTest, ListProductsTest) {
    auto products = run_async([this]() -> asio::awaitable<std::vector<Product>> {
        co_return co_await client_.list_products();
    });

    EXPECT_FALSE(products.empty());
    LOG_INFO("Found {} products (authenticated)", products.size());
}

TEST_F(CoinbaseAwaitableTest, GetProductTest) {
    auto product = run_async([this]() -> asio::awaitable<Product> {
        co_return co_await client_.get_product("BTC-USD");
    });

    EXPECT_EQ(product.product_id, "BTC-USD");
    LOG_INFO("Retrieved product: {}", product.product_id);
}

TEST_F(CoinbaseAwaitableTest, ListProductsWithFilterTest) {
    ProductQueryParams params;
    params.product_type = ProductType::SPOT;
    params.limit = 10;

    auto products = run_async([this, &params]() -> asio::awaitable<std::vector<Product>> {
        co_return co_await client_.list_public_products(params);
    });

    EXPECT_FALSE(products.empty());
    EXPECT_LE(products.size(), 10);

    // Verify all products are SPOT type
    for (const auto& product : products) {
        EXPECT_EQ(product.product_type, ProductType::SPOT);
    }
    LOG_INFO("Retrieved {} SPOT products", products.size());
}

// ============================================================================
// Order Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, ListOrdersTest) {
    auto orders = run_async([this]() -> asio::awaitable<std::vector<Order>> {
        co_return co_await client_.list_orders();
    });

    // May be empty if no orders exist
    LOG_INFO("Found {} orders", orders.size());

    if (!orders.empty()) {
        EXPECT_FALSE(orders[0].order_id.empty());
        EXPECT_FALSE(orders[0].product_id.empty());
        LOG_INFO("First order: {} ({})", orders[0].order_id, orders[0].status);
    }
}

TEST_F(CoinbaseAwaitableTest, ListOrdersWithFilterTest) {
    OrderQueryParams params;
    params.product_ids = {"BTC-USD"};
    params.order_status = {OrderStatus::OPEN, OrderStatus::PENDING};
    params.limit = 10;

    auto orders = run_async([this, &params]() -> asio::awaitable<std::vector<Order>> {
        co_return co_await client_.list_orders(params);
    });

    LOG_INFO("Found {} open/pending BTC-USD orders", orders.size());

    for (const auto& order : orders) {
        EXPECT_EQ(order.product_id, "BTC-USD");
        EXPECT_TRUE(order.status == OrderStatus::OPEN || order.status == OrderStatus::PENDING);
    }
}

// ============================================================================
// Fill Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, ListFillsTest) {
    auto fills = run_async([this]() -> asio::awaitable<std::vector<Fill>> {
        co_return co_await client_.list_fills();
    });

    // May be empty if no fills exist
    LOG_INFO("Found {} fills", fills.size());

    if (!fills.empty()) {
        EXPECT_FALSE(fills[0].entry_id.empty());
        EXPECT_FALSE(fills[0].trade_id.empty());
        EXPECT_FALSE(fills[0].product_id.empty());
        LOG_INFO("First fill: {} ({} {})", fills[0].trade_id, fills[0].size, fills[0].product_id);
    }
}

TEST_F(CoinbaseAwaitableTest, ListFillsWithFilterTest) {
    FillQueryParams params;
    params.product_ids = {"BTC-USD"};
    params.limit = 20;

    auto fills = run_async([this, &params]() -> asio::awaitable<std::vector<Fill>> {
        co_return co_await client_.list_fills(params);
    });

    LOG_INFO("Found {} BTC-USD fills", fills.size());

    for (const auto& fill : fills) {
        EXPECT_EQ(fill.product_id, "BTC-USD");
    }
}

// ============================================================================
// Market Data Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, GetBestBidAskTest) {
    std::vector<std::string> product_ids = {"BTC-USD", "ETH-USD"};

    auto price_books = run_async([this, &product_ids]() -> asio::awaitable<std::vector<PriceBook>> {
        co_return co_await client_.get_best_bid_ask(product_ids);
    });

    EXPECT_EQ(price_books.size(), 2);

    for (const auto& book : price_books) {
        EXPECT_FALSE(book.product_id.empty());
        EXPECT_GT(book.time, 0);
        LOG_INFO("{}: bid={} ask={}", book.product_id, book.bids[0].price, book.asks[0].price);
    }
}

TEST_F(CoinbaseAwaitableTest, GetProductBookTest) {
    PriceBookQueryParams params;
    params.product_id = "BTC-USD";
    params.limit = 10;

    auto response = run_async([this, &params]() -> asio::awaitable<PriceBookResponse> {
        co_return co_await client_.get_product_book(params);
    });

    EXPECT_FALSE(response.pricebook.product_id.empty());
    EXPECT_EQ(response.pricebook.product_id, "BTC-USD");
    EXPECT_FALSE(response.pricebook.bids.empty());
    EXPECT_FALSE(response.pricebook.asks.empty());
    EXPECT_LE(response.pricebook.bids.size(), 10);
    EXPECT_LE(response.pricebook.asks.size(), 10);

    LOG_INFO("BTC-USD book: {} bids, {} asks",
             response.pricebook.bids.size(), response.pricebook.asks.size());
}

TEST_F(CoinbaseAwaitableTest, GetMarketTradesTest) {
    MarketTradesQueryParams params;
    params.limit = 10;

    auto trades = run_async([this, &params]() -> asio::awaitable<MarketTrades> {
        co_return co_await client_.get_market_trades("BTC-USD", params);
    });

    EXPECT_FALSE(trades.trades.empty());
    EXPECT_LE(trades.trades.size(), 10);

    for (const auto& trade : trades.trades) {
        EXPECT_FALSE(trade.trade_id.empty());
        EXPECT_GT(trade.price, 0);
        EXPECT_GT(trade.size, 0);
        LOG_INFO("Trade: {} @ {} (size: {})", trade.trade_id, trade.price, trade.size);
    }
}

TEST_F(CoinbaseAwaitableTest, GetProductCandlesTest) {
    ProductCandlesQueryParams params;
    params.granularity = Granularity::ONE_HOUR;
    auto now = std::chrono::system_clock::now();
    auto yesterday = now - std::chrono::hours(24);
    params.start = std::chrono::duration_cast<std::chrono::seconds>(yesterday.time_since_epoch()).count();
    params.end = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    auto candles = run_async([this, &params]() -> asio::awaitable<std::vector<Candle>> {
        co_return co_await client_.get_product_candles("BTC-USD", params);
    });

    EXPECT_FALSE(candles.empty());
    LOG_INFO("Retrieved {} hourly candles for BTC-USD", candles.size());

    if (!candles.empty()) {
        EXPECT_GT(candles[0].start, 0);
        EXPECT_GT(candles[0].high, 0);
        EXPECT_GT(candles[0].low, 0);
        EXPECT_GT(candles[0].open, 0);
        EXPECT_GT(candles[0].close, 0);
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(CoinbaseAwaitableTest, GetNonExistentProductTest) {
    auto product = run_async([this]() -> asio::awaitable<Product> {
        co_return co_await client_.get_public_product("INVALID-PRODUCT");
    });

    // Should return empty product on error
    EXPECT_TRUE(product.product_id.empty() || product.product_id == "INVALID-PRODUCT");
}

TEST_F(CoinbaseAwaitableTest, GetNonExistentAccountTest) {
    auto account = run_async([this]() -> asio::awaitable<Account> {
        co_return co_await client_.get_account("00000000-0000-0000-0000-000000000000");
    });

    // Should return empty account on error
    EXPECT_TRUE(account.uuid.empty());
}

// ============================================================================
// Multiple Concurrent Operations Test
// ============================================================================

TEST_F(CoinbaseAwaitableTest, ConcurrentOperationsTest) {
    run_async([this]() -> asio::awaitable<void> {
        // Launch multiple concurrent operations by directly awaiting without storing
        // Note: These will execute sequentially, not truly concurrently, but demonstrate
        // that multiple async operations can be chained together
        auto timestamp = co_await client_.get_server_time();
        auto accounts = co_await client_.list_accounts();
        auto products = co_await client_.list_public_products();
        auto btc_product = co_await client_.get_public_product("BTC-USD");

        EXPECT_GT(timestamp, 0);
        EXPECT_FALSE(accounts.empty());
        EXPECT_FALSE(products.empty());
        EXPECT_EQ(btc_product.product_id, "BTC-USD");

        LOG_INFO("Multiple async operations completed successfully");
        co_return;
    });
}

}  // namespace coinbase::tests
