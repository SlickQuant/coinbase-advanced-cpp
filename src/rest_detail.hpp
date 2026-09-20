// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

// Internal - one definition per REST endpoint, shared by the blocking CoinbaseRestClient and the
// coroutine-based CoinbaseAwaitableRestClient.
//
// Every endpoint is split into halves that perform no I/O: a request builder (url, JWT uri, body)
// and a response parser. The only thing each client adds is the transport - Http::get()/post() for
// the blocking client, a co_await-ed Http::async_get()/async_post() for the awaitable one - so a
// URL, a signature, a request body or a parse is never written twice, and the awaitable client
// never has to reach for a blocking call to reuse the synchronous one.

#include <coinbase/auth.hpp>
#include <coinbase/rest.hpp>
#include <coinbase/utils.hpp>
#include <nlohmann/json.hpp>
#include <slick/net/http.hpp>
#include <slick/net/logging.hpp>
#include <boost/asio/awaitable.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace coinbase::detail {

using Http = slick::net::Http;
using http_headers = std::vector<std::pair<std::string, std::string>>;

enum class http_method { get, post, put, del };

constexpr std::string_view method_name(http_method method) noexcept {
    switch (method) {
        case http_method::post: return "POST";
        case http_method::put:  return "PUT";
        case http_method::del:  return "DELETE";
        case http_method::get:  break;
    }
    return "GET";
}

// A request ready to go on the wire. An empty url marks one the builder refused to make (bad
// arguments); the drivers below turn that into the fallback result without touching the network.
struct request {
    http_method method = http_method::get;
    std::string url;
    std::string body;
    http_headers headers;
};

// `path` must not carry the query string - Coinbase signs the JWT over the path alone.
inline request signed_request(http_method method, std::string_view base_url, std::string_view domain,
                              std::string_view path, std::string_view query = {}, std::string body = {}) {
    request req{method, std::format("{}{}{}", base_url, path, query), std::move(body), {}};
    req.headers.emplace_back("Authorization",
        "Bearer " + generate_coinbase_jwt(std::format("{} {}{}", method_name(method), domain, path).c_str()));
    if (!req.body.empty()) {
        req.headers.emplace_back("Content-Type", "application/json");
    }
    return req;
}

inline request public_request(http_method method, std::string_view base_url, std::string_view path,
                              std::string_view query = {}) {
    return request{method, std::format("{}{}{}", base_url, path, query), {}, {}};
}

// The two transports. Both consume req.headers; req.url stays intact for error reporting.
inline Http::Response send(request &req) {
    switch (req.method) {
        case http_method::post: return Http::post(req.url, req.body, std::move(req.headers));
        case http_method::put:  return Http::put(req.url, req.body, std::move(req.headers));
        case http_method::del:  return Http::del(req.url, req.body, std::move(req.headers));
        case http_method::get:  break;
    }
    return Http::get(req.url, std::move(req.headers));
}

// Runs on the awaiting coroutine executor and suspends - never blocks - while the socket is
// resolving, connecting, writing or reading.
inline boost::asio::awaitable<Http::Response> send_async(request &req) {
    switch (req.method) {
        case http_method::post: co_return co_await Http::async_post(req.url, req.body, std::move(req.headers));
        case http_method::put:  co_return co_await Http::async_put(req.url, req.body, std::move(req.headers));
        case http_method::del:  co_return co_await Http::async_del(req.url, req.body, std::move(req.headers));
        case http_method::get:  break;
    }
    co_return co_await Http::async_get(req.url, std::move(req.headers));
}

// ---------------------------------------------------------------------------
// Drivers
// ---------------------------------------------------------------------------

// An endpoint whose successful response is one JSON document parsed into T. A failed request is
// logged and yields `fallback`, matching how the clients have always reported REST errors.
template <typename T>
struct endpoint {
    request req;
    const char *op = "";
    T (*parse)(const json &) = nullptr;
    T fallback = T{};
};

// An endpoint that reports nothing but the HTTP status; the body is never parsed.
struct status_endpoint {
    request req;
    const char *op = "";
};

// A cursor-paginated endpoint: pages are fetched until the server stops handing back a cursor.
template <typename T, typename Params>
struct paged_endpoint {
    using item_type = T;

    std::string_view base_url;
    std::string_view domain;
    std::string_view path;
    const char *items_field = "";
    const char *op = "";
    bool stop_without_has_next = true;
    Params params{};

    request page(std::string_view cursor) const {
        Params paged = params;
        if (!cursor.empty()) {
            paged.cursor = std::string(cursor);
        }
        return signed_request(http_method::get, base_url, domain, path, paged());
    }
};

// Appends one page of items and reports whether another page is worth fetching.
template <typename T, typename Ep>
bool append_page(std::vector<T> &items, json &j, const Ep &ep, std::string &cursor) {
    auto page = j.find(ep.items_field);
    if (page != j.end() && page->is_array()) {
        items.insert(items.end(), std::make_move_iterator(page->begin()), std::make_move_iterator(page->end()));
    }
    if (ep.stop_without_has_next && !(j.contains("has_next") && j["has_next"].get<bool>())) {
        return false;
    }
    auto next = j.find("cursor");
    if (next == j.end() || !next->is_string()) {
        return false;
    }
    auto value = next->get<std::string_view>();
    if (value.empty()) {
        return false;
    }
    cursor.assign(value);
    return true;
}

template <typename T>
T run(endpoint<T> ep) {
    if (ep.req.url.empty()) {
        return std::move(ep.fallback);
    }
    try {
        auto res = send(ep.req);
        if (res.is_ok()) {
            return ep.parse(json::parse(res.result_text));
        }
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, e.what());
    }
    return std::move(ep.fallback);
}

template <typename T>
boost::asio::awaitable<T> run_async(endpoint<T> ep) {
    if (!ep.req.url.empty()) {
        try {
            auto res = co_await send_async(ep.req);
            if (res.is_ok()) {
                co_return ep.parse(json::parse(res.result_text));
            }
            LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, res.result_text);
        }
        catch (const std::exception &e) {
            LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, e.what());
        }
    }
    co_return std::move(ep.fallback);
}

inline bool run(status_endpoint ep) {
    try {
        auto res = send(ep.req);
        if (res.is_ok()) {
            return true;
        }
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, e.what());
    }
    return false;
}

inline boost::asio::awaitable<bool> run_async(status_endpoint ep) {
    try {
        auto res = co_await send_async(ep.req);
        if (res.is_ok()) {
            co_return true;
        }
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, res.result_text);
    }
    catch (const std::exception &e) {
        LOG_ERROR("{} failed. url: {}, error: {}", ep.op, ep.req.url, e.what());
    }
    co_return false;
}

template <typename T, typename Params>
std::vector<T> run(paged_endpoint<T, Params> ep) {
    std::vector<T> items;
    std::string cursor;
    for (;;) {
        auto req = ep.page(cursor);
        try {
            auto res = send(req);
            if (!res.is_ok()) {
                LOG_ERROR("{} failed. url: {}, error: {}", ep.op, req.url, res.result_text);
                break;
            }
            auto j = json::parse(res.result_text);
            if (!append_page(items, j, ep, cursor)) {
                break;
            }
        }
        catch (const std::exception &e) {
            LOG_ERROR("{} failed. url: {}, error: {}", ep.op, req.url, e.what());
            break;
        }
    }
    return items;
}

template <typename T, typename Params>
boost::asio::awaitable<std::vector<T>> run_async(paged_endpoint<T, Params> ep) {
    std::vector<T> items;
    std::string cursor;
    for (;;) {
        auto req = ep.page(cursor);
        try {
            auto res = co_await send_async(req);
            if (!res.is_ok()) {
                LOG_ERROR("{} failed. url: {}, error: {}", ep.op, req.url, res.result_text);
                break;
            }
            auto j = json::parse(res.result_text);
            if (!append_page(items, j, ep, cursor)) {
                break;
            }
        }
        catch (const std::exception &e) {
            LOG_ERROR("{} failed. url: {}, error: {}", ep.op, req.url, e.what());
            break;
        }
    }
    co_return items;
}

}   // end namespace coinbase::detail
