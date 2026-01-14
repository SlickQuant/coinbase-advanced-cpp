// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

namespace coinbase {

enum class ContractExpiryType : uint8_t {
    UNKNOWN_CONTRACT_EXPIRY_TYPE,
    EXPIRING,
    PERPETUAL,
};

inline ContractExpiryType to_contract_expiry_type(std::string_view cet) {
    if (cet == "EXPIRING") {
        return ContractExpiryType::EXPIRING;
    }

    if (cet == "PERPETUAL") {
        return ContractExpiryType::PERPETUAL;
    }

    return ContractExpiryType::UNKNOWN_CONTRACT_EXPIRY_TYPE;
}

inline std::string to_string(ContractExpiryType cet) {
    switch(cet) {
    case ContractExpiryType::UNKNOWN_CONTRACT_EXPIRY_TYPE:
        return "UNKNOWN_CONTRACT_EXPIRY_TYPE";
    case ContractExpiryType::EXPIRING:
        return "EXPIRING";
    case ContractExpiryType::PERPETUAL:
        return "PERPETUAL";
    }
    return "UNKNOWN_CONTRACT_EXPIRY_TYPE";
}

struct Commission {
    double total_commission;
    double gst_commission;
    double withholding_commission;
    double client_commission;
    double venue_commission;
    double regulatory_commission;
    double clearing_commission;
};

inline void from_json(const json &j, Commission &c) {
    DOUBLE_FROM_JSON(j, c, total_commission);
    DOUBLE_FROM_JSON(j, c, gst_commission);
    DOUBLE_FROM_JSON(j, c, withholding_commission);
    DOUBLE_FROM_JSON(j, c, client_commission);
    DOUBLE_FROM_JSON(j, c, venue_commission);
    DOUBLE_FROM_JSON(j, c, regulatory_commission);
    DOUBLE_FROM_JSON(j, c, clearing_commission);
}

enum class OrderType : uint8_t {
    UNKNOWN_ORDER_TYPE,
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT,
    BRACKET,
    TWAP,
    ROLL_OPEN,
    ROLL_CLOSE,
    LIQUIDATION,
    SCALED,
};

inline OrderType to_order_type(std::string_view ot) {
    if (ot == "MARKET" || ot == "Market") {
        return OrderType::MARKET;
    }

    if (ot == "LIMIT" || ot == "Limit") {
        return OrderType::LIMIT;
    }

    if (ot == "STOP" || ot == "Stop") {
        return OrderType::STOP;
    }

    if (ot == "STOP_LIMIT" || ot == "Stop_Limit") {
        return OrderType::STOP_LIMIT;
    }

    if (ot == "BRACKET" || ot == "Bracket") {
        return OrderType::BRACKET;
    }

    if (ot == "TWAP" || ot == "Twap") {
        return OrderType::TWAP;
    }

    if (ot == "ROLL_OPEN" || ot == "Roll_Open") {
        return OrderType::ROLL_OPEN;
    }

    if (ot == "ROLL_CLOSE" || ot == "Roll_Close") {
        return OrderType::ROLL_CLOSE;
    }

    if (ot == "LIQUIDATION" || ot == "Liquidation") {
        return OrderType::LIQUIDATION;
    }

    if (ot == "SCALED" || ot == "Scaled") {
        return OrderType::SCALED;
    }

    return OrderType::UNKNOWN_ORDER_TYPE;
}

inline std::string to_string(OrderType ot) {
    switch(ot) {
    case OrderType::UNKNOWN_ORDER_TYPE:
        return "UNKNOWN_ORDER_TYPE";
    case OrderType::MARKET:
        return "MARKET";
    case OrderType::LIMIT:
        return "LIMIT";
    case OrderType::STOP:
        return "STOP";
    case OrderType::STOP_LIMIT:
        return "STOP_LIMIT";
    case OrderType::BRACKET:
        return "BRACKET";
    case OrderType::TWAP:
        return "TWAP";
    case OrderType::ROLL_OPEN:
        return "ROLL_OPEN";
    case OrderType::ROLL_CLOSE:
        return "ROLL_CLOSE";
    case OrderType::LIQUIDATION:
        return "LIQUIDATION";
    case OrderType::SCALED:
        return "SCALED";
    }
    return "UNKNOWN_ORDER_TYPE";
}

enum class ProductType : uint8_t {
    UNKNOWN_PRODUCT_TYPE,
    SPOT,
    FUTURE,
};

inline ProductType to_product_type(std::string_view sv) {
    if (sv == "SPOT") {
        return ProductType::SPOT;
    }

    if (sv == "FUTURE") {
        return ProductType::FUTURE;
    }

    return ProductType::UNKNOWN_PRODUCT_TYPE;
}

inline std::string to_string(ProductType type) {
    switch(type) {
    case ProductType::UNKNOWN_PRODUCT_TYPE:
        return "UNKNOWN_PRODUCT_TYPE";
    case ProductType::SPOT:
        return "SPOT";
    case ProductType::FUTURE:
        return "FUTURE";
    }
    return "UNKNOWN_PRODUCT_TYPE";
}

}