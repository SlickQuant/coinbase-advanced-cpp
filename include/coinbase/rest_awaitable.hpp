// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <boost/asio/awaitable.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include <coinbase/product.hpp>
#include <coinbase/account.hpp>
#include <coinbase/order.hpp>
#include <coinbase/fill.hpp>
#include <coinbase/price_book.hpp>
#include <coinbase/trades.hpp>
#include <coinbase/candle.hpp>

using json = nlohmann::json;
namespace asio = boost::asio;

namespace coinbase {

class CoinbaseRestClient;

class CoinbaseAwaitableRestClient
{
public:
    CoinbaseAwaitableRestClient(std::string base_url = "https://api.coinbase.com");
    ~CoinbaseAwaitableRestClient();
    CoinbaseAwaitableRestClient(const CoinbaseAwaitableRestClient& other);
    CoinbaseAwaitableRestClient& operator=(const CoinbaseAwaitableRestClient& other);
    CoinbaseAwaitableRestClient(CoinbaseAwaitableRestClient&& other) noexcept;
    CoinbaseAwaitableRestClient& operator=(CoinbaseAwaitableRestClient&& other) noexcept;

    void set_base_url(std::string_view url);
    std::string_view base_url() const noexcept { return base_url_; }

    asio::awaitable<std::vector<Account>> list_accounts(const AccountQueryParams &params = {}) const;
    asio::awaitable<Account> get_account(std::string_view account_uuid) const;

    asio::awaitable<std::vector<Product>> list_products(const ProductQueryParams &params = {}) const;
    asio::awaitable<Product> get_product(std::string_view product_id, bool get_tradability_status = false) const;

    asio::awaitable<std::vector<Product>> list_public_products(const ProductQueryParams &params = {}) const;
    asio::awaitable<Product> get_public_product(std::string_view product_id) const;

    asio::awaitable<std::vector<Order>> list_orders(const OrderQueryParams &query = {}) const;
    asio::awaitable<Order> get_order(std::string_view order_id) const;

    asio::awaitable<std::vector<Fill>> list_fills(const FillQueryParams &params = {}) const;

    asio::awaitable<uint64_t> get_server_time() const;

    asio::awaitable<std::vector<PriceBook>> get_best_bid_ask(const std::vector<std::string> &product_ids) const;
    asio::awaitable<PriceBookResponse> get_product_book(const PriceBookQueryParams &params) const;
    asio::awaitable<MarketTrades> get_market_trades(std::string_view product_id, const MarketTradesQueryParams &params) const;
    asio::awaitable<std::vector<Candle>> get_product_candles(std::string_view product_id, const ProductCandlesQueryParams &params) const;

    asio::awaitable<CreateOrderResponse> create_order(
        std::string &&client_order_id,
        std::string &&product_id,
        Side side,
        OrderType order_type,
        TimeInForce time_in_force,
        double size,
        double price = NAN,
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

    asio::awaitable<ModifyOrderResponse> modify_order(
        std::string order_id,
        std::string product_id,
        double price,
        double size,
        std::optional<double> stop_price = {},
        std::optional<double> take_profit_price = {},
        std::optional<bool> cancel_attached_order = {}
    ) const;

    asio::awaitable<std::vector<CancelOrderResponse>> cancel_orders(const std::vector<std::string_view> &order_ids) const;
    
    static const Product& product(std::string_view product_id);
private:
    std::string base_url_;
    std::string domain_;
    std::unique_ptr<CoinbaseRestClient> sync_client_;
};

}   // end namespace coinbase
