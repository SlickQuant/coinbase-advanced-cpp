# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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