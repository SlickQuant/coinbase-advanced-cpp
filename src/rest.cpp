// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/rest.hpp>
#include <coinbase/auth.hpp>
#include <coinbase/utils.hpp>
#include <nlohmann/json.hpp>
#include <slick/net/http.h>
#include <format>
#include <numeric>
#include <algorithm>

using json = nlohmann::json;
using Http = slick::net::Http;

namespace coinbase {

std::once_flag CoinbaseRestClient::initialize_products_;
std::unordered_map<std::string, Product> CoinbaseRestClient::products_;

CoinbaseRestClient::CoinbaseRestClient(std::string base_url)
    : base_url_(std::move(base_url))
{
    auto pos = base_url_.find("://");
    if (pos == std::string::npos) {
        domain_ = base_url_;
    }
    else {
        domain_ = base_url_.substr(pos + 3);
    }
    std::call_once(initialize_products_, [this](){
        auto product_list = list_public_products();
        for (auto &prod : product_list) {
            products_.emplace(prod.product_id, std::move(prod));
        }
    });
}

const Product& CoinbaseRestClient::product(std::string_view product_id) {
    return products_[std::string(product_id)];
}

void CoinbaseRestClient::set_base_url(std::string_view url) {
    base_url_ = std::string(url);
    auto pos = base_url_.find("://");
    if (pos == std::string::npos) {
        domain_ = base_url_;
    }
    else {
        domain_ = base_url_.substr(pos + 3);
    }
}

uint64_t CoinbaseRestClient::get_server_time() const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/time", base_url_));
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return std::stoull(j["epochMillis"].get<std::string_view>().data());
        }
        LOG_ERROR("Failed to get_server_time. error: {}", res.result_text);
    }
    catch(const std::exception& e) {
        LOG_ERROR("Failed to get_server_time. error: {}", e.what());
    }
    return {};
}

std::vector<Account> CoinbaseRestClient::list_accounts(const AccountQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/accounts{}", base_url_, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/accounts", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            std::vector<Account> accounts = j["accounts"];
            while (j.contains("has_next") && j["has_next"].get<bool>() && !j["cursor"].get<std::string_view>().empty()) {
                AccountQueryParams new_params;
                new_params.cursor = j["cursor"].get<std::string_view>();
                res = Http::get(std::format("{}/api/v3/brokerage/accounts{}", base_url_, new_params()), {
                    {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/accounts", domain_).c_str())}
                });
                if (res.is_ok()) {
                    j = json::parse(res.result_text);
                    accounts.insert(accounts.end(), std::make_move_iterator(j["accounts"].begin()), std::make_move_iterator(j["accounts"].end()));
                }
                else {
                    LOG_ERROR("Failed to list accounts. error: {}", res.result_text);
                    break;
                }
            }
            return accounts;
        }
        LOG_ERROR("Failed to list accounts. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to list accounts. error: {}", e.what());
    }
    return {};
}

Account CoinbaseRestClient::get_account(std::string_view account_uuid) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/accounts/{}", base_url_, account_uuid), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/accounts/{}", domain_, account_uuid).c_str())}
        });
        if (res.is_ok()) {
            auto j_res = json::parse(res.result_text);
            return j_res["account"].get<Account>();
        }
        LOG_ERROR("Failed to get account {}. error: {}", account_uuid, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to get account {}. error: {}", account_uuid, e.what());
    }
    return {};
}

std::vector<Product> CoinbaseRestClient::list_products(const ProductQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/products{}", base_url_, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/products", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j["products"];
        }
        LOG_ERROR("Failed to get products. error: {}", res.result_text);
    }
    catch(const std::exception& e) {
        LOG_ERROR("Failed to get products. error: {}", e.what());
    }
    return {};
}

Product CoinbaseRestClient::get_product(std::string_view prod_id, bool get_tradability_status) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/products/{}{}", base_url_, prod_id, get_tradability_status ? "?get_tradability_status=true" : ""), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/products/{}", domain_, prod_id).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j.get<Product>();
        }
        LOG_ERROR("Failed to get product {}. error: {}", prod_id, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to get product {}. error: {}", prod_id, e.what());
    }
    return {};
}

std::vector<Product> CoinbaseRestClient::list_public_products(const ProductQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/market/products{}", base_url_, params()));
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j["products"];
        }
        LOG_ERROR("Failed to get products. error: {}", res.result_text);
    }
    catch(const std::exception& e) {
        LOG_ERROR("Failed to get products. error: {}", e.what());
    }
    return {};
}

Product CoinbaseRestClient::get_public_product(std::string_view prod_id) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/market/products/{}", base_url_, prod_id));
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j.get<Product>();
        }
        LOG_ERROR("Failed to get product {}. error: {}", prod_id, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to get product {}. error: {}", prod_id, e.what());
    }
    return {};
}

std::vector<Order> CoinbaseRestClient::list_orders(const OrderQueryParams &query) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/orders/historical/batch{}", base_url_, query()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/orders/historical/batch", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            std::vector<Order> orders = j["orders"];
            while (j.contains("has_next") && j["has_next"].get<bool>() && !j["cursor"].get<std::string_view>().empty()) {
                OrderQueryParams new_query;
                new_query.cursor = j["cursor"].get<std::string_view>();
                res = Http::get(std::format("{}/api/v3/brokerage/orders/historical/batch{}", base_url_, new_query()), {
                    {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/orders/historical/batch", domain_).c_str())}
                });
                if (res.is_ok()) {
                    j = json::parse(res.result_text);
                    orders.insert(orders.end(), std::make_move_iterator(j["orders"].begin()), std::make_move_iterator(j["orders"].end()));
                }
                else {
                    LOG_ERROR("Failed to list orders. error: {}", res.result_text);
                    break;
                }
            }
            return orders;
        }
        LOG_ERROR("Failed to list orders. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to list orders. error: {}", e.what());
    }
    return {};
}

Order CoinbaseRestClient::get_order(std::string_view order_id) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/orders/historical/{}", base_url_, order_id), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/orders/historical/{}", domain_, order_id).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            LOG_TRACE(j.dump());
            return j["order"].get<Order>();
        }
        LOG_ERROR("Failed to get order {}. error: {}", order_id, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("Failed to get order {}. error: {}", order_id, e.what());
    }
    return {};
}

std::vector<Fill> CoinbaseRestClient::list_fills(const FillQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/orders/historical/fills{}", base_url_, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/orders/historical/fills", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            std::vector<Fill> fills = j["fills"];
            while(j.contains("cursor") && !j["cursor"].get<std::string_view>().empty()) {
                FillQueryParams new_query;
                new_query.cursor = j["cursor"].get<std::string_view>();
                res = Http::get(std::format("{}/api/v3/brokerage/orders/historical/fills{}", base_url_, new_query()), {
                    {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/orders/historical/fills", domain_).c_str())}
                });
                if (res.is_ok()) {
                    j = json::parse(res.result_text);
                    fills.insert(fills.end(), std::make_move_iterator(j["fills"].begin()), std::make_move_iterator(j["fills"].end()));
                }
                else {
                    LOG_ERROR("Failed to list orders. error: {}", res.result_text);
                    break;
                }
            }
            return fills;
        }
        LOG_ERROR("list_fills failed. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("list_fills failed. error: {}", e.what());
    }
    return {};
}

std::vector<PriceBook> CoinbaseRestClient::get_best_bid_ask(const std::vector<std::string> &product_ids) const {
    try {
        if (product_ids.empty()) {
            LOG_WARN("get_best_bid_ask empty product_ids provided");
            return {};
        }

        std::vector<std::string> params(product_ids.size());
        std::transform(product_ids.begin(), product_ids.end(), params.begin(),
            [](const std::string &i) { return std::format("product_ids={}", i); });

        auto query = std::format("?{}", std::accumulate(std::next(params.begin()), params.end(), params[0],
            [](const std::string& a, const std::string &b) {
                return a + "&" + b;
            }));
        auto res = Http::get(std::format("{}/api/v3/brokerage/best_bid_ask{}", base_url_, query), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/best_bid_ask", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            LOG_TRACE(j.dump());
            return j["pricebooks"];
        }
        LOG_ERROR("get_best_bid_ask failed. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("get_best_bid_ask failed. error: {}", e.what());
    }
    return {};
}

PriceBookResponse CoinbaseRestClient::get_product_book(const PriceBookQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/product_book{}", base_url_, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/product_book", domain_).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j;
        }
        LOG_ERROR("get_product_book failed. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("get_product_book failed. error: {}", e.what());
    }
    return {};
}

MarketTrades CoinbaseRestClient::get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const {
    try {
        auto res = Http::get(std::format("{}/api/v3/brokerage/products/{}/ticker{}", base_url_, product_id, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/products/{}/ticker", domain_, product_id).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j;
        }
        LOG_ERROR("get_market_trades failed. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("get_market_trades failed. error: {}", e.what());
    }
    return {};
}

std::vector<Candle> CoinbaseRestClient::get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const
{
    try {
        LOG_TRACE(params());
        auto res = Http::get(std::format("{}/api/v3/brokerage/products/{}/candles{}", base_url_, product_id, params()), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("GET {}/api/v3/brokerage/products/{}/candles", domain_, product_id).c_str())}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            return j["candles"];
        }
        LOG_ERROR("get_product_candles failed. error: {}", res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("get_market_trades failed. error: {}", e.what());
    }
    return {};
}

CreateOrderResponse CoinbaseRestClient::create_order(
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
    std::optional<PredictionMetadata> &&prediction_metadata
) const {
    CreateOrderResponse rsp;
    try {
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
                    auto &config = order_configuration["market_market_fok"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                }
                else if (time_in_force == TimeInForce::IMMEDIATE_OR_CANCEL) {
                    auto &config = order_configuration["market_market_ioc"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                }
                else {
                    rsp.error_response.message = std::format("TimeInForce {} invalid for market order", to_string(time_in_force));
                    rsp.success = false;
                    LOG_ERROR(rsp.error_response.message);
                    return rsp;
                }

                if (stop_price.has_value() && take_profit_price.has_value()) {
                    auto &prod = product(product_id);
                    if (prod.product_type == ProductType::SPOT && side == Side::SELL) {
                        LOG_ERROR("Invalid order side for attached TP/SL");
                        rsp.error_response.message = "Invalid order side for attached TP/SL";
                        rsp.success = false;
                        return rsp;
                    }
                    body["attached_order_configuration"] = {
                        {"trigger_bracket_gtc", {
                            {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                            {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                        }}
                    };
                }
                else if (stop_price.has_value()) {
                    if (side == Side::SELL) {

                    }
                    LOG_ERROR("braket order must have both stop_price and take_profit_price");
                    rsp.error_response.message = "braket order must have both stop_price and take_profit_price";
                    rsp.success = false;
                    return rsp;
                }
                else {

                }
                break;
            }
            case OrderType::LIMIT: {
                if (std::isnan(limit_price)) {
                    LOG_ERROR("Invalid limit price NAN");
                    rsp.error_response.message = "Invalid limit price NAN";
                    rsp.success = false;
                    return rsp;
                }
                if (time_in_force == TimeInForce::FILL_OR_KILL) {
                    auto &config = order_configuration["limit_limit_fok"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                }
                else if (time_in_force == TimeInForce::IMMEDIATE_OR_CANCEL) {
                    auto &config = order_configuration["sor_limit_ioc"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                }
                else if (time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
                    auto &config = order_configuration["limit_limit_gtc"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                    config["post_only"] = post_only;
                }
                else if (time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
                    if (!end_time.has_value()) {
                        LOG_ERROR("end_time missing for limit_gtd order");
                        rsp.error_response.message = "end_time missing for limit_gtd order";
                        rsp.success = false;
                        return rsp;
                    }
                    auto &config = order_configuration["limit_limit_gtd"];
                    if (size_in_quote) {
                        config["quote_size"] = std::to_string(size);
                    }
                    else {
                        config["base_size"] = std::to_string(size);
                    }
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                    config["post_only"] = post_only;
                    config["end_time"] = timestamp_to_string(end_time.value());
                }
                else {
                    rsp.error_response.message = std::format("TimeInForce {} invalid for market order", to_string(time_in_force));
                    rsp.success = false;
                    LOG_ERROR(rsp.error_response.message);
                    return rsp;
                }

                if (stop_price.has_value() && take_profit_price.has_value()) {
                    auto &prod = product(product_id);
                    if (prod.product_type == ProductType::SPOT && side == Side::SELL) {
                        LOG_ERROR("Invalid order side for attached TP/SL");
                        rsp.error_response.message = "Invalid order side for attached TP/SL";
                        rsp.success = false;
                        return rsp;
                    }
                    body["attached_order_configuration"] = {
                        {"trigger_bracket_gtc", {
                            {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                            {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                        }}
                    };
                }
                else if (stop_price.has_value() ^ take_profit_price.has_value()) {
                    LOG_ERROR("braket order must have both stop_price and take_profit_price");
                    rsp.error_response.message = "braket order must have both stop_price and take_profit_price";
                    rsp.success = false;
                    return rsp;
                }
                break;
            }
            case OrderType::STOP_LIMIT: {
                if (size_in_quote) {
                    LOG_ERROR("Invalid parameter. stop limit order size only in base_size");
                    rsp.error_response.message = "Invalid parameter. stop limit order size only in base_size";
                    rsp.success = false;
                    return rsp;
                }
                if (!stop_price.has_value() || std::isnan(stop_price.value())) {
                    LOG_ERROR("Invalid stop_price {}", stop_price.value_or(NAN));
                    rsp.error_response.message = std::format("Invalid stop_price {}", stop_price.value_or(NAN));
                    rsp.success = false;
                    return rsp;
                }
                if (time_in_force == TimeInForce::GOOD_UNTIL_CANCELLED) {
                    auto &config = order_configuration["stop_limit_stop_limit_gtc"];
                    config["base_size"] = std::to_string(size);
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                    config["stop_price"] = to_string(stop_price.value(), product(product_id).quote_increment);
                }
                else if (time_in_force == TimeInForce::GOOD_UNTIL_DATE_TIME) {
                    if (!end_time.has_value())
                    {
                        LOG_ERROR("end_time missing for limit_gtd order");
                        rsp.error_response.message = "end_time missing for limit_gtd order";
                        rsp.success = false;
                        return rsp;
                    }
                    auto &config = order_configuration["stop_limit_stop_limit_gtd"];
                    config["base_size"] = std::to_string(size);
                    config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                    config["end_time"] = timestamp_to_string(end_time.value());
                }
                else {
                    rsp.error_response.message = std::format("TimeInForce {} invalid for market order", to_string(time_in_force));
                    rsp.success = false;
                    LOG_ERROR(rsp.error_response.message);
                    return rsp;
                }
                break;
            }
            case OrderType::TWAP: {
                if (!twap_start_time.has_value() || !end_time.has_value()) {
                    LOG_ERROR("twap order must have start and end time");
                    rsp.error_response.message = "twap order must have start and end time";
                    rsp.success = false;
                    return rsp;
                }

                auto &config = order_configuration["twap_limit_gtd"];
                if (size_in_quote) {
                    config["quote_size"] = std::to_string(size);
                }
                else {
                    config["base_size"] = std::to_string(size);
                }
                config["limit_price"] = to_string(limit_price, product(product_id).quote_increment);
                config["start_time"] = timestamp_to_string(twap_start_time.value());
                config["end_time"] = timestamp_to_string(end_time.value());
                break;
            }
            case OrderType::BRACKET: {
                auto &prod = product(product_id);
                if (prod.product_type == ProductType::SPOT && side == Side::BUY) {
                    LOG_ERROR("Invalid order side for Bracket order");
                    rsp.error_response.message = "Invalid order side for Bracket order";
                    rsp.success = false;
                    return rsp;
                }

                if (size_in_quote) {
                    LOG_ERROR("Invalid parameter. Bracket order size only in base_size");
                    rsp.error_response.message = "Invalid parameter. Bracket order size only in base_size";
                    rsp.success = false;
                    return rsp;
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
                    LOG_ERROR("braket order must have both stop_price and take_profit_price");
                    rsp.error_response.message = "braket order must have both stop_price and take_profit_price";
                    rsp.success = false;
                    return rsp;
                }
                break;
            }
            default: {
                rsp.error_response.message = std::format("OrderType {} is not supported. client_order_id: {}", to_string(order_type), client_order_id);
                LOG_ERROR(rsp.error_response.message);
                rsp.success = false;
                return rsp;
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

        LOG_TRACE("create order: {}", body.dump());
        auto res = Http::post(std::format("{}/api/v3/brokerage/orders", base_url_), body.dump(), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("POST {}/api/v3/brokerage/orders", domain_).c_str())},
            {"Content-Type", "application/json"}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            LOG_TRACE(j.dump());
            return j;
        }
        rsp.error_response.message = std::format("Failed to create order. client_order_id: {} error: {}", client_order_id, res.result_text);
        LOG_ERROR(rsp.error_response.message);
    }
    catch (const std::exception &e) {
        rsp.error_response.message = std::format("Failed to create order. client_order_id: {}  error: {}", client_order_id, e.what());
        LOG_ERROR(rsp.error_response.message);
    }
    rsp.success = false;
    return rsp;
}

ModifyOrderResponse CoinbaseRestClient::modify_order(
    std::string order_id,
    std::string product_id,
    double price,
    double size,
    std::optional<double> stop_price,
    std::optional<double> take_profit_price,
    std::optional<bool> cancel_attached_order
) const {
    ModifyOrderResponse rsp;
    try {
        json body {
            {"order_id", order_id},
            {"size", std::to_string(size)},
        };
        body["price"] = to_string(price, product(product_id).quote_increment);
        if (stop_price.has_value() && take_profit_price.has_value()) {
            auto &prod = product(product_id);
            body["attached_order_configuration"] = {
                {"trigger_bracket_gtc", {
                    {"limit_price", to_string(take_profit_price.value(), prod.quote_increment)},
                    {"stop_trigger_price", to_string(stop_price.value(), prod.quote_increment)},
                }}
            };
        }
        else if (stop_price.has_value()) {
            body["stop_price"] = to_string(stop_price.value(), product(product_id).quote_increment);
        }
        if (cancel_attached_order.has_value()) {
            body["cancel_attached_order"] = cancel_attached_order.value();
        }

        LOG_TRACE("modify order: {}", body.dump());
        auto res = Http::post(std::format("{}/api/v3/brokerage/orders/edit", base_url_), body.dump(), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("POST {}/api/v3/brokerage/orders/edit", domain_).c_str())},
            {"Content-Type", "application/json"}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            LOG_TRACE(j.dump());
            return j;
        }
        LOG_ERROR("modify_order failed. order_id: {}, error: {}", order_id, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("modify_order failed. order_id: {}, error: {}", order_id, e.what());
    }
    rsp.success = false;
    return rsp;
}

std::vector<CancelOrderResponse> CoinbaseRestClient::cancel_orders(const std::vector<std::string_view> &order_ids) const {
    std::vector<CancelOrderResponse> rt;
    try {
        json body {
            {"order_ids", order_ids},
        };

        LOG_TRACE("cancel order: {}", body.dump());
        auto res = Http::post(std::format("{}/api/v3/brokerage/orders/batch_cancel", base_url_), body.dump(), {
            {"Authorization", "Bearer " + coinbase::generate_coinbase_jwt(std::format("POST {}/api/v3/brokerage/orders/batch_cancel", domain_).c_str())},
            {"Content-Type", "application/json"}
        });
        if (res.is_ok()) {
            auto j = json::parse(res.result_text);
            LOG_TRACE(j.dump());
            return j["results"];
        }
        LOG_ERROR("cancel_orders failed. error: {}", res.result_text);
        for (auto oid : order_ids) {
            CancelOrderResponse rsp;
            rsp.success = false;
            rsp.failure_reason = "INVALID_CANCEL_REQUEST";
            rsp.order_id = oid;
            rt.emplace_back(std::move(rsp));
        }
    }
    catch (const std::exception &e) {
        for (auto oid : order_ids) {
            CancelOrderResponse rsp;
            rsp.success = false;
            rsp.failure_reason = "INVALID_CANCEL_REQUEST";
            rsp.order_id = oid;
            rt.emplace_back(std::move(rsp));
        }
        LOG_ERROR("cancel_orders failed. error: {}", e.what());
    }
    return rt;
}

}   // end namespace coinbase
