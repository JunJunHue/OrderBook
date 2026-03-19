#pragma once

#include "order_book.h"
#include "market_microstructure_analyzer.h"
#include "execution_strategy.h"
#include "tick_data.h"
#include <vector>
#include <memory>
#include <chrono>

namespace orderbook {

// Backtesting engine for execution strategies
// TODO: Optimize with event-driven simulation, realistic fill models
class Backtester {
public:
    struct ExecutionResult {
        uint64_t total_executed = 0;
        double average_price = 0.0;
        double total_cost = 0.0;
        double implementation_shortfall = 0.0; // vs arrival price
        double vwap_benchmark = 0.0;
        std::chrono::nanoseconds execution_time;
        
        // Detailed execution log
        struct Fill {
            std::chrono::nanoseconds timestamp;
            double price;
            uint64_t quantity;
        };
        std::vector<Fill> fills;
    };
    
    Backtester();
    
    // Run backtest on historical tick data
    ExecutionResult runBacktest(
        const std::vector<TickData>& tick_data,
        std::unique_ptr<ExecutionStrategy> strategy,
        double arrival_price); // Price when order was placed
    
    // Realistic fill model
    // Simulates partial fills, adverse selection, etc.
    struct FillModel {
        double fill_probability = 0.8; // Probability of getting filled at limit price
        double adverse_selection = 0.001; // Price slippage per share
        bool allow_partial_fills = true;
    };
    
    void setFillModel(const FillModel& model) {
        fill_model_ = model;
    }
    
    // Performance metrics
    struct PerformanceMetrics {
        double execution_shortfall; // vs arrival price
        double savings_vs_naive;    // vs naive market order
        double savings_vs_vwap;     // vs VWAP benchmark
        double max_drawdown;
        double sharpe_ratio;
    };
    
    PerformanceMetrics calculateMetrics(const ExecutionResult& result,
                                       double arrival_price,
                                       const std::vector<double>& market_prices) const;
    
private:
    FillModel fill_model_;
    
    // Simulate order execution
    uint64_t simulateFill(const OrderBook& book,
                         OrderSide side,
                         double limit_price,
                         uint64_t desired_quantity,
                         double& execution_price) const;
    
    // Calculate VWAP from market data
    double calculateVWAP(const std::vector<TickData>& trades,
                        std::chrono::nanoseconds start_time,
                        std::chrono::nanoseconds end_time) const;
};

} // namespace orderbook
