// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <coinbase/common.hpp>
#include <coinbase/utils.hpp>

using json = nlohmann::json;

namespace coinbase {

struct PerpetualPortfolio {
    std::string portfolio_uuid;
    std::string margin_type;
    std::string liquidation_status;
    double collateral;
    double position_notional;
    double open_position_notional;
    double pending_fees;
    double borrow;
    double accrued_interest;
    double rolling_debt;
    double portfolio_initial_margin;
    double portfolio_maintenance_margin;
    double liquidation_percentage;
    double liquidation_buffer;
    Amount portfolio_im_notional;
    Amount portfolio_mm_notional;
    Amount unrealized_pnl;
    Amount total_balance;
};

inline void from_json(const json &j, PerpetualPortfolio &p) {
    VARIABLE_FROM_JSON(j, p, portfolio_uuid);
    VARIABLE_FROM_JSON(j, p, margin_type);
    VARIABLE_FROM_JSON(j, p, liquidation_status);
    DOUBLE_FROM_JSON(j, p, collateral);
    DOUBLE_FROM_JSON(j, p, position_notional);
    DOUBLE_FROM_JSON(j, p, open_position_notional);
    DOUBLE_FROM_JSON(j, p, pending_fees);
    DOUBLE_FROM_JSON(j, p, borrow);
    DOUBLE_FROM_JSON(j, p, accrued_interest);
    DOUBLE_FROM_JSON(j, p, rolling_debt);
    DOUBLE_FROM_JSON(j, p, portfolio_initial_margin);
    DOUBLE_FROM_JSON(j, p, portfolio_maintenance_margin);
    DOUBLE_FROM_JSON(j, p, liquidation_percentage);
    DOUBLE_FROM_JSON(j, p, liquidation_buffer);
    STRUCT_FROM_JSON(j, p, portfolio_im_notional);
    STRUCT_FROM_JSON(j, p, portfolio_mm_notional);
    STRUCT_FROM_JSON(j, p, unrealized_pnl);
    STRUCT_FROM_JSON(j, p, total_balance);
}

struct PerpsPortfolioSummary {
    Amount unrealized_pnl;
    Amount buying_power;
    Amount total_balance;
    Amount max_withdrawal_amount;
};

inline void from_json(const json &j, PerpsPortfolioSummary &s) {
    STRUCT_FROM_JSON(j, s, unrealized_pnl);
    STRUCT_FROM_JSON(j, s, buying_power);
    STRUCT_FROM_JSON(j, s, total_balance);
    STRUCT_FROM_JSON(j, s, max_withdrawal_amount);
}

struct PerpsPortfolioSummaryResponse {
    std::vector<PerpetualPortfolio> portfolios;
    PerpsPortfolioSummary summary;
};

inline void from_json(const json &j, PerpsPortfolioSummaryResponse &r) {
    STRUCT_FROM_JSON(j, r, portfolios);
    STRUCT_FROM_JSON(j, r, summary);
}

struct PerpsPosition {
    std::string product_id;
    std::string product_uuid;
    std::string portfolio_uuid;
    std::string symbol;
    std::string position_side;
    std::string margin_type;
    double net_size;
    double buy_order_size;
    double sell_order_size;
    double im_contribution;
    double leverage;
    Amount vwap;
    Amount entry_vwap;
    Amount unrealized_pnl;
    Amount mark_price;
    Amount liquidation_price;
    Amount im_notional;
    Amount mm_notional;
    Amount position_notional;
    Amount aggregated_pnl;
};

inline void from_json(const json &j, PerpsPosition &p) {
    VARIABLE_FROM_JSON(j, p, product_id);
    VARIABLE_FROM_JSON(j, p, product_uuid);
    VARIABLE_FROM_JSON(j, p, portfolio_uuid);
    VARIABLE_FROM_JSON(j, p, symbol);
    VARIABLE_FROM_JSON(j, p, position_side);
    VARIABLE_FROM_JSON(j, p, margin_type);
    DOUBLE_FROM_JSON(j, p, net_size);
    DOUBLE_FROM_JSON(j, p, buy_order_size);
    DOUBLE_FROM_JSON(j, p, sell_order_size);
    DOUBLE_FROM_JSON(j, p, im_contribution);
    DOUBLE_FROM_JSON(j, p, leverage);
    STRUCT_FROM_JSON(j, p, vwap);
    STRUCT_FROM_JSON(j, p, entry_vwap);
    STRUCT_FROM_JSON(j, p, unrealized_pnl);
    STRUCT_FROM_JSON(j, p, mark_price);
    STRUCT_FROM_JSON(j, p, liquidation_price);
    STRUCT_FROM_JSON(j, p, im_notional);
    STRUCT_FROM_JSON(j, p, mm_notional);
    STRUCT_FROM_JSON(j, p, position_notional);
    STRUCT_FROM_JSON(j, p, aggregated_pnl);
}

struct PerpsPositionsResponse {
    std::vector<PerpsPosition> positions;
};

inline void from_json(const json &j, PerpsPositionsResponse &r) {
    STRUCT_FROM_JSON(j, r, positions);
}

struct PerpsBalance {
    std::string quantity;
    std::string hold;
    std::string transfer_hold;
    std::string collateral_value;
    std::string collateral_weight;
    std::string max_withdraw_amount;
    std::string loan;
    std::string loan_collateral_requirement_usd;
    std::string pledged_quantity;
};

inline void from_json(const json &j, PerpsBalance &b) {
    VARIABLE_FROM_JSON(j, b, quantity);
    VARIABLE_FROM_JSON(j, b, hold);
    VARIABLE_FROM_JSON(j, b, transfer_hold);
    VARIABLE_FROM_JSON(j, b, collateral_value);
    VARIABLE_FROM_JSON(j, b, collateral_weight);
    VARIABLE_FROM_JSON(j, b, max_withdraw_amount);
    VARIABLE_FROM_JSON(j, b, loan);
    VARIABLE_FROM_JSON(j, b, loan_collateral_requirement_usd);
    VARIABLE_FROM_JSON(j, b, pledged_quantity);
}

struct PerpsPortfolioBalance {
    std::string portfolio_uuid;
    std::vector<PerpsBalance> balances;
    bool is_margin_limit_reached;
};

inline void from_json(const json &j, PerpsPortfolioBalance &b) {
    VARIABLE_FROM_JSON(j, b, portfolio_uuid);
    STRUCT_FROM_JSON(j, b, balances);
    VARIABLE_FROM_JSON(j, b, is_margin_limit_reached);
}

}   // end namespace coinbase
