#include "market_microstructure_analyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace orderbook {

MarketMicrostructureAnalyzer::MarketMicrostructureAnalyzer(size_t window_size)
    : window_size_(window_size) {
}

void MarketMicrostructureAnalyzer::processTick(const TickData& tick, const OrderBook& book) {
    // Update spread
    double spread = book.getSpread();
    spreads_.push_back(spread);
    spread_sum_ += spread;
    spread_sum_sq_ += spread * spread;
    
    // Update volumes
    uint64_t bid_vol = book.getTotalBidVolume();
    uint64_t ask_vol = book.getTotalAskVolume();
    bid_volumes_.push_back(static_cast<double>(bid_vol));
    ask_volumes_.push_back(static_cast<double>(ask_vol));
    bid_volume_sum_ += bid_vol;
    ask_volume_sum_ += ask_vol;
    
    // Update order flow imbalance
    double imbalance = 0.0;
    if (tick.type == TickType::ORDER_ADDED) {
        imbalance = (tick.side == OrderSide::BID) ? static_cast<double>(tick.quantity) 
                                                   : -static_cast<double>(tick.quantity);
    } else if (tick.type == TickType::ORDER_REMOVED) {
        imbalance = (tick.side == OrderSide::BID) ? -static_cast<double>(tick.quantity) 
                                                   : static_cast<double>(tick.quantity);
    }
    order_flow_.push_back(imbalance);
    order_flow_sum_ += imbalance;
    
    // Track order events for cancellation rate
    if (tick.type == TickType::ORDER_ADDED) {
        recent_orders_.push_back({tick.order_id, tick.timestamp, false});
    } else if (tick.type == TickType::ORDER_REMOVED) {
        // Mark as cancelled if found
        for (auto& event : recent_orders_) {
            if (event.order_id == tick.order_id && !event.cancelled) {
                event.cancelled = true;
                break;
            }
        }
    }
    
    // Maintain window size
    if (spreads_.size() > window_size_) {
        double old_spread = spreads_.front();
        spreads_.pop_front();
        spread_sum_ -= old_spread;
        spread_sum_sq_ -= old_spread * old_spread;
    }
    
    if (bid_volumes_.size() > window_size_) {
        double old_bid = bid_volumes_.front();
        bid_volumes_.pop_front();
        bid_volume_sum_ -= old_bid;
    }
    
    if (ask_volumes_.size() > window_size_) {
        double old_ask = ask_volumes_.front();
        ask_volumes_.pop_front();
        ask_volume_sum_ -= old_ask;
    }
    
    if (order_flow_.size() > window_size_) {
        double old_flow = order_flow_.front();
        order_flow_.pop_front();
        order_flow_sum_ -= old_flow;
    }
    
    // Clean old order events
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto cutoff = now - std::chrono::milliseconds(100); // 100ms window
    recent_orders_.erase(
        std::remove_if(recent_orders_.begin(), recent_orders_.end(),
            [cutoff](const OrderEvent& e) { return e.timestamp < cutoff; }),
        recent_orders_.end()
    );
}

MarketMicrostructureAnalyzer::PatternMetrics MarketMicrostructureAnalyzer::getCurrentMetrics() const {
    PatternMetrics metrics;
    
    metrics.spread_mean = calculateSpreadMean();
    metrics.spread_std = calculateSpreadStd();
    
    if (bid_volume_sum_ + ask_volume_sum_ > 0) {
        metrics.bid_ask_imbalance = bid_volume_sum_ / (bid_volume_sum_ + ask_volume_sum_);
    } else {
        metrics.bid_ask_imbalance = 0.5;
    }
    
    if (!order_flow_.empty()) {
        metrics.order_flow_imbalance = order_flow_sum_ / order_flow_.size();
    }
    
    // Calculate cancellation rate
    size_t cancelled_count = 0;
    size_t total_recent = recent_orders_.size();
    for (const auto& event : recent_orders_) {
        if (event.cancelled) cancelled_count++;
    }
    metrics.cancellation_rate = (total_recent > 0) ? 
        static_cast<double>(cancelled_count) / total_recent : 0.0;
    
    // Simple hidden liquidity heuristic (TODO: improve with ML)
    metrics.hidden_liquidity_probability = (metrics.cancellation_rate > 0.5) ? 0.3 : 0.1;
    
    return metrics;
}

bool MarketMicrostructureAnalyzer::detectQuoteStuffing(double threshold) const {
    auto metrics = getCurrentMetrics();
    return metrics.cancellation_rate >= threshold;
}

bool MarketMicrostructureAnalyzer::detectHiddenLiquidity(double threshold) const {
    auto metrics = getCurrentMetrics();
    return metrics.hidden_liquidity_probability >= threshold;
}

bool MarketMicrostructureAnalyzer::detectInformedTrading(double threshold) const {
    auto metrics = getCurrentMetrics();
    // High order flow imbalance suggests informed trading
    return std::abs(metrics.order_flow_imbalance) >= threshold;
}

double MarketMicrostructureAnalyzer::estimateMarketImpact(uint64_t quantity, OrderSide side) const {
    auto metrics = getCurrentMetrics();
    
    // Simple linear market impact model
    // TODO: Implement Kyle's lambda or more sophisticated models
    double base_impact = metrics.spread_mean * 0.5; // Half spread
    double quantity_impact = static_cast<double>(quantity) * 0.0001; // $0.0001 per share
    
    return base_impact + quantity_impact;
}

void MarketMicrostructureAnalyzer::reset() {
    spreads_.clear();
    bid_volumes_.clear();
    ask_volumes_.clear();
    order_flow_.clear();
    recent_orders_.clear();
    
    spread_sum_ = 0.0;
    spread_sum_sq_ = 0.0;
    bid_volume_sum_ = 0.0;
    ask_volume_sum_ = 0.0;
    order_flow_sum_ = 0.0;
}

double MarketMicrostructureAnalyzer::calculateSpreadMean() const {
    if (spreads_.empty()) return 0.0;
    return spread_sum_ / spreads_.size();
}

double MarketMicrostructureAnalyzer::calculateSpreadStd() const {
    if (spreads_.size() < 2) return 0.0;
    
    double mean = calculateSpreadMean();
    double variance = (spread_sum_sq_ / spreads_.size()) - (mean * mean);
    return std::sqrt(std::max(0.0, variance));
}

} // namespace orderbook
