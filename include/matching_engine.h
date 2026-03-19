#pragma once

#include "order.h"
#include "pool_allocator.h"
#include <map>
#include <list>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace orderbook {

// Price-time priority matching engine.
//
// Supports four order types:
//   LIMIT   — rests in the book at a specified price; fills when crossed
//   MARKET  — fills immediately at the best available prices
//   STOP    — waits off-book until last_trade_price crosses stop_price, then
//             becomes a market order
//   ICEBERG — only visible_quantity is shown in the book; when that slice
//             fully fills, the next slice is loaded from hidden_quantity
//             (loaded orders go to the BACK of the queue, losing time priority)
//
// Memory: Order objects are allocated from a PoolAllocator<Order> to avoid
// malloc jitter on the hot path.
class MatchingEngine {
public:
    // -----------------------------------------------------------------------
    // Fill record
    // -----------------------------------------------------------------------
    struct Fill {
        uint64_t buy_order_id;
        uint64_t sell_order_id;
        double   price;
        uint64_t quantity;
        std::chrono::nanoseconds timestamp;
    };

    MatchingEngine();
    ~MatchingEngine();

    // -----------------------------------------------------------------------
    // Order submission
    // -----------------------------------------------------------------------
    uint64_t submitLimitOrder(OrderSide side, double price, uint64_t quantity);

    uint64_t submitMarketOrder(OrderSide side, uint64_t quantity);

    // Stop order: stored off-book. When a trade executes at a price that
    // crosses stop_price (>= for BUY, <= for SELL), the stop converts to a
    // market order.
    uint64_t submitStopOrder(OrderSide side, double stop_price, uint64_t quantity);

    // Iceberg order: only visible_quantity enters the book. The remaining
    // (total_quantity - visible_quantity) is held as hidden_quantity and
    // is loaded one slice at a time after each visible fill.
    uint64_t submitIcebergOrder(OrderSide side, double price,
                                uint64_t total_quantity, uint64_t visible_quantity);

    // Cancel a resting or pending stop order. Returns false if not found.
    bool cancelOrder(uint64_t order_id);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------
    double   getBestBid() const;
    double   getBestAsk() const;
    double   getSpread()  const;

    uint64_t getBestBidQuantity() const;
    uint64_t getBestAskQuantity() const;

    // Top-N depth (price, quantity) pairs
    void getBidDepth(std::size_t levels,
                     std::vector<std::pair<double, uint64_t>>& out) const;
    void getAskDepth(std::size_t levels,
                     std::vector<std::pair<double, uint64_t>>& out) const;

    std::size_t getOrderCount()   const { return orders_.size(); }
    double      getLastTradePrice() const { return last_trade_price_; }

    const std::vector<Fill>& getFills() const { return fills_; }
    void clearFills() { fills_.clear(); }

private:
    // -----------------------------------------------------------------------
    // Price level: FIFO queue of resting orders
    // -----------------------------------------------------------------------
    struct PriceLevel {
        std::list<Order*> orders;
        uint64_t          total_quantity = 0;
    };

    // Bids: highest price first
    std::map<double, PriceLevel, std::greater<double>> bids_;
    // Asks: lowest price first
    std::map<double, PriceLevel>                        asks_;

    // -----------------------------------------------------------------------
    // Per-order metadata for O(1) cancellation
    // -----------------------------------------------------------------------
    struct OrderLocation {
        bool is_bid  = true;
        bool is_stop = false;
        // Both are stored; only the one matching is_bid is valid.
        // std::map::iterator is default-constructible, so no union needed.
        std::map<double, PriceLevel, std::greater<double>>::iterator bid_level_it;
        std::map<double, PriceLevel>::iterator                       ask_level_it;
        std::list<Order*>::iterator                                  list_it;
    };

    std::unordered_map<uint64_t, Order*>         orders_;
    std::unordered_map<uint64_t, OrderLocation>  locations_;

    // Off-book stop orders
    std::vector<Order*> pending_stop_buy_;   // trigger when price >= stop_price
    std::vector<Order*> pending_stop_sell_;  // trigger when price <= stop_price

    std::vector<Fill> fills_;
    uint64_t          next_order_id_     = 1;
    double            last_trade_price_  = 0.0;

    PoolAllocator<Order> pool_;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------
    uint64_t generateId() { return next_order_id_++; }

    // Place a resting order into the book (no matching).
    void addToBook(Order* order);

    // Remove a resting order from the book and free it.
    void removeFromBook(uint64_t order_id);

    // Try to match crossed orders; generates Fill records.
    void tryMatch();

    // After a trade at trade_price, trigger any stop orders that crossed.
    void checkStopOrders(double trade_price);

    // Execute a market-style sweep against the opposite side.
    // quantity is updated in-place to reflect remaining unfilled shares.
    void sweepBook(Order* aggressor);

    // Handle iceberg replenishment after the visible slice of `order` fully fills.
    // Returns true if a new slice was loaded (order stays in book).
    bool replenishIceberg(Order* order, PriceLevel& level);

    // Record a fill and update last_trade_price_.
    void recordFill(uint64_t buy_id, uint64_t sell_id,
                    double price, uint64_t quantity);
};

} // namespace orderbook
