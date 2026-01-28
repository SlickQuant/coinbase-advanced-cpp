// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include <coinbase/utils.hpp>
#include <coinbase/market_data.hpp>

using json = nlohmann::json;

namespace coinbase {

template<Side side>
struct SideBook {
    using CMP = typename std::conditional<side == Side::BUY, std::greater<double>, std::less<double>>::type;
    std::map<double, double, CMP> levels;
};

struct Level2Book {
    std::string product_id;
    uint64_t last_update_time = 0;
    SideBook<Side::BUY> bids;
    SideBook<Side::SELL> asks;

    void onLevel2Snapshot(uint64_t seq_num, const Level2UpdateBatch &batch) {
        bids.levels.clear();
        asks.levels.clear();
        for (const auto &update : batch.updates) {
            if (update.side == Side::BUY) {
                bids.levels.emplace(update.price_level, update.new_quantity);
            } else {
                asks.levels.emplace(update.price_level, update.new_quantity);
            }
        }
        if (!batch.updates.empty()) {
            last_update_time = batch.updates.back().event_time;
        }
    }

    void onLevel2Updates(uint64_t seq_num, const Level2UpdateBatch &batch) {
        for (const auto &update : batch.updates) {
            if (update.event_time <= last_update_time) {
                LOG_TRACE("{} Skipping update event_time {} <= last_update_time {}", product_id, update.event_time, last_update_time);
                continue;
            }
            if (update.side == Side::BUY) {
                auto it = bids.levels.find(update.price_level);
                if (it != bids.levels.end()) {
                    if (update.new_quantity == 0) {
                        bids.levels.erase(it);
                    } else {
                        it->second = update.new_quantity;
                    }
                } else if (update.new_quantity > 0) {
                    bids.levels.emplace(update.price_level, update.new_quantity);
                }
            } else {
                auto it = asks.levels.find(update.price_level);
                if (it != asks.levels.end()) {
                    if (update.new_quantity == 0) {
                        asks.levels.erase(it);
                    } else {
                        it->second = update.new_quantity;
                    }
                } else if (update.new_quantity > 0) {
                    asks.levels.emplace(update.price_level, update.new_quantity);
                }
            }
            last_update_time = update.event_time;
        }
    }

    void onMarketTrades(uint64_t seq_num, const std::vector<MarketTrade> &trades) {
        for (const auto &trade : trades) {
            if (trade.time <= last_update_time) {
                LOG_TRACE("{} Skipping trade event_time {} <= last_update_time {}", product_id, trade.time, last_update_time);
                continue;
            }
            if (trade.side == Side::BUY) {
                auto it = bids.levels.find(trade.price);
                if (it != bids.levels.end()) {
                    it->second -= trade.size;
                    if (it->second <= 0) {
                        bids.levels.erase(it);
                    }
                }
            } else {
                auto it = asks.levels.find(trade.price);
                if (it != asks.levels.end()) {
                    it->second -= trade.size;
                    if (it->second <= 0) {
                        asks.levels.erase(it);
                    }
                }
            }
            last_update_time = trade.time;
        }
    }
};

}