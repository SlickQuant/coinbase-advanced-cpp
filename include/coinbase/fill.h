#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <coinbase/common.h>
#include <nlohmann/json.hpp>
#include <coinbase/utils.h>

using json = nlohmann::json;

namespace coinbase {

struct Fill {
    std::string entry_id;
    std::string trade_id;
    std::string order_id;
    std::string user_id;
    std::string trade_type;
    std::string product_id;
    std::string liquidity_indicator;
    std::string retail_portfolio_id;
    std::string fillSource;
    uint64_t trade_time;
    uint64_t sequence_timestamp;
    double price;
    double size;
    double commission;
    bool size_in_quote;
    Side side;
    Commission commission_detail_total;
};

inline void from_json(const json &j, Fill &f) {
    VARIABLE_FROM_JSON(j, f, entry_id);
    VARIABLE_FROM_JSON(j, f, trade_id);
    VARIABLE_FROM_JSON(j, f, order_id);
    VARIABLE_FROM_JSON(j, f, user_id);
    VARIABLE_FROM_JSON(j, f, trade_type);
    VARIABLE_FROM_JSON(j, f, product_id);
    VARIABLE_FROM_JSON(j, f, liquidity_indicator);
    VARIABLE_FROM_JSON(j, f, retail_portfolio_id);
    VARIABLE_FROM_JSON(j, f, fillSource);
    TIMESTAMP_FROM_JSON(j, f, trade_time);
    TIMESTAMP_FROM_JSON(j, f, sequence_timestamp);
    DOUBLE_FROM_JSON(j, f, price);
    DOUBLE_FROM_JSON(j, f, size);
    DOUBLE_FROM_JSON(j, f, commission);
    VARIABLE_FROM_JSON(j, f, size_in_quote);
    ENUM_FROM_JSON(j, f, side);
    STRUCT_FROM_JSON(j, f, commission_detail_total);
}

enum class TradeSortBy : uint8_t {
    UNKNOWN_SORT_BY,
    PRICE,
    TRADE_TIME,
};

inline std::string to_string(TradeSortBy tsb) {
    switch(tsb) {
    case TradeSortBy::UNKNOWN_SORT_BY:
        return "UNKNOWN_SORT_BY";
    case TradeSortBy::PRICE:
        return "PRICE";
    case TradeSortBy::TRADE_TIME:
        return "TRADE_TIME";
    }
    return "UNKNOWN_SORT_BY";
}

struct FillQueryParams {
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> trade_ids;
    std::optional<std::vector<std::string>> product_ids;
    std::optional<std::string> start_sequence_timestamp;
    std::optional<std::string> end_sequeence_timestamp;
    std::optional<uint32_t> limit;
    std::optional<std::string> cursor;
    std::optional<std::vector<std::string>> asset_filters;
    std::optional<std::vector<OrderType>> order_types;
    std::optional<Side> order_side;
    std::optional<std::vector<ProductType>> product_types;

    std::string operator()() const {
        std::vector<std::string> params;
        if (order_ids.has_value()) {
            for (const auto &oid : order_ids.value()) {
                params.emplace_back(std::format("order_ids={}", oid));
            }
        }
        if (trade_ids.has_value()) {
            for (const auto &tid : trade_ids.value()) {
                params.emplace_back(std::format("trade_ids={}", tid));
            }
        }
        if (product_ids.has_value()) {
            for (const auto &pid : product_ids.value()) {
                params.emplace_back(std::format("product_ids={}", pid));
            }
        }
        if (start_sequence_timestamp.has_value()) {
            params.emplace_back(std::format("start_sequence_timestamp={}", start_sequence_timestamp.value()));
        }
        if (end_sequeence_timestamp.has_value()) {
            params.emplace_back(std::format("end_sequeence_timestamp={}", end_sequeence_timestamp.value()));
        }
        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }
        if (cursor.has_value()) {
            params.emplace_back(std::format("cursor={}", cursor.value()));
        }
        if (asset_filters.has_value()) {
            for (const auto &f : asset_filters.value()) {
                params.emplace_back(std::format("asset_filters={}", f));
            }
        }
        if (order_types.has_value()) {
            for (auto ot : order_types.value()) {
                params.emplace_back(std::format("order_types={}", to_string(ot)));
            }
        }
        if (order_side.has_value()) {
            params.emplace_back(std::format("order_side={}", to_string(order_side.value())));
        }
        if (product_types.has_value()) {
            for (auto pt : product_types.value()) {
                params.emplace_back(std::format("product_types={}", to_string(pt)));
            }
        }

        if (params.empty()) {
            return "";
        }
        return std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
            [](const std::string& a, const std::string &b) {
                return a + "&" + b;
            }));
    }
};

}