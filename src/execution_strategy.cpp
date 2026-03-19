#include "execution_strategy.h"
#include <algorithm>
#include <cmath>

namespace orderbook {

ExecutionStrategy::ExecutionStrategy(uint64_t total_quantity, OrderSide side,
                                     std::chrono::nanoseconds duration)
    : total_quantity_(total_quantity), side_(side), duration_(duration) {
}

TWAPStrategy::TWAPStrategy(uint64_t total_quantity, OrderSide side,
                          std::chrono::nanoseconds duration,
                          size_t num_intervals)
    : ExecutionStrategy(total_quantity, side, duration),
      num_intervals_(num_intervals) {
    interval_duration_ = duration_ / num_intervals_;
}

ExecutionStrategy::ExecutionDecision TWAPStrategy::getNextDecision(
    const OrderBook& book,
    const MarketMicrostructureAnalyzer& analyzer,
    std::chrono::nanoseconds elapsed_time,
    uint64_t remaining_quantity) {
    
    ExecutionDecision decision;
    decision.side = side_;
    decision.timestamp = std::chrono::steady_clock::now().time_since_epoch();
    
    // Calculate which interval we're in
    size_t current_interval = elapsed_time.count() / interval_duration_.count();
    current_interval = std::min(current_interval, num_intervals_ - 1);
    
    // Target quantity per interval
    uint64_t target_per_interval = total_quantity_ / num_intervals_;
    
    // Calculate desired quantity for this interval
    uint64_t target_for_interval = (current_interval + 1) * target_per_interval;
    uint64_t quantity_to_trade = (target_for_interval > executed_quantity_) ?
        target_for_interval - executed_quantity_ : 0;
    
    // Cap at remaining quantity
    quantity_to_trade = std::min(quantity_to_trade, remaining_quantity);
    
    decision.quantity = quantity_to_trade;
    
    // Place limit order slightly better than best bid/ask
    if (side_ == OrderSide::BID) {
        double best_bid = book.getBestBid();
        decision.price = best_bid;
        decision.is_market_order = false;
    } else {
        double best_ask = book.getBestAsk();
        decision.price = best_ask;
        decision.is_market_order = false;
    }
    
    return decision;
}

VWAPStrategy::VWAPStrategy(uint64_t total_quantity, OrderSide side,
                          std::chrono::nanoseconds duration,
                          const std::vector<double>& volume_profile)
    : ExecutionStrategy(total_quantity, side, duration),
      volume_profile_(volume_profile) {
}

ExecutionStrategy::ExecutionDecision VWAPStrategy::getNextDecision(
    const OrderBook& book,
    const MarketMicrostructureAnalyzer& analyzer,
    std::chrono::nanoseconds elapsed_time,
    uint64_t remaining_quantity) {
    
    ExecutionDecision decision;
    decision.side = side_;
    decision.timestamp = std::chrono::steady_clock::now().time_since_epoch();
    
    // Calculate which bucket we're in based on elapsed time
    double progress = static_cast<double>(elapsed_time.count()) / duration_.count();
    size_t bucket = static_cast<size_t>(progress * volume_profile_.size());
    bucket = std::min(bucket, volume_profile_.size() - 1);
    
    // Calculate cumulative target up to this bucket
    double cumulative_target = 0.0;
    for (size_t i = 0; i <= bucket; ++i) {
        cumulative_target += volume_profile_[i];
    }
    
    uint64_t target_quantity = static_cast<uint64_t>(cumulative_target * total_quantity_);
    uint64_t quantity_to_trade = (target_quantity > executed_quantity_) ?
        target_quantity - executed_quantity_ : 0;
    
    quantity_to_trade = std::min(quantity_to_trade, remaining_quantity);
    decision.quantity = quantity_to_trade;
    
    // Place limit order
    if (side_ == OrderSide::BID) {
        decision.price = book.getBestBid();
    } else {
        decision.price = book.getBestAsk();
    }
    decision.is_market_order = false;
    
    return decision;
}

AlmgrenChrissStrategy::AlmgrenChrissStrategy(uint64_t total_quantity, OrderSide side,
                                             std::chrono::nanoseconds duration,
                                             const Parameters& params)
    : ExecutionStrategy(total_quantity, side, duration),
      params_(params) {
    computeOptimalSchedule();
}

void AlmgrenChrissStrategy::computeOptimalSchedule() {
    // Pre-compute optimal execution schedule
    // This is a simplified version - full implementation would solve
    // the optimal control problem with backward induction
    
    size_t num_steps = 100; // Discretize time into 100 steps
    execution_schedule_.resize(num_steps);
    
    double total_time = duration_.count() / 1e9; // Convert to seconds
    double dt = total_time / num_steps;
    
    // Simplified Almgren-Chriss solution
    // Optimal rate: x'(t) = -sqrt(λ/η) * tanh(sqrt(λ*η) * (T-t)) * X
    // where X is remaining quantity, T is total time, t is current time
    
    double kappa = std::sqrt(params_.permanent_impact / params_.temporary_impact);
    double remaining = static_cast<double>(total_quantity_);
    
    for (size_t i = 0; i < num_steps; ++i) {
        double t = i * dt;
        double time_remaining = total_time - t;
        
        if (time_remaining > 0) {
            double rate = kappa * std::tanh(kappa * time_remaining) * remaining;
            execution_schedule_[i] = rate * dt;
            remaining -= execution_schedule_[i];
        } else {
            execution_schedule_[i] = 0.0;
        }
    }
}

double AlmgrenChrissStrategy::computeOptimalRate(double remaining_time, double remaining_quantity) const {
    // Real-time optimal rate calculation
    double kappa = std::sqrt(params_.permanent_impact / params_.temporary_impact);
    return kappa * std::tanh(kappa * remaining_time) * static_cast<double>(remaining_quantity);
}

ExecutionStrategy::ExecutionDecision AlmgrenChrissStrategy::getNextDecision(
    const OrderBook& book,
    const MarketMicrostructureAnalyzer& analyzer,
    std::chrono::nanoseconds elapsed_time,
    uint64_t remaining_quantity) {
    
    ExecutionDecision decision;
    decision.side = side_;
    decision.timestamp = std::chrono::steady_clock::now().time_since_epoch();
    
    // Calculate remaining time
    double remaining_time_sec = (duration_ - elapsed_time).count() / 1e9;
    remaining_time_sec = std::max(0.0, remaining_time_sec);
    
    // Get optimal rate
    double optimal_rate = computeOptimalRate(remaining_time_sec, remaining_quantity);
    
    // Convert rate to quantity (simplified - assumes 1 second intervals)
    uint64_t quantity_to_trade = static_cast<uint64_t>(optimal_rate);
    quantity_to_trade = std::min(quantity_to_trade, remaining_quantity);
    
    decision.quantity = quantity_to_trade;
    
    // Adaptive pricing based on market impact
    double market_impact = analyzer.estimateMarketImpact(quantity_to_trade, side_);
    
    if (side_ == OrderSide::BID) {
        double best_bid = book.getBestBid();
        decision.price = best_bid + market_impact * 0.5; // Pay up slightly
        decision.is_market_order = (market_impact < book.getSpread() * 0.1);
    } else {
        double best_ask = book.getBestAsk();
        decision.price = best_ask - market_impact * 0.5; // Pay down slightly
        decision.is_market_order = (market_impact < book.getSpread() * 0.1);
    }
    
    return decision;
}

} // namespace orderbook
