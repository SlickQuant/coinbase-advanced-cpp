// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/websocket.hpp>

namespace coinbase {

namespace {

// The producer ids a client owns: the whole range its producer_offset names,
// [producer_offset, producer_offset + PRODUCER_IDS_PER_CLIENT). Ownership deliberately
// does not depend on which urls the client was given - claiming only the ids of the
// configured urls would let an md-only and a user-only client sit at the same
// producer_offset, a range the documented contract and isProducerOffsetAvailable() both
// hand to a single client.
constexpr uint32_t PRODUCER_IDS_PER_CLIENT = ProducerType::_PRODUCER_TYPE_COUNT_;

using ProducerBuffer = slick::stream_buffer_multiplexer::producer_buffer;

// Producer ids claimed by a WebSocketClient, keyed by multiplexer.
//
// slick::stream_buffer_multiplexer never unregisters a producer, so "already registered"
// alone separates none of the three producers a client can find at its offset: one left
// behind by a destroyed client (safe to reuse), one a live client is still writing to (a
// producer_offset collision), and one registered by code outside this library (never ours
// to write to - its owner is already relying on the single-producer contract of the stream
// buffer behind it). This registry draws both lines. An id stays claimed for the lifetime
// of the client that registered it, and for as long afterwards as that client's websocket
// session can still be writing to the buffer; a claim also remembers the producer this
// library registered under it, so that producer, and only that one, is ever reused.
//
// Only touched when a client is constructed or destroyed, never on the data path, so the
// spin lock costs nothing here and the publish path stays lock-free.
enum class ClaimState : uint8_t {
    owned,              // a live WebSocketClient owns the id
    pending_release,    // owner destroyed; frees up once its websocket session lets go
    released,           // no owner left, only the provenance of the producer remains
};

struct ProducerClaim {
    ClaimState state = ClaimState::owned;
    // The producer this library registered at the id, empty while it registered none (a
    // client owns its whole id range whether or not it uses every producer type). Held
    // weakly on purpose: the multiplexer owns the producer, and a record that expires with
    // it is what keeps a multiplexer later allocated at the same address from inheriting
    // another one's provenance - an address can be recycled, a weak_ptr cannot.
    std::weak_ptr<ProducerBuffer> producer;
};

using ProducerClaims = std::unordered_map<uint32_t, ProducerClaim>;

std::atomic_flag g_claims_lock;
std::unordered_map<const slick::stream_buffer_multiplexer*, ProducerClaims> g_producer_claims;

struct ClaimsGuard {
    ClaimsGuard() noexcept {
        while (g_claims_lock.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
    ~ClaimsGuard() noexcept { g_claims_lock.clear(std::memory_order_release); }
    ClaimsGuard(const ClaimsGuard&) = delete;
    ClaimsGuard& operator=(const ClaimsGuard&) = delete;
};

// A producer buffer is free once nothing but the multiplexer owns it. slick-net's session
// keeps shared ownership for as long as its read loop can still write to the buffer, and
// detach()/close() only start that teardown asynchronously - ~Websocket() does not wait
// for it. Handing the buffer to a new client any earlier would put two writers on one
// single-producer buffer.
bool producerQuiescent(slick::stream_buffer_multiplexer& mux, uint32_t id) noexcept {
    if (!Websocket::is_running()) {
        return true;    // no service thread, so no read loop can be writing
    }
    // The multiplexer's own reference plus the one get_producer_buffer() returns here.
    constexpr long owned_by_multiplexer_only = 2;
    if (mux.get_producer_buffer(id).use_count() > owned_by_multiplexer_only) {
        return false;
    }
    // Pairs with the releasing thread dropping the last shared_ptr to the buffer.
    std::atomic_thread_fence(std::memory_order_acquire);
    return true;
}

// Who registered the producer an id holds, as far as this library can tell.
enum class Provenance : uint8_t {
    unregistered,   // nothing is registered at the id
    ours,           // this library registered it, and it is still the same producer
    foreign,        // registered by code outside this library
};

// `claim` is the id's claim, or nullptr when it has none; `registered` is what the
// multiplexer currently holds at that id.
Provenance producerProvenance(const ProducerClaim* claim, const std::shared_ptr<ProducerBuffer>& registered) noexcept {
    if (!registered) {
        return Provenance::unregistered;
    }
    // Identity, not mere presence: a claim whose recorded producer has expired, or names
    // some other object, was left by a multiplexer that used to live at this address.
    return claim && claim->producer.lock() == registered ? Provenance::ours : Provenance::foreign;
}

// A claim carries information only while it names a live owner, a session that may still
// be writing, or a producer this library registered and could hand back. One that names
// none of those is dropped, so the registry shrinks back to empty as clients and
// multiplexers go away, and a multiplexer later allocated at the same address starts
// clean. Nothing here dereferences a multiplexer - by now it may be destroyed.
bool claimIsInert(const ProducerClaim& claim) noexcept {
    return claim.state == ClaimState::released && claim.producer.expired();
}

// Caller holds the lock. Only ever called while a client is constructed or destroyed, and
// the registry holds one entry per producer id in use, so walking all of it costs nothing.
void pruneInertClaims() noexcept {
    for (auto mux_claims = g_producer_claims.begin(); mux_claims != g_producer_claims.end(); ) {
        auto &claims = mux_claims->second;
        for (auto claim = claims.begin(); claim != claims.end(); ) {
            claim = claimIsInert(claim->second) ? claims.erase(claim) : std::next(claim);
        }
        mux_claims = claims.empty() ? g_producer_claims.erase(mux_claims) : std::next(mux_claims);
    }
}

enum class ClaimResult : uint8_t { ok, owned_by_live_client, session_still_writing, foreign_producer };

// The claim table of `mux`, or nullptr when it has no registry entry. Caller holds the
// lock, and must not keep the pointer past pruneInertClaims().
ProducerClaims* findClaims(const slick::stream_buffer_multiplexer& mux) noexcept {
    auto mux_claims = g_producer_claims.find(&mux);
    return mux_claims == g_producer_claims.end() ? nullptr : &mux_claims->second;
}

// The claim on `id`, or nullptr when it has none. `claims` is null for a multiplexer with
// no registry entry at all - one this library has never registered a producer on.
// Caller holds the lock.
ProducerClaim* findClaim(ProducerClaims* claims, uint32_t id) noexcept {
    if (!claims) {
        return nullptr;
    }
    auto claim = claims->find(id);
    return claim == claims->end() ? nullptr : &claim->second;
}

// Decides a single id: reclaims it when the client that owned it is gone and its session
// has let go, and reports whatever else still stands in the way. Caller holds the lock.
ClaimResult scanProducerId(slick::stream_buffer_multiplexer& mux, ProducerClaims* claims, uint32_t id) {
    auto* claimed = findClaim(claims, id);
    if (claimed) {
        switch (claimed->state) {
        case ClaimState::owned:
            return ClaimResult::owned_by_live_client;
        case ClaimState::pending_release:
            if (!producerQuiescent(mux, id)) {
                return ClaimResult::session_still_writing;
            }
            claimed->state = ClaimState::released;      // parked, and now free
            break;
        case ClaimState::released:
            break;
        }
    }
    // No client of this library holds the id - but it is only ours to take if nothing
    // else has registered a producer under it.
    return producerProvenance(claimed, mux.get_producer_buffer(id)) == Provenance::foreign
            ? ClaimResult::foreign_producer : ClaimResult::ok;
}

// Walks the ids a client at `producer_offset` owns, reclaiming the ones a destroyed client
// left parked, and reports the first one still in use through `conflict`. Caller holds the
// lock. Both claimProducerIds() and isProducerOffsetAvailable() decide through this, so
// "is this offset free?" and "may I have this offset?" can never answer differently.
ClaimResult scanOwnedProducerIds(slick::stream_buffer_multiplexer& mux,
                                 ProducerClaims* claims,
                                 uint32_t producer_offset,
                                 uint32_t& conflict) {
    auto result = ClaimResult::ok;
    for (uint32_t i = 0; i < PRODUCER_IDS_PER_CLIENT; ++i) {
        const uint32_t id = producer_offset + i;
        const auto id_result = scanProducerId(mux, claims, id);
        if (id_result != ClaimResult::ok && result == ClaimResult::ok) {
            conflict = id;          // first conflict wins; the loop runs on so the rest
            result = id_result;     // of the range is still reclaimed
        }
    }
    return result;
}

// Claims every id in the range or none of them. Ids parked by a destroyed client are
// reclaimed here once their session has let go, which is what lets "destroy, then
// re-create at the same producer_offset" work without anyone waiting. On failure
// `conflict` receives the id.
ClaimResult claimProducerIds(slick::stream_buffer_multiplexer& mux, uint32_t producer_offset, uint32_t& conflict) {
    ClaimsGuard guard;
    auto &claims = g_producer_claims[&mux];
    const auto result = scanOwnedProducerIds(mux, &claims, producer_offset, conflict);
    if (result != ClaimResult::ok) {
        pruneInertClaims();     // may drop the entry the lookup above just created
        return result;
    }
    for (uint32_t i = 0; i < PRODUCER_IDS_PER_CLIENT; ++i) {
        // Keeps whatever producer the claim already records: that is exactly the
        // registration the new client is about to reuse.
        claims[producer_offset + i].state = ClaimState::owned;
    }
    return result;
}

// Never waits: ids whose buffer is already free are released, the rest are parked for a
// later claim to reclaim. Destroying a client must not block a trading thread on a socket
// teardown that runs on slick-net's service thread. The recorded producer survives either
// way - that record is what lets the next client at this offset reuse the registration,
// and tell it apart from one somebody else registered.
void releaseProducerIds(slick::stream_buffer_multiplexer& mux, uint32_t producer_offset) {
    ClaimsGuard guard;
    auto* claims = findClaims(mux);
    if (!claims) {
        return;
    }
    for (uint32_t i = 0; i < PRODUCER_IDS_PER_CLIENT; ++i) {
        const uint32_t id = producer_offset + i;
        if (auto* claim = findClaim(claims, id)) {
            claim->state = producerQuiescent(mux, id) ? ClaimState::released
                                                      : ClaimState::pending_release;
        }
    }
    pruneInertClaims();
}

// Records that this library registered `producer` at `producer_id`, so a later client can
// tell it from a producer somebody else put there. The caller claimed the id first, so
// the claim exists; the lookups only keep a lost race from writing a stray entry.
void recordRegisteredProducer(slick::stream_buffer_multiplexer& mux,
                              uint32_t producer_id,
                              const std::shared_ptr<ProducerBuffer>& producer) {
    ClaimsGuard guard;
    if (auto* claim = findClaim(findClaims(mux), producer_id)) {
        claim->producer = producer;
    }
}

// The provenance of `registered`, the producer the multiplexer currently holds at
// `producer_id`. Takes the lock itself, so it is callable from outside the claim path.
Provenance registeredProducerProvenance(slick::stream_buffer_multiplexer& mux,
                                        uint32_t producer_id,
                                        const std::shared_ptr<ProducerBuffer>& registered) {
    ClaimsGuard guard;
    return producerProvenance(findClaim(findClaims(mux), producer_id), registered);
}

// The message both refusals of a foreign producer carry: the one init() raises for the
// whole range up front, and the one addOrReuseProducer() raises per id as a last resort.
std::string foreignProducerError(uint32_t producer_id) {
    return "producer_id " + std::to_string(producer_id) +
        " is registered in the stream buffer multiplexer by code outside this library. Give the "
        "WebSocketClient a producer_offset whose whole id range is its own.";
}

}   // anonymous namespace

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

void UserThreadWebsocketCallbacks::addClient(WebSocketClient* client, slick::stream_buffer_multiplexer &mux, uint32_t producer_offset) {
    assert(!mux_ || mux_ == &mux);
    mux_ = &mux;
    live_clients_[client->clientId()] = client;
    auto sz = producer_offset + ProducerType::_PRODUCER_TYPE_COUNT_;
    if (clients_.size() < sz) {
        clients_.resize(sz, nullptr);
    }
    if (producer_types_.size() < sz) {
        producer_types_.resize(sz, ProducerType::_PRODUCER_TYPE_COUNT_);
    }
}

void UserThreadWebsocketCallbacks::removeClient(WebSocketClient* client) {
    // Records this client already published stay in the multiplexer, so drop every
    // reference to it here: processData() skips control records whose client id is no
    // longer live, and the nulled routing slots make it skip pending data records too.
    live_clients_.erase(client->clientId());
    for (auto &c : clients_) {
        if (c == client) {
            c = nullptr;
        }
    }
    md_seq_nums_.erase(client);
    user_seq_nums_.erase(client);
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
                // Anything can publish to a producer id on a shared or external multiplexer,
                // so a record too short to hold the header is not one dispatchData() wrote.
                // Reading the client id and the type tag out of it would run past the record,
                // and the error payload length below (record.length - MESSAGE_HEADER_SIZE)
                // would underflow into a ~4 GB std::string.
                if (record.length < MESSAGE_HEADER_SIZE) {
                    continue;
                }
                uint64_t client_id = 0;
                memcpy(&client_id, record.data, sizeof(client_id));
                auto client_it = live_clients_.find(client_id);
                if (client_it == live_clients_.end()) {
                    continue;   // control record left behind by a destroyed client
                }
                WebSocketClient* client = client_it->second;
                const char* data_ptr = reinterpret_cast<const char*>(record.data);
                MessageType type { static_cast<const char>(record.data[sizeof(client_id)]) };
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
    const char* mux_shm_name,
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
    , owning_mux_(new slick::stream_buffer_multiplexer(std::max(md_record_size, user_record_size) * 2, mux_shm_name))
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
        user_thread_callbacks_->removeClient(this);
        user_thread_callbacks_ = nullptr;
    }
    data_handler_ = nullptr;

    // Hand the producer ids back so a client created later with the same producer_offset can
    // reuse the producers this one leaves registered in the mux. Ids whose session is still
    // closing are parked rather than waited on - see releaseProducerIds().
    releaseProducerIds(mux_, producer_offset_);
}

bool WebSocketClient::isProducerOffsetAvailable(slick::stream_buffer_multiplexer& mux, uint32_t producer_offset) noexcept {
    ClaimsGuard guard;
    // A multiplexer this library has never touched is still scanned - external code may
    // have registered a producer in the range, which the constructor refuses just as
    // firmly. It gets no registry entry of its own: the scan takes a null claim table,
    // leaving this allocation-free, as a noexcept query should be.
    uint32_t conflict = 0;
    const bool available =
        scanOwnedProducerIds(mux, findClaims(mux), producer_offset, conflict) == ClaimResult::ok;
    pruneInertClaims();     // invalidates the claim table above - nothing reads it below
    return available;
}

std::shared_ptr<slick::stream_buffer_multiplexer::producer_buffer> WebSocketClient::addOrReuseProducer(
    uint32_t producer_id,
    uint64_t capacity,
    uint32_t control_size,
    const char* shm_name
) {
    // The multiplexer keeps producers registered for its whole lifetime, so an external
    // multiplexer still holds the producers of an already destroyed client. Reuse that
    // registration - re-adding it would throw std::invalid_argument - but only when this
    // library is what registered it: writing into a producer somebody else registered would
    // put a second writer on a single-producer buffer its owner is already publishing to.
    // init() claimed the whole range first and refuses a foreign producer there, so this
    // only catches one registered in between - nothing serialises an external
    // add_producer() against this library.
    if (auto existing = mux_.get_producer_buffer(producer_id)) {
        if (registeredProducerProvenance(mux_, producer_id, existing) != Provenance::ours) {
            throw std::invalid_argument(foreignProducerError(producer_id));
        }
        if (existing->capacity() != capacity || existing->control_size() != control_size) {
            LOG_WARN("Reusing producer {} registered with capacity {} and record size {}, requested capacity {} and record size {}.",
                     producer_id, existing->capacity(), existing->control_size(), capacity, control_size);
        }
        else {
            LOG_DEBUG("Reusing producer {} already registered in the stream buffer multiplexer.", producer_id);
        }
        return existing;
    }
    auto producer = mux_.add_producer(producer_id, capacity, control_size, shm_name);
    recordRegisteredProducer(mux_, producer_id, producer);
    return producer;
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

    // Claim before registering anything: a producer left behind by a destroyed client is
    // reusable, one a live client still owns is a producer_offset collision, and one this
    // library never registered belongs to somebody else and is not ours to write to.
    uint32_t conflict = 0;
    switch (claimProducerIds(mux_, producer_offset_, conflict)) {
    case ClaimResult::owned_by_live_client:     // a bug in the caller: overlapping offsets
        throw std::invalid_argument("producer_id " + std::to_string(conflict) +
            " is owned by a live WebSocketClient. Give each client sharing a multiplexer its own producer_offset.");
    case ClaimResult::session_still_writing:    // transient: the previous session is closing
        throw std::runtime_error("producer_id " + std::to_string(conflict) +
            " is still being written by the websocket session of a destroyed WebSocketClient. Retry once "
            "WebSocketClient::isProducerOffsetAvailable() returns true.");
    case ClaimResult::foreign_producer:         // a bug in the caller: the range is not its own
        throw std::invalid_argument(foreignProducerError(conflict));
    case ClaimResult::ok:
        break;
    }

    try {
        initProducers(callbacks, md_read_buffer_size, md_record_size, md_read_buffer_shm_name,
                      user_read_buffer_size, user_record_size, user_read_buffer_shm_name, write_buffer_size);
    }
    catch (...) {
        // ~WebSocketClient() never runs for a failed constructor, so undo by hand.
        if (user_thread_callbacks_) {
            user_thread_callbacks_->removeClient(this);
        }
        else {
            delete data_handler_;
        }
        data_handler_ = nullptr;
        releaseProducerIds(mux_, producer_offset_);
        throw;
    }
}

void WebSocketClient::initProducers(
    WebsocketCallbacks *callbacks,
    uint32_t md_read_buffer_size,
    uint32_t md_record_size,
    const char* md_read_buffer_shm_name,
    uint32_t user_read_buffer_size,
    uint32_t user_record_size,
    const char* user_read_buffer_shm_name,
    uint32_t write_buffer_size
) {
    if (user_thread_callbacks_) {
        user_thread_callbacks_->addClient(this, mux_, producer_offset_);
        data_handler_ = user_thread_callbacks_;
    }
    else {
        data_handler_ = new DataHandler();
        data_handler_->callbacks_ = callbacks;
    }

    if (!user_data_url_.empty()) {
        uint32_t pid = producer_offset_ + ProducerType::USER_CTRL;
        auto user_ctrl_pb = addOrReuseProducer(pid, 4096, 256);
        producer_buffers_[pid] = user_ctrl_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::USER_CTRL);
        }
        pid = producer_offset_ + ProducerType::USER_DATA;
        user_data_producer_id_ = pid;
        auto user_data_pb = addOrReuseProducer(pid, user_read_buffer_size, user_record_size, user_read_buffer_shm_name);
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
        auto md_ctrl_pb = addOrReuseProducer(pid, 4096, 256);
        producer_buffers_[pid] = md_ctrl_pb.get();
        if (user_thread_callbacks_) {
            user_thread_callbacks_->mapProducerType(pid, ProducerType::MD_CTRL);
        }
        pid = producer_offset_ + ProducerType::MD_DATA;
        md_data_producer_id_ = pid;
        auto md_data_pb = addOrReuseProducer(pid, md_read_buffer_size, md_record_size, md_read_buffer_shm_name);
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
        auto [ptr, n] = pb->prepare(sz);
        memcpy(ptr, &client_id_, sizeof(client_id_));
        ptr[sizeof(client_id_)] = static_cast<char>(type);
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
