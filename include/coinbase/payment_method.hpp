// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <coinbase/utils.hpp>

using json = nlohmann::json;

namespace coinbase {

struct PaymentMethod {
    std::string id;
    std::string type;
    std::string name;
    std::string currency;
    bool verified;
    bool allow_buy;
    bool allow_sell;
    bool allow_deposit;
    bool allow_withdraw;
    uint64_t created_at;
    uint64_t updated_at;
};

inline void from_json(const json &j, PaymentMethod &m) {
    VARIABLE_FROM_JSON(j, m, id);
    VARIABLE_FROM_JSON(j, m, type);
    VARIABLE_FROM_JSON(j, m, name);
    VARIABLE_FROM_JSON(j, m, currency);
    VARIABLE_FROM_JSON(j, m, verified);
    VARIABLE_FROM_JSON(j, m, allow_buy);
    VARIABLE_FROM_JSON(j, m, allow_sell);
    VARIABLE_FROM_JSON(j, m, allow_deposit);
    VARIABLE_FROM_JSON(j, m, allow_withdraw);
    TIMESTAMP_FROM_JSON(j, m, created_at);
    TIMESTAMP_FROM_JSON(j, m, updated_at);
}

}   // end namespace coinbase
