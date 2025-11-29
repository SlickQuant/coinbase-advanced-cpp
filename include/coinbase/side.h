#pragma once

#include <string>

namespace coinbase {

enum class Side : uint8_t {
    BUY,
    SELL,
};

inline Side to_side(std::string_view s) {
    return s == "BUY" ? Side::BUY : Side::SELL; 
}

inline std::string to_string(Side s) {
    return s == Side::BUY ? "BUY" : "SELL";
}

}