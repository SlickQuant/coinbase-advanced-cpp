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
#include <chrono>
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

std::string to_string(WebSocketChannel channel);

enum class MessageType : char {
    MARKET_CONNECTED = 'A',
    MARKET_DISCONNECTED = 'B',
    USER_CONNECTED = 'C',
    USER_DISCONNECTED = 'D',
    MARKET_ERROR = 'E',
    USER_ERROR = 'F',
    MARKET_DATA_GAP = 'G',
    USER_DATA_GAP = 'H',
    MARKET_DATA = 'M',
    USER_DATA = 'U',

};

class WebSocketClient;

struct WebsocketCallbacks {
    virtual ~WebsocketCallbacks() = default;
    virtual void onMarketDataConnected(WebSocketClient* client) = 0;
    virtual void onUserDataConnected(WebSocketClient* client) = 0;
    virtual void onMarketDataDisconnected(WebSocketClient* client) = 0;
    virtual void onUserDataDisconnected(WebSocketClient* client) = 0;
    virtual void onLevel2Snapshot(WebSocketClient* client, uint64_t seq_num, const Level2UpdateBatch& snapshot) = 0;
    virtual void onLevel2Updates(WebSocketClient* client, uint64_t seq_num, const Level2UpdateBatch& updates) = 0;
    virtual void onMarketTradesSnapshot(WebSocketClient* client, uint64_t seq_num, const std::vector<MarketTrade>& snapshots) = 0;
    virtual void onMarketTrades(WebSocketClient* client, uint64_t seq_num, const std::vector<MarketTrade>& trades) = 0;
    virtual void onTickerSnapshot(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) = 0;
    virtual void onTickers(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Ticker>& tickers) = 0;
    virtual void onCandlesSnapshot(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) = 0;
    virtual void onCandles(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Candle>& candles) = 0;
    virtual void onStatusSnapshot(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) = 0;
    virtual void onStatus(WebSocketClient* client, uint64_t seq_num, uint64_t timestamp, const std::vector<Status>& status) = 0;
    virtual void onMarketDataGap(WebSocketClient* client) = 0;
    virtual void onUserDataGap(WebSocketClient* client) = 0;
    virtual void onUserDataSnapshot(WebSocketClient* client, uint64_t seq_num, const std::vector<Order>& orders, const std::vector<PerpetualFuturePosition>& perpetual_future_positions, const std::vector<ExpiringFuturePosition>& expiring_future_positions) = 0;
    virtual void onOrderUpdates(WebSocketClient* client, uint64_t seq_num, const std::vector<Order>& orders) = 0;
    virtual void onMarketDataError(WebSocketClient* client, std::string &&err) = 0;
    virtual void onUserDataError(WebSocketClient* client, std::string &&err) = 0;
};

struct DataHandler {
    virtual ~DataHandler() = default;

    void processMarketData(WebSocketClient *ws_client, const char* data, std::size_t size);
    void processUserData(WebSocketClient *ws_client, const char* data, std::size_t size);
    void onMarketDataError(WebSocketClient *ws_client, std::string err);
    void onUserDataError(WebSocketClient *ws_client, std::string err);
    void processLevel2Update(WebSocketClient *ws_client, const json& j);
    void processMarketTrades(WebSocketClient *ws_client, const json& j);
    void processCandles(WebSocketClient *ws_client, const json& j);
    void processTicker(WebSocketClient *ws_client, const json& j);
    void processUserEvent(WebSocketClient *ws_client, const json& j);
    bool processHeartbeat(WebSocketClient *ws_client, const json& j);
    void processStatus(WebSocketClient *ws_client, const json& j);
    void processFuturesBalanceSummary(WebSocketClient *ws_client, const json& j);
    
    virtual bool checkMarketDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num);
    virtual bool checkUserDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num);
    virtual void resetMarketDataSequence([[maybe_unused]] WebSocketClient *ws_client) {
        last_md_seq_num_ = -1;
    }
    virtual void resetUserDataSequence([[maybe_unused]] WebSocketClient *ws_client) {
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
    void processData(uint32_t max_drain_count = 100);

private:
    bool checkMarketDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num) override;
    bool checkUserDataSequenceNumber(WebSocketClient *ws_client, int64_t seq_num) override;
    void resetMarketDataSequence(WebSocketClient *ws_client) override;
    void resetUserDataSequence(WebSocketClient *ws_client) override;

private:
    friend class WebSocketClient;
    slick::SlickQueue<char> data_queue_;
    uint64_t read_cursor_ = 0;
    std::unordered_map<WebSocketClient*, std::atomic_int_fast64_t> md_seq_nums_;
    std::unordered_map<WebSocketClient*, std::atomic_int_fast64_t> user_seq_nums_;
    std::unordered_set<WebSocketClient*> md_clients_;
    std::unordered_set<WebSocketClient*> user_clients_;
};


class WebSocketClient {
public:
    WebSocketClient(
        WebsocketCallbacks *callbacks,
        std::string_view market_data_url = "wss://advanced-trade-ws.coinbase.com",
        std::string_view user_data_url = "wss://advanced-trade-ws-user.coinbase.com");
    ~WebSocketClient();

    void stop();

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
    void onMarketDataConnected();
    void onMarketDataDisconnected();
    void onUserDataConnected();
    void onUserDataDisconnected();
    void onMarketData(const char* data, std::size_t size);
    void onUserData(const char* data, std::size_t size);
    void onMarketDataError(std::string &&err);
    void onUserDataError(std::string &&err);
    void dispatchData(const char* data, std::size_t size, MessageType type);
    void runDataLogger();

private:
    friend struct UserThreadWebsocketCallbacks;
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
    std::atomic_int_fast8_t pending_md_socket_close_ = 0;
    std::atomic_int_fast8_t pending_user_socket_close_ = 0;
    uint64_t data_cursor_ = 0;
    static inline constexpr char empty_msg = '\0';
};

constexpr uint32_t MESSAGE_HEADER_SIZE = sizeof(WebSocketClient*) + 1;

}  // end namespace coinbase