#include "order_book.h"
#include "market_microstructure_analyzer.h"
#include "execution_strategy.h"
#include "backtester.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

using namespace orderbook;

// Example: Generate synthetic tick data for testing
std::vector<TickData> generateSyntheticTicks(size_t num_ticks, double base_price = 150.0) {
    std::vector<TickData> ticks;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_dist(base_price * 0.99, base_price * 1.01);
    std::uniform_int_distribution<uint64_t> qty_dist(100, 1000);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    ticks.reserve(num_ticks);
    
    for (size_t i = 0; i < num_ticks; ++i) {
        OrderSide side = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
        double price = price_dist(gen);
        uint64_t quantity = qty_dist(gen);
        
        // Round price to 2 decimal places
        price = std::round(price * 100.0) / 100.0;
        
        ticks.push_back(TickData(TickType::ORDER_ADDED, i, side, price, quantity));
    }
    
    return ticks;
}

int main() {
    std::cout << "=== High-Frequency Market Microstructure Analyzer ===" << std::endl;
    std::cout << "Order Book Execution Engine - Basic Skeleton" << std::endl << std::endl;
    
    // Initialize components
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer(1000);
    
    // Generate synthetic data
    std::cout << "Generating synthetic market data..." << std::endl;
    auto ticks = generateSyntheticTicks(10000, 150.0);
    std::cout << "Generated " << ticks.size() << " ticks" << std::endl;
    
    // Process ticks
    std::cout << "\nProcessing ticks..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& tick : ticks) {
        book.processTick(tick);
        analyzer.processTick(tick, book);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Processed " << ticks.size() << " ticks in " 
              << processing_time.count() << " microseconds" << std::endl;
    std::cout << "Throughput: " << (ticks.size() * 1000000.0 / processing_time.count()) 
              << " ticks/second" << std::endl;
    
    // Display order book state
    std::cout << "\n=== Order Book State ===" << std::endl;
    std::cout << "Best Bid: $" << book.getBestBid() << " (" 
              << book.getBestBidQuantity() << " shares)" << std::endl;
    std::cout << "Best Ask: $" << book.getBestAsk() << " (" 
              << book.getBestAskQuantity() << " shares)" << std::endl;
    std::cout << "Spread: $" << book.getSpread() << std::endl;
    
    // Display microstructure metrics
    std::cout << "\n=== Market Microstructure Metrics ===" << std::endl;
    auto metrics = analyzer.getCurrentMetrics();
    std::cout << "Spread Mean: $" << metrics.spread_mean << std::endl;
    std::cout << "Spread Std: $" << metrics.spread_std << std::endl;
    std::cout << "Bid/Ask Imbalance: " << metrics.bid_ask_imbalance << std::endl;
    std::cout << "Order Flow Imbalance: " << metrics.order_flow_imbalance << std::endl;
    std::cout << "Cancellation Rate: " << metrics.cancellation_rate << std::endl;
    
    // Pattern detection
    std::cout << "\n=== Pattern Detection ===" << std::endl;
    std::cout << "Quote Stuffing Detected: " 
              << (analyzer.detectQuoteStuffing() ? "YES" : "NO") << std::endl;
    std::cout << "Hidden Liquidity Detected: " 
              << (analyzer.detectHiddenLiquidity() ? "YES" : "NO") << std::endl;
    std::cout << "Informed Trading Detected: " 
              << (analyzer.detectInformedTrading() ? "YES" : "NO") << std::endl;
    
    // Example execution strategy
    std::cout << "\n=== Execution Strategy Example ===" << std::endl;
    uint64_t target_quantity = 10000;
    auto duration = std::chrono::minutes(30);
    
    // TWAP Strategy
    TWAPStrategy twap_strategy(target_quantity, OrderSide::BID, duration, 60);
    std::cout << "TWAP Strategy initialized for " << target_quantity 
              << " shares over 30 minutes" << std::endl;
    
    // Almgren-Chriss Strategy
    AlmgrenChrissStrategy::Parameters ac_params;
    ac_params.permanent_impact = 0.0001;
    ac_params.temporary_impact = 0.0002;
    ac_params.volatility = 0.02;
    ac_params.risk_aversion = 0.1;
    
    AlmgrenChrissStrategy ac_strategy(target_quantity, OrderSide::BID, duration, ac_params);
    std::cout << "Almgren-Chriss Strategy initialized" << std::endl;
    
    // Backtesting example
    std::cout << "\n=== Backtesting ===" << std::endl;
    Backtester backtester;
    double arrival_price = 150.0;
    
    auto twap_result = backtester.runBacktest(
        ticks,
        std::make_unique<TWAPStrategy>(target_quantity, OrderSide::BID, duration, 60),
        arrival_price
    );
    
    std::cout << "TWAP Backtest Results:" << std::endl;
    std::cout << "  Executed: " << twap_result.total_executed << " shares" << std::endl;
    std::cout << "  Average Price: $" << twap_result.average_price << std::endl;
    std::cout << "  Implementation Shortfall: $" << twap_result.implementation_shortfall << std::endl;
    
    std::cout << "\n=== Optimization Notes ===" << std::endl;
    std::cout << "TODO: Optimize the following components:" << std::endl;
    std::cout << "  1. Replace std::mutex with lock-free data structures" << std::endl;
    std::cout << "  2. Use cache-aligned memory layouts (struct-of-arrays)" << std::endl;
    std::cout << "  3. Vectorize statistics with SIMD (AVX2/AVX512)" << std::endl;
    std::cout << "  4. Implement custom memory allocators for tick data" << std::endl;
    std::cout << "  5. Zero-copy message parsing" << std::endl;
    std::cout << "  6. Profile-guided optimization" << std::endl;
    
    return 0;
}
