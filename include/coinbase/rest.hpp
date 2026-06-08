// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <coinbase/product.hpp>
#include <coinbase/account.hpp>
#include <coinbase/order.hpp>
#include <coinbase/fill.hpp>
#include <coinbase/price_book.hpp>
#include <coinbase/trades.hpp>
#include <coinbase/candle.hpp>
#include <coinbase/portfolio.hpp>
#include <coinbase/convert.hpp>
#include <coinbase/payment_method.hpp>
#include <coinbase/key_permissions.hpp>
#include <coinbase/futures.hpp>
#include <coinbase/perpetuals.hpp>

using json = nlohmann::json;

namespace coinbase {

class CoinbaseRestClient
{
public:
    CoinbaseRestClient(std::string base_url = "https://api.coinbase.com");
    ~CoinbaseRestClient() = default;

    void set_base_url(std::string_view url);
    std::string_view base_url() const noexcept { return base_url_; }

    std::vector<Account> list_accounts(const AccountQueryParams &params = {}) const;
    Account get_account(std::string_view account_uuid) const;

    std::vector<Product> list_products(const ProductQueryParams &params = {}) const;
    Product get_product(std::string_view product_id, bool get_tradability_status = false) const;

    std::vector<Product> list_public_products(const ProductQueryParams &params = {}) const;
    Product get_public_product(std::string_view product_id) const;

    std::vector<Order> list_orders(const OrderQueryParams &query = {}) const;
    Order get_order(std::string_view order_id) const;

    std::vector<Fill> list_fills(const FillQueryParams &params = {}) const;

    double get_taker_fee_rate() const;
    double get_maker_fee_rate() const;

    uint64_t get_server_time() const;

    std::vector<PriceBook> get_best_bid_ask(const std::vector<std::string> &product_ids) const;
    PriceBookResponse get_product_book(const PriceBookQueryParams &params) const;
    MarketTrades get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const;
    std::vector<Candle> get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const;

    CreateOrderResponse create_order(
        std::string &&client_order_id,
        std::string &&product_id,
        Side side,
        OrderType order_type,
        TimeInForce time_in_force,
        double size,
        double limit_price = NAN,
        bool post_only = true,
        bool size_in_quote = false,
        std::optional<double> stop_price = {},
        std::optional<double> take_profit_price = {},
        std::optional<uint64_t> end_time = {},
        std::optional<uint64_t> twap_start_time = {},
        std::optional<SorPreference> &&sor_preference = {},
        std::optional<double> &&leverage = {},
        std::optional<MarginType> &&margin_type = {},
        std::optional<json> &&attached_order_configuration = {},
        std::optional<PredictionMetadata> &&prediction_metadata = {}
    ) const;

    ModifyOrderResponse modify_order(
        std::string order_id,
        std::string product_id,
        double price,
        double size,
        std::optional<double> stop_price = {},
        std::optional<double> take_profit_price = {},
        std::optional<bool> cancel_attached_order = {}
    ) const;

    std::vector<CancelOrderResponse> cancel_orders(const std::vector<std::string_view> &order_ids) const;

    // Portfolios
    std::vector<Portfolio> list_portfolios(std::optional<PortfolioType> portfolio_type = {}) const;
    Portfolio create_portfolio(std::string_view name) const;
    PortfolioBreakdown get_portfolio_breakdown(std::string_view portfolio_uuid, std::optional<std::string_view> currency = {}) const;
    MovePortfolioFundsResult move_portfolio_funds(double value, std::string_view currency, std::string_view source_portfolio_uuid, std::string_view target_portfolio_uuid) const;
    Portfolio edit_portfolio(std::string_view portfolio_uuid, std::string_view name) const;
    bool delete_portfolio(std::string_view portfolio_uuid) const;

    // Convert
    ConvertTrade create_convert_quote(std::string_view from_account, std::string_view to_account, double amount) const;
    ConvertTrade get_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const;
    ConvertTrade commit_convert_trade(std::string_view trade_id, std::string_view from_account, std::string_view to_account) const;

    // Payment Methods
    std::vector<PaymentMethod> list_payment_methods() const;
    PaymentMethod get_payment_method(std::string_view payment_method_id) const;

    // Data API
    ApiKeyPermissions get_api_key_permissions() const;

    // Futures (CFM)
    FCMBalanceSummary get_futures_balance_summary() const;
    std::vector<FCMPosition> list_futures_positions() const;
    FCMPosition get_futures_position(std::string_view product_id) const;
    bool schedule_futures_sweep(double usd_amount) const;
    std::vector<FCMSweep> list_futures_sweeps() const;
    bool cancel_pending_futures_sweep() const;
    std::string get_intraday_margin_setting() const;
    CurrentMarginWindow get_current_margin_window(std::string_view margin_profile_type) const;
    bool set_intraday_margin_setting(std::string_view setting) const;

    // Perpetuals (INTX)
    bool allocate_portfolio(std::string_view portfolio_uuid, std::string_view symbol, double amount, std::string_view currency) const;
    PerpsPortfolioSummaryResponse get_perps_portfolio_summary(std::string_view portfolio_uuid) const;
    PerpsPositionsResponse list_perps_positions(std::string_view portfolio_uuid) const;
    PerpsPosition get_perps_position(std::string_view portfolio_uuid, std::string_view symbol) const;
    std::vector<PerpsPortfolioBalance> get_perps_portfolio_balances(std::string_view portfolio_uuid) const;
    bool opt_in_or_out_multi_asset_collateral(std::string_view portfolio_uuid, bool enabled) const;

    static const Product& product(std::string_view product_id);
private:
    std::string base_url_;
    std::string domain_;
    static std::once_flag initialize_products_;
    static std::unordered_map<std::string, Product> products_;
};

}   // end namespace coinbase
