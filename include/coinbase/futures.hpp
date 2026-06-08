// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <coinbase/common.hpp>
#include <coinbase/side.hpp>
#include <coinbase/utils.hpp>

using json = nlohmann::json;

namespace coinbase {

struct FCMBalanceSummary {
    Amount futures_buying_power;
    Amount total_usd_balance;
    Amount cbi_usd_balance;
    Amount cfm_usd_balance;
    Amount total_open_orders_hold_amount;
    Amount unrealized_pnl;
    Amount daily_realized_pnl;
    Amount initial_margin;
    Amount available_margin;
    Amount liquidation_threshold;
    Amount liquidation_buffer_amount;
    std::string liquidation_buffer_percentage;
};

inline void from_json(const json &j, FCMBalanceSummary &b) {
    STRUCT_FROM_JSON(j, b, futures_buying_power);
    STRUCT_FROM_JSON(j, b, total_usd_balance);
    STRUCT_FROM_JSON(j, b, cbi_usd_balance);
    STRUCT_FROM_JSON(j, b, cfm_usd_balance);
    STRUCT_FROM_JSON(j, b, total_open_orders_hold_amount);
    STRUCT_FROM_JSON(j, b, unrealized_pnl);
    STRUCT_FROM_JSON(j, b, daily_realized_pnl);
    STRUCT_FROM_JSON(j, b, initial_margin);
    STRUCT_FROM_JSON(j, b, available_margin);
    STRUCT_FROM_JSON(j, b, liquidation_threshold);
    STRUCT_FROM_JSON(j, b, liquidation_buffer_amount);
    VARIABLE_FROM_JSON(j, b, liquidation_buffer_percentage);
}

struct FCMPosition {
    std::string product_id;
    Side side;
    double number_of_contracts;
    double current_price;
    double avg_entry_price;
    double unrealized_pnl;
    double daily_realized_pnl;
};

inline void from_json(const json &j, FCMPosition &p) {
    VARIABLE_FROM_JSON(j, p, product_id);
    ENUM_FROM_JSON(j, p, side);
    DOUBLE_FROM_JSON(j, p, number_of_contracts);
    DOUBLE_FROM_JSON(j, p, current_price);
    DOUBLE_FROM_JSON(j, p, avg_entry_price);
    DOUBLE_FROM_JSON(j, p, unrealized_pnl);
    DOUBLE_FROM_JSON(j, p, daily_realized_pnl);
}

struct MarginWindow {
    std::string margin_window_type;
    std::string end_time;
};

inline void from_json(const json &j, MarginWindow &w) {
    VARIABLE_FROM_JSON(j, w, margin_window_type);
    VARIABLE_FROM_JSON(j, w, end_time);
}

struct FCMSweep {
    std::string id;
    std::string status;
    Amount requested_amount;
    bool should_sweep_all;
};

inline void from_json(const json &j, FCMSweep &s) {
    VARIABLE_FROM_JSON(j, s, id);
    VARIABLE_FROM_JSON(j, s, status);
    STRUCT_FROM_JSON(j, s, requested_amount);
    VARIABLE_FROM_JSON(j, s, should_sweep_all);
}

struct CurrentMarginWindow {
    MarginWindow margin_window;
    bool is_intraday_margin_killswitch_enabled;
    bool is_intraday_margin_enrollment_killswitch_enabled;
};

inline void from_json(const json &j, CurrentMarginWindow &w) {
    STRUCT_FROM_JSON(j, w, margin_window);
    VARIABLE_FROM_JSON(j, w, is_intraday_margin_killswitch_enabled);
    VARIABLE_FROM_JSON(j, w, is_intraday_margin_enrollment_killswitch_enabled);
}

}   // end namespace coinbase
