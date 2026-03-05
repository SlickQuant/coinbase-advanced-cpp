#include <gtest/gtest.h>
#include <coinbase/utils.hpp>
#include <coinbase/market_data.hpp>
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

} // namespace coinbase::tests
