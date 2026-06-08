// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <coinbase/utils.hpp>

using json = nlohmann::json;

namespace coinbase {

struct ApiKeyPermissions {
    bool can_view;
    bool can_trade;
    bool can_transfer;
    std::string portfolio_uuid;
    std::string portfolio_type;
};

inline void from_json(const json &j, ApiKeyPermissions &p) {
    VARIABLE_FROM_JSON(j, p, can_view);
    VARIABLE_FROM_JSON(j, p, can_trade);
    VARIABLE_FROM_JSON(j, p, can_transfer);
    VARIABLE_FROM_JSON(j, p, portfolio_uuid);
    VARIABLE_FROM_JSON(j, p, portfolio_type);
}

}   // end namespace coinbase
