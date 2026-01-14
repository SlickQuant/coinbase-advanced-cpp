// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <cstdint>
#include <optional>
#include <nlohmann/json.hpp>
#include <coinbase/utils.h>

using json = nlohmann::json;

namespace coinbase {

struct Candle {
    uint64_t start;
    double low;
    double high;
    double open;
    double close;
    double volume;
    std::string product_id;
};

inline void from_json(const json &j, Candle &c) {
    INT_FROM_JSON(j, c, start);
    DOUBLE_FROM_JSON(j, c, low);
    DOUBLE_FROM_JSON(j, c, high);
    DOUBLE_FROM_JSON(j, c, open);
    DOUBLE_FROM_JSON(j, c, close);
    DOUBLE_FROM_JSON(j, c, volume);
    VARIABLE_FROM_JSON(j, c, product_id);
}

enum class Granularity : uint8_t {
    UNKNOWN_GRANULARITY,
    ONE_MINUTE,
    FIVE_MINUTE,
    FIFTEEN_MINUTE,
    THIRTY_MINUTE,
    ONE_HOUR,
    TWO_HOUR,
    FOUR_HOUR,
    SIX_HOUR,
    ONE_DAY,
};

inline std::string to_string(Granularity g) {
    switch(g) {
    case Granularity::ONE_MINUTE:
        return "ONE_MINUTE";
    case Granularity::FIVE_MINUTE:
        return "FIVE_MINUTE";
    case Granularity::FIFTEEN_MINUTE:
        return "FIFTEEN_MINUTE";
    case Granularity::THIRTY_MINUTE:
        return "THIRTY_MINUTE";
    case Granularity::ONE_HOUR:
        return "ONE_HOUR";
    case Granularity::TWO_HOUR:
        return "TWO_HOUR";
    case Granularity::FOUR_HOUR:
        return "FOUR_HOUR";
    case Granularity::SIX_HOUR:
        return "SIX_HOUR";
    case Granularity::ONE_DAY:
        return "ONE_DAY";
    case Granularity::UNKNOWN_GRANULARITY:
        return "UNKNOWN_GRANULARITY";
    }
    return "UNKNOWN_GRANULARITY";
}

struct ProductCandlesQueryParams {
    uint64_t start; // UNIX timestamp in seconds
    uint64_t end;   // UNIX timestamp in seconds
    Granularity granularity;
    std::optional<uint32_t> limit;

    std::string operator()() const {
        std::vector<std::string> params({
            std::format("start={}", start),
            std::format("end={}", end),
            std::format("granularity={}", to_string(granularity))
        });
        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }

        return std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
            [](const std::string& a, const std::string &b) {
                return a + "&" + b;
            }));
    }
};

}   // end namespace coinbase