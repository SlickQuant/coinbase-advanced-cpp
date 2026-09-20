// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <gtest/gtest.h>
#include <coinbase/utils.hpp>

namespace coinbase::tests {

// Authenticated endpoints sign a JWT with COINBASE_API_KEY and COINBASE_API_SECRET, and
// generate_coinbase_jwt() throws "at least one of public or private key need to be
// present" when the secret is empty. A machine without credentials - CI, or a fresh
// clone - cannot run those tests at all, so they skip rather than fail. Public endpoints
// (server time, public products) need no credentials and always run.
inline bool hasApiCredentials() {
    static const bool available = !get_env("COINBASE_API_KEY").empty() &&
                                  !get_env("COINBASE_API_SECRET").empty();
    return available;
}

}   // namespace coinbase::tests

// First line of every test that reaches an authenticated endpoint.
#define SKIP_WITHOUT_API_CREDENTIALS()                                                  \
    do {                                                                                \
        if (!::coinbase::tests::hasApiCredentials()) {                                  \
            GTEST_SKIP() << "COINBASE_API_KEY and COINBASE_API_SECRET are not set";     \
        }                                                                               \
    } while (false)
