#include <gtest/gtest.h>
#include <string>
#include <coinbase/order.hpp>
#include <coinbase/fill.hpp>

namespace coinbase::tests {

class QueryParamsTests : public ::testing::Test {};

// Regression: start_date was serialized as "start_time", a parameter the
// Coinbase List Orders endpoint does not recognize, so the caller's start
// bound was silently ignored and older orders were still returned.
TEST_F(QueryParamsTests, OrderQueryParamsSerializesStartDateAsStartDate) {
    OrderQueryParams params;
    params.start_date = "2026-01-01T00:00:00Z";

    auto query = params();

    EXPECT_EQ(query, "?start_date=2026-01-01T00:00:00Z");
    EXPECT_EQ(query.find("start_time="), std::string::npos);
}

TEST_F(QueryParamsTests, OrderQueryParamsSerializesDateRange) {
    OrderQueryParams params;
    params.start_date = "2026-01-01T00:00:00Z";
    params.end_date = "2026-02-01T00:00:00Z";

    auto query = params();

    EXPECT_NE(query.find("start_date=2026-01-01T00:00:00Z"), std::string::npos);
    EXPECT_NE(query.find("end_date=2026-02-01T00:00:00Z"), std::string::npos);
    EXPECT_EQ(query.find("start_time="), std::string::npos);
}

TEST_F(QueryParamsTests, OrderQueryParamsEmptyWhenUnset) {
    OrderQueryParams params;
    EXPECT_TRUE(params().empty());
}

// Regression: end_sequence_timestamp was declared and serialized as
// "end_sequeence_timestamp" (note the doubled "e"), a parameter the Coinbase
// List Fills endpoint does not recognize, so the caller's end bound was
// silently ignored.
TEST_F(QueryParamsTests, FillQueryParamsSerializesEndSequenceTimestamp) {
    FillQueryParams params;
    params.end_sequence_timestamp = "2026-02-01T00:00:00Z";

    auto query = params();

    EXPECT_EQ(query, "?end_sequence_timestamp=2026-02-01T00:00:00Z");
    EXPECT_EQ(query.find("sequeence"), std::string::npos);
}

TEST_F(QueryParamsTests, FillQueryParamsSerializesSequenceRange) {
    FillQueryParams params;
    params.start_sequence_timestamp = "2026-01-01T00:00:00Z";
    params.end_sequence_timestamp = "2026-02-01T00:00:00Z";

    auto query = params();

    EXPECT_NE(query.find("start_sequence_timestamp=2026-01-01T00:00:00Z"), std::string::npos);
    EXPECT_NE(query.find("end_sequence_timestamp=2026-02-01T00:00:00Z"), std::string::npos);
    EXPECT_EQ(query.find("sequeence"), std::string::npos);
}

TEST_F(QueryParamsTests, FillQueryParamsEmptyWhenUnset) {
    FillQueryParams params;
    EXPECT_TRUE(params().empty());
}

} // namespace coinbase::tests
