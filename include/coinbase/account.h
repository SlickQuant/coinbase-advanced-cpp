// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-

#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <coinbase/utils.h>

using json = nlohmann::json;

namespace coinbase {

struct Balance
{
    double value;
    std::string currency;
};

inline void from_json(const json& j, Balance& b) {
    VARIABLE_FROM_JSON(j, b, currency);
    DOUBLE_FROM_JSON(j, b, value);
}

struct Account {
    std::string uuid;
    std::string name;
    std::string currency;
    std::string retail_portfolio_id;
    std::string platform;
    std::string type;
    Balance available_balance;
    Balance hold;
    uint64_t created_at;
    uint64_t updated_at;
    uint64_t deleted_at;
    bool is_default;
    bool active;
    bool ready;
};

inline void from_json(const json& j, Account& account) {
    VARIABLE_FROM_JSON(j, account, uuid);
    VARIABLE_FROM_JSON(j, account, name);
    VARIABLE_FROM_JSON(j, account, currency);
    VARIABLE_FROM_JSON(j, account, retail_portfolio_id);
    VARIABLE_FROM_JSON(j, account, platform);
    VARIABLE_FROM_JSON(j, account, type);
    STRUCT_FROM_JSON(j, account, available_balance);
    STRUCT_FROM_JSON(j, account, hold);
    TIMESTAMP_FROM_JSON(j, account, created_at);
    TIMESTAMP_FROM_JSON(j, account, updated_at);
    TIMESTAMP_FROM_JSON(j, account, deleted_at);
    j.at("default").get_to(account.is_default);
    VARIABLE_FROM_JSON(j, account, active);
    VARIABLE_FROM_JSON(j, account, ready);
}

struct AccountQueryParams {
    std::optional<uint32_t> limit;
    std::optional<std::string> cursor;

    std::string operator()() const {
        std::vector<std::string> params;
        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }
        if (cursor.has_value()) {
            params.emplace_back(std::format("cursor={}", cursor.value()));
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

}   // end namespace coinbase