# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [] -

### Changed
- Try to find dependent slick components using `find_package` before falling back to `FetchContent`.

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