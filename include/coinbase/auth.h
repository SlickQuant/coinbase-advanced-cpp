#pragma once

#include <string>
#define JWT_DISABLE_PICOJSON
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <random>
#include <coinbase/utils.h>

namespace coinbase {

inline std::string fix_pem_format(std::string key) {
    // Remove any quotes that might have been added
    if (key.front() == '"' && key.back() == '"') {
        key = key.substr(1, key.length() - 2);
    }
    
    // Replace literal \n with actual newlines
    size_t pos = 0;
    while ((pos = key.find("\\n", pos)) != std::string::npos) {
        key.replace(pos, 2, "\n");
        pos += 1;
    }
    return key;
}

inline std::string generate_coinbase_jwt(const char* uri = nullptr)
{
    static auto api_key = get_env("COINBASE_API_KEY");
    static auto api_secret = get_env("COINBASE_API_SECRET");
    static std::string private_key = fix_pem_format(api_secret);

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::minutes(2);

    // Generate random nonce (hex string)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    std::stringstream nonce_stream;
    nonce_stream << std::hex << dis(gen) << dis(gen);
    std::string nonce = nonce_stream.str();

    // Create the JWT
    auto jwt = jwt::create<jwt::traits::nlohmann_json>()
        .set_type("JWT")
        .set_issuer("cdp")
        .set_subject(api_key)
        .set_not_before(now)
        .set_expires_at(exp)
        .set_algorithm("ES256")
        .set_header_claim("kid", jwt::basic_claim<jwt::traits::nlohmann_json>(api_key))
        .set_header_claim("nonce", jwt::basic_claim<jwt::traits::nlohmann_json>(nonce));
    
    if (uri) {
        jwt.set_payload_claim("uri", jwt::basic_claim<jwt::traits::nlohmann_json>(std::string(uri)));
    }
    return jwt.sign(jwt::algorithm::es256("", private_key, "", ""));
}

}   // namespace coinbase