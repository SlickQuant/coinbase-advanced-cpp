// SPDX-License-Identifier: MIT
// Reader for multi_websockets_ws_callbacks — demonstrates cross-process IPC via
// a shared stream_buffer_multiplexer.
//
// multi_websockets_ws_callbacks (the producer) writes raw Coinbase JSON frames
// into named shared-memory producer buffers and publishes record headers into a
// named shared-memory queue.  This process opens the same shared memory and
// drains raw JSON messages on its own thread — no extra network connection and
// no coinbase parsing layer involved.
//
// Producer IDs opened here:
//   BTC MD_DATA: producer_offset=0 + MD_DATA=0  → producer_id=0
//   ETH MD_DATA: producer_offset=4 + MD_DATA=0  → producer_id=4
// MD_CTRL records (producer_ids 2, 6) reference local memory in the producer
// process and are silently skipped by the mux.
//
// Start multi_websockets_ws_callbacks first, then run this reader.

#include <atomic>
#include <chrono>
#include <csignal>
#include <format>
#include <iostream>
#include <memory>
#include <thread>

#include <slick/net/logging.hpp>
#include <slick/stream_buffer_multiplexer.hpp>
#include <coinbase/websocket.hpp>  // for coinbase::ProducerType

// Must match the constants in multi_websockets_ws_callbacks.cpp.
static constexpr const char* SHM_QUEUE_NAME = "coinbase_md_mux_queue";
static constexpr const char* SHM_BTC_BUF    = "coinbase_btc_md_buf";
static constexpr const char* SHM_ETH_BUF    = "coinbase_eth_md_buf";

static constexpr uint32_t BTC_MD_ID = 0;
static constexpr uint32_t ETH_MD_ID = coinbase::ProducerType::_PRODUCER_TYPE_COUNT_;

static std::atomic_bool g_running{true};

static void handle_sigint(int) {
    g_running.store(false, std::memory_order_relaxed);
}

int main() {
    std::signal(SIGINT, handle_sigint);

    slick::net::set_log_handler(
        [](slick::net::LogLevel level, const char* fmt, std::format_args args) {
            const char* prefix = "?????";
            switch (level) {
                case slick::net::LogLevel::Trace: prefix = "TRACE"; break;
                case slick::net::LogLevel::Debug: prefix = "DEBUG"; break;
                case slick::net::LogLevel::Info:  prefix = "INFO "; break;
                case slick::net::LogLevel::Warn:  prefix = "WARN "; break;
                case slick::net::LogLevel::Error: prefix = "ERROR"; break;
                case slick::net::LogLevel::Fatal: prefix = "FATAL"; break;
                default: break;
            }
            std::cout << '[' << prefix << "] " << std::vformat(fmt, args) << '\n';
        },
        []() { return slick::net::LogLevel::Debug; }
    );

    LOG_INFO("=== multi_websockets_ws_callbacks_reader ===");
    LOG_INFO("Attaches to the shared-memory mux written by multi_websockets_ws_callbacks.");
    LOG_INFO("Logs raw JSON from BTC-USD (producer_id={}) and ETH-USD (producer_id={}).",
             BTC_MD_ID, ETH_MD_ID);
    LOG_INFO("Press Ctrl-C to exit.");

    // Open the shared-memory fan-in queue — retry until the producer creates it.
    std::unique_ptr<slick::stream_buffer_multiplexer> mux;
    LOG_INFO("Waiting for shared queue '{}'...", SHM_QUEUE_NAME);
    while (g_running.load(std::memory_order_relaxed) && !mux) {
        try {
            mux = std::make_unique<slick::stream_buffer_multiplexer>(SHM_QUEUE_NAME);
        } catch (const std::exception&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (!mux) return 0;
    LOG_INFO("Shared queue opened.");

    // Open shared MD_DATA producer buffers — retry until the producer creates them.
    auto open_producer = [&](uint32_t pid, const char* shm_name, const char* label) {
        LOG_INFO("Opening {} buffer '{}'...", label, shm_name);
        while (g_running.load(std::memory_order_relaxed)) {
            try {
                mux->add_producer(pid, shm_name);
                LOG_INFO("{} buffer opened (producer_id={}).", label, pid);
                return;
            } catch (const std::exception&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    };

    open_producer(BTC_MD_ID, SHM_BTC_BUF, "BTC-USD");
    open_producer(ETH_MD_ID, SHM_ETH_BUF, "ETH-USD");
    if (!g_running.load(std::memory_order_relaxed)) return 0;
    LOG_INFO("Both buffers opened. Draining raw JSON from shared memory...");

    // Start at the current tail so we only see fresh messages.
    uint64_t cursor = mux->initial_reading_index();

    while (g_running.load(std::memory_order_relaxed)) {
        auto rec = mux->read(cursor);
        if (!rec) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        const char* sym = (rec.producer_id == BTC_MD_ID) ? "BTC-USD" : "ETH-USD";
        LOG_INFO("[{}] {}", sym,
                 std::string_view(reinterpret_cast<const char*>(rec.data), rec.length));
    }

    LOG_INFO("Reader exiting.");
}
