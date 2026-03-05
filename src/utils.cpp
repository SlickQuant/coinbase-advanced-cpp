// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#include <coinbase/utils.hpp>

namespace coinbase {

std::string get_env(std::string_view name) {
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

std::string timestamp_to_string(uint64_t timestamp_ms) {
    // Convert milliseconds to seconds
    std::time_t seconds = timestamp_ms / 1000;
    int milliseconds = timestamp_ms % 1000;

    // Convert to tm struct (UTC)
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif

    // Format: YYYY-MM-DDTHH:MM:SS.mmmZ
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);

    // Append milliseconds
    char result[32];
    std::snprintf(result, sizeof(result), "%s.%03dZ", buffer, milliseconds);

    return std::string(result);
}

uint64_t to_milliseconds(const std::string &iso_str) {
    // Parse ISO 8601 format: "YYYY-MM-DDTHH:MM:SS.sssZ"
    std::tm tm = {};
    double fractional_seconds = 0.0;

    // Find the decimal point position
    size_t dot_pos = iso_str.find('.');
    if (dot_pos != std::string::npos) {
        // Parse base timestamp
        std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        // Extract fractional seconds (e.g., ".718887" -> 0.718887)
        size_t end_pos = iso_str.find_first_of("Z+", dot_pos);
        if (end_pos == std::string::npos) end_pos = iso_str.length();
        std::string frac_str = "0" + iso_str.substr(dot_pos, end_pos - dot_pos);
        fractional_seconds = std::stod(frac_str);
    } else {
        // No fractional seconds
        std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    }

    tm.tm_year -= 1900;  // tm_year is years since 1900
    tm.tm_mon -= 1;       // tm_mon is 0-11
    tm.tm_isdst = 0;      // Not daylight saving time

    // Convert to time_t (seconds since epoch, UTC)
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif

    // Convert to milliseconds and add the fractional part
    uint64_t milliseconds = static_cast<uint64_t>(fractional_seconds * 1000.0);
    return static_cast<uint64_t>(time) * 1000 + milliseconds;
}

uint64_t to_microseconds(const std::string &iso_str) {
    // Parse ISO 8601 format: "YYYY-MM-DDTHH:MM:SS.ssssssZ"
    std::tm tm = {};
    double fractional_seconds = 0.0;

    // Find the decimal point position
    size_t dot_pos = iso_str.find('.');
    if (dot_pos != std::string::npos) {
        // Parse base timestamp
        int parsed = std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                 &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        if (parsed < 6) {
            return 0;
        }

        // Extract fractional seconds (e.g., ".718887" -> 0.718887)
        size_t end_pos = iso_str.find_first_of("Z+", dot_pos);
        if (end_pos == std::string::npos) end_pos = iso_str.length();
        std::string frac_str = "0" + iso_str.substr(dot_pos, end_pos - dot_pos);
        fractional_seconds = std::stod(frac_str);
    } else {
        // No fractional seconds
        int parsed = std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                 &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        if (parsed < 6) {
            return 0;
        }
    }

    tm.tm_year -= 1900;  // tm_year is years since 1900
    tm.tm_mon -= 1;       // tm_mon is 0-11
    tm.tm_isdst = 0;      // Not daylight saving time

    // Convert to time_t (seconds since epoch, UTC)
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif

    // Convert to microseconds and add the fractional part
    uint64_t microseconds = static_cast<uint64_t>(fractional_seconds * 1000000.0);
    return static_cast<uint64_t>(time) * 1000000ULL + microseconds;
}

uint64_t to_nanoseconds(const std::string &iso_str) {
    // Parse ISO 8601 format: "YYYY-MM-DDTHH:MM:SS.sssssssssZ"
    std::tm tm = {};
    double fractional_seconds = 0.0;

    // Find the decimal point position
    size_t dot_pos = iso_str.find('.');
    if (dot_pos != std::string::npos) {
        // Parse base timestamp
        int parsed = std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                 &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        if (parsed < 6) {
            return 0;
        }

        // Extract fractional seconds (e.g., ".718887475" -> 0.718887475)
        size_t end_pos = iso_str.find_first_of("Z+", dot_pos);
        if (end_pos == std::string::npos) end_pos = iso_str.length();
        std::string frac_str = "0" + iso_str.substr(dot_pos, end_pos - dot_pos);
        fractional_seconds = std::stod(frac_str);
    } else {
        // No fractional seconds
        int parsed = std::sscanf(iso_str.c_str(), "%d-%d-%dT%d:%d:%d",
                                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                 &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        if (parsed < 6) {
            return 0;
        }
    }

    tm.tm_year -= 1900;  // tm_year is years since 1900
    tm.tm_mon -= 1;       // tm_mon is 0-11
    tm.tm_isdst = 0;      // Not daylight saving time

    // Convert to time_t (seconds since epoch, UTC)
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif

    // Convert to nanoseconds and add the fractional part
    uint64_t nanoseconds = static_cast<uint64_t>(fractional_seconds * 1000000000.0);
    return static_cast<uint64_t>(time) * 1000000000ULL + nanoseconds;
}

}   // namespace coinbase
