// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <string_view>
#include <coinbase/utils.h>
#include <nlohmann/json.hpp>

namespace coinbase {

using json = nlohmann::json;

enum PositionSide {
    LONG,
    SHORT,
    UNKNOWN,
};

inline PositionSide to_position_side(std::string_view s) {
    if (s == "LONG") {
        return PositionSide::LONG;
    } else if (s == "SHORT") {
        return PositionSide::SHORT;
    }
    return PositionSide::UNKNOWN;
}

inline std::string to_string(PositionSide side) {
    switch (side) {
    case PositionSide::LONG:
        return "LONG";
    case PositionSide::SHORT:
        return "SHORT";
    case PositionSide::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

struct PerpetualFuturePosition {
    std::string product_id;
    std::string portfolio_uuid;
    std::string margin_type;
    double vwap;
    double entry_vwap;
    double net_size;
    double buy_order_size;
    double sell_order_size;
    double leverage;
    double mark_price;
    double liquidation_price;
    double im_notional;
    double mm_notional;
    double position_notional;
    double unrealized_pnl;
    double aggregated_pnl;
    PositionSide position_side;
};

inline void from_json(const json &j, PerpetualFuturePosition &p) {
    VARIABLE_FROM_JSON(j, p, product_id);
    VARIABLE_FROM_JSON(j, p, portfolio_uuid);
    VARIABLE_FROM_JSON(j, p, margin_type);
    DOUBLE_FROM_JSON(j, p, vwap);
    DOUBLE_FROM_JSON(j, p, entry_vwap);
    DOUBLE_FROM_JSON(j, p, net_size);
    DOUBLE_FROM_JSON(j, p, buy_order_size);
    DOUBLE_FROM_JSON(j, p, sell_order_size);
    DOUBLE_FROM_JSON(j, p, leverage);
    DOUBLE_FROM_JSON(j, p, mark_price);
    DOUBLE_FROM_JSON(j, p, liquidation_price);
    DOUBLE_FROM_JSON(j, p, im_notional);
    DOUBLE_FROM_JSON(j, p, mm_notional);
    DOUBLE_FROM_JSON(j, p, position_notional);
    DOUBLE_FROM_JSON(j, p, unrealized_pnl);
    DOUBLE_FROM_JSON(j, p, aggregated_pnl);
    VARIABLE_FROM_JSON(j, p, position_side);
}

struct ExpiringFuturePosition {
    std::string product_id;
    Side side;
    int32_t number_of_contracts;
    double realized_pnl;
    double unrealized_pnl;
    double entry_price;
};

inline void from_json(const json &j, ExpiringFuturePosition &p) {
    VARIABLE_FROM_JSON(j, p, product_id);
    ENUM_FROM_JSON(j, p, side);
    INT_FROM_JSON(j, p, number_of_contracts);
    DOUBLE_FROM_JSON(j, p, realized_pnl);
    DOUBLE_FROM_JSON(j, p, unrealized_pnl);
    DOUBLE_FROM_JSON(j, p, entry_price);
}

}