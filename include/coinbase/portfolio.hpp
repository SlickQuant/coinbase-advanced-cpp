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

enum class PortfolioType : uint8_t {
    UNDEFINED,
    DEFAULT,
    CONSUMER,
    INTX,
};

inline PortfolioType to_type(std::string_view pt) {
    if (pt == "DEFAULT") {
        return PortfolioType::DEFAULT;
    }
    if (pt == "CONSUMER") {
        return PortfolioType::CONSUMER;
    }
    if (pt == "INTX") {
        return PortfolioType::INTX;
    }
    return PortfolioType::UNDEFINED;
}

inline std::string to_string(PortfolioType pt) {
    switch (pt) {
    case PortfolioType::UNDEFINED:
        return "UNDEFINED";
    case PortfolioType::DEFAULT:
        return "DEFAULT";
    case PortfolioType::CONSUMER:
        return "CONSUMER";
    case PortfolioType::INTX:
        return "INTX";
    }
    return "UNDEFINED";
}

struct Portfolio {
    std::string name;
    std::string uuid;
    PortfolioType type;
};

inline void from_json(const json &j, Portfolio &p) {
    VARIABLE_FROM_JSON(j, p, name);
    VARIABLE_FROM_JSON(j, p, uuid);
    ENUM_FROM_JSON(j, p, type);
}

struct PortfolioBalances {
    Amount total_balance;
    Amount total_futures_balance;
    Amount total_cash_equivalent_balance;
    Amount total_crypto_balance;
    Amount total_neptune_balance;
};

inline void from_json(const json &j, PortfolioBalances &b) {
    STRUCT_FROM_JSON(j, b, total_balance);
    STRUCT_FROM_JSON(j, b, total_futures_balance);
    STRUCT_FROM_JSON(j, b, total_cash_equivalent_balance);
    STRUCT_FROM_JSON(j, b, total_crypto_balance);
    STRUCT_FROM_JSON(j, b, total_neptune_balance);
}

struct PortfolioPosition {
    std::string asset;
    std::string account_uuid;
    std::string expires_at;
    double total_balance_fiat;
    double total_balance_crypto;
    double available_to_trade_fiat;
    double allocation;
    double one_day_change;
    double leverage;
    double rate;
    Amount cost_basis;
};

inline void from_json(const json &j, PortfolioPosition &p) {
    VARIABLE_FROM_JSON(j, p, asset);
    VARIABLE_FROM_JSON(j, p, account_uuid);
    VARIABLE_FROM_JSON(j, p, expires_at);
    DOUBLE_FROM_JSON(j, p, total_balance_fiat);
    DOUBLE_FROM_JSON(j, p, total_balance_crypto);
    DOUBLE_FROM_JSON(j, p, available_to_trade_fiat);
    DOUBLE_FROM_JSON(j, p, allocation);
    DOUBLE_FROM_JSON(j, p, one_day_change);
    DOUBLE_FROM_JSON(j, p, leverage);
    DOUBLE_FROM_JSON(j, p, rate);
    STRUCT_FROM_JSON(j, p, cost_basis);
}

struct PortfolioBreakdown {
    Portfolio portfolio;
    PortfolioBalances portfolio_balances;
    std::vector<PortfolioPosition> spot_positions;
    std::vector<PortfolioPosition> perp_positions;
    std::vector<PortfolioPosition> futures_positions;
};

inline void from_json(const json &j, PortfolioBreakdown &b) {
    STRUCT_FROM_JSON(j, b, portfolio);
    STRUCT_FROM_JSON(j, b, portfolio_balances);
    STRUCT_FROM_JSON(j, b, spot_positions);
    STRUCT_FROM_JSON(j, b, perp_positions);
    STRUCT_FROM_JSON(j, b, futures_positions);
}

struct MovePortfolioFundsResult {
    std::string source_portfolio_uuid;
    std::string target_portfolio_uuid;
};

inline void from_json(const json &j, MovePortfolioFundsResult &r) {
    VARIABLE_FROM_JSON(j, r, source_portfolio_uuid);
    VARIABLE_FROM_JSON(j, r, target_portfolio_uuid);
}

}   // end namespace coinbase
