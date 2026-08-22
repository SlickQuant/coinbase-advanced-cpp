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
#include <slick/net/websocket.hpp>
#include <nlohmann/json.hpp>
#include <coinbase/market_data.hpp>
#include <coinbase/order.hpp>
#include <coinbase/position.hpp>
#include <coinbase/auth.hpp>
#include <coinbase/candle.hpp>
#include <slick/queue.h>
#include <slick/stream_buffer_multiplexer.hpp>
#include <slick/dynamic_buffer.hpp>

namespace coinbase {

using Websocket = slick::net::Websocket<slick::dynamic_buffer<slick::stream_buffer_multiplexer::producer_buffer>>;
using json = nlohmann::json;

enum WebSocketChannel : uint8_t {
    HEARTBEATS,
    LEVEL2,
    MARKET_TRADES,
    TICKER,
    USER,
    CANDLES,
    STATUS,
    TICKER_BATCH,
    FUTURES_BALANCE_SUMMARY,
    _CHANNEL_COUNT_,  // Number of channels. For internal use only.
};

std::string to_string(WebSocketChannel channel);

enum ProducerType : uint8_t {
    MD_DATA, 
    USER_DATA,
    MD_CTRL,
    USER_CTRL,
    _PRODUCER_TYPE_COUNT_,
};

static_assert((ProducerType::MD_CTRL - ProducerType::MD_DATA) == 2 && (ProducerType::USER_CTRL - ProducerType::USER_DATA) == 2);

enum class MessageType : char {
    MARKET_CONNECTED = 'A',
    MARKET_DISCONNECTED = 'B',
    USER_CONNECTED = 'C',
    USER_DISCONNECTED = 'D',
    MARKET_ERROR = 'E',
    USER_ERROR = 'F',
    MARKET_DATA_GAP = 'G',
    USER_DATA_GAP = 'H',
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
    UserThreadWebsocketCallbacks()
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
    void addClient(WebSocketClient* client, slick::stream_buffer_multiplexer &mux, uint32_t producer_offset);
    // Drops every reference to `client`. Control and data records that a destroyed
    // client left behind in the multiplexer are discarded by processData() instead of
    // reaching a callback with a dangling pointer. Called from ~WebSocketClient(), so
    // destroy clients on the thread that calls processData() or with processData()
    // stopped - both mutate the routing tables below.
    void removeClient(WebSocketClient* client);
    void mapProducerType(uint32_t producer_id, ProducerType pt);
private:
    slick::stream_buffer_multiplexer *mux_ = nullptr;
    uint64_t read_cursor_ = 0;
    // Live clients by client id. Control records carry the id rather than the client
    // pointer: ids are never reused, so a record left behind by a destroyed client can
    // never be mistaken for one from a new client that happens to reuse its address.
    std::unordered_map<uint64_t, WebSocketClient*> live_clients_;
    std::unordered_map<WebSocketClient*, std::atomic_int_fast64_t> md_seq_nums_;
    std::unordered_map<WebSocketClient*, std::atomic_int_fast64_t> user_seq_nums_;
    std::vector<WebSocketClient*> clients_;   // live client per producer_id
    std::vector<ProducerType> producer_types_;
};


class WebSocketClient {
public:
    WebSocketClient(
        WebsocketCallbacks *callbacks,
        std::string_view market_data_url = "wss://advanced-trade-ws.coinbase.com",
        std::string_view user_data_url = "wss://advanced-trade-ws-user.coinbase.com",
        const char* mux_shm_name = nullptr,
        uint32_t md_read_buffer_size = 1u << 26,            // 64 MB reading buffer
        uint32_t md_record_size = 1u << 16,                 // 64K message records
        const char* md_read_buffer_shm_name = nullptr,      // default not using shared memory
        uint32_t user_read_buffer_size = 1u << 24,          // 16 MB buffer
        uint32_t user_record_size = 1u << 16,               // 64K message records
        const char* user_read_buffer_shm_name = nullptr,    // default not using shared memory
        uint32_t write_buffer_size = 1u << 20               // 1 MB write buffer
    );

    // Shares an external multiplexer. Each client owns the producer ids in
    // [producer_offset, producer_offset + _PRODUCER_TYPE_COUNT_). Producers left
    // registered by a destroyed client are reused as-is, keeping the capacity and record
    // size they were created with. Throws std::invalid_argument when the range overlaps a
    // client that is still alive (a caller bug), and std::runtime_error while the previous
    // owner's websocket session can still be writing to those buffers (transient - poll
    // isProducerOffsetAvailable() instead of catching it).
    WebSocketClient(
        WebsocketCallbacks *callbacks,
        slick::stream_buffer_multiplexer &mux,
        std::string_view market_data_url = "wss://advanced-trade-ws.coinbase.com",
        std::string_view user_data_url = "wss://advanced-trade-ws-user.coinbase.com",
        uint32_t producer_offset = 0,
        uint32_t md_read_buffer_size = 1u << 26,            // 64 MB reading buffer
        uint32_t md_record_size = 1u << 16,                 // 64K message records
        const char* md_read_buffer_shm_name = nullptr,      // default not using shared memory
        uint32_t user_read_buffer_size = 1u << 24,          // 16 MB buffer
        uint32_t user_record_size = 1u << 16,               // 64K message records
        const char* user_read_buffer_shm_name = nullptr,    // default not using shared memory
        uint32_t write_buffer_size = 1u << 20               // 1 MB write buffer
    );

    ~WebSocketClient();

    WebSocketClient(WebSocketClient&&) = delete;
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(WebSocketClient&&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;


    std::string_view marketDataUrl() const noexcept {
        return market_data_url_;
    }
    
    std::string_view userDataUrl() const noexcept {
        return user_data_url_;
    }

    void stop();

    bool isMarketDataConnected() const {
        return market_data_websocket_ && market_data_websocket_->status() == Websocket::Status::CONNECTED;
    }
    bool isUserDataConnected() const {
        return user_data_websocket_ && user_data_websocket_->status() == Websocket::Status::CONNECTED;
    }
    void subscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);
    void unsubscribe(const std::vector<std::string> &product_ids, const std::vector<WebSocketChannel> &channels);
    void logData(std::string_view data_file);

    slick::stream_buffer_multiplexer& streamBufferMultiplexer() noexcept {
        return mux_;
    }

    // Non-blocking check that a client can be constructed at `producer_offset` on `mux`:
    // false while a live client owns those producer ids, and while the websocket session
    // of a destroyed client can still be writing to their buffers. Poll this instead of
    // catching the constructor's std::runtime_error when re-creating a client.
    static bool isProducerOffsetAvailable(slick::stream_buffer_multiplexer& mux, uint32_t producer_offset) noexcept;

    // Process-wide unique id of this client. Never reused, not even by a client
    // constructed at the same address after this one is destroyed.
    uint64_t clientId() const noexcept {
        return client_id_;
    }

private:
    void init(
        WebsocketCallbacks *callbacks,
        uint32_t md_read_buffer_size,
        uint32_t md_record_size,
        const char* md_read_buffer_shm_name,
        uint32_t user_read_buffer_size,
        uint32_t user_record_size,
        const char* user_read_buffer_shm_name,
        uint32_t write_buffer_size
    );
    // Second half of init(), split out so a failure after the producer ids are claimed
    // can release them again.
    void initProducers(
        WebsocketCallbacks *callbacks,
        uint32_t md_read_buffer_size,
        uint32_t md_record_size,
        const char* md_read_buffer_shm_name,
        uint32_t user_read_buffer_size,
        uint32_t user_record_size,
        const char* user_read_buffer_shm_name,
        uint32_t write_buffer_size
    );
    // Register a producer in the multiplexer, reusing the existing registration when
    // producer_id is already taken. slick::stream_buffer_multiplexer has no
    // remove_producer(), so producers registered by a client outlive it: a new client
    // created on an external multiplexer with the same producer_offset must reuse them
    // instead of letting add_producer() throw std::invalid_argument.
    std::shared_ptr<slick::stream_buffer_multiplexer::producer_buffer> addOrReuseProducer(
        uint32_t producer_id,
        uint64_t capacity,
        uint32_t control_size,
        const char* shm_name = nullptr
    );
    void onMarketDataConnected();
    void onMarketDataDisconnected();
    void onUserDataConnected();
    void onUserDataDisconnected();
    void onMarketData(const char* data, std::size_t size);
    void onUserData(const char* data, std::size_t size);
    void onMarketDataError(std::string &&err);
    void onUserDataError(std::string &&err);
    void dispatchData(ProducerType pt, const char* data, std::size_t size, MessageType type);
    void runDataLogger();

private:
    friend struct UserThreadWebsocketCallbacks;
    static inline std::atomic_uint_fast64_t next_client_id_ = 1;
    const uint64_t client_id_ = next_client_id_.fetch_add(1, std::memory_order_relaxed);
    DataHandler* data_handler_ = nullptr;
    std::string market_data_url_;
    std::string user_data_url_;
    std::unique_ptr<Websocket> market_data_websocket_;
    std::unique_ptr<Websocket> user_data_websocket_;
    std::array<std::unordered_set<std::string>, WebSocketChannel::_CHANNEL_COUNT_> product_ids_;
    std::vector<std::string> pending_subscriptions_;
    std::string user_id_;
    std::unique_ptr<slick::stream_buffer_multiplexer> owning_mux_;
    slick::stream_buffer_multiplexer &mux_;
    uint32_t producer_offset_;
    UserThreadWebsocketCallbacks *user_thread_callbacks_ = nullptr;
    std::vector<slick::stream_buffer_multiplexer::producer_buffer*> producer_buffers_;
    std::fstream data_log_;
    std::thread logger_thread_;
    std::atomic_bool logger_run_ = false;
    uint64_t log_cursor_ = 0;
    uint32_t md_data_producer_id_ = std::numeric_limits<uint32_t>::max();
    uint32_t user_data_producer_id_ = std::numeric_limits<uint32_t>::max();
    static inline constexpr char empty_msg = '\0';
};

// Control-record header: client id followed by the MessageType tag.
constexpr uint32_t MESSAGE_HEADER_SIZE = sizeof(uint64_t) + sizeof(char);

}  // end namespace coinbase