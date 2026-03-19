#pragma once

#include "order_book.h"
#include "tick_data.h"
#include <deque>
#include <vector>
#include <atomic>
#include <chrono>

namespace orderbook {

// Detects market microstructure patterns
// TODO: Optimize with SIMD, O(1) rolling statistics, vectorized calculations
class MarketMicrostructureAnalyzer {
public:
    MarketMicrostructureAnalyzer(size_t window_size = 1000);
    
    // Process a tick and update statistics
    void processTick(const TickData& tick, const OrderBook& book);
    
    // Pattern detection
    struct PatternMetrics {
        double spread_mean;
        double spread_std;
        double bid_ask_imbalance; // Ratio of bid volume to ask volume
        double order_flow_imbalance; // Net buy pressure
        double cancellation_rate; // Fraction of orders cancelled quickly
        double hidden_liquidity_probability; // Estimated hidden liquidity
    };
    
    PatternMetrics getCurrentMetrics() const;
    
    // Specific pattern detectors
    bool detectQuoteStuffing(double threshold = 0.9) const;
    bool detectHiddenLiquidity(double threshold = 0.3) const;
    bool detectInformedTrading(double threshold = 0.7) const;
    
    // Market impact estimation
    double estimateMarketImpact(uint64_t quantity, OrderSide side) const;
    
    // Reset statistics
    void reset();
    
private:
    size_t window_size_;
    
    // Rolling windows for statistics
    std::deque<double> spreads_;
    std::deque<double> bid_volumes_;
    std::deque<double> ask_volumes_;
    std::deque<double> order_flow_;
    
    // Order tracking for cancellation rate
    struct OrderEvent {
        uint64_t order_id;
        std::chrono::nanoseconds timestamp;
        bool cancelled;
    };
    std::deque<OrderEvent> recent_orders_;
    
    // Statistics
    double spread_sum_ = 0.0;
    double spread_sum_sq_ = 0.0;
    double bid_volume_sum_ = 0.0;
    double ask_volume_sum_ = 0.0;
    double order_flow_sum_ = 0.0;
    
    // Helper methods
    void updateRollingStatistics();
    double calculateSpreadMean() const;
    double calculateSpreadStd() const;
};

} // namespace orderbook
