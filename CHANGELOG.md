# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-06-23

### Changed
- Simplified the installed CMake package config to use `find_dependency(...)` for `nlohmann_json`, OpenSSL, `jwt-cpp`, and `slick-net 3.0.0` instead of trying to fetch slick-net from the installed config
- Documented logging configuration through slick-net's logging hooks, including level filtering, cleanup, and an optional `slick-logger` bridge
- Updated tests to configure slick-net logging directly instead of using the removed Coinbase logging wrapper

### Deprecated
- Deprecated the `include/coinbase/logging.hpp` compatibility wrapper; consumers should include `<slick/net/logging.hpp>` and call `slick::net::set_log_handler()` / `slick::net::clear_log_handler()` directly

## [1.0.0] - 2026-06-19

### Added
- Five runnable examples (`examples/`): `market_data_ws_callbacks` and `market_data_user_thread_callbacks` for one or more symbols; `multi_websockets_ws_callbacks` and `multi_websockets_user_thread_callbacks` demonstrating two symbols (BTC-USD + ETH-USD, one symbol per WebSocket) sharing a `stream_buffer_multiplexer` via `producer_offset`; `multi_websockets_ws_callbacks_reader` demonstrating cross-process IPC by attaching to the named shared-memory mux and producer buffers written by `multi_websockets_ws_callbacks` — raw JSON is logged on a second process with no extra network connection
- `BUILD_COINBASE_ADVANCED_EXAMPLES` CMake option to build examples
- Second `WebSocketClient` constructor accepting an external `slick::stream_buffer_multiplexer`, enabling multiple clients to share one multiplexer
- `producer_offset` parameter on both `WebSocketClient` constructors to assign non-overlapping producer ID ranges per client
- Buffer-sizing parameters on `WebSocketClient` constructors: `md_read_buffer_size`, `md_record_size`, `user_read_buffer_size`, `user_record_size`, `write_buffer_size` (all with sensible defaults)
- Shared-memory name parameters (`md_read_buffer_shm_name`, `user_read_buffer_shm_name`) for zero-copy IPC via named shared memory
- `WebSocketClient::marketDataUrl()`, `userDataUrl()`, and `streamBufferMultiplexer()` accessors
- `ProducerType` enum (`MD_DATA`, `USER_DATA`, `MD_CTRL`, `USER_CTRL`) for typed producer-buffer routing

### Changed
- **BREAKING:** Upgraded `slick-net` from v2.1.0 to v3.0.0; `slick-stream-buffer-multiplexer` and `slick-dynamic-buffer` are bundled in slick-net v3.0.0
- **BREAKING:** `UserThreadWebsocketCallbacks` constructor no longer accepts a `queue_size` parameter; buffer sizing is now controlled via `WebSocketClient` constructor arguments
- **BREAKING:** `WebSocketClient::logData()` no longer accepts a `data_queue_size` parameter
- **BREAKING:** `WebSocketChannel::__COUNT__` renamed to `_CHANNEL_COUNT_`
- `UserThreadWebsocketCallbacks` now uses `slick::stream_buffer_multiplexer` instead of `SlickQueue<char>` for inter-thread data delivery — zero-copy, lock-free, and multiplexed across multiple clients
- `WebSocketClient` WebSocket objects are now created at construction time (not lazily on first `subscribe()`)
- `dispatchData()` now routes to dedicated per-`ProducerType` producer buffers; raw market/user data is written directly by slick-net's websocket layer, eliminating an extra copy
- Data logger reads from the shared multiplexer using producer IDs rather than a separate queue
- `WebSocketClient` is now explicitly non-copyable and non-movable
- `reset_callbacks()` replaced with `detach()` in destructor (slick-net v3.0.0 API change)
- `MessageType::MARKET_DATA` and `MessageType::USER_DATA` removed; data routing is now handled by producer type rather than a message type header

## [0.3.0] - 2026-06-08

### Added
- taker_fee_rate and maker_fee_rate in REST api.
- Portfolios endpoints: `list_portfolios`, `create_portfolio`, `get_portfolio_breakdown`, `move_portfolio_funds`, `edit_portfolio`, `delete_portfolio`
- Convert endpoints: `create_convert_quote`, `get_convert_trade`, `commit_convert_trade`
- Payment Methods endpoints: `list_payment_methods`, `get_payment_method`
- Data API endpoint: `get_api_key_permissions`
- Futures (CFM) endpoints: `get_futures_balance_summary`, `list_futures_positions`, `get_futures_position`, `schedule_futures_sweep`, `list_futures_sweeps`, `cancel_pending_futures_sweep`, `get_intraday_margin_setting`, `get_current_margin_window`, `set_intraday_margin_setting`
- Perpetuals (INTX) endpoints: `allocate_portfolio`, `get_perps_portfolio_summary`, `list_perps_positions`, `get_perps_position`, `get_perps_portfolio_balances`, `opt_in_or_out_multi_asset_collateral`
- Async mirrors of all the above in `CoinbaseAwaitableRestClient`, plus new data model headers (`portfolio.hpp`, `convert.hpp`, `payment_method.hpp`, `key_permissions.hpp`, `futures.hpp`, `perpetuals.hpp`) and `Amount` type in `common.hpp`

### Changed
- Change log file opening mode to append for data logging
- Upgraded slick-net dependency from v2.0.0 to v2.1.0
- Changed `market_data_websocket_` and `user_data_websocket_` members from `shared_ptr` to `unique_ptr`
- Refactored WebSocketClient destructor to call `reset_callbacks()` before closing sockets, eliminating busy-wait polling loops on disconnect
- Removed atomic `pending_md_socket_close_` and `pending_user_socket_close_` counters
- `stop()` no longer resets websocket pointers; connection state is preserved for reconnect
- Disconnect callbacks no longer reset websocket pointers
- `subscribe()` now checks socket status to reopen a disconnected (but existing) connection instead of only creating on null
- Heartbeat subscription is now sent immediately after `open()` on user data socket creation

### Fixed
- `unsubscribe()` now holds a reference to the `unique_ptr` instead of copying it
- `double_from_json` now handles fields the API returns as raw JSON numbers (not just stringified numbers), fixing parsing of `PortfolioPosition.allocation`/`available_to_trade_fiat`

## [0.2.2] - 2026-03-05

### Changed
- Enhance ISO 8601 parsing with microsecond and nanosecond support
- Convert all timestamp to nanoseconds

### Added
- timestamp parsing tests

## [0.2.1] - 2026-02-19

### Changed
- **BREAKING:** Renamed WebSocket channel `HEARTBEAT` to `HEARTBEATS` to match Coinbase API specification
- Improved JSON parsing macros to check for field existence before parsing (`TIMESTAMP_FROM_JSON`, `NANOSECONDS_FROM_JSON`, `DOUBLE_FROM_JSON`, `INT_FROM_JSON`)
- Enhanced error logging to combine context and error messages for better debugging

### Fixed
- Fixed Order JSON parsing to handle optional fields (`edit_history`, `creation_time`, `current_pending_replace`, `attached_order_configuration`)
- Fixed Order parsing to support both `creation_time` and `created_time` field names
- Fixed WebSocket error handling to properly process and dispatch error messages instead of throwing exceptions
- Fixed user event processing to use correct JSON path for order updates (`event.at("orders")` instead of `j.at("orders")`)
- Fixed sequence number checks to handle messages without `sequence_num` field
- Improved null-safety in JSON parsing throughout order and websocket modules

## [0.2.0] - 2026-02-19

### Added
- Comprehensive unit tests for CoinbaseAwaitableRestClient coroutine-based API
- Tests for all async REST endpoints including accounts, products, orders, fills, and market data
- Concurrent operations test demonstrating proper async usage

### Changed
- **BREAKING:** Converted from header-only to static library for significantly faster downstream builds (5-10x improvement)
- Changed precompiled headers from INTERFACE to PRIVATE (only affects library compilation, not downstream consumers)
- Simplified dependency management - OpenSSL and slick-net are now bundled in the static library
- Fixed CoinbaseAwaitableRestClient to use proper Boost.Asio coroutines with `co_return`
- Changed return type from `std::awaitable` to `asio::awaitable` (Boost.Asio)

### Migration Guide
Projects using this library must rebuild and reinstall. No source code changes are required in consuming projects, but you must:
1. Rebuild coinbase-advanced-cpp as a static library
2. Reinstall to your package manager or install prefix
3. Rebuild your project (you will see significant compilation speedup)

## [0.1.2] - 2026-02-03

### Added
- Cross-platform support for `get_env` utility function (Unix/macOS/Windows)

### Fixed
- Fix macro definitions to use correct field access syntax
- Replace `std::chrono::parse` with cross-platform manual parsing in `to_milliseconds` and `to_nanoseconds` for macOS compatibility
- Use UTC-aware time conversion (`timegm`/`_mkgmtime`) instead of local time (`mktime`) for ISO 8601 timestamp parsing
- Replace `std::format` with chrono formatters with `strftime` for GCC 14 compatibility in `timestamp_to_string`
- Fix `std::string_view` to `std::string` conversion in `logData` for `fstream::open` compatibility
- Fix linker error by making `empty_msg` static member variable `inline constexpr`

### Removed
- Unused `reconnectMarketData` and `reconnectUserData` methods from WebSocket class
- `level2_book.hpp` header file (functionality integrated elsewhere)

## [0.1.1] - 2026-02-01

### Added
- DataHandler class to process Coinbase market and user data
- Unit test for UserThreadWebsocketCallbacks multiple client support
- IsMarketDataConnected and IsUserDataConnected methods
- WebSocket connection lifecycle callbacks for market/user data
- WebSocketClient stop method for explicit shutdown
- WebSocket test for repeated connect/disconnect cycles
- Test for repeated connect/disconnect scenarios to validate stability

### Changed
- Try to find dependent slick components using `find_package` before falling back to `FetchContent`
- Changed header files from .h to .hpp
- Decoupled UserThreadWebsocketCallbacks from WebSocketeClient to support multiple WebSocketClient
- Create market data and user data websocket when url is set
- UserThreadWebsocketCallbacks now drains multiple queued messages per tick for higher throughput
- Updated slick-net to v1.2.3 and report version when found
- Added stricter warning and release optimization flags for MSVC and non-MSVC builds
- WebSocketCallbacks now receive WebSocketClient pointers on all events, and error callbacks take rvalue strings
- WebSocketClient now initializes sockets on subscribe and dispatches connect/disconnect events through the data queue
- UserThreadWebsocketCallbacks now track per-client sequence numbers and active client sets
- Refactored REST API tests to improve order handling and logging
- Added `order_` member variable to store order details for reuse in tests
- Enhanced error logging for order creation, modification, and cancellation
- Adjusted order quantities for better precision in tests
- Refactored WebSocket tests to include connection and disconnection tracking

### Fixed
- Various WebSocket unit tests not waiting for snapshot
- Fixed duplicated Candle definition
- WebSocket logger now writes correct payload offsets and labels user data correctly
- Sequence-number checks now accept first message even if the sequence does not start at 0
- Level2 book compile issues and trade timestamp handling
- Trades JSON parsing (pass-by-reference)
- Empty API secret handling in PEM formatting
- PriceBookResponse parsing when pricebook is missing
- Order status string typo and size_ratio field name
- Missing default return in FCM trading session state parsing
- WebSocket teardown now waits briefly for disconnect callbacks and clears per-client sequence state
- REST API tests now clean up created orders on failures and log API errors for debugging
- Build warnings across multiple files

## [0.1.0] - 2026-01-13 

### Added
- Data logger implementation for tracking application activity
- Comprehensive unit tests for REST API endpoints
- Comprehensive unit tests for WebSocket functionality
- Support for multiple order types including market, limit, stop limit, bracket, and TWAP orders
- Async/Await support using C++ coroutines for REST operations
- Documentation in README.md with usage examples
- SPDX header in header files
- CHANGELOG.md
- GitHub release workflow

### Changed
- Separated market data and user data handling functions for better organization
- Fixed level2 message side parsing to correctly handle order book updates
- Inlined dispatchData and processData functions for improved performance
- Refactored WebSocket callbacks to support thread-safe user data handling
- Updated slick-queue to v1.2.2
- Updated slick-net to v1.2.2
- Renamed repository from coinbase_advanced_cpp to coinbase-advanced-cpp (hyphenated naming follows recommended convention)
- License years

### Fixed
- Level2 message side parsing issue that was causing incorrect order book updates
- Various minor bugs in WebSocket message processing

## [0.1.0-candidate] - 2025-11-29

### Added
- Initial implementation of Coinbase Advanced API C++ SDK
- REST API client with support for accounts, orders, products, trades, and market data
- WebSocket client with support for level2, ticker, market trades, and user data channels
- Complete implementation of Coinbase Advanced API endpoints
- JWT authentication support using jwt-cpp library
- Type safety with full C++ type definitions for all API responses
- Modern C++ features including C++20, RAII, smart pointers, and modern C++ best practices
- Thread-safe design for multi-threaded applications
