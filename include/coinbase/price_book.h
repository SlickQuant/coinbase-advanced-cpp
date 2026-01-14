// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <coinbase/utils.h>

using json = nlohmann::json;

namespace coinbase {

struct PriceLevel {
    double price;
    double size;
};

inline void from_json(const json &j, PriceLevel &l) {
    DOUBLE_FROM_JSON(j, l, price);
    DOUBLE_FROM_JSON(j, l, size);
}

struct PriceBook {
    std::string product_id;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    uint64_t time;
};

inline void from_json(const json &j, PriceBook &b) {
    VARIABLE_FROM_JSON(j, b, product_id);
    STRUCT_FROM_JSON(j, b, bids);
    STRUCT_FROM_JSON(j, b, asks);
    TIMESTAMP_FROM_JSON(j, b, time);
}

struct PriceBookQueryParams {
    std::string product_id;
    std::optional<uint32_t> limit;
    std::optional<double> aggregation_price_increment;

    std::string operator()() const {
        std::vector<std::string> params;
        params.emplace_back(std::format("product_id={}", product_id));

        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }

        if (aggregation_price_increment.has_value()) {
            params.emplace_back(std::format("aggregation_price_increment={}", std::to_string(aggregation_price_increment.value())));
        }

        return std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
            [](const std::string& a, const std::string &b) {
                return a + "&" + b;
            }));
    }
};

struct PriceBookResponse {
    PriceBook pricebook;
    double last;
    double mid_market;
    double spread_bps;
    double spread_absolute;
};

inline void from_json(const json &j, PriceBookResponse &b) {
    STRUCT_FROM_JSON(j, b, pricebook);
    b.pricebook = j["pricebook"];
    DOUBLE_FROM_JSON(j, b, last);
    DOUBLE_FROM_JSON(j, b, mid_market);
    DOUBLE_FROM_JSON(j, b, spread_bps);
    DOUBLE_FROM_JSON(j, b, spread_absolute);
}

}