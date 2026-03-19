#include "order_book.h"
#include <algorithm>
#include <stdexcept>

namespace orderbook {

OrderBook::OrderBook() = default;

OrderBook::~OrderBook() {
    clear();
}

void OrderBook::processTick(const TickData& tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (tick.type) {
        case TickType::ORDER_ADDED:
            addOrder(tick.side, tick.price, tick.quantity, tick.order_id);
            break;
        case TickType::ORDER_REMOVED:
            removeOrder(tick.order_id);
            break;
        case TickType::ORDER_MODIFIED:
            modifyOrder(tick.order_id, tick.quantity);
            break;
        case TickType::TRADE_EXECUTED:
            // Handle trade execution (remove filled orders)
            removeOrder(tick.order_id);
            break;
    }
}

double OrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) return 0.0;
    return bids_.begin()->first;
}

double OrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) return 0.0;
    return asks_.begin()->first;
}

double OrderBook::getSpread() const {
    double bid = getBestBid();
    double ask = getBestAsk();
    if (bid == 0.0 || ask == 0.0) return 0.0;
    return ask - bid;
}

uint64_t OrderBook::getBestBidQuantity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) return 0;
    return bids_.begin()->second.total_quantity;
}

uint64_t OrderBook::getBestAskQuantity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) return 0;
    return asks_.begin()->second.total_quantity;
}

uint64_t OrderBook::getBidQuantity(double price) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = bids_.find(price);
    if (it == bids_.end()) return 0;
    return it->second.total_quantity;
}

uint64_t OrderBook::getAskQuantity(double price) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = asks_.find(price);
    if (it == asks_.end()) return 0;
    return it->second.total_quantity;
}

void OrderBook::getBidDepth(size_t levels, std::vector<DepthLevel>& result) const {
    std::lock_guard<std::mutex> lock(mutex_);
    result.clear();
    result.reserve(levels);
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count++ >= levels) break;
        result.push_back({price, level.total_quantity});
    }
}

void OrderBook::getAskDepth(size_t levels, std::vector<DepthLevel>& result) const {
    std::lock_guard<std::mutex> lock(mutex_);
    result.clear();
    result.reserve(levels);
    
    size_t count = 0;
    for (const auto& [price, level] : asks_) {
        if (count++ >= levels) break;
        result.push_back({price, level.total_quantity});
    }
}

uint64_t OrderBook::getTotalBidVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [price, level] : bids_) {
        total += level.total_quantity;
    }
    return total;
}

uint64_t OrderBook::getTotalAskVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [price, level] : asks_) {
        total += level.total_quantity;
    }
    return total;
}

void OrderBook::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Delete all orders
    for (auto& [id, order] : orders_) {
        delete order;
    }
    orders_.clear();
    bids_.clear();
    asks_.clear();
}

void OrderBook::addOrder(OrderSide side, double price, uint64_t quantity, uint64_t order_id) {
    // Create new order
    Order* order = new Order(order_id, side, OrderType::LIMIT, price, quantity);
    orders_[order_id] = order;
    
    // Get or create price level
    PriceLevel* level = getOrCreatePriceLevel(side, price);
    
    // Add to linked list
    order->next = level->first_order;
    if (level->first_order) {
        level->first_order->prev = order;
    }
    level->first_order = order;
    level->total_quantity += quantity;
}

void OrderBook::removeOrder(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;
    
    Order* order = it->second;
    double price = order->price;
    OrderSide side = order->side;
    
    // Remove from linked list
    if (order->prev) {
        order->prev->next = order->next;
    }
    if (order->next) {
        order->next->prev = order->prev;
    }
    
    // Update price level
    if (side == OrderSide::BID) {
        auto level_it = bids_.find(price);
        if (level_it != bids_.end()) {
            if (level_it->second.first_order == order) {
                level_it->second.first_order = order->next;
            }
            level_it->second.total_quantity -= order->quantity;
            if (level_it->second.total_quantity == 0) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            if (level_it->second.first_order == order) {
                level_it->second.first_order = order->next;
            }
            level_it->second.total_quantity -= order->quantity;
            if (level_it->second.total_quantity == 0) {
                asks_.erase(level_it);
            }
        }
    }
    
    delete order;
    orders_.erase(it);
}

void OrderBook::modifyOrder(uint64_t order_id, uint64_t new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;
    
    Order* order = it->second;
    double price = order->price;
    OrderSide side = order->side;
    
    int64_t quantity_diff = static_cast<int64_t>(new_quantity) - static_cast<int64_t>(order->quantity);
    order->quantity = new_quantity;
    
    // Update price level
    if (side == OrderSide::BID) {
        auto level_it = bids_.find(price);
        if (level_it != bids_.end()) {
            level_it->second.total_quantity += quantity_diff;
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            level_it->second.total_quantity += quantity_diff;
        }
    }
}

PriceLevel* OrderBook::getOrCreatePriceLevel(OrderSide side, double price) {
    if (side == OrderSide::BID) {
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            auto [new_it, inserted] = bids_.emplace(price, PriceLevel(price));
            return &new_it->second;
        }
        return &it->second;
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) {
            auto [new_it, inserted] = asks_.emplace(price, PriceLevel(price));
            return &new_it->second;
        }
        return &it->second;
    }
}

} // namespace orderbook
