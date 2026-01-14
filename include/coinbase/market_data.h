// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <coinbase/utils.h>
#include <coinbase/common.h>

namespace coinbase {

struct Level2Update {
    uint64_t event_time;
    Side side;
    double price_level;
    double new_quantity;
};

inline void from_json(const json &j, Level2Update &l) {
    TIMESTAMP_FROM_JSON(j, l, event_time);
    ENUM_FROM_JSON(j, l, side);
    DOUBLE_FROM_JSON(j, l, price_level);
    DOUBLE_FROM_JSON(j, l, new_quantity);
}

struct Level2UpdateBatch {
    std::string product_id;
    std::vector<Level2Update> updates;
};

inline void from_json(const json &j, Level2UpdateBatch &l) {
    j.at("product_id").get_to(l.product_id);
    l.updates.reserve(j.at("updates").size());
    l.updates = j["updates"];
}

struct Ticker {
    std::string product_id;
    double price;
    double volume_24_h;
    double low_24_h;
    double high_24_h;
    double low_52_w;
    double high_52_w;
    double price_percent_chg_24_h;
    double best_bid;
    double best_bid_quantity;
    double best_ask;
    double best_ask_quantity;
};

inline void from_json(const json &j, Ticker &t) {
    VARIABLE_FROM_JSON(j, t, product_id);
    DOUBLE_FROM_JSON(j, t, price);
    DOUBLE_FROM_JSON(j, t, volume_24_h);
    DOUBLE_FROM_JSON(j, t, low_24_h);
    DOUBLE_FROM_JSON(j, t, high_24_h);
    DOUBLE_FROM_JSON(j, t, low_52_w);
    DOUBLE_FROM_JSON(j, t, high_52_w);
    DOUBLE_FROM_JSON(j, t, price_percent_chg_24_h);
    DOUBLE_FROM_JSON(j, t, best_bid);
    DOUBLE_FROM_JSON(j, t, best_bid_quantity);
    DOUBLE_FROM_JSON(j, t, best_ask);
    DOUBLE_FROM_JSON(j, t, best_ask_quantity);
}

struct MarketTrade {
    std::string trade_id;
    std::string product_id;
    uint64_t time;
    double price;
    double size;
    Side side;
};

inline void from_json(const json &j, MarketTrade &m) {
    VARIABLE_FROM_JSON(j, m, trade_id);
    VARIABLE_FROM_JSON(j, m, product_id);
    TIMESTAMP_FROM_JSON(j, m, time);
    DOUBLE_FROM_JSON(j, m, price);
    DOUBLE_FROM_JSON(j, m, size);
    ENUM_FROM_JSON(j, m, side);
}

struct Candle {
    uint64_t start;
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string product_id;
};

inline void from_json(const json &j, Candle &c) {
    TIMESTAMP_FROM_JSON(j, c, start);
    DOUBLE_FROM_JSON(j, c, open);
    DOUBLE_FROM_JSON(j, c, high);
    DOUBLE_FROM_JSON(j, c, low);
    DOUBLE_FROM_JSON(j, c, close);
    DOUBLE_FROM_JSON(j, c, volume);
    VARIABLE_FROM_JSON(j, c, product_id);
}

struct Status {
    ProductType product_type;
    std::string id;
    std::string base_currency;
    std::string quote_currency;
    std::string display_name;
    std::string status;
    std::string status_message;
    double base_increment;
    double quote_increment;
    double min_market_funds;
};

inline void from_json(const json &j, Status &s) {
    ENUM_FROM_JSON(j, s, product_type);
    VARIABLE_FROM_JSON(j, s, id);
    VARIABLE_FROM_JSON(j, s, base_currency);
    VARIABLE_FROM_JSON(j, s, quote_currency);
    VARIABLE_FROM_JSON(j, s, display_name);
    VARIABLE_FROM_JSON(j, s, status);
    VARIABLE_FROM_JSON(j, s, status_message);
    DOUBLE_FROM_JSON(j, s, base_increment);
    DOUBLE_FROM_JSON(j, s, quote_increment);
    DOUBLE_FROM_JSON(j, s, min_market_funds);
}

} // end namespace coinbase
