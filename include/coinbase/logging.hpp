#pragma once

#include <slick/net/logging.hpp>

namespace coinbase::logging {

using LogLevel = slick::net::LogLevel;
using LogHandler = slick::net::LogHandler;

void set_log_handler(LogHandler handler);
void clear_log_handler() noexcept;

} // namespace coinbase::logging
