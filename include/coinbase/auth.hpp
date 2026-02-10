// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

#include <string>

namespace coinbase {

// Fix PEM format by removing quotes and replacing literal \n with newlines
std::string fix_pem_format(std::string key);

// Generate Coinbase JWT token for API authentication
std::string generate_coinbase_jwt(const char* uri = nullptr);

}   // namespace coinbase
