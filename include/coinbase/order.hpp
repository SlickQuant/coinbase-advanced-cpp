// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <optional>
#include <mutex>
#include <coinbase/utils.hpp>
#include <coinbase/common.hpp>

namespace coinbase {

struct MarketConfig {
    std::optional<double> quote_size;
    std::optional<double> base_size;
};

inline void from_json(const json &j, MarketConfig &m) {
    if (j.contains("quote_size")) {
        DOUBLE_FROM_JSON(j, m, quote_size);
    }
    if (j.contains("base_size")) {
        DOUBLE_FROM_JSON(j, m, base_size);
    }
} 

struct LimitConfig : public MarketConfig {
    double limit_price;
};

inline void from_json(const json &j, LimitConfig &l) {
    from_json(j, static_cast<MarketConfig&>(l));
    DOUBLE_FROM_JSON(j, l, limit_price);
}

struct LimitGtcConfig : public LimitConfig {
    bool post_only;
};

inline void from_json(const json &j, LimitGtcConfig &l) {
    from_json(j, static_cast<LimitConfig&>(l));
    VARIABLE_FROM_JSON(j, l, post_only);
}

struct LimitGtdConfig : public LimitGtcConfig {
    uint64_t end_time;
};

inline void from_json(const json &j, LimitGtdConfig &l) {
    from_json(j, static_cast<LimitGtcConfig&>(l));
    TIMESTAMP_FROM_JSON(j, l, end_time);
}

struct TwapGtdConfig : public LimitConfig {
    uint64_t start_time;
    uint64_t end_time;
    uint32_t number_buckets;
    double bucket_size;
    std::string bucket_duration;
};

inline void from_json(const json &j, TwapGtdConfig &c) {
    from_json(j, static_cast<LimitConfig&>(c));
    TIMESTAMP_FROM_JSON(j, c, start_time);
    TIMESTAMP_FROM_JSON(j, c, end_time);
    INT_FROM_JSON(j, c, number_buckets);
    DOUBLE_FROM_JSON(j, c, bucket_size);
    VARIABLE_FROM_JSON(j, c, bucket_duration);
}

enum class StopDirection : uint8_t {
    STOP_DIRECTION_STOP_UP,
    STOP_DIRECTION_STOP_DOWN,
    STOP_DIRECTION_UNKNOWN,
};

inline StopDirection to_stop_direction(std::string_view sv) {
    if (sv == "STOP_DIRECTION_STOP_UP") {
        return StopDirection::STOP_DIRECTION_STOP_UP;
    }

    if (sv == "STOP_DIRECTION_STOP_DOWN") {
        return StopDirection::STOP_DIRECTION_STOP_DOWN;
    }
    return StopDirection::STOP_DIRECTION_UNKNOWN;
}

inline std::string to_string(StopDirection sd) {
    switch(sd) {
    case StopDirection::STOP_DIRECTION_STOP_UP:
        return "STOP_DIRECTION_STOP_UP";
    case StopDirection::STOP_DIRECTION_STOP_DOWN:
        return "STOP_DIRECTION_STOP_DOWN";
    case StopDirection::STOP_DIRECTION_UNKNOWN:
        return "";
    }
    return "";
}

struct StopLimitConfig {
    std::optional<double> base_size;
    double limit_price;
    double stop_price;
    std::optional<StopDirection> stop_direction;
};

inline void from_json(const json &j, StopLimitConfig &c) {
    if (j.contains("base_size")) {
        DOUBLE_FROM_JSON(j, c, base_size);
    }
    DOUBLE_FROM_JSON(j, c, limit_price);
    DOUBLE_FROM_JSON(j, c, stop_price);
    ENUM_FROM_JSON(j, c, stop_direction);
}

struct StopLimitGtdConfig : public StopLimitConfig {
    uint64_t end_time;
};

inline void from_json(const json &j, StopLimitGtdConfig &c) {
    from_json(j, static_cast<StopLimitConfig&>(c));
    TIMESTAMP_FROM_JSON(j, c, end_time);
}

struct TriggerBracketConfig {
    std::optional<double> base_size;
    double limit_price;
    double stop_trigger_price;
};

inline void from_json(const json &j, TriggerBracketConfig &c) {
    if (j.contains("base_size")) {
        DOUBLE_FROM_JSON(j, c, base_size);
    }
    DOUBLE_FROM_JSON(j, c, limit_price);
    DOUBLE_FROM_JSON(j, c, stop_trigger_price);
}

struct TriggerBracketGtdConfig : public TriggerBracketConfig {
    uint64_t end_time;
};

inline void from_json(const json &j, TriggerBracketGtdConfig &c) {
    from_json(j, static_cast<TriggerBracketConfig&>(c));
    TIMESTAMP_FROM_JSON(j, c, end_time);
}

struct ScaledLimitConfig : public MarketConfig{
    std::vector<LimitGtcConfig> orders;
    uint32_t num_orders;
    double min_price;
    double max_price;
    std::string price_distribution;
    std::string size_distribution;
    double size_diff;
    double size_ratio;
};

inline void from_json(const json &j, ScaledLimitConfig &c) {
    c.orders = j["orders"];
    from_json(j, static_cast<MarketConfig&>(c));
    INT_FROM_JSON(j, c, num_orders);
    DOUBLE_FROM_JSON(j, c, min_price);
    DOUBLE_FROM_JSON(j, c, max_price);
    VARIABLE_FROM_JSON(j, c, price_distribution);
    VARIABLE_FROM_JSON(j, c, size_distribution);
    DOUBLE_FROM_JSON(j, c, size_diff);
    DOUBLE_FROM_JSON(j, c, size_ratio);
}

struct OrderConfiguration {
    std::optional<MarketConfig> market_market_ioc;
    std::optional<MarketConfig> market_market_fok;
    std::optional<LimitConfig> sor_limit_ioc;
    std::optional<LimitGtcConfig> limit_limit_gtc;
    std::optional<LimitGtdConfig> limit_limit_gtd;
    std::optional<LimitConfig> limit_limit_fok;
    std::optional<TwapGtdConfig> twap_limit_gtd;
    std::optional<StopLimitConfig> stop_limit_stop_limit_gtc;
    std::optional<StopLimitGtdConfig> stop_limit_stop_limit_gtd;
    std::optional<TriggerBracketConfig> trigger_bracket_gtc;
    std::optional<TriggerBracketGtdConfig> trigger_bracket_gtd;
    std::optional<ScaledLimitConfig> scaled_limit_gtc;
};

inline void from_json(const json &j, OrderConfiguration &c) {
    if (j.contains("market_market_ioc")) {
        c.market_market_ioc = j["market_market_ioc"];
    }

    if (j.contains("market_market_fok")) {
        c.market_market_fok = j["market_market_fok"];
    }

    if (j.contains("sor_limit_ioc")) {
        c.sor_limit_ioc = j["sor_limit_ioc"];
    }

    if (j.contains("limit_limit_gtc")) {
        c.limit_limit_gtc = j["limit_limit_gtc"];
    }
    
    if (j.contains("limit_limit_gtd")) {
        c.limit_limit_gtd = j["limit_limit_gtd"];
    }

    if (j.contains("limit_limit_fok")) {
        c.limit_limit_fok = j["limit_limit_fok"];
    }

    if (j.contains("twap_limit_gtd")) {
        c.twap_limit_gtd = j["twap_limit_gtd"];
    }

    if (j.contains("stop_limit_stop_limit_gtc")) {
        c.stop_limit_stop_limit_gtc = j["stop_limit_stop_limit_gtc"];
    }

    if (j.contains("stop_limit_stop_limit_gtd")) {
        c.stop_limit_stop_limit_gtd = j["stop_limit_stop_limit_gtd"];
    }

    if (j.contains("trigger_bracket_gtc")) {
        c.trigger_bracket_gtc = j["trigger_bracket_gtc"];
    }

    if (j.contains("trigger_bracket_gtd")) {
        c.trigger_bracket_gtd = j["trigger_bracket_gtd"];
    }

    if (j.contains("scaled_limit_gtc")) {
        c.scaled_limit_gtc = j["scaled_limit_gtc"];
    }
}

struct Replace {
    double price;
    double size;
    uint64_t replace_accept_timestamp;
};

inline void from_json(const json &j, Replace &h) {
    DOUBLE_FROM_JSON(j, h, price);
    DOUBLE_FROM_JSON(j, h, size);
    TIMESTAMP_FROM_JSON(j, h, replace_accept_timestamp);
}

enum class OrderStatus : uint8_t {
    PENDING,
    OPEN,
    FILLED,
    CANCELLED,
    EXPIRED,
    FAILED,
    QUEUED,
    CANCEL_QUEUED,
    EDIT_QUEUED,
    UNKNOWN_ORDER_STATUS,
};

inline OrderStatus to_order_status(std::string_view s) {
    if (s == "PENDING") {
        return OrderStatus::PENDING;
    }

    if (s == "OPEN") {
        return OrderStatus::OPEN;
    }

    if (s == "FILLED") {
        return OrderStatus::FILLED;
    }

    if (s == "CANCELLED") {
        return OrderStatus::CANCELLED;
    }

    if (s == "EXPIRED") {
        return OrderStatus::EXPIRED;
    }

    if (s == "FAILED") {
        return OrderStatus::FAILED;
    }

    if (s == "QUEUED") {
        return OrderStatus::QUEUED;
    }

    if (s == "CANCEL_QUEUED") {
        return OrderStatus::CANCEL_QUEUED;
    }

    if (s == "EDIT_QUEUED") {
        return OrderStatus::EDIT_QUEUED;
    }

    return OrderStatus::UNKNOWN_ORDER_STATUS;
}

inline std::string to_string(OrderStatus status) {
    switch(status) {
    case OrderStatus::PENDING:
        return "PENDING";
    case OrderStatus::OPEN:
        return "OPEN";
    case OrderStatus::FILLED:
        return "FILLED";
    case OrderStatus::CANCELLED:
        return "CANCELLED";
    case OrderStatus::EXPIRED:
        return "EXPIRED";
    case OrderStatus::FAILED:
        return "FAILED";
    case OrderStatus::QUEUED:
        return "QUEUED";
    case OrderStatus::CANCEL_QUEUED:
        return "CANCEL_QUEUED";
    case OrderStatus::EDIT_QUEUED:
        return "EDIT_QUEUED";
    case OrderStatus::UNKNOWN_ORDER_STATUS:
        return "UNKNOWN_ORDER_STATUS";
    }
    return "UNKNOWN_ORDER_STATUS";
}

enum class TimeInForce: uint8_t {
    UNKNOWN_TIME_IN_FORCE,
    GOOD_UNTIL_DATE_TIME,
    GOOD_UNTIL_CANCELLED,
    IMMEDIATE_OR_CANCEL,
    FILL_OR_KILL,
};

inline TimeInForce to_time_in_force(std::string_view tif) {
    if (tif == "GOOD_UNTIL_DATE_TIME") {
        return TimeInForce::GOOD_UNTIL_DATE_TIME;
    }

    if (tif == "GOOD_UNTIL_CANCELLED") {
        return TimeInForce::GOOD_UNTIL_CANCELLED;
    }

    if (tif == "IMMEDIATE_OR_CANCEL") {
        return TimeInForce::IMMEDIATE_OR_CANCEL;
    }
 
    if (tif == "FILL_OR_KILL") {
        return TimeInForce::FILL_OR_KILL;
    }

    return TimeInForce::UNKNOWN_TIME_IN_FORCE;
}

inline std::string to_string(TimeInForce tif) {
    switch(tif) {
    case TimeInForce::UNKNOWN_TIME_IN_FORCE:
        return "UNKNOWN_TIME_IN_FORCE";
    case TimeInForce::GOOD_UNTIL_DATE_TIME:
        return "GOOD_UNTIL_DATE_TIME";
    case TimeInForce::GOOD_UNTIL_CANCELLED:
        return "GOOD_UNTIL_CANCELLED";
    case TimeInForce::IMMEDIATE_OR_CANCEL:
        return "IMMEDIATE_OR_CANCEL";
    case TimeInForce::FILL_OR_KILL:
        return "FILL_OR_KILL";
    }
    return "UNKNOWN_TIME_IN_FORCE";
}

enum class OrderPlacementSource : uint8_t {
    UNKNOWN_PLACEMENT_SOURCE,
    RETAIL_SIMPLE,
    RETAIL_ADVANCED,
};

inline OrderPlacementSource to_order_placement_source(std::string_view sv) {
    if (sv == "RETAIL_SIMPLE") {
        return OrderPlacementSource::RETAIL_SIMPLE;
    }

    if (sv == "RETAIL_ADVANCED") {
        return OrderPlacementSource::RETAIL_ADVANCED;
    }

    return OrderPlacementSource::UNKNOWN_PLACEMENT_SOURCE;
}

inline std::string to_string(OrderPlacementSource ps) {
    switch(ps) {
    case OrderPlacementSource::UNKNOWN_PLACEMENT_SOURCE:
        return "UNKNOWN_PLACEMENT_SOURCE";
    case OrderPlacementSource::RETAIL_SIMPLE:
        return "RETAIL_SIMPLE";
    case OrderPlacementSource::RETAIL_ADVANCED:
        return "RETAIL_ADVANCED";
    }
    return "UNKNOWN_PLACEMENT_SOURCE";
}

enum class MarginType : uint8_t {
    CROSS,
    ISOLATED,
    NONE,
};

inline std::string to_string(MarginType mt) {
    switch(mt) {
    case MarginType::CROSS:
        return "CROSS";
    case MarginType::ISOLATED:
        return "ISOLATED";
    case MarginType::NONE:
        return "";
    }
    return "";
}

enum class SorPreference : uint8_t {
    SOR_PREFERENCE_UNSPECIFIED,
    SOR_ENABLED,
    SOR_DISABLED,
};

inline std::string to_string(SorPreference sp) {
    switch(sp) {
    case SorPreference::SOR_PREFERENCE_UNSPECIFIED:
        return "SOR_PREFERENCE_UNSPECIFIED";
    case SorPreference::SOR_ENABLED:
        return "SOR_ENABLED";
    case SorPreference::SOR_DISABLED:
        return "SOR_DISABLED";
    }
    return "";
}

struct PredictionMetadata {
    enum class PredictionSide : uint8_t {
        PREDICTION_SIDE_UNKNOWN,
        PREDICTION_SIDE_YES,
        PREDICTION_SIDE_NO,
    };

    PredictionSide prediction_side;
};

inline std::string to_string(PredictionMetadata::PredictionSide ps) {
    switch(ps) {
    case PredictionMetadata::PredictionSide::PREDICTION_SIDE_UNKNOWN:
        return "PREDICTION_SIDE_UNKNOWN";
    case PredictionMetadata::PredictionSide::PREDICTION_SIDE_YES:
        return "PREDICTION_SIDE_YES";
    case PredictionMetadata::PredictionSide::PREDICTION_SIDE_NO:
        return "PREDICTION_SIDE_NO";
    }
    return "";
}

inline void to_json(json &j, PredictionMetadata &pm) {
    j["prediction_side"] = to_string(pm.prediction_side);
}

struct Order {
    std::string client_order_id;
    std::string order_id;
    std::string product_id;
    std::string user_id;
    std::string trigger_status;
    std::string reject_Reason;
    std::string product_type;
    std::string reject_message;
    std::string cancel_message;
    std::string margin_type;
    std::string retail_portfolio_id;
    std::string originating_order_id;
    std::string attached_order_id;
    OrderConfiguration order_configuration;
    OrderConfiguration attached_order_configuration;
    Replace current_pending_replace;
    uint64_t created_time = 0;
    uint64_t last_fill_time = 0;
    double completion_percentage = 0;
    double fee = 0;
    double avg_price = 0;
    double leaves_quantity = 0;
    double cumulative_quantity = 0;
    double filled_value = 0;
    double total_fees = 0;
    double total_value_after_fees = 0;
    double outstanding_hold_amount = 0;
    double leverage = 0;
    double workable_size = 0;
    double workable_size_completion_pct = 0;
    uint32_t number_of_fills = 0;
    std::vector<Replace> edit_history;
    Side side;
    OrderStatus status = OrderStatus::UNKNOWN_ORDER_STATUS;
    TimeInForce time_in_force = TimeInForce::UNKNOWN_TIME_IN_FORCE;
    OrderType order_type = OrderType::UNKNOWN_ORDER_TYPE;
    OrderPlacementSource order_placement_source = OrderPlacementSource::UNKNOWN_PLACEMENT_SOURCE;
    ContractExpiryType contract_expiry_type = ContractExpiryType::UNKNOWN_CONTRACT_EXPIRY_TYPE;
    bool pending_cancel = false;
    bool size_in_quote = false;
    bool size_inclusive_of_fees = false;
    bool settled = false;
    bool is_liquidation = false;
};

inline void from_json(const json &j, Order &o) {
    try {
        VARIABLE_FROM_JSON(j, o, client_order_id);
        VARIABLE_FROM_JSON(j, o, order_id);
        VARIABLE_FROM_JSON(j, o, product_id);
        VARIABLE_FROM_JSON(j, o, trigger_status);
        VARIABLE_FROM_JSON(j, o, reject_Reason);
        VARIABLE_FROM_JSON(j, o, product_type);
        VARIABLE_FROM_JSON(j, o, reject_message);
        VARIABLE_FROM_JSON(j, o, cancel_message);
        VARIABLE_FROM_JSON(j, o, margin_type);
        VARIABLE_FROM_JSON(j, o, retail_portfolio_id);
        VARIABLE_FROM_JSON(j, o, originating_order_id);
        VARIABLE_FROM_JSON(j, o, attached_order_id);
        if (!j["current_pending_replace"].is_null()) {
            o.current_pending_replace = j["current_pending_replace"];
        }
        TIMESTAMP_FROM_JSON(j, o, created_time);
        TIMESTAMP_FROM_JSON(j, o, last_fill_time);
        DOUBLE_FROM_JSON(j, o, completion_percentage);
        DOUBLE_FROM_JSON(j, o, fee);
        DOUBLE_FROM_JSON(j, o, filled_value);
        DOUBLE_FROM_JSON(j, o, total_fees);
        DOUBLE_FROM_JSON(j, o, total_value_after_fees);
        DOUBLE_FROM_JSON(j, o, outstanding_hold_amount);
        DOUBLE_FROM_JSON(j, o, leverage);
        DOUBLE_FROM_JSON(j, o, workable_size);
        DOUBLE_FROM_JSON(j, o, workable_size_completion_pct);
        INT_FROM_JSON(j, o, number_of_fills);
        o.edit_history = j["edit_history"];
        ENUM_FROM_JSON(j, o, side);
        o.status = to_order_status(j.at("status").get<std::string_view>());
        ENUM_FROM_JSON(j, o, contract_expiry_type);
        ENUM_FROM_JSON(j, o, time_in_force);
        ENUM_FROM_JSON(j, o, order_type);
        ENUM_FROM_JSON(j, o, order_placement_source);
        VARIABLE_FROM_JSON(j, o, pending_cancel);
        VARIABLE_FROM_JSON(j, o, size_in_quote);
        VARIABLE_FROM_JSON(j, o, size_inclusive_of_fees);
        VARIABLE_FROM_JSON(j, o, settled);
        VARIABLE_FROM_JSON(j, o, is_liquidation);
        o.order_configuration = j["order_configuration"];
        if (j.contains("attached_order_configuration")) {
            o.attached_order_configuration = j["attached_order_configuration"];
        }
    }
    catch (const std::exception &e) {
        LOG_INFO(j.dump().c_str());
        LOG_ERROR(e.what());
    }
}

inline void from_snapshot(const json &j, Order &o) {
    VARIABLE_FROM_JSON(j, o, client_order_id);
    VARIABLE_FROM_JSON(j, o, order_id);
    VARIABLE_FROM_JSON(j, o, product_id);
    DOUBLE_FROM_JSON(j, o, avg_price);
    DOUBLE_FROM_JSON(j, o, completion_percentage);
    ENUM_FROM_JSON(j, o, contract_expiry_type);
    DOUBLE_FROM_JSON(j, o, cumulative_quantity);
    DOUBLE_FROM_JSON(j, o, filled_value);
    DOUBLE_FROM_JSON(j, o, leaves_quantity);
    INT_FROM_JSON(j, o, number_of_fills);
    ENUM_FROM_JSON(j, o, order_type);
    ENUM_FROM_JSON(j, o, time_in_force);
    if (o.order_type == OrderType::LIMIT) {
        if (o.time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
            LimitGtcConfig limit_limit_gtc;
            DOUBLE_FROM_JSON(j, limit_limit_gtc, limit_price);
            BOOL_FROM_JSON(j, limit_limit_gtc, post_only);
            o.order_configuration.limit_limit_gtc = limit_limit_gtc;
        }
        else if (o.time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
            LimitGtdConfig limit_limit_gtd;
            DOUBLE_FROM_JSON(j, limit_limit_gtd, limit_price);
            BOOL_FROM_JSON(j, limit_limit_gtd, post_only);
            o.order_configuration.limit_limit_gtd = limit_limit_gtd;
        }
    }
    else if (o.order_type == OrderType::STOP_LIMIT) {
        if (o.time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
            StopLimitConfig stop_limit_stop_limit_gtc;
            DOUBLE_FROM_JSON(j, stop_limit_stop_limit_gtc, stop_price);
            DOUBLE_FROM_JSON(j, stop_limit_stop_limit_gtc, limit_price);
            o.order_configuration.stop_limit_stop_limit_gtc = stop_limit_stop_limit_gtc;
        }
        else if (o.time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
            StopLimitGtdConfig stop_limit_stop_limit_gtd;
            DOUBLE_FROM_JSON(j, stop_limit_stop_limit_gtd, stop_price);
            DOUBLE_FROM_JSON(j, stop_limit_stop_limit_gtd, limit_price);
            o.order_configuration.stop_limit_stop_limit_gtd = stop_limit_stop_limit_gtd;
        }
    }
    o.side = to_side(j["order_side"].get<std::string_view>());
    DOUBLE_FROM_JSON(j, o, outstanding_hold_amount);
    VARIABLE_FROM_JSON(j, o, reject_Reason);
    VARIABLE_FROM_JSON(j, o, retail_portfolio_id);
    o.status = to_order_status(j["status"].get<std::string_view>());
    DOUBLE_FROM_JSON(j, o, total_fees);
    DOUBLE_FROM_JSON(j, o, total_value_after_fees);
    VARIABLE_FROM_JSON(j, o, trigger_status);
    o.created_time = milliseconds_from_json(j, "creation_time");
}

enum class SortBy : uint8_t {
    UNKNOWN_SORT_BY,
    LIMIT_PRICE,
    LAST_FILL_TIME,
};

inline std::string to_string(SortBy sb) {
    switch(sb) {
    case SortBy::UNKNOWN_SORT_BY:
        return "UNKNOWN_SORT_BY";
    case SortBy::LIMIT_PRICE:
        return "LIMIT_PRICE";
    case SortBy::LAST_FILL_TIME:
        return "LAST_FILL_TIME";
    }
    return "UNKNOWN_SORT_BY";
}

struct OrderQueryParams {
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> product_ids;
    std::optional<std::string> product_type;
    std::optional<std::vector<OrderStatus>> order_status;
    std::optional<std::vector<TimeInForce>> time_in_forces;
    std::optional<std::vector<OrderType>> order_types;
    std::optional<Side> order_side;
    std::optional<std::string> start_date;
    std::optional<std::string> end_date;
    std::optional<OrderPlacementSource> order_placement_source;
    std::optional<ContractExpiryType> contract_expiry_type;
    std::optional<std::vector<std::string>> asset_filters;
    std::optional<uint32_t> limit;
    std::optional<std::string> cursor;
    std::optional<SortBy> sort_by;
    std::optional<bool> use_simplified_total_value_calculation;

    std::string operator()() const {
        std::vector<std::string> params;
        if (order_ids.has_value()) {
            for (const auto &oid : order_ids.value()) {
                params.emplace_back(std::format("order_ids={}", oid));
            }
        }

        if (product_ids.has_value()) {
            for (const auto &prod_id : product_ids.value()) {
                params.emplace_back(std::format("product_ids={}", prod_id));
            }
        }

        if (product_type.has_value()) {
            params.emplace_back(std::format("product_type={}", product_type.value()));
        }

        if (order_status.has_value()) {
            for (auto status : order_status.value()) {
                params.emplace_back(std::format("order_status={}", to_string(status)));
            }
        }

        if (time_in_forces.has_value()) {
            for (auto tif : time_in_forces.value()) {
                params.emplace_back(std::format("time_in_forces={}", to_string(tif)));
            }
        }

        if (order_types.has_value()) {
            for (auto ot : order_types.value()) {
                params.emplace_back(std::format("order_types={}", to_string(ot)));
            }
        }

        if (order_side.has_value()) {
            params.emplace_back(std::format("order_side={}", to_string(order_side.value())));
        }

        if (start_date.has_value()) {
            params.emplace_back(std::format("start_time={}", start_date.value()));
        }

        if (end_date.has_value()) {
            params.emplace_back(std::format("end_date={}", end_date.value()));
        }

        if (order_placement_source.has_value()) {
            params.emplace_back(std::format("order_placement_source={}", to_string(order_placement_source.value())));
        }

        if (contract_expiry_type.has_value()) {
            params.emplace_back(std::format("contract_expiry_type={}", to_string(contract_expiry_type.value())));
        }

        if (asset_filters.has_value()) {
            for (auto &filter : asset_filters.value()) {
                params.emplace_back(std::format("asset_filters={}", filter));
            }
        }

        if (limit.has_value()) {
            params.emplace_back(std::format("limit={}", limit.value()));
        }

        if (cursor.has_value()) {
            params.emplace_back(std::format("cursor={}", cursor.value()));
        }

        if (sort_by.has_value()) {
            params.emplace_back(std::format("sort_by={}", to_string(sort_by.value())));
        }

        if (use_simplified_total_value_calculation.has_value()) {
            params.emplace_back(std::format("use_simplified_total_value_calculation={}", use_simplified_total_value_calculation.value() ? "true" : "false"));
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

struct SuccessResponse {
    std::string order_id;
    std::string product_id;
    std::string client_order_id;
    Side side;
};

inline void from_json(const json &j, SuccessResponse &rsp) {
    VARIABLE_FROM_JSON(j, rsp, order_id);
    VARIABLE_FROM_JSON(j, rsp, product_id);
    VARIABLE_FROM_JSON(j, rsp, client_order_id);
    ENUM_FROM_JSON(j, rsp, side);
}

struct ErrorResponse {
    std::string message;
    std::string error_details;
    std::string new_order_failure_reason;
};

inline void from_json(const json &j, ErrorResponse &rsp) {
    VARIABLE_FROM_JSON(j, rsp, message);
    VARIABLE_FROM_JSON(j, rsp, error_details);
    if (j.contains("new_order_failure_reason")) {
        VARIABLE_FROM_JSON(j, rsp, new_order_failure_reason);
    }
}

struct CreateOrderResponse {
    bool success;
    SuccessResponse success_response;
    ErrorResponse error_response;
    OrderConfiguration order_configuration;
};

inline void from_json(const json &j, CreateOrderResponse &r) {
    VARIABLE_FROM_JSON(j, r, success);
    if (j.contains("success_response")) {
        STRUCT_FROM_JSON(j, r, success_response);
    }

    if (j.contains("error_response")) {
        STRUCT_FROM_JSON(j, r, error_response);
    }

    if (j.contains("order_configuration")) {
        STRUCT_FROM_JSON(j, r, order_configuration);
    }
}

struct CancelOrderResponse {
    bool success;
    std::string failure_reason;
    std::string order_id;
};

inline void from_json(const json &j, CancelOrderResponse &r) {
    VARIABLE_FROM_JSON(j, r, success);
    VARIABLE_FROM_JSON(j, r, failure_reason);
    VARIABLE_FROM_JSON(j, r, order_id);
}

struct ModifyOrderParams {
    std::string order_id;
    double price;
    double size;
    std::optional<OrderConfiguration> attached_order_configuration;
    std::optional<bool> cancel_attached_order;
    std::optional<double> stop_price;
};

struct ModifyOrderResponse {
    bool success;
    std::vector<json> errors;
};

inline void from_json(const json &j, ModifyOrderResponse &r) {
    VARIABLE_FROM_JSON(j, r, success);
    STRUCT_FROM_JSON(j, r, errors);
}

}