#pragma once

#include "order.h"
#include <cstdint>
#include <chrono>

namespace orderbook {

enum class TickType {
    ORDER_ADDED,
    ORDER_REMOVED,
    ORDER_MODIFIED,
    TRADE_EXECUTED
};

struct TickData {
    TickType type;
    uint64_t order_id;
    OrderSide side;
    double price;
    uint64_t quantity;
    std::chrono::nanoseconds timestamp;
    
    // For trade execution
    double execution_price = 0.0;
    uint64_t execution_quantity = 0;
    
    TickData(TickType t, uint64_t id, OrderSide s, double p, uint64_t q)
        : type(t), order_id(id), side(s), price(p), quantity(q),
          timestamp(std::chrono::steady_clock::now().time_since_epoch()) {}
};

} // namespace orderbook
