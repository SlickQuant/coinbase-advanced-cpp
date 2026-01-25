// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include <array>
#include <memory>
#include <fstream>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <slick/net/websocket.h>
#include <nlohmann/json.hpp>
#include <coinbase/market_data.hpp>
#include <coinbase/order.hpp>
#include <coinbase/position.hpp>
#include <coinbase/auth.hpp>
#include <coinbase/candle.hpp>
#include <slick/queue.h>

namespace coinbase {

using Websocket = slick::net::Websocket;
using json = nlohmann::json;

enum WebSocketChannel : uint8_t {
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

class WebSocketClient;

struct WebsocketCallbacks {
    virtual ~WebsocketCallbacks() = default;
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
    virtual void onUserDataGap() = 0;
    virtual void onUserDataSnapshot(uint64_t seq_num, const std::vector<Order>& orders, const std::vector<PerpetualFuturePosition>& perpetual_future_positions, const std::vector<ExpiringFuturePosition>& expiring_future_positions) = 0;
    virtual void onOrderUpdates(uint64_t seq_num, const std::vector<Order>& orders) = 0;
    virtual void onMarketDataError(std::string err) = 0;
    virtual void onUserDataError(std::string err) = 0;
};

struct DataHandler {
    virtual ~DataHandler() = default;

    bool processMarketData(void *ws_client, const char* data, std::size_t size);
    bool processUserData(void *ws_client, const char* data, std::size_t size);
    void onMarketDataError(std::string err);
    void onUserDataError(std::string err);
    void processLevel2Update(const json& j);
    void processMarketTrades(const json& j);
    void processCandles(const json& j);
    void processTicker(const json& j);
    void processUserEvent(const json& j);
    bool processHeartbeat(const json& j);
    void processStatus(const json& j);
    void processFuturesBalanceSummary(const json& j);
    
    virtual bool checkMarketDataSequenceNumber(void *ws_client, int64_t seq_num);
    virtual bool checkUserDataSequenceNumber(void *ws_client, int64_t seq_num);
    virtual void resetMarketDataSequence(void [[maybe_unused]] *ws_client) {
        last_md_seq_num_ = -1;
    }
    virtual void resetUserDataSequence(void [[maybe_unused]] *ws_client) {
        last_user_seq_num_ = -1;
    }

protected:
    friend class WebSocketClient;
    WebsocketCallbacks* callbacks_ = nullptr;
    int64_t last_md_seq_num_ = -1;
    int64_t last_user_seq_num_ = -1;
};

struct UserThreadWebsocketCallbacks : public DataHandler, public WebsocketCallbacks
{
    UserThreadWebsocketCallbacks(uint32_t queue_size = 16777216)
        : data_queue_(queue_size)
    {
        callbacks_ = this;
    }

    ~UserThreadWebsocketCallbacks() override = default;
    
    // Process data in the user thread. Callbacks will be invoked in the user thread.
    void processData();

private:
    bool checkMarketDataSequenceNumber(void *ws_client, int64_t seq_num) override {
        auto it = md_seq_nums_.find(ws_client);
        if (it == md_seq_nums_.end()) [[unlikely]] {
            it = md_seq_nums_.emplace(ws_client, -1).first;
        }
        auto &seq_atomic = it->second;
        auto last_seq = seq_atomic.load(std::memory_order_acquire);
        if (seq_num != last_seq + 1) {
            LOG_ERROR("market data message lost. seq_num: {}, last_md_seq_num: {}", seq_num, last_seq);
            callbacks_->onMarketDataGap();
            return false;
        }
        seq_atomic.store(seq_num, std::memory_order_release);
        return true;
    }

    bool checkUserDataSequenceNumber(void *ws_client, int64_t seq_num) override {
        auto it = user_seq_nums_.find(ws_client);
        if (it == user_seq_nums_.end()) [[unlikely]] {
            it = user_seq_nums_.emplace(ws_client, -1).first;
        }
        auto &seq_atomic = it->second;
        auto last_seq = seq_atomic.load(std::memory_order_acquire);
        if (seq_num != last_seq + 1) {
            LOG_ERROR("user data message lost. seq_num: {}, last_user_seq_num: {}", seq_num, last_seq);
            callbacks_->onUserDataGap();
            return false;
        }
        seq_atomic.store(seq_num, std::memory_order_release);
        return true;
    }

    void resetMarketDataSequence(void *ws_client) override {
        md_seq_nums_[ws_client].store(-1, std::memory_order_release);
    }

    void resetUserDataSequence(void *ws_client) override {
        user_seq_nums_[ws_client].store(-1, std::memory_order_release);
    }

private:
    friend class WebSocketClient;
    slick::SlickQueue<char> data_queue_;
    uint64_t read_cursor_ = 0;
    std::unordered_map<void*, std::atomic_int_fast64_t> md_seq_nums_;
    std::unordered_map<void*, std::atomic_int_fast64_t> user_seq_nums_;
};


class WebSocketClient {
public:
    WebSocketClient(
        WebsocketCallbacks *callbacks,
        std::string_view market_data_url = "wss://advanced-trade-ws.coinbase.com",
        std::string_view user_data_url = "wss://advanced-trade-ws-user.coinbase.com");
    ~WebSocketClient();

    bool isMarketDataConnected() const {
        return market_data_websocket_ && market_data_websocket_->status() == Websocket::Status::CONNECTED;
    }
    bool isUserDataConnected() const {
        return user_data_websocket_ && user_data_websocket_->status() == Websocket::Status::CONNECTED;
    }
    void subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);
    void unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);
    void logData(std::string_view data_file, uint32_t queue_size = 16777216);

private:
    void onMarketData(const char* data, std::size_t size);
    void onUserData(const char* data, std::size_t size);
    void onMarketDataError(std::string err);
    void onUserDataError(std::string err);
    void reconnectMarketData();
    void reconnectUserData();
    void dispatchData(const char* data, std::size_t size, char channel);
    void runDataLogger();

private:
    friend class UserThreadWebsocketCallbacks;
    DataHandler* data_handler_ = nullptr;
    std::string market_data_url_;
    std::string user_data_url_;
    std::shared_ptr<Websocket> market_data_websocket_;
    std::shared_ptr<Websocket> user_data_websocket_;
    std::array<std::unordered_set<std::string>, static_cast<uint8_t>(WebSocketChannel::__COUNT__)> product_ids_;
    std::vector<std::string> pending_subscriptions_;
    std::string user_id_;
    UserThreadWebsocketCallbacks *user_thread_callbacks_ = nullptr;
    std::fstream data_log_;
    slick::SlickQueue<char> *data_queue_ = nullptr;
    std::thread logger_thread_;
    std::atomic_bool logger_run_ = false;
    uint64_t data_cursor_ = 0;
};



inline void UserThreadWebsocketCallbacks::processData() {
    auto [data_ptr, data_size] = data_queue_.read(read_cursor_);
    if (data_ptr && data_size > 0) {
        void* client = nullptr;
        memcpy(&client, data_ptr, sizeof(void*));
        char channel = data_ptr[sizeof(void*)];
        data_ptr += sizeof(void*) + 1;
        if (channel == 'U') {
            processUserData(client, data_ptr, data_size - sizeof(void*) - 1);
        }
        else {
            processMarketData(client, data_ptr, data_size - sizeof(void*) - 1);
        }
    }
}


inline WebSocketClient::WebSocketClient(
    WebsocketCallbacks *callbacks,
    std::string_view market_data_url,
    std::string_view user_data_url)
    : market_data_url_(market_data_url)
    , user_data_url_(user_data_url)
    , user_thread_callbacks_(dynamic_cast<UserThreadWebsocketCallbacks*>(callbacks))
{
    if (user_thread_callbacks_) {
        data_queue_ = &user_thread_callbacks_->data_queue_;
        data_handler_ = user_thread_callbacks_;
    }
    else {
        data_handler_ = new DataHandler();
        data_handler_->callbacks_ = callbacks;
    }

    if (!market_data_url.empty()) {
        market_data_websocket_ = std::make_shared<Websocket>(
            market_data_url_,
            [this]() { LOG_INFO("Market data connected"); },
            [this]() { LOG_INFO("Market data disconnected"); },
            [this](const char* data, std::size_t size) { onMarketData(data, size); },
            [this](std::string err) { onMarketDataError(err); }
        );
    }

    if (!user_data_url.empty()) {
        user_data_websocket_ = std::make_shared<Websocket>(
            user_data_url_,
            [this]() { LOG_INFO("User data connected"); },
            [this]() { LOG_INFO("User data disconnected"); },
            [this](const char* data, std::size_t size) { onUserData(data, size); },
            [this](std::string err) { onUserDataError(err); }
        );
    }
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

    logger_run_.store(false, std::memory_order_release);
    if (logger_thread_.joinable()) {
        logger_thread_.join();
    }
    if (data_queue_ && !user_thread_callbacks_) {
        delete data_queue_;
        data_queue_ = nullptr;
    }

    if (!user_thread_callbacks_) {
        delete data_handler_;
        data_handler_ = nullptr;
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

        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized.", to_string(channel));
            continue;
        }

        if (websocket->status() > Websocket::Status::CONNECTED) {
            websocket->open();
        }
        auto subscribe_str = subscribe_json.dump();
        websocket->send(subscribe_str.c_str(), subscribe_str.size());
        if (channel == WebSocketChannel::HEARTBEAT && user_data_websocket_) {
            if (user_data_websocket_->status() > Websocket::Status::CONNECTED) {
                user_data_websocket_->open();
            }
            user_data_websocket_->send(subscribe_str.c_str(), subscribe_str.size());
        }
    }
}

inline void WebSocketClient::unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto websocket = channel == WebSocketChannel::USER ? user_data_websocket_ : market_data_websocket_;
        auto unsubscribe_json = json{{"type", "unsubscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        auto unsubscribe_str = unsubscribe_json.dump();
        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized.", to_string(channel));
            continue;
        }
        if (websocket->status() > Websocket::Status::CONNECTED) {
            websocket->send(unsubscribe_str.c_str(), unsubscribe_str.size());
        }
        if (channel == WebSocketChannel::HEARTBEAT) {
            if (user_data_websocket_ && user_data_websocket_->status() > Websocket::Status::CONNECTED) {
                user_data_websocket_->send(unsubscribe_str.c_str(), unsubscribe_str.size());
            }
        }
    }
}

inline void WebSocketClient::logData(std::string_view data_file, uint32_t data_queue_size) {
    data_log_.open(data_file, std::ios::out | std::ios::ate);
    if (data_log_.is_open()) {
        if (user_thread_callbacks_) {
            data_queue_ = &user_thread_callbacks_->data_queue_;
        }
        else {
            data_queue_ = new slick::SlickQueue<char>(data_queue_size);
        }
        logger_run_.store(true, std::memory_order_release);
        logger_thread_ = std::thread([this](){
            runDataLogger();
        });
    }
    else {
        LOG_ERROR("Failed to open data_file {}.", data_file);
    }
}

inline void WebSocketClient::dispatchData(const char* data, std::size_t size, char channel) {
    assert(data_queue_);
    auto sz = sizeof(void*) + size + 1;
    auto index = data_queue_->reserve(sz);
    auto dest = (*data_queue_)[index];
    void *self = this;
    memcpy(dest, &self, sizeof(void*));
    dest[sizeof(void*)] = channel;
    memcpy(dest + sizeof(void*) + 1, data, size);
    data_queue_->publish(index, sz);
}

inline void WebSocketClient::runDataLogger() {
    if (!data_queue_ || !data_log_.is_open()) {
        return;
    }

    while (logger_run_.load(std::memory_order_relaxed)) {
        auto [data, size] = data_queue_->read(data_cursor_);
        if (data) {
            data += sizeof(void*) + 1;     // skip client and channel identifier
            data_log_.write(data, size - sizeof(void*) - 1);
            data_log_ << std::endl;
        }
    }

    // drain data queue
    while (true) {
        auto [data, size] = data_queue_->read(data_cursor_);
        if (!data) {
            break;
        }
        ++data;     // skip channel identifier
        data_log_.write(data, size - 1);
        data_log_ << std::endl;
    }
}

inline void WebSocketClient::onMarketData(const char* data, std::size_t size) {
    if (user_thread_callbacks_) {
        dispatchData(data, size, 'M');
    }
    else {
        if (!data_handler_->processMarketData(this, data, size)) {
            reconnectMarketData();
        }
        // If data_queue_ is not nullptr, data logger is enabled.
        // dispatch data for logging
        if (data_queue_) {
            dispatchData(data, size, 'M');
        }
    }
}

inline void WebSocketClient::onUserData(const char* data, std::size_t size) {
    if (user_thread_callbacks_) {
        dispatchData(data, size, 'U');
    }
    else {
        if (!data_handler_->processUserData(this, data, size)) {
            reconnectUserData();
        }
        // If data_queue_ is not nullptr, data logger is enabled.
        // dispatch data for logging
        if (data_queue_) {
            dispatchData(data, size, 'M');
        }
    }
}

inline void WebSocketClient::reconnectMarketData() {
    market_data_websocket_->close();
    market_data_websocket_ = std::make_shared<Websocket>(
        market_data_url_,
        [this]() { LOG_INFO("Market data connected"); },
        [this]() { LOG_INFO("Market data disconnected"); },
        [this](const char* data, std::size_t size) { onMarketData(data, size); },
        [this](std::string err) { onMarketDataError(err); });
    data_handler_->resetMarketDataSequence(this);
}

inline void WebSocketClient::reconnectUserData() {
    user_data_websocket_->close();
    user_data_websocket_ = std::make_shared<Websocket>(
        user_data_url_,
        [this]() { LOG_INFO("User data connected"); },
        [this]() { LOG_INFO("User data disconnected"); },
        [this](const char* data, std::size_t size) { onUserData(data, size); },
        [this](std::string err) { onUserDataError(err); });
    data_handler_->resetUserDataSequence(this);
}

inline void WebSocketClient::onMarketDataError(std::string err) {
    LOG_ERROR("market data error: {}", err);
    data_handler_->callbacks_->onMarketDataError(err);
    reconnectMarketData();
}

inline void WebSocketClient::onUserDataError(std::string err) {
    LOG_ERROR("user data error: {}", err);
    data_handler_->callbacks_->onUserDataError(err);
    reconnectUserData();
}

inline bool DataHandler::processMarketData(void *ws_client, const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        if (!checkMarketDataSequenceNumber(ws_client, j["sequence_num"])) {
            return false;
        }
        auto channel = j["channel"];
        if (channel == "l2_data") {
            processLevel2Update(j);
        }
        else if (channel == "ticker" || channel == "ticker_batch") {
            processTicker(j);
        }
        else if (channel == "market_trades") {
            processMarketTrades(j);
        }
        else if (channel == "candles") {
            processCandles(j);
        }
        else if (channel == "status") {
            processStatus(j);
        }
        else if (channel == "subscriptions") {
        }
        else if (channel == "heartbeat") {
            processHeartbeat(j);
        }
        else {
            LOG_ERROR("unknown channel: {}", to_string(channel));
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("error: {}. data: {}", e.what(), std::string_view(data, size));
    }
    return true;
}

inline bool DataHandler::processUserData(void *ws_client, const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        if (!checkUserDataSequenceNumber(ws_client, j["sequence_num"])) {
            return false;
        }

        auto channel = j["channel"];
        if (channel == "user") {
            processUserEvent(j);
        }
        else if (channel == "subscriptions") {
        }
        else if (channel == "heartbeat") {
            processHeartbeat(j);
        }
        else if (channel == "futures_balance_summary") {
        }
        else {
            LOG_ERROR("unknown channel: {}", to_string(channel));
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("error: {}. data: {}", e.what(), std::string_view(data, size));
    }
    return true;
}

inline void DataHandler::processLevel2Update(const json &j) {
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

inline void DataHandler::processTicker(const json &j) {
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

inline void DataHandler::processMarketTrades(const json &j) {
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

inline void DataHandler::processCandles(const json &j) {
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

inline void DataHandler::processStatus(const json &j) {
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

inline void DataHandler::processUserEvent(const json &j) {
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

inline bool DataHandler::processHeartbeat(const json& j) {
    return true;
}

inline bool DataHandler::checkMarketDataSequenceNumber([[maybe_unused]] void* client, int64_t seq_num) {
    if (seq_num != last_md_seq_num_ + 1) {
        LOG_ERROR("market data message lost. seq_num: {}, last_md_seq_num: {}", seq_num, last_md_seq_num_);
        callbacks_->onMarketDataGap();
        return false;
    }
    last_md_seq_num_ = seq_num;
    return true;
}

inline bool DataHandler::checkUserDataSequenceNumber([[maybe_unused]] void* client, int64_t seq_num) {
    if (seq_num != last_user_seq_num_ + 1) {
        LOG_ERROR("user data message lost. seq_num: {}, last_user_seq_num: {}", seq_num, last_user_seq_num_);
        callbacks_->onUserDataGap();
        return false;
    }
    last_user_seq_num_ = seq_num;
    return true;
}

}  // end namespace coinbase