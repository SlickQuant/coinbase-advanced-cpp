// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <nlohmann/json.hpp>
#include <coinbase/utils.hpp>
#include <coinbase/common.hpp>

namespace coinbase {

using json = nlohmann::json;

enum class FcmTradingSessionState : uint8_t {
    FCM_TRADING_SESSION_STATE_UNDEFINED,
    FCM_TRADING_SESSION_STATE_PRE_OPEN,
    FCM_TRADING_SESSION_STATE_PRE_OPEN_NO_CANCEL,
    FCM_TRADING_SESSION_STATE_OPEN,
    FCM_TRADING_SESSION_STATE_CLOSE,
};

inline FcmTradingSessionState to_fcm_trading_session_state(std::string_view state) {
    if (state == "FCM_TRADING_SESSION_STATE_PRE_OPEN") {
        return FcmTradingSessionState::FCM_TRADING_SESSION_STATE_PRE_OPEN;
    }

    if (state == "FCM_TRADING_SESSION_STATE_PRE_OPEN_NO_CANCEL") {
        return FcmTradingSessionState::FCM_TRADING_SESSION_STATE_PRE_OPEN_NO_CANCEL;
    }

    if (state == "FCM_TRADING_SESSION_STATE_OPEN") {
        return FcmTradingSessionState::FCM_TRADING_SESSION_STATE_OPEN;
    }

    if (state == "FCM_TRADING_SESSION_STATE_CLOSE") {
        return FcmTradingSessionState::FCM_TRADING_SESSION_STATE_CLOSE;
    }

    return FcmTradingSessionState::FCM_TRADING_SESSION_STATE_UNDEFINED;
}

struct Maintenance {
    uint64_t start_time;
    uint64_t end_time;
};

inline void from_json(const json &j, Maintenance &m) {
    TIMESTAMP_FROM_JSON(j, m, start_time);
    TIMESTAMP_FROM_JSON(j, m, end_time);
}

struct FcmTradingSessionDetails {
    uint64_t open_time;
    uint64_t close_time;
    Maintenance maintenance;
    FcmTradingSessionState session_state;
    std::string close_reason;
    bool is_session_open;
    bool after_hour_order_entry_disabled;
};

inline void from_json(const json &j, FcmTradingSessionDetails &d) {
    TIMESTAMP_FROM_JSON(j, d, open_time);
    TIMESTAMP_FROM_JSON(j, d, close_time);
    d.session_state = to_fcm_trading_session_state(j.at("session_state").get<std::string_view>());
    VARIABLE_FROM_JSON(j, d, close_reason);
    VARIABLE_FROM_JSON(j, d, is_session_open);
    VARIABLE_FROM_JSON(j, d, after_hour_order_entry_disabled);
}

struct PerpetualDetails {
    double open_interest;
    double funding_rate;
    double max_leverage;
    uint64_t funding_time;
    std::string base_asset_uuid;
    std::string underlying_type;
};

inline void from_json(const json &j, PerpetualDetails &p) {
    DOUBLE_FROM_JSON(j, p, open_interest);
    DOUBLE_FROM_JSON(j, p, funding_rate);
    DOUBLE_FROM_JSON(j, p, max_leverage);
    TIMESTAMP_FROM_JSON(j, p, funding_time);
    VARIABLE_FROM_JSON(j, p, base_asset_uuid);
    VARIABLE_FROM_JSON(j, p, underlying_type);
}

struct MarginRate {
    double long_margin_rate;
    double short_margin_rate;
};

inline void from_json(const json &j, MarginRate &r) {
    DOUBLE_FROM_JSON(j, r, long_margin_rate);
    DOUBLE_FROM_JSON(j, r, short_margin_rate);
}

struct FutureProductDetails {
    std::string venue;
    std::string contract_code;
    std::string contract_root_unit;
    std::string group_description;
    std::string contract_expiry_timezone;
    std::string group_short_description;
    std::string risk_managed_by;
    std::string contract_expiry_type;
    std::string contract_display_name;
    std::string contract_expiry_name;
    std::string funding_interval;
    std::string open_interest;
    std::string funding_rate;
    std::string display_name;
    uint64_t contract_expiry;
    uint64_t time_to_expiry_ms;
    uint64_t funding_time;
    double contract_size;
    PerpetualDetails perpetual_details;
    MarginRate intraday_margin_rate;
    MarginRate overnight_margin_rate;
    bool non_crypto;
    bool twenty_four_by_seven;
};

inline void from_json(const json &j, FutureProductDetails &d) {
    VARIABLE_FROM_JSON(j, d, venue);
    VARIABLE_FROM_JSON(j, d, contract_code);
    VARIABLE_FROM_JSON(j, d, contract_root_unit);
    VARIABLE_FROM_JSON(j, d, group_description);
    VARIABLE_FROM_JSON(j, d, contract_expiry_timezone);
    VARIABLE_FROM_JSON(j, d, group_short_description);
    VARIABLE_FROM_JSON(j, d, risk_managed_by);
    VARIABLE_FROM_JSON(j, d, contract_expiry_type);
    VARIABLE_FROM_JSON(j, d, contract_display_name);
    VARIABLE_FROM_JSON(j, d, contract_expiry_name);
    VARIABLE_FROM_JSON(j, d, funding_interval);
    VARIABLE_FROM_JSON(j, d, open_interest);
    VARIABLE_FROM_JSON(j, d, funding_rate);
    VARIABLE_FROM_JSON(j, d, display_name);
    TIMESTAMP_FROM_JSON(j, d, contract_expiry);
    TIMESTAMP_FROM_JSON(j, d, time_to_expiry_ms);
    TIMESTAMP_FROM_JSON(j, d, funding_time);
    DOUBLE_FROM_JSON(j, d, contract_size);
    STRUCT_FROM_JSON(j, d, perpetual_details);
    STRUCT_FROM_JSON(j, d, intraday_margin_rate);
    STRUCT_FROM_JSON(j, d, overnight_margin_rate);
    VARIABLE_FROM_JSON(j, d, non_crypto);
    VARIABLE_FROM_JSON(j, d, twenty_four_by_seven);
}

struct SettlementSource {
    std::string url;
    std::string name;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SettlementSource, url, name)
};

struct PredictionMarketProductDetails {
    std::string contract_code;
    std::string group_description;
    std::string group_short_description;
    std::string venue;
    std::string sole_venue_product_id;
    std::string event_title;
    std::string event_subtitle;
    std::string series_ticker;
    std::string event_ticker;
    std::string market_ticker;
    std::string sector;
    std::string yes_subtitle;
    std::string rules_primary;
    std::string rules_secondary;
    std::string early_close_condition;
    std::string series_cbrn;
    std::string event_cbrn;
    std::string market_cbrn;
    std::string scope;
    std::string yes_titles;
    std::vector<std::string> prehibitions;
    std::vector<SettlementSource> settlement_sources;
    std::vector<std::string> tags;
    uint64_t contract_expiry;
    uint64_t trade_starting_time;
    uint64_t settlement_timestamp;
    uint64_t settlement_timer_seconds;
    double settlement_price;
    bool twenty_four_by_seven;
    bool can_close_early;
};

inline void from_json(const json &j, PredictionMarketProductDetails &p) {
    VARIABLE_FROM_JSON(j, p, contract_code);
    VARIABLE_FROM_JSON(j, p, group_description);
    VARIABLE_FROM_JSON(j, p, group_short_description);
    VARIABLE_FROM_JSON(j, p, venue);
    VARIABLE_FROM_JSON(j, p, sole_venue_product_id);
    VARIABLE_FROM_JSON(j, p, event_title);
    VARIABLE_FROM_JSON(j, p, event_subtitle);
    VARIABLE_FROM_JSON(j, p, series_ticker);
    VARIABLE_FROM_JSON(j, p, event_ticker);
    VARIABLE_FROM_JSON(j, p, market_ticker);
    VARIABLE_FROM_JSON(j, p, sector);
    VARIABLE_FROM_JSON(j, p, yes_subtitle);
    VARIABLE_FROM_JSON(j, p, rules_primary);
    VARIABLE_FROM_JSON(j, p, rules_secondary);
    VARIABLE_FROM_JSON(j, p, early_close_condition);
    VARIABLE_FROM_JSON(j, p, series_cbrn);
    VARIABLE_FROM_JSON(j, p, event_cbrn);
    VARIABLE_FROM_JSON(j, p, market_cbrn);
    VARIABLE_FROM_JSON(j, p, scope);
    VARIABLE_FROM_JSON(j, p, yes_titles);
    p.prehibitions = j["prehibitions"];
    p.settlement_sources = j["settlement_sources"];
    p.tags = j["tags"];
    TIMESTAMP_FROM_JSON(j, p, contract_expiry);
    TIMESTAMP_FROM_JSON(j, p, trade_starting_time);
    TIMESTAMP_FROM_JSON(j, p, settlement_timestamp);
    TIMESTAMP_FROM_JSON(j, p, settlement_timer_seconds);
    DOUBLE_FROM_JSON(j, p, settlement_price);
    VARIABLE_FROM_JSON(j, p, twenty_four_by_seven);
    VARIABLE_FROM_JSON(j, p, can_close_early);
}

struct EquityProductDetails {
    std::string equity_subtype;
    std::string ticker;
    std::string description;
    uint64_t trading_halted_start_time;
    uint64_t trading_halted_end_time;
    double open_price;
    double volume_today;
    bool fractionable;
    bool liquidate_only;
    bool trading_halted;
};

inline void from_json(const json &j, EquityProductDetails &d) {
    VARIABLE_FROM_JSON(j, d, equity_subtype);
    VARIABLE_FROM_JSON(j, d, ticker);
    VARIABLE_FROM_JSON(j, d, description);
    TIMESTAMP_FROM_JSON(j, d, trading_halted_start_time);
    TIMESTAMP_FROM_JSON(j, d, trading_halted_end_time);
    DOUBLE_FROM_JSON(j, d, open_price);
    DOUBLE_FROM_JSON(j, d, volume_today);
    VARIABLE_FROM_JSON(j, d, fractionable);
    VARIABLE_FROM_JSON(j, d, liquidate_only);
    VARIABLE_FROM_JSON(j, d, trading_halted);
}

struct Product {
    std::string product_id;
    std::string base_name;
    std::string base_display_symbol;
    std::string quote_name;
    std::string quote_display_symbol;
    std::string status;
    std::string quote_currency_id;
    std::string base_currency_id;
    std::string alias;
    std::string display_name;
    std::string product_venue;
    std::vector<std::string> alias_to;
    double price;
    double price_percentage_change_24h;
    double price_increment;
    double volume_24h;
    double volume_percentage_change_24h;
    double base_increment = 0.;
    double base_min_size = 0.;
    double base_max_size = 0.;
    double quote_increment = 0.;
    double quote_min_size = 0.;
    double quote_max_size = 0.;
    double mid_market_price;
    double approximate_quote_24h_volume;
    double market_cap;
    uint64_t new_at;
    ProductType product_type;
    bool watched;
    bool is_disabled;
    bool is_new;
    bool cancel_only;
    bool limit_only;
    bool post_only;
    bool trading_disabled;
    bool auction_mode;
    bool view_only;
    FcmTradingSessionDetails fcm_trading_session_details;
    FutureProductDetails future_product_details;
    EquityProductDetails equity_product_details;
};

inline void from_json(const json& j, Product& p) {
    VARIABLE_FROM_JSON(j, p, product_id);
    VARIABLE_FROM_JSON(j, p, base_name);
    VARIABLE_FROM_JSON(j, p, quote_name);
    VARIABLE_FROM_JSON(j, p, status);
    DOUBLE_FROM_JSON(j, p, base_increment);
    DOUBLE_FROM_JSON(j, p, base_min_size);
    DOUBLE_FROM_JSON(j, p, base_max_size);
    DOUBLE_FROM_JSON(j, p, quote_increment);
    DOUBLE_FROM_JSON(j, p, quote_min_size);
    DOUBLE_FROM_JSON(j, p, quote_max_size);
    ENUM_FROM_JSON(j, p, product_type);
}

enum class ExpiringContractStatus : uint8_t {
    UNKNOWN_EXPIRING_CONTRACT_STATUS,
    STATUS_UNEXPIRED,
    STATUS_EXPIRED,
    STATUS_ALL,
};

inline std::string to_string(ExpiringContractStatus status) {
    switch(status) {
    case ExpiringContractStatus::UNKNOWN_EXPIRING_CONTRACT_STATUS:
        return "UNKNOWN_EXPIRING_CONTRACT_STATUS";
    case ExpiringContractStatus::STATUS_UNEXPIRED:
        return "STATUS_UNEXPIRED";
    case ExpiringContractStatus::STATUS_EXPIRED:
        return "STATUS_EXPIRED";
    case ExpiringContractStatus::STATUS_ALL:
        return "STATUS_ALL";
    }
    return "UNKNOWN_EXPIRING_CONTRACT_STATUS";
}

enum class ProductsSortOrder {
    PRODUCTS_SORT_ORDER_UNDEFINED,
    PRODUCTS_SORT_ORDER_VOLUME_24H_DESCENDING,
    PRODUCTS_SORT_ORDER_LIST_TIME_DESCENDING 
};

inline std::string to_string(ProductsSortOrder pso) {
    switch(pso) {
    case ProductsSortOrder::PRODUCTS_SORT_ORDER_UNDEFINED:
        return "UNKNOWN_EXPIRING_CONTRACT_STATUS";
    case ProductsSortOrder::PRODUCTS_SORT_ORDER_VOLUME_24H_DESCENDING:
        return "PRODUCTS_SORT_ORDER_VOLUME_24H_DESCENDING";
    case ProductsSortOrder::PRODUCTS_SORT_ORDER_LIST_TIME_DESCENDING:
        return "PRODUCTS_SORT_ORDER_LIST_TIME_DESCENDING";
    }
    return "PRODUCTS_SORT_ORDER_UNDEFINED";
}

struct ProductQueryParams {
    std::optional<uint32_t> limit;
    std::optional<uint32_t> offset;
    std::optional<ProductType> product_type;
    std::optional<std::vector<std::string>> product_ids;
    std::optional<ContractExpiryType> contract_expiry_type;
    std::optional<ExpiringContractStatus> expiring_contract_status;
    std::optional<bool> get_tradability_status;
    std::optional<bool> get_all_products;
    std::optional<ProductsSortOrder> products_sort_order;

    std::string operator()() const {
        std::vector<std::string> params;
        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }

        if (offset.has_value()) {
            params.emplace_back(std::format("offset={}", offset.value()));
        }

        if (product_type.has_value()) {
            params.emplace_back(std::format("product_type={}", to_string(product_type.value())));
        }

        if (product_ids.has_value()) {
            for (auto id : product_ids.value()) {
                params.emplace_back(std::format("product_ids={}", id));
            }
        }

        if (contract_expiry_type.has_value()) {
            params.emplace_back(std::format("contract_expiry_type={}", to_string(contract_expiry_type.value())));
        }

        if (expiring_contract_status.has_value()) {
            params.emplace_back(std::format("expiring_contract_status={}", to_string(expiring_contract_status.value())));
        }

        if (get_tradability_status.has_value()) {
            params.emplace_back(std::format("get_tradability_status={}", get_tradability_status.value() ? "true" : "false"));
        }

        if (get_all_products.has_value()) {
            params.emplace_back(std::format("get_all_products={}", get_all_products.value() ? "true" : "false"));
        }

        if (products_sort_order.has_value()) {
            params.emplace_back(std::format("products_sort_order={}", to_string(products_sort_order.value())));
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