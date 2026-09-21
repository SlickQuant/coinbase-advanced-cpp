// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include <coinbase/rest.hpp>
#include <coinbase/utils.hpp>

namespace coinbase::tests {

namespace {

// Nothing listens here, so every request against it fails immediately - the stand-in for a
// custom endpoint and for an initialization that could not reach the server.
constexpr const char *unreachable_url = "http://127.0.0.1:1";
constexpr const char *public_url = "https://api.coinbase.com";

}   // namespace

class ProductCacheTests : public ::testing::Test {};

// Regression: product() looked the id up with operator[], so a miss default-constructed a
// Product in the shared cache and handed back a reference to it. The caller saw a product whose
// increments were all zero, and the miss was never reported.
TEST_F(ProductCacheTests, UnknownProductIsReportedAndNotInserted) {
    EXPECT_EQ(CoinbaseRestClient::find_product(unreachable_url, "NO-SUCH-PRODUCT"), nullptr);

    // A second lookup still misses: the first one inserted nothing.
    EXPECT_EQ(CoinbaseRestClient::find_product(unreachable_url, "NO-SUCH-PRODUCT"), nullptr);

    EXPECT_THROW((void)CoinbaseRestClient::product(unreachable_url, "NO-SUCH-PRODUCT"),
                 std::out_of_range);
}

// Why the miss above has to be reported rather than papered over with a default Product: the
// number of decimals comes from the increment, so a zero increment silently rounds a price to
// whole units and the order goes out at the wrong price.
TEST_F(ProductCacheTests, ZeroIncrementRoundsPriceToWholeUnits) {
    EXPECT_EQ(to_string(43521.37, 0.0), "43521");
    EXPECT_EQ(to_string(43521.37, 0.01), "43521.37");
}

// Regression: initialization ran under one process-wide call_once, so a first attempt that failed
// - a network blip, an endpoint that was briefly down - consumed the flag and left the cache
// permanently empty. A failed fetch now caches nothing, so the next call tries again.
TEST_F(ProductCacheTests, FailedInitializationIsNotCached) {
    EXPECT_FALSE(CoinbaseRestClient::initialize_products(unreachable_url));
    EXPECT_EQ(CoinbaseRestClient::find_product(unreachable_url, "BTC-USD"), nullptr);

    // Still false rather than a cached success: the endpoint was retried, not remembered as
    // initialized.
    EXPECT_FALSE(CoinbaseRestClient::initialize_products(unreachable_url));
    EXPECT_EQ(CoinbaseRestClient::find_product(unreachable_url, "BTC-USD"), nullptr);
}

// Regression: the cache was a single process-wide map, so whichever client was constructed first
// bound it to its endpoint and every later client - a sandbox, a mock, a replay server - read
// that endpoint's increments. Products are now keyed by the endpoint that served them.
// Public endpoint, no credentials needed.
TEST_F(ProductCacheTests, ProductsAreCachedPerEndpoint) {
    ASSERT_TRUE(CoinbaseRestClient::initialize_products(public_url));

    const auto *prod = CoinbaseRestClient::find_product(public_url, "BTC-USD");
    ASSERT_NE(prod, nullptr);
    EXPECT_EQ(prod->product_id, "BTC-USD");
    EXPECT_GT(prod->quote_increment, 0.);

    // The production catalog does not answer for an endpoint that never initialized.
    EXPECT_EQ(CoinbaseRestClient::find_product(unreachable_url, "BTC-USD"), nullptr);
}

// Regression: with the product missing, a limit order was priced against a zero increment and
// sent with the price rounded to whole units. It is now rejected before anything is signed or
// sent, and the rejection names the product. No credentials needed - the order never leaves.
TEST_F(ProductCacheTests, LimitOrderForUnknownProductIsRejectedLocally) {
    CoinbaseRestClient client(unreachable_url);

    auto response = client.create_order("test-unknown-product", "NO-SUCH-PRODUCT",
                                        Side::BUY, OrderType::LIMIT,
                                        TimeInForce::GOOD_UNTIL_CANCELLED,
                                        1.0, 43521.37);

    EXPECT_FALSE(response.success);
    EXPECT_NE(response.error_response.message.find("NO-SUCH-PRODUCT"), std::string::npos);
    EXPECT_TRUE(response.success_response.order_id.empty());
}

// The same guard on the modify path, which formats the new price against the increment too.
TEST_F(ProductCacheTests, ModifyOrderForUnknownProductIsRejectedLocally) {
    CoinbaseRestClient client(unreachable_url);

    auto response = client.modify_order("no-such-order", "NO-SUCH-PRODUCT", 43521.37, 1.0);

    EXPECT_FALSE(response.success);
}

}   // namespace coinbase::tests
