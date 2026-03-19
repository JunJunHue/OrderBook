#include "backtester.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace orderbook {

Backtester::Backtester() {
    fill_model_ = FillModel{0.8, 0.001, true};
}

Backtester::ExecutionResult Backtester::runBacktest(
    const std::vector<TickData>& tick_data,
    std::unique_ptr<ExecutionStrategy> strategy,
    double arrival_price) {
    
    ExecutionResult result;
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer;
    
    auto start_time = std::chrono::steady_clock::now();
    uint64_t executed_quantity = 0;
    double total_cost = 0.0;
    
    // Process ticks and execute strategy
    for (const auto& tick : tick_data) {
        // Update order book
        book.processTick(tick);
        analyzer.processTick(tick, book);
        
        // Get strategy decision
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto decision = strategy->getNextDecision(book, analyzer, elapsed,
                                                   strategy->getTotalQuantity() - executed_quantity);
        
        if (decision.quantity > 0) {
            // Simulate fill
            double execution_price = 0.0;
            uint64_t filled = simulateFill(book, decision.side, decision.price,
                                          decision.quantity, execution_price);
            
            if (filled > 0) {
                executed_quantity += filled;
                total_cost += execution_price * filled;
                
                result.fills.push_back({
                    decision.timestamp,
                    execution_price,
                    filled
                });
            }
        }
        
        // Check if complete
        if (strategy->isComplete(executed_quantity)) {
            break;
        }
    }
    
    result.total_executed = executed_quantity;
    result.average_price = (executed_quantity > 0) ? total_cost / executed_quantity : 0.0;
    result.total_cost = total_cost;
    result.implementation_shortfall = (arrival_price - result.average_price) * executed_quantity;
    result.execution_time = std::chrono::steady_clock::now() - start_time;
    
    // Calculate VWAP benchmark
    result.vwap_benchmark = calculateVWAP(tick_data, start_time.time_since_epoch(),
                                          std::chrono::steady_clock::now().time_since_epoch());
    
    return result;
}

uint64_t Backtester::simulateFill(const OrderBook& book,
                                 OrderSide side,
                                 double limit_price,
                                 uint64_t desired_quantity,
                                 double& execution_price) const {
    // Simple fill model - TODO: implement more realistic adverse selection, partial fills
    
    if (side == OrderSide::BID) {
        double best_ask = book.getBestAsk();
        if (best_ask == 0.0) return 0;
        
        // Check if limit price can cross the spread
        if (limit_price >= best_ask || fill_model_.fill_probability > 0.5) {
            // Market order or aggressive limit order
            execution_price = best_ask + fill_model_.adverse_selection * desired_quantity;
            return desired_quantity;
        } else {
            // Passive limit order - lower fill probability
            if (fill_model_.fill_probability > 0.3) {
                execution_price = limit_price;
                return fill_model_.allow_partial_fills ? 
                    static_cast<uint64_t>(desired_quantity * fill_model_.fill_probability) :
                    desired_quantity;
            }
        }
    } else {
        double best_bid = book.getBestBid();
        if (best_bid == 0.0) return 0;
        
        if (limit_price <= best_bid || fill_model_.fill_probability > 0.5) {
            execution_price = best_bid - fill_model_.adverse_selection * desired_quantity;
            return desired_quantity;
        } else {
            if (fill_model_.fill_probability > 0.3) {
                execution_price = limit_price;
                return fill_model_.allow_partial_fills ?
                    static_cast<uint64_t>(desired_quantity * fill_model_.fill_probability) :
                    desired_quantity;
            }
        }
    }
    
    return 0;
}

double Backtester::calculateVWAP(const std::vector<TickData>& trades,
                                std::chrono::nanoseconds start_time,
                                std::chrono::nanoseconds end_time) const {
    double total_value = 0.0;
    uint64_t total_volume = 0;
    
    for (const auto& tick : trades) {
        if (tick.type == TickType::TRADE_EXECUTED &&
            tick.timestamp >= start_time && tick.timestamp <= end_time) {
            total_value += tick.execution_price * tick.execution_quantity;
            total_volume += tick.execution_quantity;
        }
    }
    
    return (total_volume > 0) ? total_value / total_volume : 0.0;
}

Backtester::PerformanceMetrics Backtester::calculateMetrics(
    const ExecutionResult& result,
    double arrival_price,
    const std::vector<double>& market_prices) const {
    
    PerformanceMetrics metrics;
    
    // Implementation shortfall
    metrics.execution_shortfall = result.implementation_shortfall;
    
    // Savings vs naive (assuming market order at arrival price + spread)
    double naive_price = arrival_price + (result.average_price - arrival_price) * 1.5;
    double naive_cost = naive_price * result.total_executed;
    metrics.savings_vs_naive = naive_cost - result.total_cost;
    
    // Savings vs VWAP
    double vwap_cost = result.vwap_benchmark * result.total_executed;
    metrics.savings_vs_vwap = vwap_cost - result.total_cost;
    
    // Max drawdown (simplified)
    if (!market_prices.empty()) {
        double max_price = *std::max_element(market_prices.begin(), market_prices.end());
        double min_price = *std::min_element(market_prices.begin(), market_prices.end());
        metrics.max_drawdown = (max_price - min_price) / max_price;
    }
    
    // Sharpe ratio (simplified - would need returns series)
    metrics.sharpe_ratio = 0.0; // TODO: calculate properly
    
    return metrics;
}

} // namespace orderbook
