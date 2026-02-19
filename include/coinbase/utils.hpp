// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>
#include <stdlib.h>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <coinbase/side.hpp>
#include <slick/net/logging.hpp>

using json = nlohmann::json;

namespace coinbase {

// Get environment variable (cross-platform)
std::string get_env(std::string_view name);

// Convert milliseconds timestamp to ISO 8601 string
std::string timestamp_to_string(uint64_t timestamp_ms);

// Parse ISO 8601 string to milliseconds
uint64_t to_milliseconds(const std::string &iso_str);

// Parse ISO 8601 string to nanoseconds
uint64_t to_nanoseconds(const std::string &iso_str);

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
#define VARIABLE_FROM_JSON(j, o, field) if (j.contains(#field) && !j[#field].is_null()) try { j.at(#field).get_to(o.field); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
#define BOOL_FROM_JSON(j, o, field) if (j.contains(#field) && !j[#field].is_null() && j[#field].is_string()) { if (j[#field] == "true") o.field = true; else if (j[#field] == "false") o.field = false; } else try { j.at(#field).get_to(o.field); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
#define BOOL_FROM_JSON_VALUE(j, o, field, j_name) if (j.contains(j_name) && !j[j_name].is_null() && j[j_name].is_string()) { if (j[j_name] == "true") o.field = true; else if (j[j_name] == "false") o.field = false; } else try { j.at(j_name).get_to(o.field); } catch(const std::exception &e) { LOG_INFO(j_name " {}", j.dump()); LOG_ERROR(e.what()); }
#define STRUCT_FROM_JSON(j, o, field) if (j.contains(#field) && !j[#field].is_null()) from_json(j[#field], o.field)
#define ENUM_FROM_JSON(j, o, field) if (j.contains(#field)) try { o.field = to_##field(j.at(#field).get<std::string_view>()); } catch(const std::exception &e) { LOG_INFO(#field " {}", j.dump()); LOG_ERROR(e.what()); }
}   // end namespace coinbase