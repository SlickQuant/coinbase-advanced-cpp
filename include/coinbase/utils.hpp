// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <stdlib.h>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <nlohmann/json.hpp>
#include <coinbase/side.hpp>

using json = nlohmann::json;

namespace coinbase {

inline std::string get_env(std::string_view name) {
#ifdef _WIN32
    // Windows: use getenv_s (secure version)
    size_t size;
    auto err_code = getenv_s(&size, nullptr, 0, name.data());
    std::string value;

    if (size > 0) {
        char *buffer = new char[size];  // the size include null terminator
        err_code = getenv_s(&size, buffer, size, name.data());
        if (err_code == 0) {
            value = std::string(buffer, size - 1);  // remove the null terminator
        }
        delete[] buffer;
    }
    return value;
#else
    // Unix/macOS: use standard getenv
    const char* value = std::getenv(name.data());
    return value ? std::string(value) : std::string();
#endif
}

inline std::string timestamp_to_string(uint64_t timestamp_ms) {
    auto tp = std::chrono::system_clock::time_point{
        std::chrono::milliseconds{timestamp_ms}
    };
    return std::format("{:%FT%T}.{:03}Z",
        std::chrono::floor<std::chrono::seconds>(tp),
        timestamp_ms % 1000);
}

inline uint64_t to_milliseconds(const std::string &iso_str) {
    std::istringstream iss(iso_str);
    std::chrono::system_clock::time_point tp;
    iss >> std::chrono::parse("%FT%T", tp);
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count();
    
    return static_cast<uint64_t>(ms);
}

inline uint64_t to_nanoseconds(const std::string &iso_str) {
    std::istringstream iss(iso_str);
    std::chrono::sys_time<std::chrono::nanoseconds> tp;
    iss >> std::chrono::parse("%FT%T", tp);
    
    if (iss.fail()) {
        return 0;
    }

    return tp.time_since_epoch().count();
}

inline uint64_t milliseconds_from_json(const json &j, std::string_view field) {
    return j.at(field).is_null() ? 0 : to_milliseconds(j.at(field).get<std::string>());
}

inline uint64_t nanoseconds_from_json(const json &j, std::string_view field) {
    return j.at(field).is_null() ? 0 : to_nanoseconds(j.at(field).get<std::string>());
}

inline double double_from_json(const json &j, std::string_view field) {
    try
    {
        auto f = j.at(field).get<std::string_view>();
        if (f.empty()) {
            return 0.;
        }
        return std::stod(f.data());
    }
    catch(const std::exception& e)
    {
        LOG_INFO("{} {}", field, j.dump());
        LOG_ERROR(e.what());
    }
    return 0.;
}

inline int32_t int_from_json(const json &j, std::string_view field) {
    try
    {
        auto v = j.at(field).get<std::string_view>();
        if (v.empty()) {
            return 0;
        }
        return std::stoi(v.data());
    }
    catch(const std::exception& e)
    {
        LOG_INFO("{} {}", field, j.dump());
        LOG_ERROR(e.what());
    }
    return 0;
}

constexpr double epsilon = 1e-9;
constexpr double default_norm_factor = 1e8;

inline static double fix_floating_error(double value, int32_t norm_factor = default_norm_factor) {
    auto v = value + epsilon;
    return ((double)((int64_t)(v * norm_factor)) / norm_factor);
}

inline static uint32_t compute_number_decimals(double value) {
    value = std::abs(fix_floating_error(value));
    uint32_t count = 0;
    while (value - std::floor(value) > epsilon) {
        ++count;
        value *= 10;
    }
    return count;
}

inline std::string to_string(double value, double min_increment) {
    std::ostringstream oss;
    oss.precision(compute_number_decimals(min_increment));
    oss << std::fixed << fix_floating_error(value);
    return oss.str();
}

inline std::string to_string(double value, Side side, double min_increment) {
    std::ostringstream oss;
    oss.precision(compute_number_decimals(min_increment));
    if (side == Side::BUY) {
        oss << std::fixed << (value - 0.5 * min_increment);
    }
    else {
        oss << std::fixed << (value + 0.5 * min_increment);
    }
    return oss.str();
}

#define TIMESTAMP_FROM_JSON(j, o, field) o.field = milliseconds_from_json(j, #field)
#define NANOSECONDS_FROM_JSON(j, o, field) o.field = nanoseconds_from_json(j, #field)
#define DOUBLE_FROM_JSON(j, o, field) o.field = double_from_json(j, #field)
#define INT_FROM_JSON(j, o, field) o.field = int_from_json(j, #field)
#define VARIABLE_FROM_JSON(j, o, field) if (j.contains(#field)) try { j.at(#field).get_to(o.field); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
#define BOOL_FROM_JSON(j, o, field) if (j.contains(#field) && !j[#field].is_null() && j[#field].is_string()) { if (j[#field] == "true") o.field = true; else if (j[#field] == "false") o.field = false; } else try { j.at(#field).get_to(o.field); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
#define STRUCT_FROM_JSON(j, o, field) if (j.contains(#field) && !j[#field].is_null()) from_json(j[#field], o.field)
#define ENUM_FROM_JSON(j, o, field) if (j.contains(#field)) try { o.field = to_##field(j.at(#field).get<std::string_view>()); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
}   // end namespace coinbase