// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/rest_awaitable.hpp>
#include <coinbase/rest.hpp>
#include <utility>

namespace coinbase {

namespace {

std::string extract_domain(std::string_view base_url) {
    auto pos = base_url.find("://");
    if (pos == std::string::npos) {
        return std::string(base_url);
    }
    return std::string(base_url.substr(pos + 3));
}

}  // namespace

CoinbaseAwaitableRestClient::CoinbaseAwaitableRestClient(std::string base_url)
    : base_url_(std::move(base_url))
    , domain_(extract_domain(base_url_))
    , sync_client_(std::make_unique<CoinbaseRestClient>(base_url_))
{
}

CoinbaseAwaitableRestClient::~CoinbaseAwaitableRestClient() = default;

CoinbaseAwaitableRestClient::CoinbaseAwaitableRestClient(const CoinbaseAwaitableRestClient& other)
    : base_url_(other.base_url_)
    , domain_(other.domain_)
    , sync_client_(std::make_unique<CoinbaseRestClient>(other.base_url_))
{
}

CoinbaseAwaitableRestClient& CoinbaseAwaitableRestClient::operator=(const CoinbaseAwaitableRestClient& other) {
    if (this == &other) {
        return *this;
    }
    base_url_ = other.base_url_;
    domain_ = other.domain_;
    sync_client_ = std::make_unique<CoinbaseRestClient>(base_url_);
    return *this;
}

CoinbaseAwaitableRestClient::CoinbaseAwaitableRestClient(CoinbaseAwaitableRestClient&& other) noexcept = default;

CoinbaseAwaitableRestClient& CoinbaseAwaitableRestClient::operator=(CoinbaseAwaitableRestClient&& other) noexcept = default;

const Product& CoinbaseAwaitableRestClient::product(std::string_view product_id) {
    return CoinbaseRestClient::product(product_id);
}

void CoinbaseAwaitableRestClient::set_base_url(std::string_view url) {
    base_url_ = std::string(url);
    domain_ = extract_domain(base_url_);
    sync_client_->set_base_url(base_url_);
}

asio::awaitable<uint64_t> CoinbaseAwaitableRestClient::get_server_time() const {
    co_return sync_client_->get_server_time();
}

asio::awaitable<std::vector<Account>> CoinbaseAwaitableRestClient::list_accounts(const AccountQueryParams &params) const {
    co_return sync_client_->list_accounts(params);
}

asio::awaitable<Account> CoinbaseAwaitableRestClient::get_account(std::string_view account_uuid) const {
    co_return sync_client_->get_account(account_uuid);
}

asio::awaitable<std::vector<Product>> CoinbaseAwaitableRestClient::list_products(const ProductQueryParams &params) const {
    co_return sync_client_->list_products(params);
}

asio::awaitable<Product> CoinbaseAwaitableRestClient::get_product(std::string_view product_id, bool get_tradability_status) const {
    co_return sync_client_->get_product(product_id, get_tradability_status);
}

asio::awaitable<std::vector<Product>> CoinbaseAwaitableRestClient::list_public_products(const ProductQueryParams &params) const {
    co_return sync_client_->list_public_products(params);
}

asio::awaitable<Product> CoinbaseAwaitableRestClient::get_public_product(std::string_view product_id) const {
    co_return sync_client_->get_public_product(product_id);
}

asio::awaitable<std::vector<Order>> CoinbaseAwaitableRestClient::list_orders(const OrderQueryParams &query) const {
    co_return sync_client_->list_orders(query);
}

asio::awaitable<Order> CoinbaseAwaitableRestClient::get_order(std::string_view order_id) const {
    co_return sync_client_->get_order(order_id);
}

asio::awaitable<std::vector<Fill>> CoinbaseAwaitableRestClient::list_fills(const FillQueryParams &params) const {
    co_return sync_client_->list_fills(params);
}

asio::awaitable<std::vector<PriceBook>> CoinbaseAwaitableRestClient::get_best_bid_ask(const std::vector<std::string> &product_ids) const {
    co_return sync_client_->get_best_bid_ask(product_ids);
}

asio::awaitable<PriceBookResponse> CoinbaseAwaitableRestClient::get_product_book(const PriceBookQueryParams &params) const {
    co_return sync_client_->get_product_book(params);
}

asio::awaitable<MarketTrades> CoinbaseAwaitableRestClient::get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const {
    co_return sync_client_->get_market_trades(product_id, params);
}

asio::awaitable<std::vector<Candle>> CoinbaseAwaitableRestClient::get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const {
    co_return sync_client_->get_product_candles(product_id, params);
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
    co_return sync_client_->create_order(
        std::move(client_order_id),
        std::move(product_id),
        side,
        order_type,
        time_in_force,
        size,
        price,
        post_only,
        size_in_quote,
        stop_price,
        take_profit_price,
        end_time,
        twap_start_time,
        std::move(sor_preference),
        std::move(leverage),
        std::move(margin_type),
        std::move(attached_order_configuration),
        std::move(prediction_metadata));
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
    co_return sync_client_->modify_order(
        std::move(order_id),
        std::move(product_id),
        price,
        size,
        stop_price,
        take_profit_price,
        cancel_attached_order);
}

asio::awaitable<std::vector<CancelOrderResponse>> CoinbaseAwaitableRestClient::cancel_orders(const std::vector<std::string_view> &order_ids) const {
    co_return sync_client_->cancel_orders(order_ids);
}

asio::awaitable<std::vector<Portfolio>> CoinbaseAwaitableRestClient::list_portfolios(std::optional<PortfolioType> portfolio_type) const {
    co_return sync_client_->list_portfolios(portfolio_type);
}

asio::awaitable<Portfolio> CoinbaseAwaitableRestClient::create_portfolio(std::string_view name) const {
    co_return sync_client_->create_portfolio(name);
}

asio::awaitable<PortfolioBreakdown> CoinbaseAwaitableRestClient::get_portfolio_breakdown(std::string_view portfolio_uuid, std::optional<std::string_view> currency) const {
    co_return sync_client_->get_portfolio_breakdown(portfolio_uuid, currency);
}

asio::awaitable<MovePortfolioFundsResult> CoinbaseAwaitableRestClient::move_portfolio_funds(double value, std::string_view currency, std::string_view source_portfolio_uuid, std::string_view target_portfolio_uuid) const {
    co_return sync_client_->move_portfolio_funds(value, currency, source_portfolio_uuid, target_portfolio_uuid);
}

asio::awaitable<Portfolio> CoinbaseAwaitableRestClient::edit_portfolio(std::string_view portfolio_uuid, std::string_view name) const {
    co_return sync_client_->edit_portfolio(portfolio_uuid, name);
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::delete_portfolio(std::string_view portfolio_uuid) const {
    co_return sync_client_->delete_portfolio(portfolio_uuid);
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::create_convert_quote(std::string_view from_account, std::string_view to_account, double amount) const {
    co_return sync_client_->create_convert_quote(from_account, to_account, amount);
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::get_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    co_return sync_client_->get_convert_trade(trade_id, from_account, to_account);
}

asio::awaitable<ConvertTrade> CoinbaseAwaitableRestClient::commit_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const {
    co_return sync_client_->commit_convert_trade(trade_id, from_account, to_account);
}

asio::awaitable<std::vector<PaymentMethod>> CoinbaseAwaitableRestClient::list_payment_methods() const {
    co_return sync_client_->list_payment_methods();
}

asio::awaitable<PaymentMethod> CoinbaseAwaitableRestClient::get_payment_method(std::string_view payment_method_id) const {
    co_return sync_client_->get_payment_method(payment_method_id);
}

asio::awaitable<ApiKeyPermissions> CoinbaseAwaitableRestClient::get_api_key_permissions() const {
    co_return sync_client_->get_api_key_permissions();
}

asio::awaitable<FCMBalanceSummary> CoinbaseAwaitableRestClient::get_futures_balance_summary() const {
    co_return sync_client_->get_futures_balance_summary();
}

asio::awaitable<std::vector<FCMPosition>> CoinbaseAwaitableRestClient::list_futures_positions() const {
    co_return sync_client_->list_futures_positions();
}

asio::awaitable<FCMPosition> CoinbaseAwaitableRestClient::get_futures_position(std::string_view product_id) const {
    co_return sync_client_->get_futures_position(product_id);
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::schedule_futures_sweep(double usd_amount) const {
    co_return sync_client_->schedule_futures_sweep(usd_amount);
}

asio::awaitable<std::vector<FCMSweep>> CoinbaseAwaitableRestClient::list_futures_sweeps() const {
    co_return sync_client_->list_futures_sweeps();
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::cancel_pending_futures_sweep() const {
    co_return sync_client_->cancel_pending_futures_sweep();
}

asio::awaitable<std::string> CoinbaseAwaitableRestClient::get_intraday_margin_setting() const {
    co_return sync_client_->get_intraday_margin_setting();
}

asio::awaitable<CurrentMarginWindow> CoinbaseAwaitableRestClient::get_current_margin_window(std::string_view margin_profile_type) const {
    co_return sync_client_->get_current_margin_window(margin_profile_type);
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::set_intraday_margin_setting(std::string_view setting) const {
    co_return sync_client_->set_intraday_margin_setting(setting);
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::allocate_portfolio(std::string_view portfolio_uuid, std::string_view symbol, double amount, std::string_view currency) const {
    co_return sync_client_->allocate_portfolio(portfolio_uuid, symbol, amount, currency);
}

asio::awaitable<PerpsPortfolioSummaryResponse> CoinbaseAwaitableRestClient::get_perps_portfolio_summary(std::string_view portfolio_uuid) const {
    co_return sync_client_->get_perps_portfolio_summary(portfolio_uuid);
}

asio::awaitable<PerpsPositionsResponse> CoinbaseAwaitableRestClient::list_perps_positions(std::string_view portfolio_uuid) const {
    co_return sync_client_->list_perps_positions(portfolio_uuid);
}

asio::awaitable<PerpsPosition> CoinbaseAwaitableRestClient::get_perps_position(std::string_view portfolio_uuid, std::string_view symbol) const {
    co_return sync_client_->get_perps_position(portfolio_uuid, symbol);
}

asio::awaitable<std::vector<PerpsPortfolioBalance>> CoinbaseAwaitableRestClient::get_perps_portfolio_balances(std::string_view portfolio_uuid) const {
    co_return sync_client_->get_perps_portfolio_balances(portfolio_uuid);
}

asio::awaitable<bool> CoinbaseAwaitableRestClient::opt_in_or_out_multi_asset_collateral(std::string_view portfolio_uuid, bool enabled) const {
    co_return sync_client_->opt_in_or_out_multi_asset_collateral(portfolio_uuid, enabled);
}

}   // end namespace coinbase
