// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/rest.hpp>
#include "rest_endpoints.hpp"

namespace coinbase {

std::once_flag CoinbaseRestClient::initialize_products_;
std::unordered_map<std::string, Product> CoinbaseRestClient::products_;

namespace {

std::string extract_domain(std::string_view base_url) {
    auto pos = base_url.find("://");
    if (pos == std::string_view::npos) {
        return std::string(base_url);
    }
    return std::string(base_url.substr(pos + 3));
}

}   // namespace

CoinbaseRestClient::CoinbaseRestClient(std::string base_url)
    : base_url_(std::move(base_url))
    , domain_(extract_domain(base_url_))
{
    initialize_products(base_url_);
}

const Product& CoinbaseRestClient::product(std::string_view product_id) {
    return products_[std::string(product_id)];
}

void CoinbaseRestClient::initialize_products(std::string_view base_url) {
    std::call_once(initialize_products_, [base_url]() {
        for (auto &prod : detail::run(detail::list_public_products(base_url, {}))) {
            products_.emplace(prod.product_id, std::move(prod));
        }
    });
}

void CoinbaseRestClient::set_base_url(std::string_view url) {
    base_url_ = std::string(url);
    domain_ = extract_domain(base_url_);
}

uint64_t CoinbaseRestClient::get_server_time() const {
    return detail::run(detail::get_server_time(base_url_));
}

std::vector<Account> CoinbaseRestClient::list_accounts(const AccountQueryParams &params) const {
    return detail::run(detail::list_accounts(base_url_, domain_, params));
}

Account CoinbaseRestClient::get_account(std::string_view account_uuid) const {
    return detail::run(detail::get_account(base_url_, domain_, account_uuid));
}

std::vector<Product> CoinbaseRestClient::list_products(const ProductQueryParams &params) const {
    return detail::run(detail::list_products(base_url_, domain_, params));
}

Product CoinbaseRestClient::get_product(std::string_view prod_id, bool get_tradability_status) const {
    return detail::run(detail::get_product(base_url_, domain_, prod_id, get_tradability_status));
}

std::vector<Product> CoinbaseRestClient::list_public_products(const ProductQueryParams &params) const {
    return detail::run(detail::list_public_products(base_url_, params));
}

Product CoinbaseRestClient::get_public_product(std::string_view prod_id) const {
    return detail::run(detail::get_public_product(base_url_, prod_id));
}

std::vector<Order> CoinbaseRestClient::list_orders(const OrderQueryParams &query) const {
    return detail::run(detail::list_orders(base_url_, domain_, query));
}

Order CoinbaseRestClient::get_order(std::string_view order_id) const {
    return detail::run(detail::get_order(base_url_, domain_, order_id));
}

std::vector<Fill> CoinbaseRestClient::list_fills(const FillQueryParams &params) const {
    return detail::run(detail::list_fills(base_url_, domain_, params));
}

double CoinbaseRestClient::get_taker_fee_rate() const {
    return detail::run(detail::get_taker_fee_rate(base_url_, domain_));
}

double CoinbaseRestClient::get_maker_fee_rate() const {
    return detail::run(detail::get_maker_fee_rate(base_url_, domain_));
}

std::vector<PriceBook> CoinbaseRestClient::get_best_bid_ask(const std::vector<std::string> &product_ids) const {
    return detail::run(detail::get_best_bid_ask(base_url_, domain_, product_ids));
}

PriceBookResponse CoinbaseRestClient::get_product_book(const PriceBookQueryParams &params) const {
    return detail::run(detail::get_product_book(base_url_, domain_, params));
}

MarketTrades CoinbaseRestClient::get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const {
    return detail::run(detail::get_market_trades(base_url_, domain_, product_id, params));
}

std::vector<Candle> CoinbaseRestClient::get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const {
    return detail::run(detail::get_product_candles(base_url_, domain_, product_id, params));
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
    return detail::run(detail::make_create_order(base_url_, domain_,
        std::move(client_order_id), std::move(product_id), side, order_type, time_in_force, size,
        limit_price, post_only, size_in_quote, stop_price, take_profit_price, end_time, twap_start_time,
        std::move(sor_preference), std::move(leverage), std::move(margin_type),
        std::move(attached_order_configuration), std::move(prediction_metadata)));
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
    return detail::run(detail::make_modify_order(base_url_, domain_, std::move(order_id), product_id,
                                                 price, size, stop_price, take_profit_price,
                                                 cancel_attached_order));
}

std::vector<CancelOrderResponse> CoinbaseRestClient::cancel_orders(const std::vector<std::string_view> &order_ids) const {
    return detail::run(detail::make_cancel_orders(base_url_, domain_, order_ids));
}

std::vector<Portfolio> CoinbaseRestClient::list_portfolios(std::optional<PortfolioType> portfolio_type) const {
    return detail::run(detail::list_portfolios(base_url_, domain_, portfolio_type));
}

Portfolio CoinbaseRestClient::create_portfolio(std::string_view name) const {
    return detail::run(detail::create_portfolio(base_url_, domain_, name));
}

PortfolioBreakdown CoinbaseRestClient::get_portfolio_breakdown(std::string_view portfolio_uuid, std::optional<std::string_view> currency) const {
    return detail::run(detail::get_portfolio_breakdown(base_url_, domain_, portfolio_uuid, currency));
}

MovePortfolioFundsResult CoinbaseRestClient::move_portfolio_funds(double value, std::string_view currency, std::string_view source_portfolio_uuid, std::string_view target_portfolio_uuid) const {
    return detail::run(detail::move_portfolio_funds(base_url_, domain_, value, currency, source_portfolio_uuid, target_portfolio_uuid));
}

Portfolio CoinbaseRestClient::edit_portfolio(std::string_view portfolio_uuid, std::string_view name) const {
    return detail::run(detail::edit_portfolio(base_url_, domain_, portfolio_uuid, name));
}

bool CoinbaseRestClient::delete_portfolio(std::string_view portfolio_uuid) const {
    return detail::run(detail::delete_portfolio(base_url_, domain_, portfolio_uuid));
}

ConvertTrade CoinbaseRestClient::create_convert_quote(std::string_view from_account, std::string_view to_account, double amount) const {
    return detail::run(detail::create_convert_quote(base_url_, domain_, from_account, to_account, amount));
}

ConvertTrade CoinbaseRestClient::get_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    return detail::run(detail::get_convert_trade(base_url_, domain_, trade_id, from_account, to_account));
}

ConvertTrade CoinbaseRestClient::commit_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    return detail::run(detail::commit_convert_trade(base_url_, domain_, trade_id, from_account, to_account));
}

std::vector<PaymentMethod> CoinbaseRestClient::list_payment_methods() const {
    return detail::run(detail::list_payment_methods(base_url_, domain_));
}

PaymentMethod CoinbaseRestClient::get_payment_method(std::string_view payment_method_id) const {
    return detail::run(detail::get_payment_method(base_url_, domain_, payment_method_id));
}

ApiKeyPermissions CoinbaseRestClient::get_api_key_permissions() const {
    return detail::run(detail::get_api_key_permissions(base_url_, domain_));
}

FCMBalanceSummary CoinbaseRestClient::get_futures_balance_summary() const {
    return detail::run(detail::get_futures_balance_summary(base_url_, domain_));
}

std::vector<FCMPosition> CoinbaseRestClient::list_futures_positions() const {
    return detail::run(detail::list_futures_positions(base_url_, domain_));
}

FCMPosition CoinbaseRestClient::get_futures_position(std::string_view product_id) const {
    return detail::run(detail::get_futures_position(base_url_, domain_, product_id));
}

bool CoinbaseRestClient::schedule_futures_sweep(double usd_amount) const {
    return detail::run(detail::schedule_futures_sweep(base_url_, domain_, usd_amount));
}

std::vector<FCMSweep> CoinbaseRestClient::list_futures_sweeps() const {
    return detail::run(detail::list_futures_sweeps(base_url_, domain_));
}

bool CoinbaseRestClient::cancel_pending_futures_sweep() const {
    return detail::run(detail::cancel_pending_futures_sweep(base_url_, domain_));
}

std::string CoinbaseRestClient::get_intraday_margin_setting() const {
    return detail::run(detail::get_intraday_margin_setting(base_url_, domain_));
}

CurrentMarginWindow CoinbaseRestClient::get_current_margin_window(std::string_view margin_profile_type) const {
    return detail::run(detail::get_current_margin_window(base_url_, domain_, margin_profile_type));
}

bool CoinbaseRestClient::set_intraday_margin_setting(std::string_view setting) const {
    return detail::run(detail::set_intraday_margin_setting(base_url_, domain_, setting));
}

bool CoinbaseRestClient::allocate_portfolio(std::string_view portfolio_uuid, std::string_view symbol, double amount, std::string_view currency) const {
    return detail::run(detail::allocate_portfolio(base_url_, domain_, portfolio_uuid, symbol, amount, currency));
}

PerpsPortfolioSummaryResponse CoinbaseRestClient::get_perps_portfolio_summary(std::string_view portfolio_uuid) const {
    return detail::run(detail::get_perps_portfolio_summary(base_url_, domain_, portfolio_uuid));
}

PerpsPositionsResponse CoinbaseRestClient::list_perps_positions(std::string_view portfolio_uuid) const {
    return detail::run(detail::list_perps_positions(base_url_, domain_, portfolio_uuid));
}

PerpsPosition CoinbaseRestClient::get_perps_position(std::string_view portfolio_uuid, std::string_view symbol) const {
    return detail::run(detail::get_perps_position(base_url_, domain_, portfolio_uuid, symbol));
}

std::vector<PerpsPortfolioBalance> CoinbaseRestClient::get_perps_portfolio_balances(std::string_view portfolio_uuid) const {
    return detail::run(detail::get_perps_portfolio_balances(base_url_, domain_, portfolio_uuid));
}

bool CoinbaseRestClient::opt_in_or_out_multi_asset_collateral(std::string_view portfolio_uuid, bool enabled) const {
    return detail::run(detail::opt_in_or_out_multi_asset_collateral(base_url_, domain_, portfolio_uuid, enabled));
}

}   // end namespace coinbase
