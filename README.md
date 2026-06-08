# Coinbase Advanced API C++ SDK

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Static Library](https://img.shields.io/badge/library-static-brightgreen.svg)](#installation)
[![Lock-free](https://img.shields.io/badge/concurrency-lock--free-orange.svg)](#architecture)
[![GitHub release](https://img.shields.io/github/v/release/SlickQuant/slick-logger)](https://github.com/SlickQuant/slick-logger/releases)

A modern C++ SDK for interacting with the Coinbase Advanced API, providing both REST and WebSocket functionality for trading, market data, and account management.

## Features

- **REST API Support**: Complete implementation of Coinbase Advanced API endpoints for accounts, orders, products, trades, and more
- **WebSocket Support**: Real-time market data streaming with level2, ticker, market trades, and user data channels
  - Connection lifecycle callbacks for connect/disconnect events
  - Per-client sequence number tracking for multiple concurrent connections
  - Explicit shutdown control with `stop()` method
- **Comprehensive Order Types**: Support for market, limit, stop limit, bracket, and TWAP orders
- **Type Safety**: Full C++ type definitions for all API responses with JSON serialization/deserialization
- **Modern C++**: Uses C++20 features, RAII, smart pointers, and modern C++ best practices
- **Async/Await Support**: Built-in async support using C++ coroutines
- **Thread-Safe**: Designed for multi-threaded applications with lock-free data structures

## Architecture

The SDK is organized into several key components:

```
include/coinbase/
├── account.hpp          # Account and balance management
├── auth.hpp             # Authentication utilities
├── candle.hpp           # Candlestick data
├── common.hpp           # Common types and enums
├── convert.hpp          # Currency conversion (Convert) data models
├── fill.hpp             # Fill data
├── futures.hpp          # Futures (CFM) data models
├── key_permissions.hpp  # API key permissions (Data API) data models
├── logging.hpp          # Logging utilities
├── market_data.hpp      # Market data structures
├── order.hpp            # Order management
├── payment_method.hpp   # Payment methods data models
├── perpetuals.hpp       # Perpetuals (INTX) data models
├── portfolio.hpp        # Portfolios data models
├── position.hpp         # Position management
├── price_book.hpp       # Price book data
├── product.hpp          # Product information
├── rest.hpp             # REST client implementation
├── rest_awaitable.hpp   # Async REST operations
├── side.hpp             # Order side definitions
├── trades.hpp           # Trade data
├── utils.hpp            # Utility functions
└── websocket.hpp        # WebSocket client implementation
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
cmake --build . --config Release
cmake --install . --prefix /usr/local  # or your preferred install location
```

The library is built as a static library for optimal compilation performance in downstream projects.

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
#include <coinbase/rest.hpp>

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

// Portfolios
auto portfolios = client.list_portfolios();
auto breakdown = client.get_portfolio_breakdown(portfolios.front().uuid);

// Convert (quote only - commit_convert_trade executes a real conversion)
auto quote = client.create_convert_quote(from_account_uuid, to_account_uuid, 10.0);

// Futures (CFM)
auto balance_summary = client.get_futures_balance_summary();
auto positions = client.list_futures_positions();
```

#### Async REST Client

`CoinbaseAwaitableRestClient` mirrors every `CoinbaseRestClient` method as a C++20 coroutine returning `asio::awaitable<T>`, so the same endpoints (including all of the ones listed in [API Endpoints](#api-endpoints)) can be awaited from coroutine-based code:

```cpp
#include <coinbase/rest_awaitable.hpp>

coinbase::CoinbaseAwaitableRestClient client;

asio::awaitable<void> run() {
    auto accounts = co_await client.list_accounts();
    auto portfolios = co_await client.list_portfolios();
    auto permissions = co_await client.get_api_key_permissions();
    // ...
}
```

#### WebSocket Client

The SDK provides two callback mechanisms for handling WebSocket data:

1. **`WebsocketCallbacks`**: Callbacks are invoked directly on the WebSocket thread. Use this for simple applications or when you want immediate processing.

2. **`UserThreadWebsocketCallbacks`**: Callbacks are invoked on your own thread. The WebSocket data is queued in a lock-free queue, and you control when to process it by calling `processData()`. Use this for better control over threading and to avoid blocking the WebSocket thread.

##### Using WebsocketCallbacks (WebSocket Thread)

```cpp
#include <coinbase/websocket.hpp>

class MyCallbacks : public coinbase::WebsocketCallbacks {
public:
    // Connection lifecycle callbacks
    void onMarketDataConnected(coinbase::WebSocketClient* client) override {
        // Handle market data connection established
    }

    void onMarketDataDisconnected(coinbase::WebSocketClient* client) override {
        // Handle market data disconnection
    }

    void onUserDataConnected(coinbase::WebSocketClient* client) override {
        // Handle user data connection established
    }

    void onUserDataDisconnected(coinbase::WebSocketClient* client) override {
        // Handle user data disconnection
    }

    // Data callbacks
    void onLevel2Snapshot(coinbase::WebSocketClient* client, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& snapshot) override {
        // Handle level2 snapshot
    }

    void onLevel2Updates(coinbase::WebSocketClient* client, uint64_t seq_num,
                         const coinbase::Level2UpdateBatch& updates) override {
        // Handle level2 updates
    }

    void onMarketTrades(coinbase::WebSocketClient* client, uint64_t seq_num,
                        const std::vector<coinbase::MarketTrade>& trades) override {
        // Handle market trades
    }

    // Error callbacks
    void onMarketDataError(coinbase::WebSocketClient* client, std::string&& err) override {
        // Handle market data errors
    }

    void onUserDataError(coinbase::WebSocketClient* client, std::string&& err) override {
        // Handle user data errors
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

// Explicitly stop WebSocket connections when done
client.stop();
```

##### Using UserThreadWebsocketCallbacks (User Thread)

```cpp
#include <coinbase/websocket.hpp>

class MyCallbacks : public coinbase::UserThreadWebsocketCallbacks {
public:
    // Same callback signatures as WebsocketCallbacks
    void onMarketDataConnected(coinbase::WebSocketClient* client) override {
        // Handle market data connection established
    }

    void onLevel2Snapshot(coinbase::WebSocketClient* client, uint64_t seq_num,
                          const coinbase::Level2UpdateBatch& snapshot) override {
        // Handle level2 snapshot
    }

    // ... other callback methods
};

// Create callbacks and WebSocket client
MyCallbacks callbacks;
coinbase::WebSocketClient client(&callbacks);

// Subscribe to channels
client.subscribe({"BTC-USD", "ETH-USD"}, {
    coinbase::WebSocketChannel::LEVEL2,
    coinbase::WebSocketChannel::TICKER
});

// In your main loop or dedicated thread, process queued data
while (running) {
    // Process up to 100 queued messages per call
    callbacks.processData(100);

    // Your other application logic here
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

client.stop();
```

**Key Differences:**
- **`WebsocketCallbacks`**: Immediate processing on WebSocket I/O thread. Simple but can block WebSocket operations if callbacks are slow.
- **`UserThreadWebsocketCallbacks`**: Deferred processing on your thread. Better performance and control, but requires calling `processData()` regularly. Uses lock-free queues for efficient data transfer between threads.

## API Endpoints

### REST API

- **Accounts**: List accounts, get account details
- **Products**: List products, get product details
- **Orders**: Create, list, get, modify, and cancel orders
- **Fills**: List fills
- **Fees**: Get taker and maker fee rates
- **Market Data**: Get best bid/ask, price book, market trades, candles
- **Time**: Get server time
- **Portfolios**: List portfolios, create/edit/delete a portfolio, get portfolio breakdown, move funds between portfolios
- **Convert**: Create a convert quote, get a convert trade, commit a convert trade
- **Payment Methods**: List payment methods, get payment method details
- **Data API**: Get API key permissions
- **Futures (CFM)**: Get balance summary, list/get positions, schedule/list/cancel sweeps, get/set intraday margin settings, get current margin window
- **Perpetuals (INTX)**: Allocate portfolio, get portfolio summary, list/get positions, get portfolio balances, opt in/out of multi-asset collateral

All REST endpoints are available on both the synchronous `CoinbaseRestClient` and the coroutine-based `CoinbaseAwaitableRestClient` (see [Async REST Client](#async-rest-client)).

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
