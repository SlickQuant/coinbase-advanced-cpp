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

struct ConvertTradePaymentMethod {
    std::string type;
    std::string network;
};

inline void from_json(const json &j, ConvertTradePaymentMethod &m) {
    VARIABLE_FROM_JSON(j, m, type);
    VARIABLE_FROM_JSON(j, m, network);
}

struct Fee {
    std::string title;
    std::string description;
    std::string label;
    Amount amount;
};

inline void from_json(const json &j, Fee &f) {
    VARIABLE_FROM_JSON(j, f, title);
    VARIABLE_FROM_JSON(j, f, description);
    VARIABLE_FROM_JSON(j, f, label);
    STRUCT_FROM_JSON(j, f, amount);
}

struct ConvertTrade {
    std::string id;
    std::string status;
    std::string user_reference;
    std::string source_currency;
    std::string source_id;
    std::string target_id;
    Amount user_entered_amount;
    Amount amount;
    Amount subtotal;
    Amount total;
    Amount total_fee;
    Amount exchange_rate;
    Amount fiat_denoted_total;
    std::vector<Fee> fees;
    ConvertTradePaymentMethod source;
    ConvertTradePaymentMethod target;
};

inline void from_json(const json &j, ConvertTrade &t) {
    VARIABLE_FROM_JSON(j, t, id);
    VARIABLE_FROM_JSON(j, t, status);
    VARIABLE_FROM_JSON(j, t, user_reference);
    VARIABLE_FROM_JSON(j, t, source_currency);
    VARIABLE_FROM_JSON(j, t, source_id);
    VARIABLE_FROM_JSON(j, t, target_id);
    STRUCT_FROM_JSON(j, t, user_entered_amount);
    STRUCT_FROM_JSON(j, t, amount);
    STRUCT_FROM_JSON(j, t, subtotal);
    STRUCT_FROM_JSON(j, t, total);
    STRUCT_FROM_JSON(j, t, total_fee);
    STRUCT_FROM_JSON(j, t, exchange_rate);
    STRUCT_FROM_JSON(j, t, fiat_denoted_total);
    STRUCT_FROM_JSON(j, t, fees);
    STRUCT_FROM_JSON(j, t, source);
    STRUCT_FROM_JSON(j, t, target);
}

}   // end namespace coinbase
