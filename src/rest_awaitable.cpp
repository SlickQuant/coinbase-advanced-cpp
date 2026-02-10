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

}   // end namespace coinbase
