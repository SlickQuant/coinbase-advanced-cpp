# Coinbase Advanced API C++ SDK

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)](#installation)
[![Lock-free](https://img.shields.io/badge/concurrency-lock--free-orange.svg)](#architecture)
[![GitHub release](https://img.shields.io/github/v/release/SlickQuant/slick-logger)](https://github.com/SlickQuant/slick-logger/releases)

A modern C++ SDK for interacting with the Coinbase Advanced API, providing both REST and WebSocket functionality for trading, market data, and account management.

## Features

- **REST API Support**: Complete implementation of Coinbase Advanced API endpoints for accounts, orders, products, trades, and more
- **WebSocket Support**: Real-time market data streaming with level2, ticker, market trades, and user data channels
- **Comprehensive Order Types**: Support for market, limit, stop limit, bracket, and TWAP orders
- **Type Safety**: Full C++ type definitions for all API responses with JSON serialization/deserialization
- **Modern C++**: Uses C++20 features, RAII, smart pointers, and modern C++ best practices
- **Async/Await Support**: Built-in async support using C++ coroutines
- **Thread-Safe**: Designed for multi-threaded applications

## Architecture

The SDK is organized into several key components:

```
include/coinbase/
├── account.h          # Account and balance management
├── auth.h             # Authentication utilities
├── candle.h           # Candlestick data
├── common.h           # Common types and enums
├── fill.h             # Fill data
├── level2_book.h      # Level2 order book
├── market_data.h      # Market data structures
├── order.h            # Order management
├── position.h         # Position management
├── price_book.h       # Price book data
├── product.h          # Product information
├── rest.h             # REST client implementation
├── rest_awaitable.h   # Async REST operations
├── side.h             # Order side definitions
├── trades.h           # Trade data
├── utils.h            # Utility functions
└── websocket.h        # WebSocket client implementation
```

## Installation

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.20+
- OpenSSL
- nlohmann/json (JSON library)
- jwt-cpp (JSON Web Token library)
- slick-net (networking library - automatically fetched via CMake)
- vcpkg (optional, dependency management)

### Building

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Using vcpkg (optional)

This repo includes a `vcpkg.json` manifest. If you use vcpkg, dependencies are installed automatically via the toolchain file. If a vcpkg port for `slick-net` is available, it will be used; otherwise CMake falls back to FetchContent.

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Usage

#### REST Client

```cpp
#include <coinbase/rest.h>

// Create client
coinbase::CoinbaseRestClient client;

// Get server time
auto timestamp = client.get_server_time();

// List accounts
auto accounts = client.list_accounts();

// Get product information
auto product = client.get_product("BTC-USD");

// Create order
auto response = client.create_order(
    "client_order_id_123",
    "BTC-USD",
    coinbase::Side::BUY,
    coinbase::OrderType::LIMIT,
    coinbase::TimeInForce::GOOD_UNTIL_CANCELLED,
    0.001,
    50000.0,
    true  // post_only
);

// Cancel order
auto cancel_response = client.cancel_orders({"order_id_123"});
```

#### WebSocket Client

```cpp
#include <coinbase/websocket.h>

class MyCallbacks : public coinbase::WebsocketCallbacks {
public:
    void onLevel2Snapshot(uin64_t seq_num, const coinbase::Level2UpdateBatch& snapshot) override {
        // Handle level2 snapshot
    }
    
    void onLevel2Updates(uin64_t seq_num, const coinbase::Level2UpdateBatch& updates) override {
        // Handle level2 updates
    }
    
    void onMarketTrades(uin64_t seq_num, const std::vector<coinbase::MarketTrade>& trades) override {
        // Handle market trades
    }
    
    // ... other callback methods
};

// Create WebSocket client
MyCallbacks callbacks;
coinbase::WebSocketClient client(&callbacks);

// Subscribe to channels
std::vector<std::string> product_ids = {"BTC-USD", "ETH-USD"};
std::vector<coinbase::WebSocketChannel> channels = {
    coinbase::WebSocketChannel::LEVEL2,
    coinbase::WebSocketChannel::TICKER
};
client.subscribe(product_ids, channels);
```

## API Endpoints

### REST API

- **Accounts**: List accounts, get account details
- **Products**: List products, get product details
- **Orders**: Create, list, get, modify, and cancel orders
- **Fills**: List fills
- **Market Data**: Get best bid/ask, price book, market trades, candles
- **Time**: Get server time

### WebSocket API

- **Level2**: Order book updates
- **Ticker**: Price updates
- **Market Trades**: Trade updates
- **User**: User-specific data (orders, positions)
- **Candles**: Candlestick data
- **Status**: Product status updates

## Order Types

The SDK supports multiple order types:

- **Market Orders**: Execute immediately at market price
- **Limit Orders**: Execute at specified price
- **Stop Limit Orders**: Execute when stop price is hit, then as limit order
- **Bracket Orders**: Place both stop loss and take profit orders
- **TWAP Orders**: Split order execution over time

## Authentication

The SDK handles JWT authentication automatically using the `coinbase::generate_coinbase_jwt` function. You need to provide your API key and secret.

## Testing

The SDK includes comprehensive unit tests using Google Test. To run tests:

```bash
cd build
cmake -DBUILD_COINBASE_ADVANCED_TESTS=ON ..
cmake --build .
cd tests
./coinbase_advance_tests
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please submit a pull request with your changes.

## Support

For issues and questions, please open an issue on the GitHub repository.
