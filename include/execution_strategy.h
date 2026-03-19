#pragma once

#include "order.h"
#include "order_book.h"
#include "market_microstructure_analyzer.h"
#include <chrono>
#include <vector>

namespace orderbook {

// Base class for execution strategies
class ExecutionStrategy {
public:
    struct ExecutionDecision {
        OrderSide side;
        double price;      // Limit price (0.0 for market orders)
        uint64_t quantity;
        bool is_market_order;
        std::chrono::nanoseconds timestamp;
    };
    
    ExecutionStrategy(uint64_t total_quantity, OrderSide side,
                     std::chrono::nanoseconds duration);
    virtual ~ExecutionStrategy() = default;
    
    // Get next execution decision based on current market state
    virtual ExecutionDecision getNextDecision(
        const OrderBook& book,
        const MarketMicrostructureAnalyzer& analyzer,
        std::chrono::nanoseconds elapsed_time,
        uint64_t remaining_quantity) = 0;
    
    // Check if execution is complete
    bool isComplete(uint64_t executed_quantity) const {
        return executed_quantity >= total_quantity_;
    }

    uint64_t getTotalQuantity() const { return total_quantity_; }
    
protected:
    uint64_t total_quantity_;
    OrderSide side_;
    std::chrono::nanoseconds duration_;
    uint64_t executed_quantity_ = 0;
};

// Time-Weighted Average Price strategy
// Splits order evenly over time
class TWAPStrategy : public ExecutionStrategy {
public:
    TWAPStrategy(uint64_t total_quantity, OrderSide side,
                std::chrono::nanoseconds duration,
                size_t num_intervals = 60);
    
    ExecutionDecision getNextDecision(
        const OrderBook& book,
        const MarketMicrostructureAnalyzer& analyzer,
        std::chrono::nanoseconds elapsed_time,
        uint64_t remaining_quantity) override;
    
private:
    size_t num_intervals_;
    std::chrono::nanoseconds interval_duration_;
};

// Volume-Weighted Average Price strategy
// Matches trading to market volume profile
class VWAPStrategy : public ExecutionStrategy {
public:
    VWAPStrategy(uint64_t total_quantity, OrderSide side,
                std::chrono::nanoseconds duration,
                const std::vector<double>& volume_profile);
    
    ExecutionDecision getNextDecision(
        const OrderBook& book,
        const MarketMicrostructureAnalyzer& analyzer,
        std::chrono::nanoseconds elapsed_time,
        uint64_t remaining_quantity) override;
    
private:
    std::vector<double> volume_profile_; // Normalized volume per time bucket
    size_t current_bucket_ = 0;
};

// Almgren-Chriss optimal execution strategy
// Balances market impact vs timing risk
class AlmgrenChrissStrategy : public ExecutionStrategy {
public:
    struct Parameters {
        double permanent_impact;    // λ (lambda) - permanent price impact
        double temporary_impact;    // η (eta) - temporary price impact
        double volatility;          // σ (sigma) - price volatility
        double risk_aversion;       // γ (gamma) - risk aversion parameter
    };
    
    AlmgrenChrissStrategy(uint64_t total_quantity, OrderSide side,
                         std::chrono::nanoseconds duration,
                         const Parameters& params);
    
    ExecutionDecision getNextDecision(
        const OrderBook& book,
        const MarketMicrostructureAnalyzer& analyzer,
        std::chrono::nanoseconds elapsed_time,
        uint64_t remaining_quantity) override;
    
private:
    Parameters params_;
    std::vector<double> execution_schedule_; // Pre-computed optimal schedule
    
    // Solve optimal execution trajectory
    void computeOptimalSchedule();
    double computeOptimalRate(double remaining_time, double remaining_quantity) const;
};

} // namespace orderbook
