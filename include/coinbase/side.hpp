// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>

namespace coinbase {

enum class Side : uint8_t {
    BUY,
    SELL,
};

inline Side to_side(std::string_view s) {
    return (s == "BUY" || s == "bid") ? Side::BUY : Side::SELL; 
}

inline std::string to_string(Side s) {
    return s == Side::BUY ? "BUY" : "SELL";
}

}