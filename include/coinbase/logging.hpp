#pragma once

#include <slick/net/logging.hpp>

namespace coinbase::logging {

using LogLevel = slick::net::LogLevel;
using LogHandler = slick::net::LogHandler;

[[deprecated("Use slick::net::set_log_handler from <slick/net/logging.hpp>.")]]
void set_log_handler(LogHandler handler);

[[deprecated("Use slick::net::clear_log_handler from <slick/net/logging.hpp>.")]]
void clear_log_handler() noexcept;

} // namespace coinbase::logging
