#pragma once 

#include <string>
#include <vector>
#include <optional>
#include <coinbase/common.h>
#include <nlohmann/json.hpp>
#include <coinbase/utils.h>

using json = nlohmann::json;

namespace coinbase {

struct Trades {
    std::string trade_id;
    std::string product_id;
    std::string exchange;
    double price;
    double size;
    uint64_t time;
    Side side;
};

inline void from_json(const json &j, Trades t) {
    VARIABLE_FROM_JSON(j, t, trade_id);
    VARIABLE_FROM_JSON(j, t, product_id);
    VARIABLE_FROM_JSON(j, t, exchange);
    DOUBLE_FROM_JSON(j, t, price);
    DOUBLE_FROM_JSON(j, t, size);
    TIMESTAMP_FROM_JSON(j, t, time);
    ENUM_FROM_JSON(j, t, side);
}

struct MarketTrades {
    std::vector<Trades> trades;
    double best_bid;
    double best_ask;
};

inline void from_json(const json &j, MarketTrades &mt) {
    STRUCT_FROM_JSON(j, mt, trades);
    DOUBLE_FROM_JSON(j, mt, best_bid);
    DOUBLE_FROM_JSON(j, mt, best_ask);
}

struct MarketTradesQueryParams {
    uint32_t limit;
    std::optional<uint64_t> start;
    std::optional<uint64_t> end;

    std::string operator()() const {
        std::vector<std::string> params{std::format("limit={}", limit)};
        if (start.has_value()) {
            params.emplace_back(std::format("start={}", timestamp_to_string(start.value())));
        }
        if (end.has_value()) {
            params.emplace_back(std::format("end={}", timestamp_to_string(end.value())));
        }
        return std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
            [](const std::string& a, const std::string &b) {
                return a + "&" + b;
            }));
    }
};

}   // end namespace coinbase