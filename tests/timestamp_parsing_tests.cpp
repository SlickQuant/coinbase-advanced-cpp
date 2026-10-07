#include <gtest/gtest.h>
#include <coinbase/utils.hpp>
#include <coinbase/market_data.hpp>
#include <coinbase/product.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace coinbase::tests {

class TimestampParsingTests : public ::testing::Test {};

TEST_F(TimestampParsingTests, ParseL2UpdateMessage) {
    // Real L2 update message from Coinbase WebSocket
    std::string msg = R"({
        "channel":"l2_data",
        "timestamp":"2026-03-05T09:05:32.483569449Z",
        "sequence_num":229,
        "events":[{
            "type":"update",
            "product_id":"BIP-20DEC30-CDE",
            "updates":[
                {"side":"bid","event_time":"2026-03-05T09:05:32.449176Z","price_level":"72575","new_quantity":"0"},
                {"side":"bid","event_time":"2026-03-05T09:05:32.449176Z","price_level":"72570","new_quantity":"68"},
                {"side":"offer","event_time":"2026-03-05T09:05:32.449176Z","price_level":"72580","new_quantity":"13"}
            ]
        }]
    })";

    auto j = json::parse(msg);

    // Test main timestamp parsing (nanoseconds - 9 fractional digits)
    ASSERT_TRUE(j.contains("timestamp"));
    uint64_t main_timestamp = nanoseconds_from_json(j, "timestamp");

    // Expected: 2026-03-05T09:05:32.483569449Z
    // Breakdown: 2026-03-05 09:05:32 = Unix timestamp base
    // .483569449 = 483569449 nanoseconds

    // The timestamp should be in nanoseconds since it has 9 fractional digits
    EXPECT_EQ(main_timestamp, 1772701532483569449ULL); // Exact nanosecond timestamp

    // Test Level2Update parsing (microseconds - 6 fractional digits)
    ASSERT_TRUE(j.contains("events"));
    ASSERT_GT(j["events"].size(), 0);

    Level2UpdateBatch batch = j["events"][0];
    ASSERT_EQ(batch.product_id, "BIP-20DEC30-CDE");
    ASSERT_EQ(batch.updates.size(), 3);

    // Expected: 2026-03-05T09:05:32.449176Z
    // .449176 = 6 fractional digits = microseconds

    EXPECT_EQ(batch.updates[0].event_time, 1772701532449176000ULL); // Exact microsecond timestamp

    // Verify side parsing
    EXPECT_EQ(batch.updates[0].side, Side::BUY); // "bid" = BUY

    // Verify price and quantity parsing
    EXPECT_DOUBLE_EQ(batch.updates[0].price_level, 72575.0);
    EXPECT_DOUBLE_EQ(batch.updates[0].new_quantity, 0.0);

    // Test the third update (offer side)
    EXPECT_EQ(batch.updates[2].side, Side::SELL); // "offer" = SELL
    EXPECT_DOUBLE_EQ(batch.updates[2].price_level, 72580.0);
    EXPECT_DOUBLE_EQ(batch.updates[2].new_quantity, 13.0);
}

// Regression: new_at is an ISO-8601 timestamp string, but it was the one timestamp field in
// Product parsed with INT_FROM_JSON. std::stoi stopped at the first non-digit and returned the
// year, so new_at silently held 2023 instead of a timestamp - and because stoi succeeded, nothing
// was logged. Offline, no credentials needed.
TEST_F(TimestampParsingTests, ProductNewAtIsParsedAsTimestamp) {
    auto j = json::parse(R"({
        "product_id":"BTC-USD",
        "quote_increment":"0.01",
        "new_at":"2023-01-01T00:00:00Z"
    })");

    auto p = j.get<Product>();

    EXPECT_EQ(p.new_at, to_nanoseconds("2023-01-01T00:00:00Z"));
    EXPECT_NE(p.new_at, 2023u);   // what std::stoi produced
}

// Regression: the venue nulls new_at on futures contracts. The value macros guarded on contains()
// but not on null, so the helper threw, the catch logged the whole product JSON at ERROR, and it
// did so once per product parsed - which filled the logs as soon as futures were cached.
TEST_F(TimestampParsingTests, NullFieldLeavesDefaultAndStillParsesTheRest) {
    auto j = json::parse(R"({
        "product_id":"BIT-30OCT26-CDE",
        "quote_increment":"0.01",
        "new_at":null
    })");

    auto p = j.get<Product>();

    EXPECT_EQ(p.new_at, 0u);
    // The null field does not abort the rest of the object.
    EXPECT_EQ(p.product_id, "BIT-30OCT26-CDE");
    EXPECT_DOUBLE_EQ(p.quote_increment, 0.01);
}

} // namespace coinbase::tests
