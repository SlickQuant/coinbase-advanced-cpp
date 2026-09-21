// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

// Internal - the catalogue of Coinbase Advanced Trade endpoints. Each entry builds a request and
// says how to read the response; it performs no I/O, so CoinbaseRestClient and
// CoinbaseAwaitableRestClient share every definition here and differ only in how they send it.

#include "rest_detail.hpp"

namespace coinbase::detail {

// ---------------------------------------------------------------------------
// Server time
// ---------------------------------------------------------------------------

inline endpoint<uint64_t> get_server_time(std::string_view base_url) {
    return {
        public_request(http_method::get, base_url, "/api/v3/brokerage/time"),
        "get_server_time",
        [](const json &j) -> uint64_t { return std::stoull(j["epochMillis"].get<std::string_view>().data()); }
    };
}

// ---------------------------------------------------------------------------
// Accounts
// ---------------------------------------------------------------------------

inline paged_endpoint<Account, AccountQueryParams> list_accounts(std::string_view base_url, std::string_view domain,
                                                                 const AccountQueryParams &params) {
    return { std::string(base_url), std::string(domain), "/api/v3/brokerage/accounts", "accounts", "list_accounts", true, params };
}

inline endpoint<Account> get_account(std::string_view base_url, std::string_view domain, std::string_view account_uuid) {
    return {
        signed_request(http_method::get, base_url, domain, std::format("/api/v3/brokerage/accounts/{}", account_uuid)),
        "get_account",
        [](const json &j) { return j["account"].get<Account>(); }
    };
}

// ---------------------------------------------------------------------------
// Products
// ---------------------------------------------------------------------------

inline endpoint<std::vector<Product>> list_products(std::string_view base_url, std::string_view domain,
                                                    const ProductQueryParams &params) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/products", params()),
        "list_products",
        [](const json &j) { return j["products"].get<std::vector<Product>>(); }
    };
}

inline endpoint<Product> get_product(std::string_view base_url, std::string_view domain,
                                     std::string_view product_id, bool get_tradability_status) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/products/{}", product_id),
                       get_tradability_status ? "?get_tradability_status=true" : ""),
        "get_product",
        [](const json &j) { return j.get<Product>(); }
    };
}

inline endpoint<std::vector<Product>> list_public_products(std::string_view base_url, const ProductQueryParams &params) {
    return {
        public_request(http_method::get, base_url, "/api/v3/brokerage/market/products", params()),
        "list_public_products",
        [](const json &j) { return j["products"].get<std::vector<Product>>(); }
    };
}

inline endpoint<Product> get_public_product(std::string_view base_url, std::string_view product_id) {
    return {
        public_request(http_method::get, base_url, std::format("/api/v3/brokerage/market/products/{}", product_id)),
        "get_public_product",
        [](const json &j) { return j.get<Product>(); }
    };
}

// ---------------------------------------------------------------------------
// Order queries
// ---------------------------------------------------------------------------

inline paged_endpoint<Order, OrderQueryParams> list_orders(std::string_view base_url, std::string_view domain,
                                                           const OrderQueryParams &query) {
    return { std::string(base_url), std::string(domain), "/api/v3/brokerage/orders/historical/batch", "orders", "list_orders", true, query };
}

inline endpoint<Order> get_order(std::string_view base_url, std::string_view domain, std::string_view order_id) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/orders/historical/{}", order_id)),
        "get_order",
        [](const json &j) { LOG_TRACE(j.dump().c_str()); return j["order"].get<Order>(); }
    };
}

inline paged_endpoint<Fill, FillQueryParams> list_fills(std::string_view base_url, std::string_view domain,
                                                        const FillQueryParams &params) {
    // The fills endpoint hands back a cursor without a has_next flag.
    return { std::string(base_url), std::string(domain), "/api/v3/brokerage/orders/historical/fills", "fills", "list_fills", false, params };
}

// ---------------------------------------------------------------------------
// Fee rates
// ---------------------------------------------------------------------------

inline endpoint<double> get_taker_fee_rate(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/transaction_summary"),
        "get_taker_fee_rate",
        [](const json &j) { return atof(j["fee_tier"]["taker_fee_rate"].get<std::string>().c_str()); },
        0.0012
    };
}

inline endpoint<double> get_maker_fee_rate(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/transaction_summary"),
        "get_maker_fee_rate",
        [](const json &j) { return atof(j["fee_tier"]["maker_fee_rate"].get<std::string>().c_str()); },
        0.006
    };
}

// ---------------------------------------------------------------------------
// Market data
// ---------------------------------------------------------------------------

inline endpoint<std::vector<PriceBook>> get_best_bid_ask(std::string_view base_url, std::string_view domain,
                                                         const std::vector<std::string> &product_ids) {
    if (product_ids.empty()) {
        LOG_WARN("get_best_bid_ask empty product_ids provided");
        return {};
    }

    std::vector<std::string> params(product_ids.size());
    std::transform(product_ids.begin(), product_ids.end(), params.begin(),
        [](const std::string &i) { return std::format("product_ids={}", i); });

    auto query = std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
        [](const std::string &a, const std::string &b) {
            return a + "&" + b;
        }));

    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/best_bid_ask", query),
        "get_best_bid_ask",
        [](const json &j) { LOG_TRACE(j.dump().c_str()); return j["pricebooks"].get<std::vector<PriceBook>>(); }
    };
}

inline endpoint<PriceBookResponse> get_product_book(std::string_view base_url, std::string_view domain,
                                                    const PriceBookQueryParams &params) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/product_book", params()),
        "get_product_book",
        [](const json &j) { return j.get<PriceBookResponse>(); }
    };
}

inline endpoint<MarketTrades> get_market_trades(std::string_view base_url, std::string_view domain,
                                                std::string_view product_id, const MarketTradesQueryParams &params) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/products/{}/ticker", product_id), params()),
        "get_market_trades",
        [](const json &j) { return j.get<MarketTrades>(); }
    };
}

inline endpoint<std::vector<Candle>> get_product_candles(std::string_view base_url, std::string_view domain,
                                                         std::string_view product_id,
                                                         const ProductCandlesQueryParams &params) {
    auto query = params();
    LOG_TRACE(query.c_str());
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/products/{}/candles", product_id), query),
        "get_product_candles",
        [](const json &j) { return j["candles"].get<std::vector<Candle>>(); }
    };
}

// ---------------------------------------------------------------------------
// Create order
// ---------------------------------------------------------------------------

// Builds the create-order request body. Returns an empty optional and fills `error` when the order
// is invalid, so it is rejected locally instead of costing a round trip.
inline std::optional<json> make_create_order_body(
    const std::string &client_order_id,
    const std::string &product_id,
    Side side,
    OrderType order_type,
    TimeInForce time_in_force,
    double size,
    double limit_price,
    bool post_only,
    bool size_in_quote,
    std::optional<double> stop_price,
    std::optional<double> take_profit_price,
    std::optional<uint64_t> end_time,
    std::optional<uint64_t> twap_start_time,
    const std::optional<SorPreference> &sor_preference,
    const std::optional<double> &leverage,
    const std::optional<MarginType> &margin_type,
    const std::optional<json> &attached_order_configuration,
    std::optional<PredictionMetadata> &prediction_metadata,   // to_json() takes it by mutable reference
    std::string &error)
{
    auto reject = [&error](std::string message) -> std::optional<json> {
        error = std::move(message);
        return std::nullopt;
    };
    auto size_field = [size_in_quote](json &config, double value) {
        if (size_in_quote) {
            config["quote_size"] = std::to_string(value);
        }
        else {
            config["base_size"] = std::to_string(value);
        }
    };

    json body {
        {"client_order_id", client_order_id},
        {"product_id", product_id},
        {"side", to_string(side)},
        {"order_configuration", {}}
    };
    auto &order_configuration = body["order_configuration"];

    switch (order_type) {
        case OrderType::MARKET: {
            if (!std::isnan(limit_price)) {
                LOG_WARN("limit price ignored. Limit price should not be set for market order");
            }
            if (time_in_force == TimeInForce::FILL_OR_KILL) {
                size_field(order_configuration["market_market_fok"], size);
            }
            else if (time_in_force == TimeInForce::IMMEDIATE_OR_CANCEL) {
                size_field(order_configuration["market_market_ioc"], size);
            }
            else {
                return reject(std::format("TimeInForce {} invalid for market order", to_string(time_in_force)));
            }

            if (stop_price.has_value() && take_profit_price.has_value()) {
                auto &prod = CoinbaseRestClient::product(product_id);
                if (prod.product_type == ProductType::SPOT && side == Side::SELL) {
                    return reject("Invalid order side for attached TP/SL");
                }
                body["attached_order_configuration"] = {
                    {"trigger_bracket_gtc", {
                        {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                        {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                    }}
                };
            }
            else if (stop_price.has_value()) {
                return reject("braket order must have both stop_price and take_profit_price");
            }
            break;
        }
        case OrderType::LIMIT: {
            if (std::isnan(limit_price)) {
                return reject("Invalid limit price NAN");
            }
            const auto quote_increment = CoinbaseRestClient::product(product_id).quote_increment;
            if (time_in_force == TimeInForce::FILL_OR_KILL) {
                auto &config = order_configuration["limit_limit_fok"];
                size_field(config, size);
                config["limit_price"] = to_string(limit_price, quote_increment);
            }
            else if (time_in_force == TimeInForce::IMMEDIATE_OR_CANCEL) {
                auto &config = order_configuration["sor_limit_ioc"];
                size_field(config, size);
                config["limit_price"] = to_string(limit_price, quote_increment);
            }
            else if (time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
                auto &config = order_configuration["limit_limit_gtc"];
                size_field(config, size);
                config["limit_price"] = to_string(limit_price, quote_increment);
                config["post_only"] = post_only;
            }
            else if (time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
                if (!end_time.has_value()) {
                    return reject("end_time missing for limit_gtd order");
                }
                auto &config = order_configuration["limit_limit_gtd"];
                size_field(config, size);
                config["limit_price"] = to_string(limit_price, quote_increment);
                config["post_only"] = post_only;
                config["end_time"] = timestamp_to_string(end_time.value());
            }
            else {
                return reject(std::format("TimeInForce {} invalid for market order", to_string(time_in_force)));
            }

            if (stop_price.has_value() && take_profit_price.has_value()) {
                auto &prod = CoinbaseRestClient::product(product_id);
                if (prod.product_type == ProductType::SPOT && side == Side::SELL) {
                    return reject("Invalid order side for attached TP/SL");
                }
                body["attached_order_configuration"] = {
                    {"trigger_bracket_gtc", {
                        {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                        {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                    }}
                };
            }
            else if (stop_price.has_value() ^ take_profit_price.has_value()) {
                return reject("braket order must have both stop_price and take_profit_price");
            }
            break;
        }
        case OrderType::STOP_LIMIT: {
            if (size_in_quote) {
                return reject("Invalid parameter. stop limit order size only in base_size");
            }
            if (!stop_price.has_value() || std::isnan(stop_price.value())) {
                return reject(std::format("Invalid stop_price {}", stop_price.value_or(NAN)));
            }
            const auto quote_increment = CoinbaseRestClient::product(product_id).quote_increment;
            if (time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
                auto &config = order_configuration["stop_limit_stop_limit_gtc"];
                config["base_size"] = std::to_string(size);
                config["limit_price"] = to_string(limit_price, quote_increment);
                config["stop_price"] = to_string(stop_price.value(), quote_increment);
            }
            else if (time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
                if (!end_time.has_value()) {
                    return reject("end_time missing for limit_gtd order");
                }
                auto &config = order_configuration["stop_limit_stop_limit_gtd"];
                config["base_size"] = std::to_string(size);
                config["limit_price"] = to_string(limit_price, quote_increment);
                config["end_time"] = timestamp_to_string(end_time.value());
            }
            else {
                return reject(std::format("TimeInForce {} invalid for market order", to_string(time_in_force)));
            }
            break;
        }
        case OrderType::TWAP: {
            if (!twap_start_time.has_value() || !end_time.has_value()) {
                return reject("twap order must have start and end time");
            }

            auto &config = order_configuration["twap_limit_gtd"];
            size_field(config, size);
            config["limit_price"] = to_string(limit_price, CoinbaseRestClient::product(product_id).quote_increment);
            config["start_time"] = timestamp_to_string(twap_start_time.value());
            config["end_time"] = timestamp_to_string(end_time.value());
            break;
        }
        case OrderType::BRACKET: {
            auto &prod = CoinbaseRestClient::product(product_id);
            if (prod.product_type == ProductType::SPOT && side == Side::BUY) {
                return reject("Invalid order side for Bracket order");
            }
            if (size_in_quote) {
                return reject("Invalid parameter. Bracket order size only in base_size");
            }

            if (stop_price.has_value() && take_profit_price.has_value()) {
                order_configuration["trigger_bracket_gtc"] = {
                    {"base_size", std::to_string(size)},
                    {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                    {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                };
            }
            else if (stop_price.has_value() && !std::isnan(limit_price)) {
                // use limit_price as take_profit_price for stop loss only bracket order
                order_configuration["trigger_bracket_gtc"] = {
                    {"base_size", std::to_string(size)},
                    {"limit_price", to_string(limit_price, prod.quote_increment)},
                    {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                };
            }
            else {
                return reject("braket order must have both stop_price and take_profit_price");
            }
            break;
        }
        default: {
            return reject(std::format("OrderType {} is not supported. client_order_id: {}",
                                      to_string(order_type), client_order_id));
        }
    }

    if (leverage.has_value()) {
        body["leverage"] = std::to_string(leverage.value());
    }
    if (margin_type.has_value()) {
        body["margin_type"] = to_string(margin_type.value());
    }
    if (attached_order_configuration.has_value()) {
        body["attached_order_configuration"] = attached_order_configuration.value();
    }
    body["sor_preference"] = to_string(sor_preference.value_or(SorPreference::SOR_ENABLED));
    if (prediction_metadata.has_value()) {
        to_json(body["prediction_metadata"], prediction_metadata.value());
    }
    return body;
}

// A create-order request, or - when req.url is empty - the rejection to hand straight back. It owns
// the client_order_id the response is reported against, so the drivers below still have it once the
// caller's arguments are gone.
struct create_order_request {
    request req;
    std::string client_order_id;
    CreateOrderResponse rejected;
};

inline create_order_request make_create_order(
    std::string_view base_url,
    std::string_view domain,
    std::string &&client_order_id,
    std::string &&product_id,
    Side side,
    OrderType order_type,
    TimeInForce time_in_force,
    double size,
    double limit_price,
    bool post_only,
    bool size_in_quote,
    std::optional<double> stop_price,
    std::optional<double> take_profit_price,
    std::optional<uint64_t> end_time,
    std::optional<uint64_t> twap_start_time,
    std::optional<SorPreference> &&sor_preference,
    std::optional<double> &&leverage,
    std::optional<MarginType> &&margin_type,
    std::optional<json> &&attached_order_configuration,
    std::optional<PredictionMetadata> &&prediction_metadata)
{
    create_order_request out;
    try {
        std::string error;
        auto body = make_create_order_body(client_order_id, product_id, side, order_type, time_in_force,
                                           size, limit_price, post_only, size_in_quote, stop_price,
                                           take_profit_price, end_time, twap_start_time, sor_preference,
                                           leverage, margin_type, attached_order_configuration,
                                           prediction_metadata, error);
        if (!body.has_value()) {
            LOG_ERROR(error.c_str());
            out.rejected.success = false;
            out.rejected.error_response.message = std::move(error);
            out.client_order_id = std::move(client_order_id);
            return out;
        }

        LOG_TRACE("create order: {}", body->dump());
        out.req = signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/orders", {}, body->dump());
    }
    catch (const std::exception &e) {
        out.req = {};
        out.rejected.success = false;
        out.rejected.error_response.message =
            std::format("Failed to create order. client_order_id: {}  error: {}", client_order_id, e.what());
        LOG_ERROR(out.rejected.error_response.message.c_str());
    }
    out.client_order_id = std::move(client_order_id);
    return out;
}

inline CreateOrderResponse parse_create_order(const Http::Response &res, std::string_view client_order_id) {
    CreateOrderResponse rsp;
    rsp.success = res.is_ok();
    if (!res.result_text.empty()) {
        auto j = json::parse(res.result_text);
        LOG_TRACE(j.dump().c_str());
        return j;
    }
    rsp.error_response.message = std::format("Failed to create order. client_order_id: {} error: {}",
                                             client_order_id, res.result_text);
    LOG_ERROR(rsp.error_response.message.c_str());
    rsp.success = false;
    return rsp;
}

inline CreateOrderResponse create_order_failure(std::string_view client_order_id, std::string_view what) {
    CreateOrderResponse rsp;
    rsp.error_response.message = std::format("Failed to create order. client_order_id: {}  error: {}",
                                             client_order_id, what);
    LOG_ERROR(rsp.error_response.message.c_str());
    rsp.success = false;
    return rsp;
}

inline CreateOrderResponse run(create_order_request order) {
    if (order.req.url.empty()) {
        return std::move(order.rejected);
    }
    try {
        auto res = send(order.req);
        return parse_create_order(res, order.client_order_id);
    }
    catch (const std::exception &e) {
        return create_order_failure(order.client_order_id, e.what());
    }
}

inline boost::asio::awaitable<CreateOrderResponse> run_async(create_order_request order) {
    if (order.req.url.empty()) {
        co_return std::move(order.rejected);
    }
    try {
        auto res = co_await send_async(order.req);
        co_return parse_create_order(res, order.client_order_id);
    }
    catch (const std::exception &e) {
        co_return create_order_failure(order.client_order_id, e.what());
    }
}

// ---------------------------------------------------------------------------
// Modify order
// ---------------------------------------------------------------------------

// A modify-order request paired with the order id the response is reported against, owned so the
// drivers below still have it once the caller's arguments are gone. An empty req.url marks a
// request the builder could not make - an unknown product, or a signature that failed.
struct modify_order_request {
    request req;
    std::string order_id;
};

inline modify_order_request make_modify_order(
    std::string_view base_url,
    std::string_view domain,
    std::string order_id,
    std::string_view product_id,
    double price,
    double size,
    std::optional<double> stop_price,
    std::optional<double> take_profit_price,
    std::optional<bool> cancel_attached_order)
{
    modify_order_request out;
    out.order_id = std::move(order_id);
    try {
        auto &prod = CoinbaseRestClient::product(product_id);
        json body {
            {"order_id", out.order_id},
            {"size", std::to_string(size)},
        };
        body["price"] = to_string(price, prod.quote_increment);
        if (stop_price.has_value() && take_profit_price.has_value()) {
            body["attached_order_configuration"] = {
                {"trigger_bracket_gtc", {
                    {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                    {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                }}
            };
        }
        else if (stop_price.has_value()) {
            body["stop_price"] = to_string(stop_price.value(), prod.quote_increment);
        }
        if (cancel_attached_order.has_value()) {
            body["cancel_attached_order"] = cancel_attached_order.value();
        }

        LOG_TRACE("modify order: {}", body.dump());
        out.req = signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/orders/edit", {}, body.dump());
    }
    catch (const std::exception &e) {
        LOG_ERROR("modify_order failed. order_id: {}, error: {}", out.order_id, e.what());
    }
    return out;
}

inline ModifyOrderResponse parse_modify_order(const Http::Response &res, std::string_view order_id) {
    ModifyOrderResponse rsp;
    rsp.success = res.is_ok();
    if (!res.result_text.empty()) {
        auto j = json::parse(res.result_text);
        LOG_TRACE(j.dump().c_str());
        return j;
    }
    LOG_ERROR("modify_order failed. order_id: {}, error: {}", order_id, res.reason);
    rsp.success = false;
    return rsp;
}

inline ModifyOrderResponse modify_order_failure() {
    ModifyOrderResponse rsp;
    rsp.success = false;
    return rsp;
}

inline ModifyOrderResponse run(modify_order_request op) {
    if (!op.req.url.empty()) {
        try {
            auto res = send(op.req);
            return parse_modify_order(res, op.order_id);
        }
        catch (const std::exception &e) {
            LOG_ERROR("modify_order failed. order_id: {}, error: {}", op.order_id, e.what());
        }
    }
    return modify_order_failure();
}

inline boost::asio::awaitable<ModifyOrderResponse> run_async(modify_order_request op) {
    if (!op.req.url.empty()) {
        try {
            auto res = co_await send_async(op.req);
            co_return parse_modify_order(res, op.order_id);
        }
        catch (const std::exception &e) {
            LOG_ERROR("modify_order failed. order_id: {}, error: {}", op.order_id, e.what());
        }
    }
    co_return modify_order_failure();
}

// ---------------------------------------------------------------------------
// Cancel orders
// ---------------------------------------------------------------------------

// A cancel-orders request together with the ids it covers. Both the failure fallback and the
// parsed response are reported per id, long after the request went out, so the ids are copied out
// of the caller's vector of views rather than borrowed from it.
struct cancel_orders_request {
    request req;
    std::vector<std::string> order_ids;
};

// `Ids` is any range of string-like ids - the caller's std::string_view vector, or the owned copy
// carried by cancel_orders_request.
template <typename Ids>
std::vector<CancelOrderResponse> cancel_orders_failure(const Ids &order_ids) {
    std::vector<CancelOrderResponse> rt;
    rt.reserve(order_ids.size());
    for (const auto &oid : order_ids) {
        CancelOrderResponse rsp;
        rsp.success = false;
        rsp.failure_reason = "INVALID_CANCEL_REQUEST";
        rsp.order_id = oid;
        rt.emplace_back(std::move(rsp));
    }
    return rt;
}

template <typename Ids>
std::vector<CancelOrderResponse> parse_cancel_orders(const Http::Response &res, const Ids &order_ids) {
    if (!res.result_text.empty()) {
        auto j = json::parse(res.result_text);
        LOG_TRACE(j.dump().c_str());
        return j["results"].get<std::vector<CancelOrderResponse>>();
    }
    LOG_ERROR("cancel_orders failed. error: {}", res.result_text);
    return cancel_orders_failure(order_ids);
}

inline cancel_orders_request make_cancel_orders(std::string_view base_url, std::string_view domain,
                                                const std::vector<std::string_view> &order_ids) {
    cancel_orders_request out;
    out.order_ids.reserve(order_ids.size());
    for (auto oid : order_ids) {
        out.order_ids.emplace_back(oid);
    }
    try {
        json body {
            {"order_ids", out.order_ids},
        };
        LOG_TRACE("cancel order: {}", body.dump());
        out.req = signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/orders/batch_cancel", {}, body.dump());
    }
    catch (const std::exception &e) {
        LOG_ERROR("cancel_orders failed. error: {}", e.what());
    }
    return out;
}

inline std::vector<CancelOrderResponse> run(cancel_orders_request op) {
    if (!op.req.url.empty()) {
        try {
            auto res = send(op.req);
            return parse_cancel_orders(res, op.order_ids);
        }
        catch (const std::exception &e) {
            LOG_ERROR("cancel_orders failed. error: {}", e.what());
        }
    }
    return cancel_orders_failure(op.order_ids);
}

inline boost::asio::awaitable<std::vector<CancelOrderResponse>> run_async(cancel_orders_request op) {
    if (!op.req.url.empty()) {
        try {
            auto res = co_await send_async(op.req);
            co_return parse_cancel_orders(res, op.order_ids);
        }
        catch (const std::exception &e) {
            LOG_ERROR("cancel_orders failed. error: {}", e.what());
        }
    }
    co_return cancel_orders_failure(op.order_ids);
}

// ---------------------------------------------------------------------------
// Portfolios
// ---------------------------------------------------------------------------

inline endpoint<std::vector<Portfolio>> list_portfolios(std::string_view base_url, std::string_view domain,
                                                        std::optional<PortfolioType> portfolio_type) {
    std::string query;
    if (portfolio_type.has_value()) {
        query = std::format("?portfolio_type={}", to_string(portfolio_type.value()));
    }
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/portfolios", query),
        "list_portfolios",
        [](const json &j) { return j["portfolios"].get<std::vector<Portfolio>>(); }
    };
}

inline endpoint<Portfolio> create_portfolio(std::string_view base_url, std::string_view domain, std::string_view name) {
    json body {
        {"name", name},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/portfolios", {}, body.dump()),
        "create_portfolio",
        [](const json &j) { return j["portfolio"].get<Portfolio>(); }
    };
}

inline endpoint<PortfolioBreakdown> get_portfolio_breakdown(std::string_view base_url, std::string_view domain,
                                                            std::string_view portfolio_uuid,
                                                            std::optional<std::string_view> currency) {
    std::string query;
    if (currency.has_value()) {
        query = std::format("?currency={}", currency.value());
    }
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/portfolios/{}", portfolio_uuid), query),
        "get_portfolio_breakdown",
        [](const json &j) { return j["breakdown"].get<PortfolioBreakdown>(); }
    };
}

inline endpoint<MovePortfolioFundsResult> move_portfolio_funds(std::string_view base_url, std::string_view domain,
                                                               double value, std::string_view currency,
                                                               std::string_view source_portfolio_uuid,
                                                               std::string_view target_portfolio_uuid) {
    json body {
        {"funds", {
            {"value", std::to_string(value)},
            {"currency", currency},
        }},
        {"source_portfolio_uuid", source_portfolio_uuid},
        {"target_portfolio_uuid", target_portfolio_uuid},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/portfolios/move_funds", {}, body.dump()),
        "move_portfolio_funds",
        [](const json &j) { return j.get<MovePortfolioFundsResult>(); }
    };
}

inline endpoint<Portfolio> edit_portfolio(std::string_view base_url, std::string_view domain,
                                          std::string_view portfolio_uuid, std::string_view name) {
    json body {
        {"name", name},
    };
    return {
        signed_request(http_method::put, base_url, domain,
                       std::format("/api/v3/brokerage/portfolios/{}", portfolio_uuid), {}, body.dump()),
        "edit_portfolio",
        [](const json &j) { return j["portfolio"].get<Portfolio>(); }
    };
}

inline status_endpoint delete_portfolio(std::string_view base_url, std::string_view domain,
                                        std::string_view portfolio_uuid) {
    return {
        signed_request(http_method::del, base_url, domain,
                       std::format("/api/v3/brokerage/portfolios/{}", portfolio_uuid)),
        "delete_portfolio"
    };
}

// ---------------------------------------------------------------------------
// Convert
// ---------------------------------------------------------------------------

inline endpoint<ConvertTrade> create_convert_quote(std::string_view base_url, std::string_view domain,
                                                   std::string_view from_account, std::string_view to_account,
                                                   double amount) {
    json body {
        {"from_account", from_account},
        {"to_account", to_account},
        {"amount", std::to_string(amount)},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/convert/quote", {}, body.dump()),
        "create_convert_quote",
        [](const json &j) { return j["trade"].get<ConvertTrade>(); }
    };
}

inline endpoint<ConvertTrade> get_convert_trade(std::string_view base_url, std::string_view domain,
                                                std::string_view trade_id, std::string_view from_account,
                                                std::string_view to_account) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/convert/trade/{}", trade_id),
                       std::format("?from_account={}&to_account={}", from_account, to_account)),
        "get_convert_trade",
        [](const json &j) { return j["trade"].get<ConvertTrade>(); }
    };
}

inline endpoint<ConvertTrade> commit_convert_trade(std::string_view base_url, std::string_view domain,
                                                   std::string_view trade_id, std::string_view from_account,
                                                   std::string_view to_account) {
    json body {
        {"from_account", from_account},
        {"to_account", to_account},
    };
    return {
        signed_request(http_method::post, base_url, domain,
                       std::format("/api/v3/brokerage/convert/trade/{}", trade_id), {}, body.dump()),
        "commit_convert_trade",
        [](const json &j) { return j["trade"].get<ConvertTrade>(); }
    };
}

// ---------------------------------------------------------------------------
// Payment methods
// ---------------------------------------------------------------------------

inline endpoint<std::vector<PaymentMethod>> list_payment_methods(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/payment_methods"),
        "list_payment_methods",
        [](const json &j) { return j["payment_methods"].get<std::vector<PaymentMethod>>(); }
    };
}

inline endpoint<PaymentMethod> get_payment_method(std::string_view base_url, std::string_view domain,
                                                  std::string_view payment_method_id) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/payment_methods/{}", payment_method_id)),
        "get_payment_method",
        [](const json &j) { return j["payment_method"].get<PaymentMethod>(); }
    };
}

// ---------------------------------------------------------------------------
// Data API
// ---------------------------------------------------------------------------

inline endpoint<ApiKeyPermissions> get_api_key_permissions(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/key_permissions"),
        "get_api_key_permissions",
        [](const json &j) { return j.get<ApiKeyPermissions>(); }
    };
}

// ---------------------------------------------------------------------------
// Futures (CFM)
// ---------------------------------------------------------------------------

inline endpoint<FCMBalanceSummary> get_futures_balance_summary(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/cfm/balance_summary"),
        "get_futures_balance_summary",
        [](const json &j) { return j["balance_summary"].get<FCMBalanceSummary>(); }
    };
}

inline endpoint<std::vector<FCMPosition>> list_futures_positions(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/cfm/positions"),
        "list_futures_positions",
        [](const json &j) { return j["positions"].get<std::vector<FCMPosition>>(); }
    };
}

inline endpoint<FCMPosition> get_futures_position(std::string_view base_url, std::string_view domain,
                                                  std::string_view product_id) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/cfm/positions/{}", product_id)),
        "get_futures_position",
        [](const json &j) { return j["position"].get<FCMPosition>(); }
    };
}

inline endpoint<bool> schedule_futures_sweep(std::string_view base_url, std::string_view domain, double usd_amount) {
    json body {
        {"usd_amount", std::to_string(usd_amount)},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/cfm/sweeps/schedule", {}, body.dump()),
        "schedule_futures_sweep",
        [](const json &j) { return j.value("success", false); }
    };
}

inline endpoint<std::vector<FCMSweep>> list_futures_sweeps(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/cfm/sweeps"),
        "list_futures_sweeps",
        [](const json &j) { return j["sweeps"].get<std::vector<FCMSweep>>(); }
    };
}

inline endpoint<bool> cancel_pending_futures_sweep(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::del, base_url, domain, "/api/v3/brokerage/cfm/sweeps"),
        "cancel_pending_futures_sweep",
        [](const json &j) { return j.value("success", false); }
    };
}

inline endpoint<std::string> get_intraday_margin_setting(std::string_view base_url, std::string_view domain) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/cfm/intraday/margin_setting"),
        "get_intraday_margin_setting",
        [](const json &j) { return j.value("setting", std::string{}); }
    };
}

inline endpoint<CurrentMarginWindow> get_current_margin_window(std::string_view base_url, std::string_view domain,
                                                               std::string_view margin_profile_type) {
    return {
        signed_request(http_method::get, base_url, domain, "/api/v3/brokerage/cfm/intraday/current_margin_window",
                       std::format("?margin_profile_type={}", margin_profile_type)),
        "get_current_margin_window",
        [](const json &j) { return j.get<CurrentMarginWindow>(); }
    };
}

inline endpoint<bool> set_intraday_margin_setting(std::string_view base_url, std::string_view domain,
                                                  std::string_view setting) {
    json body {
        {"setting", setting},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/cfm/intraday/margin_setting",
                       {}, body.dump()),
        "set_intraday_margin_setting",
        [](const json &j) { return j.value("success", false); }
    };
}

// ---------------------------------------------------------------------------
// Perpetuals (INTX)
// ---------------------------------------------------------------------------

inline status_endpoint allocate_portfolio(std::string_view base_url, std::string_view domain,
                                          std::string_view portfolio_uuid, std::string_view symbol,
                                          double amount, std::string_view currency) {
    json body {
        {"portfolio_uuid", portfolio_uuid},
        {"symbol", symbol},
        {"amount", std::to_string(amount)},
        {"currency", currency},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/intx/allocate", {}, body.dump()),
        "allocate_portfolio"
    };
}

inline endpoint<PerpsPortfolioSummaryResponse> get_perps_portfolio_summary(std::string_view base_url,
                                                                           std::string_view domain,
                                                                           std::string_view portfolio_uuid) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/intx/portfolio/{}", portfolio_uuid)),
        "get_perps_portfolio_summary",
        [](const json &j) { return j.get<PerpsPortfolioSummaryResponse>(); }
    };
}

inline endpoint<PerpsPositionsResponse> list_perps_positions(std::string_view base_url, std::string_view domain,
                                                             std::string_view portfolio_uuid) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/intx/positions/{}", portfolio_uuid)),
        "list_perps_positions",
        [](const json &j) { return j.get<PerpsPositionsResponse>(); }
    };
}

inline endpoint<PerpsPosition> get_perps_position(std::string_view base_url, std::string_view domain,
                                                  std::string_view portfolio_uuid, std::string_view symbol) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/intx/positions/{}/{}", portfolio_uuid, symbol)),
        "get_perps_position",
        [](const json &j) { return j["position"].get<PerpsPosition>(); }
    };
}

inline endpoint<std::vector<PerpsPortfolioBalance>> get_perps_portfolio_balances(std::string_view base_url,
                                                                                 std::string_view domain,
                                                                                 std::string_view portfolio_uuid) {
    return {
        signed_request(http_method::get, base_url, domain,
                       std::format("/api/v3/brokerage/intx/balances/{}", portfolio_uuid)),
        "get_perps_portfolio_balances",
        [](const json &j) { return j["portfolio_balances"].get<std::vector<PerpsPortfolioBalance>>(); }
    };
}

inline status_endpoint opt_in_or_out_multi_asset_collateral(std::string_view base_url, std::string_view domain,
                                                            std::string_view portfolio_uuid, bool enabled) {
    json body {
        {"portfolio_uuid", portfolio_uuid},
        {"multi_asset_collateral_enabled", enabled},
    };
    return {
        signed_request(http_method::post, base_url, domain, "/api/v3/brokerage/intx/multi_asset_collateral",
                       {}, body.dump()),
        "opt_in_or_out_multi_asset_collateral"
    };
}

}   // end namespace coinbase::detail
