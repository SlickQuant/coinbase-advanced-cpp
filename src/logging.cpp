#include <coinbase/logging.hpp>
#include <slick/net/logging.hpp>

namespace coinbase::logging {

void set_log_handler(LogHandler handler) {
    slick::net::set_log_handler(std::move(handler));
}

void clear_log_handler() noexcept {
    slick::net::clear_log_handler();
}

} // namespace coinbase::logging
