#pragma once

#include "order.h"
#include "tick_data.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace orderbook {

// Price level: aggregates all orders at a specific price
struct PriceLevel {
    double price;
    uint64_t total_quantity;
    Order* first_order; // Head of linked list of orders at this price
    
    PriceLevel(double p) : price(p), total_quantity(0), first_order(nullptr) {}
};

// Ultra-fast order book implementation
// TODO: Optimize with lock-free structures, cache-aligned memory, SIMD
class OrderBook {
public:
    OrderBook();
    ~OrderBook();
    
    // Process a tick update
    void processTick(const TickData& tick);
    
    // Get best bid/ask
    double getBestBid() const;
    double getBestAsk() const;
    double getSpread() const;
    
    // Get total quantity at best bid/ask
    uint64_t getBestBidQuantity() const;
    uint64_t getBestAskQuantity() const;
    
    // Get depth at a price level
    uint64_t getBidQuantity(double price) const;
    uint64_t getAskQuantity(double price) const;
    
    // Get order book depth (top N levels)
    struct DepthLevel {
        double price;
        uint64_t quantity;
    };
    void getBidDepth(size_t levels, std::vector<DepthLevel>& result) const;
    void getAskDepth(size_t levels, std::vector<DepthLevel>& result) const;
    
    // Statistics
    uint64_t getTotalBidVolume() const;
    uint64_t getTotalAskVolume() const;
    
    // Clear the order book
    void clear();
    
private:
    // Bids: price -> PriceLevel (sorted descending)
    std::map<double, PriceLevel, std::greater<double>> bids_;
    
    // Asks: price -> PriceLevel (sorted ascending)
    std::map<double, PriceLevel> asks_;
    
    // Order lookup: order_id -> Order*
    std::unordered_map<uint64_t, Order*> orders_;
    
    // Thread safety (TODO: Replace with lock-free)
    mutable std::mutex mutex_;
    
    // Helper methods
    void addOrder(OrderSide side, double price, uint64_t quantity, uint64_t order_id);
    void removeOrder(uint64_t order_id);
    void modifyOrder(uint64_t order_id, uint64_t new_quantity);
    PriceLevel* getOrCreatePriceLevel(OrderSide side, double price);
};

} // namespace orderbook
