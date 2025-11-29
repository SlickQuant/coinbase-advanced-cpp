#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include <array>
#include <memory>
#include <slick/net/websocket.h>
#include <nlohmann/json.hpp>
#include <coinbase/market_data.h>
#include <coinbase/order.h>
#include <coinbase/position.h>
#include <coinbase/auth.h>

namespace coinbase {

using Websocket = slick::net::Websocket;
using json = nlohmann::json;

enum class WebSocketChannel : uint8_t {
    HEARTBEAT,
    LEVEL2,
    MARKET_TRADES,
    TICKER,
    USER,
    CANDLES,
    STATUS,
    TICKER_BATCH,
    FUTURES_BALANCE_SUMMARY,
    __COUNT__,  // Number of channels. For internal use only.
};

inline std::string to_string(WebSocketChannel channel) {
    switch(channel) {
    case WebSocketChannel::HEARTBEAT:
        return "heartbeat";
    case WebSocketChannel::LEVEL2:
        return "level2";
    case WebSocketChannel::MARKET_TRADES:
        return "market_trades";
    case WebSocketChannel::TICKER:
        return "ticker";
    case WebSocketChannel::USER:
        return "user";
    case WebSocketChannel::CANDLES:
        return "candles";
    case WebSocketChannel::STATUS:
        return "status";
    case WebSocketChannel::TICKER_BATCH:
        return "ticker_batch";
    case WebSocketChannel::FUTURES_BALANCE_SUMMARY:
        return "futures_balance_summary";
    case WebSocketChannel::__COUNT__:
        break;
    }
    return "UNKNOWN_CHANNEL";
}

struct WebsocketCallbacks {
    virtual void onLevel2Snapshot(const Level2UpdateBatch& snapshot) = 0;
    virtual void onLevel2Updates(const Level2UpdateBatch& updates) = 0;
    virtual void onMarketTradesSnapshot(const std::vector<MarketTrade>& snapshots) = 0;
    virtual void onMarketTrades(const std::vector<MarketTrade>& trades) = 0;
    virtual void onTickerSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) = 0;
    virtual void onTickers(uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) = 0;
    virtual void onCandlesSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) = 0;
    virtual void onCandles(uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) = 0;
    virtual void onStatusSnapshot(uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) = 0;
    virtual void onStatus(uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) = 0;
    virtual void onMarketDataGap() = 0;
    virtual void onUserDataSnapshot(uint64_t seq_num, const std::vector<Order>& orders, const std::vector<PerpetualFuturePosition>& perpetual_future_positions, const std::vector<ExpiringFuturePosition>& expiring_future_positions) = 0;
    virtual void onOrderUpdates(uint64_t seq_num, const std::vector<Order>& orders) = 0;
    virtual void onMarketDataError(std::string err) = 0;
    virtual void onUserDataError(std::string err) = 0;
};

class WebSocketClient {
public:
    WebSocketClient(
        WebsocketCallbacks *callbacks,
        std::string_view market_data_url = "wss://advanced-trade-ws.coinbase.com",
        std::string_view user_data_url = "wss://advanced-trade-ws-user.coinbase.com");
    ~WebSocketClient();

    void subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);
    void unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);

private:
    void onData(const char* data, std::size_t size);
    void onMarketDataError(std::string err);
    void onUserDataError(std::string err);
    void checkMarketDataSequenceNumber(int64_t seq_num);
    void checkUserDataSequenceNumber(int64_t seq_num);
    void reconnectMarketData();
    void reconnectUserData();
    void processLevel2Update(const json& j);
    void processMarketTrades(const json& j);
    void processCandles(const json& j);
    void processTicker(const json& j);
    void processUserEvent(const json& j);
    void processHeartbeat(const json& j);
    void processStatus(const json& j);
    void processFuturesBalanceSummary(const json& j);

private:
    WebsocketCallbacks *callbacks_;
    std::string market_data_url_;
    std::string user_data_url_;
    std::shared_ptr<Websocket> market_data_websocket_;
    std::shared_ptr<Websocket> user_data_websocket_;
    int64_t last_md_seq_num_ = -1;
    int64_t last_user_seq_num_ = -1;
    std::array<std::unordered_set<std::string>, static_cast<uint8_t>(WebSocketChannel::__COUNT__)> product_ids_;
    std::vector<std::string> pending_subscriptions_;
    std::string user_id_;
};


inline WebSocketClient::WebSocketClient(
    WebsocketCallbacks *callbacks,
    std::string_view market_data_url,
    std::string_view user_data_url)
    : callbacks_(callbacks)
    , market_data_url_(market_data_url)
    , user_data_url_(user_data_url)
    , market_data_websocket_(std::make_shared<Websocket>(
        market_data_url_,
        [this]() { LOG_INFO("Market data connected"); },
        [this]() { LOG_INFO("Market data disconnected"); },
        [this](const char* data, std::size_t size) { onData(data, size); },
        [this](std::string err) { onMarketDataError(err); }))
    , user_data_websocket_(std::make_shared<Websocket>(
        user_data_url_, 
        [this]() { LOG_INFO("User data connected"); },
        [this]() { LOG_INFO("User data disconnected"); },
        [this](const char* data, std::size_t size) { onData(data, size); },
        [this](std::string err) { onUserDataError(err); }))
{
}

inline WebSocketClient::~WebSocketClient() {
    LOG_INFO("WebSocketClient destructor called");
    if (market_data_websocket_) {
        LOG_INFO("Closing market data websocket");
        market_data_websocket_->close();
        market_data_websocket_.reset();
    }
    if (user_data_websocket_) {
        LOG_INFO("Closing user data websocket");
        user_data_websocket_->close();
        user_data_websocket_.reset();
    }
}

inline void WebSocketClient::subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto products = product_ids_[static_cast<uint8_t>(channel)];
        auto subscribe_json = json{{"type", "subscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        std::shared_ptr<Websocket> websocket;
        if (channel == WebSocketChannel::USER) {
            websocket = user_data_websocket_;
            subscribe_json["jwt"] = generate_coinbase_jwt(user_data_url_.c_str());
        }
        else {
            websocket = market_data_websocket_;
        }
        if (websocket->status() > Websocket::Status::CONNECTED) {
            websocket->open();
        }
        auto subscribe_str = subscribe_json.dump();
        websocket->send(subscribe_str.c_str(), subscribe_str.size());
        if (channel == WebSocketChannel::HEARTBEAT) {
            if (market_data_websocket_->status() > Websocket::Status::CONNECTED) {
                market_data_websocket_->open();
            }
            market_data_websocket_->send(subscribe_str.c_str(), subscribe_str.size());
        }
    }
}

inline void WebSocketClient::unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto websocket = channel == WebSocketChannel::USER ? user_data_websocket_ : market_data_websocket_;
        auto unsubscribe_json = json{{"type", "unsubscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        auto unsubscribe_str = unsubscribe_json.dump();
        if (websocket->status() > Websocket::Status::CONNECTED) {
            websocket->send(unsubscribe_str.c_str(), unsubscribe_str.size());
        }
        if (channel == WebSocketChannel::HEARTBEAT) {
            if (market_data_websocket_->status() > Websocket::Status::CONNECTED) {
                market_data_websocket_->send(unsubscribe_str.c_str(), unsubscribe_str.size());
            }
        }
    }
}

inline void WebSocketClient::onData(const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        auto channel = j["channel"];
        if (channel == "l2_data") {
            checkMarketDataSequenceNumber(j["sequence_num"]);
            processLevel2Update(j);
        }
        else if (channel == "ticker" || channel == "ticker_batch") {
            checkMarketDataSequenceNumber(j["sequence_num"]);
            processTicker(j);
        }
        else if (channel == "market_trades") {
            checkMarketDataSequenceNumber(j["sequence_num"]);
            processMarketTrades(j);
        }
        else if (channel == "candles") {
            checkMarketDataSequenceNumber(j["sequence_num"]);
            processCandles(j);
        }
        else if (channel == "status") {
            checkMarketDataSequenceNumber(j["sequence_num"]);
            processStatus(j);
        }
        else if (channel == "user") {
            checkUserDataSequenceNumber(j["sequence_num"].get<uint64_t>());
            processUserEvent(j);
        }
        else if (channel == "subscriptions") {
            for (auto &event : j["events"]) {
                auto &subscriptions = event["subscriptions"];
                for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it) {
                    if (it.key() == "user") {
                        checkUserDataSequenceNumber(j["sequence_num"].get<uint64_t>());
                    }
                    else {
                        checkMarketDataSequenceNumber(j["sequence_num"].get<uint64_t>());
                    }
                }
            }
        }
        else if (channel == "heartbeat") {
            processHeartbeat(j);
        }
        else if (channel == "futures_balance_summary") {
            checkUserDataSequenceNumber(j["sequence_num"]);
        }
        else {
            LOG_ERROR("unknown channel: {}", to_string(channel));
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("error: {}. data: {}", e.what(), std::string_view(data, size));
    }
}

inline void WebSocketClient::processLevel2Update(const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onLevel2Snapshot(event);
        }
        else if (event["type"] == "update") {
            callbacks_->onLevel2Updates(event);
        }
        else {
            LOG_WARN("unknown l2_data event type: {}", event["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processTicker(const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onTickerSnapshot(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["tickers"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onTickers(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["tickers"]);
        }
        else {
            LOG_WARN("unknown ticker event type: {}", j["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processMarketTrades(const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onMarketTradesSnapshot(event["trades"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onMarketTrades(event["trades"]);
        }
        else {
            LOG_WARN("unknown market_trades event type: {}", event["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processCandles(const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onCandlesSnapshot(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["candles"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onCandles(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["candles"]);
        }
        else {
            LOG_WARN("unknown candles event type: {}", j["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processStatus(const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onStatusSnapshot(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["products"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onStatus(j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["products"]);
        }
        else {
            LOG_WARN("unknown status event type: {}", j["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processUserEvent(const json &j) {
    for (auto& event : j["events"]) {
        if (event["type"] == "snapshot") {
            auto &positions = event.at("positions");
            std::vector<Order> orders;
            for (auto& order : event.at("orders")) {
                orders.push_back({});
                from_snapshot(order, orders.back());
            }
            callbacks_->onUserDataSnapshot(j.at("sequence_num").get<uint64_t>(), orders, positions.at("perpetual_futures_positions"), positions.at("expiring_futures_positions"));
        }
        else if (event["type"] == "update") {
            callbacks_->onOrderUpdates(j.at("sequence_num").get<uint64_t>(), j.at("orders"));
        }
        else {
            LOG_WARN("unknown user event type: {}", j["type"].get<std::string_view>());
        }
    }
}

inline void WebSocketClient::processHeartbeat(const json& j) {
    
}

inline void WebSocketClient::checkMarketDataSequenceNumber(int64_t seq_num) {
    if (seq_num != last_md_seq_num_ + 1) {
        LOG_ERROR("market data message lost. seq_num: {}, last_md_seq_num: {}", seq_num, last_md_seq_num_);
        callbacks_->onMarketDataGap();
        reconnectMarketData();
        return;
    }
    last_md_seq_num_ = seq_num; 
}

inline void WebSocketClient::checkUserDataSequenceNumber(int64_t seq_num) {
    if (seq_num != last_user_seq_num_ + 1) {
        LOG_ERROR("user data message lost. seq_num: {}, last_user_seq_num: {}", seq_num, last_user_seq_num_);
        reconnectUserData();
        return;
    }
    last_user_seq_num_ = seq_num;
}

inline void WebSocketClient::reconnectMarketData() {
    market_data_websocket_->close();
    market_data_websocket_ = std::make_shared<Websocket>(
        market_data_url_,
        [this]() { LOG_INFO("Market data connected"); },
        [this]() { LOG_INFO("Market data disconnected"); },
        [this](const char* data, std::size_t size) { onData(data, size); },
        [this](std::string err) { onMarketDataError(err); });
    last_md_seq_num_ = -1;
}

inline void WebSocketClient::reconnectUserData() {
    user_data_websocket_->close();
    user_data_websocket_ = std::make_shared<Websocket>(
        user_data_url_,
        [this]() { LOG_INFO("User data connected"); },
        [this]() { LOG_INFO("User data disconnected"); },
        [this](const char* data, std::size_t size) { onData(data, size); },
        [this](std::string err) { onUserDataError(err); });
    last_user_seq_num_ = -1;
}

inline void WebSocketClient::onMarketDataError(std::string err) {
    LOG_ERROR("market data error: {}", err);
    callbacks_->onMarketDataError(err);
    reconnectMarketData();
}

inline void WebSocketClient::onUserDataError(std::string err) {
    LOG_ERROR("user data error: {}", err);
    callbacks_->onUserDataError(err);
    reconnectUserData();
}

}  // end namespace coinbase