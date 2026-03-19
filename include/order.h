#pragma once

#include <cstdint>
#include <chrono>

namespace orderbook {

enum class OrderSide {
    BID,   // Buy order
    ASK    // Sell order
};

enum class OrderType {
    LIMIT,   // Limit order (price specified)
    MARKET,  // Market order (execute immediately)
    STOP,    // Stop order (trigger at stop_price, then execute as market)
    ICEBERG  // Iceberg order (only visible_quantity shown; hidden refills after each fill)
};

struct Order {
    uint64_t order_id;
    OrderSide side;
    OrderType type;
    double price;           // Limit price (ignored for market orders; stop_price for STOP orders)
    uint64_t quantity;      // Remaining quantity (visible for iceberg)
    std::chrono::nanoseconds timestamp;

    // STOP order: trigger price
    double stop_price = 0.0;

    // ICEBERG order: original visible slice size and remaining hidden quantity
    uint64_t visible_quantity = 0;
    uint64_t hidden_quantity  = 0;

    // For tracking in order book (legacy linked-list fields, kept for OrderBook compatibility)
    Order* next = nullptr;
    Order* prev = nullptr;

    Order(uint64_t id, OrderSide s, OrderType t, double p, uint64_t q)
        : order_id(id), side(s), type(t), price(p), quantity(q),
          timestamp(std::chrono::steady_clock::now().time_since_epoch()) {}
};

} // namespace orderbook
