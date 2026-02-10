// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/websocket.hpp>

namespace coinbase {

std::string to_string(WebSocketChannel channel) {
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

// UserThreadWebsocketCallbacks implementation
bool UserThreadWebsocketCallbacks::checkMarketDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num) {
    auto it = md_seq_nums_.find(ws_client);
    if (it == md_seq_nums_.end()) [[unlikely]] {
        it = md_seq_nums_.emplace(ws_client, seq_num - 1).first;
    }
    auto &seq_atomic = it->second;
    auto last_seq = seq_atomic.load(std::memory_order_acquire);
    if (seq_num != last_seq + 1) {
        LOG_ERROR("market data message lost. seq_num: {}, last_md_seq_num: {}", seq_num, last_seq);
        callbacks_->onMarketDataGap(ws_client);
        return false;
    }
    seq_atomic.store(seq_num, std::memory_order_release);
    return true;
}

bool UserThreadWebsocketCallbacks::checkUserDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num) {
    auto it = user_seq_nums_.find(ws_client);
    if (it == user_seq_nums_.end()) [[unlikely]] {
        it = user_seq_nums_.emplace(ws_client, seq_num - 1).first;
    }
    auto &seq_atomic = it->second;
    auto last_seq = seq_atomic.load(std::memory_order_acquire);
    if (seq_num != last_seq + 1) {
        LOG_ERROR("user data message lost. seq_num: {}, last_user_seq_num: {}", seq_num, last_seq);
        callbacks_->onUserDataGap(ws_client);
        return false;
    }
    seq_atomic.store(seq_num, std::memory_order_release);
    return true;
}

void UserThreadWebsocketCallbacks::resetMarketDataSequence(WebSocketClient *ws_client) {
    md_seq_nums_.erase(ws_client);
}

void UserThreadWebsocketCallbacks::resetUserDataSequence(WebSocketClient *ws_client) {
    user_seq_nums_.erase(ws_client);
}

void UserThreadWebsocketCallbacks::processData(uint32_t max_drain_count) {
    uint32_t i = 0;
    do {
        auto [data_ptr, data_size] = data_queue_.read(read_cursor_);
        if (!data_ptr || data_size == 0) {
            break;
        }
        WebSocketClient* client = nullptr;
        memcpy(&client, data_ptr, sizeof(WebSocketClient*));
        MessageType type {data_ptr[sizeof(WebSocketClient*)]};
        data_ptr += MESSAGE_HEADER_SIZE;
        switch (type) {
            case MessageType::MARKET_CONNECTED:
                md_clients_.emplace(client);
                callbacks_->onMarketDataConnected(client);
                break;
            case MessageType::MARKET_DISCONNECTED:
                callbacks_->onMarketDataDisconnected(client);
                resetMarketDataSequence(client);
                md_clients_.erase(client);
                break;
            case MessageType::MARKET_DATA:
                if (md_clients_.contains(client)) {
                    processMarketData(client, data_ptr, data_size - MESSAGE_HEADER_SIZE);
                }
                break;
            case MessageType::USER_CONNECTED:
                user_clients_.emplace(client);
                callbacks_->onUserDataConnected(client);
                break;
            case MessageType::USER_DISCONNECTED:
                callbacks_->onUserDataDisconnected(client);
                resetUserDataSequence(client);
                user_clients_.erase(client);
                break;
            case MessageType::USER_DATA:
                if (user_clients_.contains(client)) {
                    processUserData(client, data_ptr, data_size - MESSAGE_HEADER_SIZE);
                }
                break;
            case MessageType::MARKET_ERROR:
                if (md_clients_.contains(client)) {
                    callbacks_->onMarketDataError(client, std::string(data_ptr, data_size - MESSAGE_HEADER_SIZE));
                }
                break;
            case MessageType::USER_ERROR:
                if (user_clients_.contains(client)) {
                    callbacks_->onUserDataError(client, std::string(data_ptr, data_size - MESSAGE_HEADER_SIZE));
                }
                break;
            case MessageType::MARKET_DATA_GAP:
                if (md_clients_.contains(client)) {
                    callbacks_->onMarketDataGap(client);
                }
                break;
            case MessageType::USER_DATA_GAP:
                if (user_clients_.contains(client)) {
                    callbacks_->onUserDataGap(client);
                }
                break;
        }
    }
    while(++i < max_drain_count);
}

// WebSocketClient implementation
WebSocketClient::WebSocketClient(
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
}

WebSocketClient::~WebSocketClient() {
    if (market_data_websocket_) {
        if (market_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            market_data_websocket_->close();
            if (pending_md_socket_close_.fetch_add(1, std::memory_order_acq_rel) == 0) {
                // waiting for on disconnected callback
                auto start = std::chrono::system_clock::now();
                while(pending_md_socket_close_.load(std::memory_order_acquire) == 1 &&
                      (std::chrono::system_clock::now() - start) < std::chrono::seconds(2));
            }
        }
        market_data_websocket_.reset();
    }
    if (user_data_websocket_) {
        if (user_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            user_data_websocket_->close();
            if (pending_user_socket_close_.fetch_add(1, std::memory_order_acq_rel) == 0) {
                auto start = std::chrono::system_clock::now();
                while(pending_user_socket_close_.load(std::memory_order_acquire) == 1 &&
                      (std::chrono::system_clock::now() - start) < std::chrono::seconds(2));
            }
        }
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
    }
    else {
        user_thread_callbacks_ = nullptr;
    }
    data_handler_ = nullptr;
}

void WebSocketClient::stop() {
    if (market_data_websocket_) {
        if (market_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            market_data_websocket_->close();
        }
        market_data_websocket_.reset();
    }
    if (user_data_websocket_) {
        if (user_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            user_data_websocket_->close();
        }
        user_data_websocket_.reset();
    }
}

void WebSocketClient::subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto products = product_ids_[static_cast<uint8_t>(channel)];
        auto subscribe_json = json{{"type", "subscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        std::shared_ptr<Websocket> websocket;
        if (channel == WebSocketChannel::USER) {
            if (!user_data_websocket_ && !user_data_url_.empty()) {
                user_data_websocket_ = std::make_shared<Websocket>(
                    user_data_url_,
                    [this]() { onUserDataConnected(); },
                    [this]() { onUserDataDisconnected(); },
                    [this](const char* data, std::size_t size) { onUserData(data, size); },
                    [this](std::string err) { onUserDataError(std::move(err)); }
                );
                user_data_websocket_->open();
                pending_user_socket_close_.store(0, std::memory_order_release);

                // subscribe heartbeat to keep user channel alive
                auto heartbeat_sub = json{{"type", "subscribe"}, {"channel", "heartbeat"}};
                heartbeat_sub["jwt"] = generate_coinbase_jwt(user_data_url_.c_str());
                auto subscribe_str = subscribe_json.dump();
                user_data_websocket_->send(subscribe_str.c_str(), subscribe_str.size());
            }
            websocket = user_data_websocket_;
            subscribe_json["jwt"] = generate_coinbase_jwt(user_data_url_.c_str());
        }
        else {
            if (!market_data_websocket_ && !market_data_url_.empty()) {
                market_data_websocket_ = std::make_shared<Websocket>(
                    market_data_url_,
                    [this]() { onMarketDataConnected(); },
                    [this]() { onMarketDataDisconnected(); },
                    [this](const char* data, std::size_t size) { onMarketData(data, size); },
                    [this](std::string err) { onMarketDataError(std::move(err)); }
                );
                market_data_websocket_->open();
                pending_md_socket_close_.store(0, std::memory_order_release);
            }
            websocket = market_data_websocket_;
        }

        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized.", to_string(channel));
            continue;
        }

        auto subscribe_str = subscribe_json.dump();
        websocket->send(subscribe_str.c_str(), subscribe_str.size());
    }
}

void WebSocketClient::unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto websocket = channel == WebSocketChannel::USER ? user_data_websocket_ : market_data_websocket_;
        auto unsubscribe_json = json{{"type", "unsubscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        auto unsubscribe_str = unsubscribe_json.dump();
        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized.", to_string(channel));
            continue;
        }
        if (websocket->status() <= Websocket::Status::CONNECTED) {
            websocket->send(unsubscribe_str.c_str(), unsubscribe_str.size());
        }
        if (channel == WebSocketChannel::HEARTBEAT) {
            if (user_data_websocket_ && user_data_websocket_->status() <= Websocket::Status::CONNECTED) {
                user_data_websocket_->send(unsubscribe_str.c_str(), unsubscribe_str.size());
            }
        }
    }
}

void WebSocketClient::logData(std::string_view data_file, uint32_t data_queue_size) {
    data_log_.open(std::string(data_file), std::ios::out | std::ios::ate);
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

void WebSocketClient::dispatchData(const char* data, std::size_t size, MessageType type) {
    assert(data_queue_);
    auto sz = (uint32_t)(MESSAGE_HEADER_SIZE + size);
    auto index = data_queue_->reserve(sz);
    auto dest = (*data_queue_)[index];
    void *self = this;
    memcpy(dest, &self, sizeof(WebSocketClient*));
    dest[sizeof(WebSocketClient*)] = static_cast<char>(type);
    memcpy(dest + MESSAGE_HEADER_SIZE, data, size);
    data_queue_->publish(index, sz);
}

void WebSocketClient::runDataLogger() {
    if (!data_queue_ || !data_log_.is_open()) {
        return;
    }

    while (logger_run_.load(std::memory_order_relaxed)) {
        auto [data, size] = data_queue_->read(data_cursor_);
        if (!data) {
            std::this_thread::yield();
            continue;
        }
        MessageType type {data[sizeof(WebSocketClient*)]};
        if (type == MessageType::MARKET_DATA || type == MessageType::USER_DATA) {
            data += MESSAGE_HEADER_SIZE;     // skip client and type identifier
            data_log_.write(data, size - MESSAGE_HEADER_SIZE);
            data_log_ << std::endl;
        }
    }

    // drain data queue
    while (true) {
        auto [data, size] = data_queue_->read(data_cursor_);
        if (!data) {
            break;
        }
        MessageType type {data[sizeof(WebSocketClient*)]};
        if (type == MessageType::MARKET_DATA || type == MessageType::USER_DATA) {
            data += MESSAGE_HEADER_SIZE;     // skip client and channel identifier
            data_log_.write(data, size - MESSAGE_HEADER_SIZE);
            data_log_ << std::endl;
        }
    }
}

void WebSocketClient::onMarketDataConnected() {
    if (user_thread_callbacks_) {
        dispatchData(&empty_msg, 1, MessageType::MARKET_CONNECTED);
    }
    else {
        data_handler_->callbacks_->onMarketDataConnected(this);
    }
}

void WebSocketClient::onMarketDataDisconnected() {
    if (user_thread_callbacks_) {
        dispatchData(&empty_msg, 1, MessageType::MARKET_DISCONNECTED);
    }
    else {
        data_handler_->callbacks_->onMarketDataDisconnected(this);
        data_handler_->resetMarketDataSequence(this);
    }
    pending_md_socket_close_.fetch_add(1, std::memory_order_acq_rel);
    market_data_websocket_.reset();
}


void WebSocketClient::onUserDataConnected() {
    if (user_thread_callbacks_) {
        dispatchData(&empty_msg, 1, MessageType::USER_CONNECTED);
    }
    else {
        data_handler_->callbacks_->onUserDataConnected(this);
    }
}

void WebSocketClient::onUserDataDisconnected() {
    if (user_thread_callbacks_) {
        dispatchData(&empty_msg, 1, MessageType::USER_DISCONNECTED);
    }
    else {
        data_handler_->callbacks_->onUserDataDisconnected(this);
        data_handler_->resetUserDataSequence(this);
    }
    pending_user_socket_close_.fetch_add(1, std::memory_order_acq_rel);
    user_data_websocket_.reset();
}

void WebSocketClient::onMarketData(const char* data, std::size_t size) {
    if (user_thread_callbacks_) {
        dispatchData(data, size, MessageType::MARKET_DATA);
    }
    else {
        data_handler_->processMarketData(this, data, size);
        // If data_queue_ is not nullptr, data logger is enabled.
        // dispatch data for logging
        if (data_queue_) {
            dispatchData(data, size, MessageType::MARKET_DATA);
        }
    }
}

void WebSocketClient::onUserData(const char* data, std::size_t size) {
    if (user_thread_callbacks_) {
        dispatchData(data, size, MessageType::USER_DATA);
    }
    else {
        data_handler_->processUserData(this, data, size);
        // If data_queue_ is not nullptr, data logger is enabled.
        // dispatch data for logging
        if (data_queue_) {
            dispatchData(data, size, MessageType::USER_DATA);
        }
    }
}

void WebSocketClient::onMarketDataError(std::string &&err) {
    if (user_thread_callbacks_) {
        dispatchData(err.c_str(), err.size(), MessageType::MARKET_ERROR);
    }
    else {
        data_handler_->callbacks_->onMarketDataError(this, std::move(err));
    }
}

void WebSocketClient::onUserDataError(std::string &&err) {
    if (user_thread_callbacks_) {
        dispatchData(err.c_str(), err.size(), MessageType::USER_ERROR);
    }
    else {
        data_handler_->callbacks_->onUserDataError(this, std::move(err));
    }
}

// DataHandler implementation
void DataHandler::processMarketData(WebSocketClient *ws_client, const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        checkMarketDataSequenceNumber(ws_client, j["sequence_num"]);
        auto channel = j["channel"];
        if (channel == "l2_data") {
            processLevel2Update(ws_client, j);
        }
        else if (channel == "ticker" || channel == "ticker_batch") {
            processTicker(ws_client, j);
        }
        else if (channel == "market_trades") {
            processMarketTrades(ws_client, j);
        }
        else if (channel == "candles") {
            processCandles(ws_client, j);
        }
        else if (channel == "status") {
            processStatus(ws_client, j);
        }
        else if (channel == "subscriptions") {
        }
        else if (channel == "heartbeat") {
            processHeartbeat(ws_client, j);
        }
        else {
            LOG_ERROR("unknown channel: {}", channel.is_string() ? channel.get<std::string_view>() : channel.dump());
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("error: {}. data: {}", e.what(), std::string_view(data, size));
    }
}

void DataHandler::processUserData(WebSocketClient *ws_client, const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        checkUserDataSequenceNumber(ws_client, j["sequence_num"]);
        auto channel = j["channel"];
        if (channel == "user") {
            processUserEvent(ws_client, j);
        }
        else if (channel == "subscriptions") {
        }
        else if (channel == "heartbeat") {
            processHeartbeat(ws_client, j);
        }
        else if (channel == "futures_balance_summary") {
        }
        else {
            LOG_ERROR("unknown channel: {}", channel.is_string() ? channel.get<std::string_view>() : channel.dump());
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("error: {}. data: {}", e.what(), std::string_view(data, size));
    }
}

void DataHandler::processLevel2Update(WebSocketClient *ws_client, const json &j) {
    auto seq_num = j["sequence_num"].get<uint64_t>();
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onLevel2Snapshot(ws_client, seq_num, event);
        }
        else if (event["type"] == "update") {
            callbacks_->onLevel2Updates(ws_client, seq_num, event);
        }
        else {
            LOG_WARN("unknown l2_data event type: {}", event["type"].get<std::string_view>());
        }
    }
}

void DataHandler::processTicker(WebSocketClient *ws_client, const json &j) {
    auto seq_num = j["sequence_num"].get<uint64_t>();
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onTickerSnapshot(ws_client, seq_num, to_nanoseconds(j["timestamp"]), event["tickers"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onTickers(ws_client, seq_num, to_nanoseconds(j["timestamp"]), event["tickers"]);
        }
        else {
            LOG_WARN("unknown ticker event type: {}", j["type"].get<std::string_view>());
        }
    }
}

void DataHandler::processMarketTrades(WebSocketClient *ws_client, const json &j) {
    auto seq_num = j["sequence_num"].get<uint64_t>();
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onMarketTradesSnapshot(ws_client, seq_num, event["trades"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onMarketTrades(ws_client, seq_num, event["trades"]);
        }
        else {
            LOG_WARN("unknown market_trades event type: {}", event["type"].get<std::string_view>());
        }
    }
}

void DataHandler::processCandles(WebSocketClient *ws_client, const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onCandlesSnapshot(ws_client, j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["candles"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onCandles(ws_client, j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["candles"]);
        }
        else {
            LOG_WARN("unknown candles event type: {}", j["type"].get<std::string_view>());
        }
    }
}

void DataHandler::processStatus(WebSocketClient *ws_client, const json &j) {
    for (const auto &event : j["events"]) {
        if (event["type"] == "snapshot") {
            callbacks_->onStatusSnapshot(ws_client, j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["products"]);
        }
        else if (event["type"] == "update") {
            callbacks_->onStatus(ws_client, j["sequence_num"].get<uint64_t>(), to_nanoseconds(j["timestamp"]), event["products"]);
        }
        else {
            LOG_WARN("unknown status event type: {}", j["type"].get<std::string_view>());
        }
    }
}

void DataHandler::processUserEvent(WebSocketClient *ws_client, const json &j) {
    for (auto& event : j["events"]) {
        if (event["type"] == "snapshot") {
            auto &positions = event.at("positions");
            std::vector<Order> orders;
            for (auto& order : event.at("orders")) {
                orders.push_back({});
                from_snapshot(order, orders.back());
            }
            callbacks_->onUserDataSnapshot(ws_client, j.at("sequence_num").get<uint64_t>(), orders, positions.at("perpetual_futures_positions"), positions.at("expiring_futures_positions"));
        }
        else if (event["type"] == "update") {
            callbacks_->onOrderUpdates(ws_client, j.at("sequence_num").get<uint64_t>(), j.at("orders"));
        }
        else {
            LOG_WARN("unknown user event type: {}", j["type"].get<std::string_view>());
        }
    }
}

bool DataHandler::processHeartbeat([[maybe_unused]] WebSocketClient *ws_client, [[maybe_unused]] const json& j) {
    return true;
}

bool DataHandler::checkMarketDataSequenceNumber([[maybe_unused]] WebSocketClient* ws_client, int64_t seq_num) {
    if (last_md_seq_num_ < 0) [[unlikely]] {
        last_md_seq_num_ = seq_num;
        return true;
    }
    if (seq_num != last_md_seq_num_ + 1) {
        LOG_ERROR("market data message lost. seq_num: {}, last_md_seq_num: {}", seq_num, last_md_seq_num_);
        callbacks_->onMarketDataGap(ws_client);
        return false;
    }
    last_md_seq_num_ = seq_num;
    return true;
}

bool DataHandler::checkUserDataSequenceNumber([[maybe_unused]] WebSocketClient* ws_client, int64_t seq_num) {
    if (last_user_seq_num_ < 0) [[unlikely]] {
        last_user_seq_num_ = seq_num;
        return true;
    }
    if (seq_num != last_user_seq_num_ + 1) {
        LOG_ERROR("user data message lost. seq_num: {}, last_user_seq_num: {}", seq_num, last_user_seq_num_);
        callbacks_->onUserDataGap(ws_client);
        return false;
    }
    last_user_seq_num_ = seq_num;
    return true;
}

}  // end namespace coinbase
