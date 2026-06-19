// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/websocket.hpp>

namespace coinbase {

std::string to_string(WebSocketChannel channel) {
    switch(channel) {
    case WebSocketChannel::HEARTBEATS:
        return "heartbeats";
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
    case WebSocketChannel::_CHANNEL_COUNT_:
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

void UserThreadWebsocketCallbacks::addClient(slick::stream_buffer_multiplexer &mux, uint32_t producer_offset) {
    assert(!mux_ || mux_ == &mux);
    mux_ = &mux;
    auto sz = producer_offset + ProducerType::_PRODUCER_TYPE_COUNT_;
    if (clients_.size() < sz) {
        clients_.resize(sz, nullptr);
    }
    if (producer_types_.size() < sz) {
        producer_types_.resize(sz, ProducerType::_PRODUCER_TYPE_COUNT_);
    }
}

void UserThreadWebsocketCallbacks::mapProducerType(uint32_t producer_id, ProducerType pt) {
    assert(producer_id < producer_types_.size());
    producer_types_[producer_id] = pt;
}

void UserThreadWebsocketCallbacks::processData(uint32_t max_drain_count) {
    if (!mux_) return;

    uint32_t i = 0;
    do {
        auto record = mux_->read(read_cursor_);
        if (!record) {
            // no data available
            break;
        }
        if (record.producer_id >= producer_types_.size()) {     // unknown producer_id
            continue;
        }

        auto prod_type = producer_types_[record.producer_id];
        switch (prod_type) {
            case ProducerType::MD_CTRL:
            case ProducerType::USER_CTRL: {
                const char* data_ptr = reinterpret_cast<const char*>(record.data);
                WebSocketClient* client = nullptr;
                memcpy(&client, record.data, sizeof(WebSocketClient*));
                MessageType type { static_cast<const char>(record.data[sizeof(WebSocketClient*)]) };
                data_ptr += MESSAGE_HEADER_SIZE;
                switch (type) {
                    case MessageType::MARKET_CONNECTED:
                        clients_[record.producer_id] = client;
                        clients_[record.producer_id - 2] = client;  // set client to MD_DATA's producer_id
                        callbacks_->onMarketDataConnected(client);
                        break;
                    case MessageType::MARKET_DISCONNECTED:
                        callbacks_->onMarketDataDisconnected(client);
                        resetMarketDataSequence(client);
                        clients_[record.producer_id] = nullptr;
                        clients_[record.producer_id - 2] = nullptr;
                        break;
                    case MessageType::USER_CONNECTED:
                        clients_[record.producer_id] = client;
                        clients_[record.producer_id - 2] = client;  // set client to USER_DATA's producer_id
                        callbacks_->onUserDataConnected(client);
                        break;
                    case MessageType::USER_DISCONNECTED:
                        callbacks_->onUserDataDisconnected(client);
                        resetUserDataSequence(client);
                        clients_[record.producer_id] = nullptr;
                        clients_[record.producer_id - 2] = nullptr;
                        break;
                    case MessageType::MARKET_ERROR:
                        if (clients_[record.producer_id]) {
                            callbacks_->onMarketDataError(client, std::string(data_ptr, record.length - MESSAGE_HEADER_SIZE));
                        }
                        break;
                    case MessageType::USER_ERROR:
                        if (clients_[record.producer_id]) {
                            callbacks_->onUserDataError(client, std::string(data_ptr, record.length - MESSAGE_HEADER_SIZE));
                        }
                        break;
                    case MessageType::MARKET_DATA_GAP:
                        if (clients_[record.producer_id]) {
                            callbacks_->onMarketDataGap(client);
                        }
                        break;
                    case MessageType::USER_DATA_GAP:
                        if (clients_[record.producer_id]) {
                            callbacks_->onUserDataGap(client);
                        }
                        break;
                }
                break;
            }
            case ProducerType::MD_DATA:
                if (clients_[record.producer_id]) {
                    processMarketData(clients_[record.producer_id], reinterpret_cast<const char*>(record.data), record.length);
                }
                break;
            case ProducerType::USER_DATA:
                if (clients_[record.producer_id]) {
                    processUserData(clients_[record.producer_id], reinterpret_cast<const char*>(record.data), record.length);
                }
                break;
            case ProducerType::_PRODUCER_TYPE_COUNT_:
                continue;
        }
    }
    while(++i < max_drain_count);
}

// WebSocketClient implementation
WebSocketClient::WebSocketClient(
    WebsocketCallbacks *callbacks,
    std::string_view market_data_url,
    std::string_view user_data_url,
    uint32_t md_read_buffer_size,
    uint32_t md_record_size,
    const char* md_read_buffer_shm_name,
    uint32_t user_read_buffer_size,
    uint32_t user_record_size,
    const char* user_read_buffer_shm_name,
    uint32_t write_buffer_size
)
    : market_data_url_(market_data_url)
    , user_data_url_(user_data_url)
    , owning_mux_(new slick::stream_buffer_multiplexer(std::max(md_record_size, user_record_size) * 2))
    , mux_(*owning_mux_.get())
    , producer_offset_(0)
    , user_thread_callbacks_(dynamic_cast<UserThreadWebsocketCallbacks*>(callbacks))
{
    init(
        callbacks,
        md_read_buffer_size,
        md_record_size,
        md_read_buffer_shm_name,
        user_read_buffer_size,
        user_record_size,
        user_read_buffer_shm_name,
        write_buffer_size
    );
}

WebSocketClient::WebSocketClient(
    WebsocketCallbacks *callbacks,
    slick::stream_buffer_multiplexer &mux,
    std::string_view market_data_url,
    std::string_view user_data_url,
    uint32_t producer_offset,
    uint32_t md_read_buffer_size,
    uint32_t md_record_size,
    const char* md_read_buffer_shm_name,
    uint32_t user_read_buffer_size,
    uint32_t user_record_size,
    const char* user_read_buffer_shm_name,
    uint32_t write_buffer_size
)
    : market_data_url_(market_data_url)
    , user_data_url_(user_data_url)
    , mux_(mux)
    , producer_offset_(producer_offset)
    , user_thread_callbacks_(dynamic_cast<UserThreadWebsocketCallbacks*>(callbacks))
{
    init(
        callbacks,
        md_read_buffer_size,
        md_record_size,
        md_read_buffer_shm_name,
        user_read_buffer_size,
        user_record_size,
        user_read_buffer_shm_name,
        write_buffer_size
    );
}

WebSocketClient::~WebSocketClient() {
    if (market_data_websocket_) {
        if (market_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            market_data_websocket_->detach();
            market_data_websocket_->close();
        }
        market_data_websocket_.reset();
    }
    if (user_data_websocket_) {
        if (user_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            user_data_websocket_->detach();
            user_data_websocket_->close();
        }
        user_data_websocket_.reset();
    }

    logger_run_.store(false, std::memory_order_release);
    if (logger_thread_.joinable()) {
        logger_thread_.join();
    }

    if (!user_thread_callbacks_) {
        delete data_handler_;
    }
    else {
        user_thread_callbacks_ = nullptr;
    }
    data_handler_ = nullptr;
}

void WebSocketClient::init(
    WebsocketCallbacks *callbacks,
    uint32_t md_read_buffer_size,
    uint32_t md_record_size,
    const char* md_read_buffer_shm_name,
    uint32_t user_read_buffer_size,
    uint32_t user_record_size,
    const char* user_read_buffer_shm_name,
    uint32_t write_buffer_size
) {
    assert(producer_offset_ + ProducerType::_PRODUCER_TYPE_COUNT_ < std::numeric_limits<uint32_t>::max());
    producer_buffers_.resize(producer_offset_ + ProducerType::_PRODUCER_TYPE_COUNT_, nullptr);

    if (user_thread_callbacks_) {
        user_thread_callbacks_->addClient(mux_, producer_offset_);
        data_handler_ = user_thread_callbacks_;
    }
    else {
        data_handler_ = new DataHandler();
        data_handler_->callbacks_ = callbacks;
    }

    if (!user_data_url_.empty()) {
        uint32_t pid = producer_offset_ + ProducerType::USER_CTRL;
        auto user_ctrl_pb = mux_.add_producer(pid, 4096, 256);
        producer_buffers_[pid] = user_ctrl_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::USER_CTRL);
        }
        pid = producer_offset_ + ProducerType::USER_DATA;
        user_data_producer_id_ = pid;
        auto user_data_pb = mux_.add_producer(pid, user_read_buffer_size, user_record_size, user_read_buffer_shm_name);
        producer_buffers_[pid] = user_data_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::USER_DATA);
        }
        user_data_websocket_ = std::make_unique<Websocket>(
            user_data_url_,
            [this]() { onUserDataConnected(); },
            [this]() { onUserDataDisconnected(); },
            [this](const char* data, std::size_t size) { onUserData(data, size); },
            [this](std::string err) { onUserDataError(std::move(err)); },
            user_data_pb,
            write_buffer_size
        );
    }

    if (!market_data_url_.empty()) {
        uint32_t pid = producer_offset_ + ProducerType::MD_CTRL;
        auto md_ctrl_pb = mux_.add_producer(pid, 4096, 256);
        producer_buffers_[pid] = md_ctrl_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::MD_CTRL);
        }
        pid = producer_offset_ + ProducerType::MD_DATA;
        md_data_producer_id_ = pid;
        auto md_data_pb = mux_.add_producer(pid, md_read_buffer_size, md_record_size, md_read_buffer_shm_name);
        producer_buffers_[pid] = md_data_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::MD_DATA);
        }
        market_data_websocket_ = std::make_unique<Websocket>(
            market_data_url_,
            [this]() { onMarketDataConnected(); },
            [this]() { onMarketDataDisconnected(); },
            [this](const char* data, std::size_t size) { onMarketData(data, size); },
            [this](std::string err) { onMarketDataError(std::move(err)); },
            md_data_pb,
            write_buffer_size
        );
    }
}

void WebSocketClient::stop() {
    if (market_data_websocket_) {
        if (market_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            market_data_websocket_->close();
        }
    }
    if (user_data_websocket_) {
        if (user_data_websocket_->status() != Websocket::Status::DISCONNECTED) {
            user_data_websocket_->close();
        }
    }
}

void WebSocketClient::subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto products = product_ids_[static_cast<uint8_t>(channel)];
        auto subscribe_json = json{{"type", "subscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        Websocket* websocket = nullptr;
        if (channel == WebSocketChannel::USER) {
            if (user_data_websocket_) {
                if (user_data_websocket_->status() > Websocket::Status::CONNECTED) {
                    user_data_websocket_->open();
                    
                    // subscribe heartbeat to keep user channel alive
                    auto heartbeat_sub = json{{"type", "subscribe"}, {"channel", "heartbeats"}};
                    heartbeat_sub["jwt"] = generate_coinbase_jwt(user_data_url_.c_str());
                    auto subscribe_str = heartbeat_sub.dump();
                    user_data_websocket_->send(subscribe_str.c_str(), subscribe_str.size());
                }
                websocket = user_data_websocket_.get();
                subscribe_json["jwt"] = generate_coinbase_jwt(user_data_url_.c_str());
            }
        }
        else {
            if (market_data_websocket_) {
                if (market_data_websocket_->status() > Websocket::Status::CONNECTED) {
                    market_data_websocket_->open();
                }
                websocket = market_data_websocket_.get();
            }
        }

        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized, URL is empty.", to_string(channel));
            continue;
        }

        auto subscribe_str = subscribe_json.dump();
        websocket->send(subscribe_str.c_str(), subscribe_str.size());
    }
}

void WebSocketClient::unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels) {
    for (auto channel : channels) {
        auto &websocket = channel == WebSocketChannel::USER ? user_data_websocket_ : market_data_websocket_;
        auto unsubscribe_json = json{{"type", "unsubscribe"}, {"product_ids", product_ids}, {"channel", to_string(channel)}};
        auto unsubscribe_str = unsubscribe_json.dump();
        if (websocket == nullptr) {
            LOG_WARN("WebSocket for channel {} is not initialized.", to_string(channel));
            continue;
        }
        if (websocket->status() <= Websocket::Status::CONNECTED) {
            websocket->send(unsubscribe_str.c_str(), unsubscribe_str.size());
        }
        if (channel == WebSocketChannel::HEARTBEATS) {
            if (user_data_websocket_ && user_data_websocket_->status() <= Websocket::Status::CONNECTED) {
                user_data_websocket_->send(unsubscribe_str.c_str(), unsubscribe_str.size());
            }
        }
    }
}

void WebSocketClient::logData(std::string_view data_file) {
    data_log_.open(std::string(data_file), std::ios::out | std::ios::app);
    if (data_log_.is_open()) {
        logger_run_.store(true, std::memory_order_release);
        logger_thread_ = std::thread([this](){
            runDataLogger();
        });
    }
    else {
        LOG_ERROR("Failed to open data_file {}.", data_file);
    }
}

void WebSocketClient::dispatchData(ProducerType pt, const char* data, std::size_t size, MessageType type) {
    assert(pt > ProducerType::USER_DATA && (producer_offset_ + pt) < producer_buffers_.size());
    auto *pb = producer_buffers_[producer_offset_ + pt];
    if (pb) [[likely]] {
        auto sz = (uint32_t)(MESSAGE_HEADER_SIZE + size);
        void *self = this;
        auto [ptr, n] = pb->prepare(sz);
        memcpy(ptr, &self, sizeof(WebSocketClient*));
        ptr[sizeof(WebSocketClient*)] = static_cast<char>(type);
        memcpy(ptr + MESSAGE_HEADER_SIZE, data, size);
        pb->commit(sz);
        pb->consume(sz);
    }
}

void WebSocketClient::runDataLogger() {
    if (!data_log_.is_open()) {
        return;
    }

    while (logger_run_.load(std::memory_order_relaxed)) {
        auto record = mux_.read(log_cursor_);
        if (!record) {
            std::this_thread::yield();
            continue;
        }

        if (record.producer_id == md_data_producer_id_ || record.producer_id == user_data_producer_id_) {
            data_log_.write(reinterpret_cast<const char*>(record.data), record.length);
            data_log_ << std::endl;
        }
    }

    // drain data queue
    while (true) {
        auto record = mux_.read(log_cursor_);
        if (!record) {
            break;
        }
        if (record.producer_id == md_data_producer_id_ || record.producer_id == user_data_producer_id_) {
            data_log_.write(reinterpret_cast<const char*>(record.data), record.length);
            data_log_ << std::endl;
        }
    }
}

void WebSocketClient::onMarketDataConnected() {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::MD_CTRL, &empty_msg, 1, MessageType::MARKET_CONNECTED);
    }
    else {
        data_handler_->callbacks_->onMarketDataConnected(this);
    }
}

void WebSocketClient::onMarketDataDisconnected() {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::MD_CTRL, &empty_msg, 1, MessageType::MARKET_DISCONNECTED);
    }
    else {
        data_handler_->callbacks_->onMarketDataDisconnected(this);
        data_handler_->resetMarketDataSequence(this);
    }
}


void WebSocketClient::onUserDataConnected() {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::USER_CTRL, &empty_msg, 1, MessageType::USER_CONNECTED);
    }
    else {
        data_handler_->callbacks_->onUserDataConnected(this);
    }
}

void WebSocketClient::onUserDataDisconnected() {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::USER_CTRL, &empty_msg, 1, MessageType::USER_DISCONNECTED);
    }
    else {
        data_handler_->callbacks_->onUserDataDisconnected(this);
        data_handler_->resetUserDataSequence(this);
    }
}

void WebSocketClient::onMarketData(const char* data, std::size_t size) {
    if (!user_thread_callbacks_) {
        data_handler_->processMarketData(this, data, size);
    }
}

void WebSocketClient::onUserData(const char* data, std::size_t size) {
    if (!user_thread_callbacks_) {
        data_handler_->processUserData(this, data, size);
    }
}

void WebSocketClient::onMarketDataError(std::string &&err) {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::MD_CTRL, err.c_str(), err.size(), MessageType::MARKET_ERROR);
    }
    else {
        data_handler_->callbacks_->onMarketDataError(this, std::move(err));
    }
}

void WebSocketClient::onUserDataError(std::string &&err) {
    if (user_thread_callbacks_) {
        dispatchData(ProducerType::USER_CTRL, err.c_str(), err.size(), MessageType::USER_ERROR);
    }
    else {
        data_handler_->callbacks_->onUserDataError(this, std::move(err));
    }
}

// DataHandler implementation
void DataHandler::processMarketData(WebSocketClient *ws_client, const char* data, std::size_t size) {
    try {
        auto j = json::parse(data, data + size);
        if (j.contains("sequence_num")) {
            checkMarketDataSequenceNumber(ws_client, j["sequence_num"]);
        }
        if (j["type"] == "error") {
            callbacks_->onMarketDataError(ws_client, j["message"]);
            return;
        }
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
        else if (channel == "heartbeats") {
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
        if (j.contains("sequence_num")) {
            checkUserDataSequenceNumber(ws_client, j["sequence_num"]);
        }
        if (j["type"] == "error") {
            callbacks_->onUserDataError(ws_client, j["message"]);
            return;
        }
        auto channel = j["channel"];
        if (channel == "user") {
            processUserEvent(ws_client, j);
        }
        else if (channel == "subscriptions") {
        }
        else if (channel == "heartbeats") {
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
        std::vector<Order> orders;
        for (auto& order : event.at("orders")) {
            orders.push_back({});
            from_snapshot(order, orders.back());
        }
        if (event["type"] == "snapshot") {
            auto &positions = event.at("positions");
            callbacks_->onUserDataSnapshot(ws_client, j.at("sequence_num").get<uint64_t>(), orders, positions.at("perpetual_futures_positions"), positions.at("expiring_futures_positions"));
        }
        else if (event["type"] == "update") {
            callbacks_->onOrderUpdates(ws_client, j.at("sequence_num").get<uint64_t>(), orders);
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
