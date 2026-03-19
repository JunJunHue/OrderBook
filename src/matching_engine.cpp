#include "matching_engine.h"
#include <algorithm>
#include <stdexcept>

namespace orderbook {

// ============================================================
// Construction / Destruction
// ============================================================

MatchingEngine::MatchingEngine() = default;

MatchingEngine::~MatchingEngine() {
    for (auto& [id, order] : orders_) {
        order->~Order();
        pool_.deallocate(order);
    }
    for (auto* o : pending_stop_buy_)  { o->~Order(); pool_.deallocate(o); }
    for (auto* o : pending_stop_sell_) { o->~Order(); pool_.deallocate(o); }
}

// ============================================================
// Order submission
// ============================================================

uint64_t MatchingEngine::submitLimitOrder(OrderSide side, double price, uint64_t quantity) {
    uint64_t id = generateId();
    Order* order = pool_.allocate();
    new (order) Order(id, side, OrderType::LIMIT, price, quantity);

    orders_[id] = order;
    addToBook(order);
    tryMatch();
    return id;
}

uint64_t MatchingEngine::submitMarketOrder(OrderSide side, uint64_t quantity) {
    uint64_t id = generateId();
    Order* order = pool_.allocate();
    new (order) Order(id, side, OrderType::MARKET, 0.0, quantity);
    // Market orders are not tracked in orders_ / locations_ because they are
    // consumed immediately and never rest in the book.
    sweepBook(order);

    order->~Order();
    pool_.deallocate(order);
    return id;
}

uint64_t MatchingEngine::submitStopOrder(OrderSide side, double stop_price, uint64_t quantity) {
    uint64_t id = generateId();
    Order* order = pool_.allocate();
    new (order) Order(id, side, OrderType::STOP, stop_price, quantity);
    order->stop_price = stop_price;

    orders_[id] = order;
    OrderLocation loc;
    loc.is_bid  = (side == OrderSide::BID);
    loc.is_stop = true;
    locations_[id] = loc;

    if (side == OrderSide::BID) {
        pending_stop_buy_.push_back(order);
    } else {
        pending_stop_sell_.push_back(order);
    }

    // Check whether it should trigger immediately based on last known price
    if (last_trade_price_ > 0.0) {
        checkStopOrders(last_trade_price_);
    }
    return id;
}

uint64_t MatchingEngine::submitIcebergOrder(OrderSide side, double price,
                                            uint64_t total_quantity,
                                            uint64_t visible_qty) {
    if (visible_qty == 0 || visible_qty > total_quantity) {
        visible_qty = total_quantity;
    }
    uint64_t id = generateId();
    Order* order = pool_.allocate();
    new (order) Order(id, side, OrderType::ICEBERG, price, visible_qty);
    order->visible_quantity = visible_qty;
    order->hidden_quantity  = total_quantity - visible_qty;

    orders_[id] = order;
    addToBook(order);
    tryMatch();
    return id;
}

bool MatchingEngine::cancelOrder(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return false;

    Order* order = it->second;

    if (order->type == OrderType::STOP) {
        // Remove from pending stop list
        auto& vec = (order->side == OrderSide::BID) ? pending_stop_buy_ : pending_stop_sell_;
        vec.erase(std::find(vec.begin(), vec.end(), order));
        orders_.erase(it);
        locations_.erase(order_id);
        order->~Order();
        pool_.deallocate(order);
        return true;
    }

    removeFromBook(order_id);
    return true;
}

// ============================================================
// Queries
// ============================================================

double MatchingEngine::getBestBid() const {
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double MatchingEngine::getBestAsk() const {
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

double MatchingEngine::getSpread() const {
    double bid = getBestBid();
    double ask = getBestAsk();
    return (bid > 0.0 && ask > 0.0) ? ask - bid : 0.0;
}

uint64_t MatchingEngine::getBestBidQuantity() const {
    return bids_.empty() ? 0 : bids_.begin()->second.total_quantity;
}

uint64_t MatchingEngine::getBestAskQuantity() const {
    return asks_.empty() ? 0 : asks_.begin()->second.total_quantity;
}

void MatchingEngine::getBidDepth(std::size_t levels,
                                 std::vector<std::pair<double, uint64_t>>& out) const {
    out.clear();
    for (const auto& [price, level] : bids_) {
        if (out.size() >= levels) break;
        out.emplace_back(price, level.total_quantity);
    }
}

void MatchingEngine::getAskDepth(std::size_t levels,
                                 std::vector<std::pair<double, uint64_t>>& out) const {
    out.clear();
    for (const auto& [price, level] : asks_) {
        if (out.size() >= levels) break;
        out.emplace_back(price, level.total_quantity);
    }
}

// ============================================================
// Internal: add resting order to book (no matching)
// ============================================================

void MatchingEngine::addToBook(Order* order) {
    OrderLocation loc;
    if (order->side == OrderSide::BID) {
        auto& level = bids_[order->price];
        level.orders.push_back(order);
        level.total_quantity += order->quantity;

        loc.is_bid       = true;
        loc.bid_level_it = bids_.find(order->price);
        loc.list_it      = std::prev(level.orders.end());
    } else {
        auto& level = asks_[order->price];
        level.orders.push_back(order);
        level.total_quantity += order->quantity;

        loc.is_bid       = false;
        loc.ask_level_it = asks_.find(order->price);
        loc.list_it      = std::prev(level.orders.end());
    }
    locations_[order->order_id] = loc;
}

// ============================================================
// Internal: remove resting order from book and free memory
// ============================================================

void MatchingEngine::removeFromBook(uint64_t order_id) {
    auto loc_it = locations_.find(order_id);
    if (loc_it == locations_.end()) return;

    OrderLocation& loc = loc_it->second;
    Order* order = orders_[order_id];

    if (loc.is_bid) {
        PriceLevel& level = loc.bid_level_it->second;
        level.total_quantity -= order->quantity;
        level.orders.erase(loc.list_it);
        if (level.orders.empty()) {
            bids_.erase(loc.bid_level_it);
        }
    } else {
        PriceLevel& level = loc.ask_level_it->second;
        level.total_quantity -= order->quantity;
        level.orders.erase(loc.list_it);
        if (level.orders.empty()) {
            asks_.erase(loc.ask_level_it);
        }
    }

    locations_.erase(loc_it);
    orders_.erase(order_id);
    order->~Order();
    pool_.deallocate(order);
}

// ============================================================
// Internal: price-time priority matching
// ============================================================

void MatchingEngine::tryMatch() {
    while (!bids_.empty() && !asks_.empty()) {
        auto bid_level_it = bids_.begin();
        auto ask_level_it = asks_.begin();

        double best_bid = bid_level_it->first;
        double best_ask = ask_level_it->first;

        if (best_bid < best_ask) break; // No crossing

        PriceLevel& bid_level = bid_level_it->second;
        PriceLevel& ask_level = ask_level_it->second;

        Order* bid_order = bid_level.orders.front();
        Order* ask_order = ask_level.orders.front();

        // Fill at the maker's price.
        // The order with the earlier timestamp is the passive (maker) side.
        double fill_price = (bid_order->timestamp <= ask_order->timestamp)
                                ? best_bid   // bid was there first → bid is maker
                                : best_ask;  // ask was there first → ask is maker

        uint64_t fill_qty = std::min(bid_order->quantity, ask_order->quantity);

        recordFill(bid_order->order_id, ask_order->order_id, fill_price, fill_qty);

        bid_order->quantity -= fill_qty;
        ask_order->quantity -= fill_qty;
        bid_level.total_quantity -= fill_qty;
        ask_level.total_quantity -= fill_qty;

        // Handle fully-filled bid
        if (bid_order->quantity == 0) {
            if (!replenishIceberg(bid_order, bid_level)) {
                // Completely filled — remove
                locations_.erase(bid_order->order_id);
                orders_.erase(bid_order->order_id);
                bid_level.orders.pop_front();
                bid_order->~Order();
                pool_.deallocate(bid_order);
            }
        }

        // Handle fully-filled ask
        if (ask_order->quantity == 0) {
            if (!replenishIceberg(ask_order, ask_level)) {
                locations_.erase(ask_order->order_id);
                orders_.erase(ask_order->order_id);
                ask_level.orders.pop_front();
                ask_order->~Order();
                pool_.deallocate(ask_order);
            }
        }

        // Clean up empty price levels
        if (bid_level.orders.empty()) bids_.erase(bid_level_it);
        if (ask_level.orders.empty()) asks_.erase(ask_level_it);
    }

    if (last_trade_price_ > 0.0) {
        checkStopOrders(last_trade_price_);
    }
}

// ============================================================
// Internal: market-order sweep
// ============================================================

void MatchingEngine::sweepBook(Order* aggressor) {
    auto& opposite_bids = bids_;
    auto& opposite_asks = asks_;

    if (aggressor->side == OrderSide::BID) {
        // Buy sweep: eat through asks (lowest first)
        while (aggressor->quantity > 0 && !opposite_asks.empty()) {
            auto level_it     = opposite_asks.begin();
            PriceLevel& level = level_it->second;
            double price      = level_it->first;

            while (aggressor->quantity > 0 && !level.orders.empty()) {
                Order* maker    = level.orders.front();
                uint64_t fill_q = std::min(aggressor->quantity, maker->quantity);

                recordFill(aggressor->order_id, maker->order_id, price, fill_q);

                aggressor->quantity -= fill_q;
                maker->quantity     -= fill_q;
                level.total_quantity -= fill_q;

                if (maker->quantity == 0) {
                    if (!replenishIceberg(maker, level)) {
                        locations_.erase(maker->order_id);
                        orders_.erase(maker->order_id);
                        level.orders.pop_front();
                        maker->~Order();
                        pool_.deallocate(maker);
                    }
                }
            }
            if (level.orders.empty()) opposite_asks.erase(level_it);
        }
    } else {
        // Sell sweep: eat through bids (highest first)
        while (aggressor->quantity > 0 && !opposite_bids.empty()) {
            auto level_it     = opposite_bids.begin();
            PriceLevel& level = level_it->second;
            double price      = level_it->first;

            while (aggressor->quantity > 0 && !level.orders.empty()) {
                Order* maker    = level.orders.front();
                uint64_t fill_q = std::min(aggressor->quantity, maker->quantity);

                recordFill(maker->order_id, aggressor->order_id, price, fill_q);

                aggressor->quantity -= fill_q;
                maker->quantity     -= fill_q;
                level.total_quantity -= fill_q;

                if (maker->quantity == 0) {
                    if (!replenishIceberg(maker, level)) {
                        locations_.erase(maker->order_id);
                        orders_.erase(maker->order_id);
                        level.orders.pop_front();
                        maker->~Order();
                        pool_.deallocate(maker);
                    }
                }
            }
            if (level.orders.empty()) opposite_bids.erase(level_it);
        }
    }

    if (last_trade_price_ > 0.0) {
        checkStopOrders(last_trade_price_);
    }
}

// ============================================================
// Internal: stop order triggering
// ============================================================

void MatchingEngine::checkStopOrders(double trade_price) {
    // BUY stops trigger when trade_price >= stop_price
    std::vector<Order*> triggered_buys;
    for (auto it = pending_stop_buy_.begin(); it != pending_stop_buy_.end(); ) {
        if (trade_price >= (*it)->stop_price) {
            triggered_buys.push_back(*it);
            it = pending_stop_buy_.erase(it);
        } else {
            ++it;
        }
    }

    // SELL stops trigger when trade_price <= stop_price
    std::vector<Order*> triggered_sells;
    for (auto it = pending_stop_sell_.begin(); it != pending_stop_sell_.end(); ) {
        if (trade_price <= (*it)->stop_price) {
            triggered_sells.push_back(*it);
            it = pending_stop_sell_.erase(it);
        } else {
            ++it;
        }
    }

    // Convert triggered stops to market orders
    for (Order* stop : triggered_buys) {
        locations_.erase(stop->order_id);
        orders_.erase(stop->order_id);
        uint64_t qty = stop->quantity;
        stop->~Order();
        pool_.deallocate(stop);
        submitMarketOrder(OrderSide::BID, qty);
    }
    for (Order* stop : triggered_sells) {
        locations_.erase(stop->order_id);
        orders_.erase(stop->order_id);
        uint64_t qty = stop->quantity;
        stop->~Order();
        pool_.deallocate(stop);
        submitMarketOrder(OrderSide::ASK, qty);
    }
}

// ============================================================
// Internal: iceberg replenishment
// Returns true if the order was refilled and should stay in book.
// ============================================================

bool MatchingEngine::replenishIceberg(Order* order, PriceLevel& level) {
    if (order->type != OrderType::ICEBERG || order->hidden_quantity == 0) {
        return false;
    }

    // Load the next slice
    uint64_t refill = std::min(order->visible_quantity, order->hidden_quantity);
    order->quantity         = refill;
    order->hidden_quantity -= refill;
    level.total_quantity   += refill;

    // Move to back of queue (loses time priority, standard iceberg behaviour)
    level.orders.pop_front();
    level.orders.push_back(order);

    // Update location iterator
    auto& loc = locations_[order->order_id];
    loc.list_it = std::prev(level.orders.end());

    return true;
}

// ============================================================
// Internal: record fill
// ============================================================

void MatchingEngine::recordFill(uint64_t buy_id, uint64_t sell_id,
                                double price, uint64_t quantity) {
    fills_.push_back({
        buy_id,
        sell_id,
        price,
        quantity,
        std::chrono::steady_clock::now().time_since_epoch()
    });
    last_trade_price_ = price;
}

} // namespace orderbook
