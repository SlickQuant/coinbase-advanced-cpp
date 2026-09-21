// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/rest_awaitable.hpp>
#include <coinbase/rest.hpp>
#include "rest_endpoints.hpp"

namespace coinbase {

namespace {

std::string extract_domain(std::string_view base_url) {
    auto pos = base_url.find("://");
    if (pos == std::string_view::npos) {
        return std::string(base_url);
    }
    return std::string(base_url.substr(pos + 3));
}

}   // namespace

CoinbaseAwaitableRestClient::CoinbaseAwaitableRestClient(std::string base_url)
    : base_url_(std::move(base_url))
    , domain_(extract_domain(base_url_))
{
    // The shared product cache is primed once, with one blocking request; construct the client
    // before entering the event loop rather than from inside a coroutine.
    CoinbaseRestClient::initialize_products(base_url_);
}

const Product& CoinbaseAwaitableRestClient::product(std::string_view product_id) {
    return CoinbaseRestClient::product(product_id);
}

void CoinbaseAwaitableRestClient::set_base_url(std::string_view url) {
    base_url_ = std::string(url);
    domain_ = extract_domain(base_url_);
}

asio::awaitable<uint64_t> CoinbaseAwaitableRestClient::get_server_time() const {
    return detail::run_async(detail::get_server_time(base_url_));
}

asio::awaitable<std::vector<Account>> CoinbaseAwaitableRestClient::list_accounts(const AccountQueryParams &params) const {
    return detail::run_async(detail::list_accounts(base_url_, domain_, params));
}

asio::awaitable<Account> CoinbaseAwaitableRestClient::get_account(std::string_view account_uuid) const {
    return detail::run_async(detail::get_account(base_url_, domain_, account_uuid));
}

asio::awaitable<std::vector<Product>> CoinbaseAwaitableRestClient::list_products(const ProductQueryParams &params) const {
    return detail::run_async(detail::list_products(base_url_, domain_, params));
}

asio::awaitable<Product> CoinbaseAwaitableRestClient::get_product(std::string_view product_id, bool get_tradability_status) const {
    return detail::run_async(detail::get_product(base_url_, domain_, product_id, get_tradability_status));
}

asio::awaitable<std::vector<Product>> CoinbaseAwaitableRestClient::list_public_products(const ProductQueryParams &params) const {
    return detail::run_async(detail::list_public_products(base_url_, params));
}

asio::awaitable<Product> CoinbaseAwaitableRestClient::get_public_product(std::string_view product_id) const {
    return detail::run_async(detail::get_public_product(base_url_, product_id));
}

asio::awaitable<std::vector<Order>> CoinbaseAwaitableRestClient::list_orders(const OrderQueryParams &query) const {
    return detail::run_async(detail::list_orders(base_url_, domain_, query));
}

asio::awaitable<Order> CoinbaseAwaitableRestClient::get_order(std::string_view order_id) const {
    return detail::run_async(detail::get_order(base_url_, domain_, order_id));
}

asio::awaitable<std::vector<Fill>> CoinbaseAwaitableRestClient::list_fills(const FillQueryParams &params) const {
    return detail::run_async(detail::list_fills(base_url_, domain_, params));
}

asio::awaitable<std::vector<PriceBook>> CoinbaseAwaitableRestClient::get_best_bid_ask(const std::vector<std::string> &product_ids) const {
    return detail::run_async(detail::get_best_bid_ask(base_url_, domain_, product_ids));
}

asio::awaitable<PriceBookResponse> CoinbaseAwaitableRestClient::get_product_book(const PriceBookQueryParams &params) const {
    return detail::run_async(detail::get_product_book(base_url_, domain_, params));
}

asio::awaitable<MarketTrades> CoinbaseAwaitableRestClient::get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const {
    return detail::run_async(detail::get_market_trades(base_url_, domain_, product_id, params));
}

asio::awaitable<std::vector<Candle>> CoinbaseAwaitableRestClient::get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const {
    return detail::run_async(detail::get_product_candles(base_url_, domain_, product_id, params));
}

asio::awaitable<CreateOrderResponse> CoinbaseAwaitableRestClient::create_order(
    std::string &&client_order_id,
    std::string &&product_id,
    Side side,
    OrderType order_type,
    TimeInForce time_in_force,
    double size,
    double price,
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
    return detail::run_async(detail::make_create_order(base_url_, domain_,
        std::move(client_order_id), std::move(product_id), side, order_type, time_in_force, size,
        price, post_only, size_in_quote, stop_price, take_profit_price, end_time, twap_start_time,
        std::move(sor_preference), std::move(leverage), std::move(margin_type),
        std::move(attached_order_configuration), std::move(prediction_metadata)));
}

asio::awaitable<ModifyOrderResponse> CoinbaseAwaitableRestClient::modify_order(
    std::string order_id,
    std::string product_id,
    double price,
    double size,
    std::optional<double> stop_price,
    std::optional<double> take_profit_price,
    std::optional<bool> cancel_attached_order
) const {
    return detail::run_async(detail::make_modify_order(base_url_, domain_, std::move(order_id),
                                                       product_id, price, size, stop_price,
                                                       take_profit_price, cancel_attached_order));
}

asio::awaitable<std::vector<CancelOrderResponse>> CoinbaseAwaitableRestClient::cancel_orders(const std::vector<std::string_view> &order_ids) const {
    return detail::run_async(detail::make_cancel_orders(base_url_, domain_, order_ids));
}

asio::awaitable<std::vector<Portfolio>> CoinbaseAwaitableRestClient::list_portfolios(std::optional<PortfolioType> portfolio_type) const {
    return detail::run_async(detail::list_portfolios(base_url_, domain_, portfolio_type));
}

asio::awaitable<Portfolio> CoinbaseAwaitableRestClient::create_portfolio(std::string_view name) const {
    return detail::run_async(detail::create_portfolio(base_url_, domain_, name));
}

asio::awaitable<PortfolioBreakdown> CoinbaseAwaitableRestClient::get_portfolio_breakdown(std::string_view portfolio_uuid, std::optional<std::string_view> currency) const {
    return detail::run_async(detail::get_portfolio_breakdown(base_url_, domain_, portfolio_uuid, currency));
}

asio::awaitable<MovePortfolioFundsResult> CoinbaseAwaitableRestClient::move_portfolio_funds(double value, std::string_view currency, std::string_view source_portfolio_uuid, std::string_view target_portfolio_uuid) const {
    return detail::run_async(detail::move_portfolio_funds(base_url_, domain_, value, currency, source_portfolio_uuid, target_portfolio_uuid));
}

asio::awaitable<Portfolio> CoinbaseAwaitableRestClient::edit_portfolio(std::string_view portfolio_uuid, std::string_view name) const {
    return detail::run_async(detail::edit_portfolio(base_url_, domain_, portfolio_uuid, name));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::delete_portfolio(std::string_view portfolio_uuid) const {
    return detail::run_async(detail::delete_portfolio(base_url_, domain_, portfolio_uuid));
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::create_convert_quote(std::string_view from_account, std::string_view to_account, double amount) const {
    return detail::run_async(detail::create_convert_quote(base_url_, domain_, from_account, to_account, amount));
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::get_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    return detail::run_async(detail::get_convert_trade(base_url_, domain_, trade_id, from_account, to_account));
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::commit_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    return detail::run_async(detail::commit_convert_trade(base_url_, domain_, trade_id, from_account, to_account));
}

asio::awaitable<std::vector<PaymentMethod>> CoinbaseAwaitableRestClient::list_payment_methods() const {
    return detail::run_async(detail::list_payment_methods(base_url_, domain_));
}

asio::awaitable<PaymentMethod> CoinbaseAwaitableRestClient::get_payment_method(std::string_view payment_method_id) const {
    return detail::run_async(detail::get_payment_method(base_url_, domain_, payment_method_id));
}

asio::awaitable<ApiKeyPermissions> CoinbaseAwaitableRestClient::get_api_key_permissions() const {
    return detail::run_async(detail::get_api_key_permissions(base_url_, domain_));
}

asio::awaitable<FCMBalanceSummary> CoinbaseAwaitableRestClient::get_futures_balance_summary() const {
    return detail::run_async(detail::get_futures_balance_summary(base_url_, domain_));
}

asio::awaitable<std::vector<FCMPosition>> CoinbaseAwaitableRestClient::list_futures_positions() const {
    return detail::run_async(detail::list_futures_positions(base_url_, domain_));
}

asio::awaitable<FCMPosition> CoinbaseAwaitableRestClient::get_futures_position(std::string_view product_id) const {
    return detail::run_async(detail::get_futures_position(base_url_, domain_, product_id));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::schedule_futures_sweep(double usd_amount) const {
    return detail::run_async(detail::schedule_futures_sweep(base_url_, domain_, usd_amount));
}

asio::awaitable<std::vector<FCMSweep>> CoinbaseAwaitableRestClient::list_futures_sweeps() const {
    return detail::run_async(detail::list_futures_sweeps(base_url_, domain_));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::cancel_pending_futures_sweep() const {
    return detail::run_async(detail::cancel_pending_futures_sweep(base_url_, domain_));
}

asio::awaitable<std::string> CoinbaseAwaitableRestClient::get_intraday_margin_setting() const {
    return detail::run_async(detail::get_intraday_margin_setting(base_url_, domain_));
}

asio::awaitable<CurrentMarginWindow> CoinbaseAwaitableRestClient::get_current_margin_window(std::string_view margin_profile_type) const {
    return detail::run_async(detail::get_current_margin_window(base_url_, domain_, margin_profile_type));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::set_intraday_margin_setting(std::string_view setting) const {
    return detail::run_async(detail::set_intraday_margin_setting(base_url_, domain_, setting));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::allocate_portfolio(std::string_view portfolio_uuid, std::string_view symbol, double amount, std::string_view currency) const {
    return detail::run_async(detail::allocate_portfolio(base_url_, domain_, portfolio_uuid, symbol, amount, currency));
}

asio::awaitable<PerpsPortfolioSummaryResponse> CoinbaseAwaitableRestClient::get_perps_portfolio_summary(std::string_view portfolio_uuid) const {
    return detail::run_async(detail::get_perps_portfolio_summary(base_url_, domain_, portfolio_uuid));
}

asio::awaitable<PerpsPositionsResponse> CoinbaseAwaitableRestClient::list_perps_positions(std::string_view portfolio_uuid) const {
    return detail::run_async(detail::list_perps_positions(base_url_, domain_, portfolio_uuid));
}

asio::awaitable<PerpsPosition> CoinbaseAwaitableRestClient::get_perps_position(std::string_view portfolio_uuid, std::string_view symbol) const {
    return detail::run_async(detail::get_perps_position(base_url_, domain_, portfolio_uuid, symbol));
}

asio::awaitable<std::vector<PerpsPortfolioBalance>> CoinbaseAwaitableRestClient::get_perps_portfolio_balances(std::string_view portfolio_uuid) const {
    return detail::run_async(detail::get_perps_portfolio_balances(base_url_, domain_, portfolio_uuid));
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::opt_in_or_out_multi_asset_collateral(std::string_view portfolio_uuid, bool enabled) const {
    return detail::run_async(detail::opt_in_or_out_multi_asset_collateral(base_url_, domain_, portfolio_uuid, enabled));
}

}   // end namespace coinbase
