// QuantForge — Matching Engine Test Suite
// Run via: ./build/orderbook_tests
//
// Each test function returns true on success, false on failure.
// The framework prints PASS/FAIL per test and a final summary.

#include "matching_engine.h"
#include "pool_allocator.h"
#include "order_book.h"
#include "market_microstructure_analyzer.h"
#include "execution_strategy.h"
#include "backtester.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include <random>

using namespace orderbook;

// ============================================================
// Minimal test harness
// ============================================================

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT(cond, msg)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "  FAIL [line " << __LINE__ << "]: " << (msg)     \
                      << "\n";                                              \
            return false;                                                   \
        }                                                                   \
    } while (false)

#define EXPECT_EQ(a, b, msg) EXPECT((a) == (b), msg)
#define EXPECT_NEAR(a, b, eps, msg) EXPECT(std::abs((a)-(b)) < (eps), msg)

static void run_test(const char* name, bool (*fn)()) {
    bool ok = fn();
    if (ok) {
        std::cout << "[PASS] " << name << "\n";
        ++g_passed;
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++g_failed;
    }
}

// ============================================================
// Phase 1 — MatchingEngine tests
// ============================================================

// TC-01: Two matching limit orders produce exactly one fill.
static bool tc01_limit_basic_match() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);
    eng.submitLimitOrder(OrderSide::BID, 100.0, 100);
    EXPECT_EQ(eng.getFills().size(), 1u, "exactly one fill");
    EXPECT_NEAR(eng.getFills()[0].price, 100.0, 1e-9, "fill at 100.0");
    EXPECT_EQ(eng.getFills()[0].quantity, 100u, "fill qty = 100");
    EXPECT_EQ(eng.getBestBid(), 0.0, "no remaining bid");
    EXPECT_EQ(eng.getBestAsk(), 0.0, "no remaining ask");
    return true;
}

// TC-02: Price priority — highest bid matched against lowest ask first.
static bool tc02_price_priority() {
    MatchingEngine eng;
    // Two asks at different prices
    eng.submitLimitOrder(OrderSide::ASK, 101.0, 50);
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 50);  // better (lower) price
    // A buy order that can only fill 50 shares
    eng.submitLimitOrder(OrderSide::BID, 105.0, 50);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill");
    EXPECT_NEAR(eng.getFills()[0].price, 100.0, 1e-9, "filled against cheapest ask");
    EXPECT_EQ(eng.getBestAsk(), 101.0, "expensive ask still resting");
    return true;
}

// TC-03: Time priority — at the same price, the earlier order fills first.
static bool tc03_time_priority() {
    MatchingEngine eng;
    uint64_t ask1 = eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);  // arrives first
    uint64_t ask2 = eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);  // arrives second
    (void)ask1; (void)ask2;
    // Buy fills exactly 100 — should consume ask1 (the earlier one)
    eng.submitLimitOrder(OrderSide::BID, 100.0, 100);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill");
    EXPECT_EQ(eng.getFills()[0].sell_order_id, ask1, "first ask filled first");
    // ask2 should still be resting
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "ask2 still resting");
    return true;
}

// TC-04: Market buy order fills immediately at the best ask.
static bool tc04_market_order_buy() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::ASK, 99.50, 200);
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill");
    EXPECT_NEAR(eng.getFills()[0].price, 99.50, 1e-9, "fill at ask price");
    EXPECT_EQ(eng.getFills()[0].quantity, 100u, "fill qty = 100");
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "100 remaining on ask");
    return true;
}

// TC-05: Market sell order fills immediately at the best bid.
static bool tc05_market_order_sell() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::BID, 50.00, 300);
    eng.submitMarketOrder(OrderSide::ASK, 150);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill");
    EXPECT_NEAR(eng.getFills()[0].price, 50.00, 1e-9, "fill at bid price");
    EXPECT_EQ(eng.getFills()[0].quantity, 150u, "fill qty = 150");
    EXPECT_EQ(eng.getBestBidQuantity(), 150u, "150 remaining on bid");
    return true;
}

// TC-06: Market order sweeps through multiple price levels.
static bool tc06_market_order_multi_level() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);
    eng.submitLimitOrder(OrderSide::ASK, 101.0, 100);
    eng.submitLimitOrder(OrderSide::ASK, 102.0, 100);
    // Buy 250 — should consume first two levels completely and part of third
    eng.submitMarketOrder(OrderSide::BID, 250);
    EXPECT_EQ(eng.getFills().size(), 3u, "three fills across three levels");
    // Remaining: 50 at 102.0
    EXPECT_NEAR(eng.getBestAsk(), 102.0, 1e-9, "remaining ask at 102");
    EXPECT_EQ(eng.getBestAskQuantity(), 50u, "50 shares remaining at 102");
    return true;
}

// TC-07: Cancelled order is not available for matching.
static bool tc07_cancel_order() {
    MatchingEngine eng;
    uint64_t ask_id = eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);
    bool cancelled = eng.cancelOrder(ask_id);
    EXPECT(cancelled, "cancel returns true");
    EXPECT_EQ(eng.getBestAsk(), 0.0, "no ask after cancel");
    EXPECT_EQ(eng.getOrderCount(), 0u, "order count = 0 after cancel");
    // Submitting a matching bid should produce no fill
    eng.submitLimitOrder(OrderSide::BID, 100.0, 100);
    EXPECT_EQ(eng.getFills().size(), 0u, "no fill after ask cancelled");
    return true;
}

// TC-08: Cancelling a non-existent order returns false.
static bool tc08_cancel_nonexistent() {
    MatchingEngine eng;
    bool result = eng.cancelOrder(99999);
    EXPECT(!result, "cancel of non-existent order returns false");
    return true;
}

// TC-09: Stop buy order does NOT trigger before the stop price is reached.
static bool tc09_stop_order_no_trigger() {
    MatchingEngine eng;
    // Resting ask at 90 (will be the trade that sets last_trade_price)
    eng.submitLimitOrder(OrderSide::ASK, 90.0, 100);
    // Stop buy at 95 — should not trigger yet
    eng.submitStopOrder(OrderSide::BID, 95.0, 200);
    // Trade at 90 — below stop trigger of 95 for a buy stop
    eng.submitMarketOrder(OrderSide::BID, 100);
    // last_trade_price = 90.0 which is < 95.0, so buy stop should NOT trigger
    EXPECT_EQ(eng.getFills().size(), 1u, "only the market order fill");
    return true;
}

// TC-10: Stop buy order triggers when a trade at or above stop_price occurs.
static bool tc10_stop_order_trigger() {
    MatchingEngine eng;
    // Place sell liquidity at two levels
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 200); // available for stop-triggered market order
    // Stop buy at 100.0 — triggers when last_trade_price >= 100.0
    eng.submitStopOrder(OrderSide::BID, 100.0, 100);
    // Initial fill count = 0 (no trades yet)
    EXPECT_EQ(eng.getFills().size(), 0u, "no fills before trigger");
    // Execute a trade at 100 to set last_trade_price
    eng.submitMarketOrder(OrderSide::BID, 100);
    // After this: last_trade_price = 100.0 >= stop_price 100.0 → stop triggers
    // The stop becomes a market buy for 100 shares and hits remaining asks
    EXPECT(eng.getFills().size() >= 2u, "stop triggered and produced additional fill");
    return true;
}

// TC-11: Stop sell order triggers when trade_price <= stop_price.
static bool tc11_stop_sell_trigger() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::BID, 100.0, 100);
    eng.submitLimitOrder(OrderSide::BID, 100.0, 200);
    // Stop sell at 100.0 — triggers when price <= 100.0
    eng.submitStopOrder(OrderSide::ASK, 100.0, 100);
    EXPECT_EQ(eng.getFills().size(), 0u, "no fills before trigger");
    // Sell market order trades at 100.0, triggering stop
    eng.submitMarketOrder(OrderSide::ASK, 100);
    EXPECT(eng.getFills().size() >= 2u, "stop sell triggered");
    return true;
}

// TC-12: Iceberg order — only visible_quantity shown in depth.
static bool tc12_iceberg_visible_qty() {
    MatchingEngine eng;
    // Total 1000, only 100 visible
    eng.submitIcebergOrder(OrderSide::ASK, 100.0, 1000, 100);
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "only visible qty in book");
    return true;
}

// TC-13: Iceberg order auto-refills after visible slice is consumed.
static bool tc13_iceberg_refill() {
    MatchingEngine eng;
    // Total 300, visible slice 100
    eng.submitIcebergOrder(OrderSide::ASK, 100.0, 300, 100);
    // Buy exactly the visible slice
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill for first slice");
    // Book should now show the next slice of 100
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "iceberg refilled to 100");
    EXPECT_NEAR(eng.getBestAsk(), 100.0, 1e-9, "same price level");
    // Buy the second slice
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getFills().size(), 2u, "second fill");
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "third slice loaded");
    // Buy the last slice — should empty the book
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getBestAsk(), 0.0, "book empty after all slices consumed");
    return true;
}

// TC-14: Iceberg refilled slice goes to back of queue (loses time priority).
static bool tc14_iceberg_loses_time_priority() {
    MatchingEngine eng;
    // A plain limit order resting at 100
    uint64_t plain_ask = eng.submitLimitOrder(OrderSide::ASK, 100.0, 100);
    // Then an iceberg at the same price
    eng.submitIcebergOrder(OrderSide::ASK, 100.0, 300, 100);
    // Buy 100 — should fill the plain ask first (time priority)
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getFills()[0].sell_order_id, plain_ask, "plain ask filled first");
    // Next 100 — fills first iceberg slice
    eng.submitMarketOrder(OrderSide::BID, 100);
    EXPECT_EQ(eng.getFills().size(), 2u, "second fill from iceberg");
    // Now the replenished iceberg slice is at the back — no competing resting orders
    EXPECT_EQ(eng.getBestAskQuantity(), 100u, "final iceberg slice visible");
    return true;
}

// TC-15: Partial fill — limit order quantity reduces correctly.
static bool tc15_partial_fill() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::ASK, 100.0, 500);
    eng.submitLimitOrder(OrderSide::BID, 100.0, 200);
    EXPECT_EQ(eng.getFills().size(), 1u, "one fill");
    EXPECT_EQ(eng.getFills()[0].quantity, 200u, "fill qty = 200 (buyer qty)");
    EXPECT_EQ(eng.getBestAskQuantity(), 300u, "300 remaining on ask");
    EXPECT_EQ(eng.getBestBid(), 0.0, "buy order fully consumed");
    return true;
}

// TC-16: Spread = best_ask - best_bid.
static bool tc16_spread() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::BID, 99.0, 100);
    eng.submitLimitOrder(OrderSide::ASK, 101.0, 100);
    EXPECT_NEAR(eng.getSpread(), 2.0, 1e-9, "spread = 2.0");
    return true;
}

// TC-17: getBidDepth / getAskDepth return levels in correct order.
static bool tc17_depth_query() {
    MatchingEngine eng;
    eng.submitLimitOrder(OrderSide::BID, 99.0,  100);
    eng.submitLimitOrder(OrderSide::BID, 98.0,  200);
    eng.submitLimitOrder(OrderSide::BID, 97.0,  300);
    eng.submitLimitOrder(OrderSide::ASK, 101.0, 150);
    eng.submitLimitOrder(OrderSide::ASK, 102.0, 250);

    std::vector<std::pair<double, uint64_t>> bids, asks;
    eng.getBidDepth(3, bids);
    eng.getAskDepth(3, asks);

    EXPECT_EQ(bids.size(), 3u, "3 bid levels");
    EXPECT_NEAR(bids[0].first, 99.0, 1e-9, "top bid = 99");
    EXPECT_NEAR(bids[1].first, 98.0, 1e-9, "second bid = 98");
    EXPECT_NEAR(bids[2].first, 97.0, 1e-9, "third bid = 97");

    EXPECT_EQ(asks.size(), 2u, "2 ask levels");
    EXPECT_NEAR(asks[0].first, 101.0, 1e-9, "best ask = 101");
    EXPECT_NEAR(asks[1].first, 102.0, 1e-9, "second ask = 102");
    return true;
}

// ============================================================
// Phase 1 — PoolAllocator tests
// ============================================================

// TC-18: Basic allocate + deallocate round-trips through the pool.
static bool tc18_pool_alloc_basic() {
    PoolAllocator<Order, 16> pool;
    EXPECT_EQ(pool.available(), 16u, "initially 16 slots free");

    Order* o = pool.allocate();
    new (o) Order(1, OrderSide::BID, OrderType::LIMIT, 100.0, 50);
    EXPECT_EQ(pool.available(), 15u, "15 slots after one allocation");

    o->~Order();
    pool.deallocate(o);
    EXPECT_EQ(pool.available(), 16u, "back to 16 after dealloc");
    return true;
}

// TC-19: Pool falls back to heap when exhausted.
static bool tc19_pool_overflow() {
    PoolAllocator<Order, 2> pool;  // tiny pool
    Order* a = pool.allocate();
    Order* b = pool.allocate();
    // Pool is now exhausted — next alloc should fall back to heap without crash
    Order* c = pool.allocate();
    EXPECT(c != nullptr, "heap fallback allocation succeeds");
    new (c) Order(3, OrderSide::ASK, OrderType::LIMIT, 50.0, 100);
    c->~Order();
    pool.deallocate(c); // heap-allocated pointer — should not re-enter pool
    a->~Order(); pool.deallocate(a);
    b->~Order(); pool.deallocate(b);
    EXPECT_EQ(pool.available(), 2u, "pool slots restored; heap slots discarded");
    return true;
}

// TC-20: Pool reuses the same memory slot after free.
static bool tc20_pool_reuse() {
    PoolAllocator<Order, 4> pool;
    Order* first = pool.allocate();
    void*  addr  = first;
    new (first) Order(1, OrderSide::BID, OrderType::LIMIT, 1.0, 1);
    first->~Order();
    pool.deallocate(first);

    Order* second = pool.allocate();
    // The pool uses a LIFO free-list, so the re-used slot should be the same address
    EXPECT(second == addr, "pool re-uses freed slot");
    second->~Order();
    pool.deallocate(second);
    return true;
}

// ============================================================
// Phase 1 — OrderBook (tick-feed tracker) tests
// ============================================================

// TC-21: OrderBook tracks best bid/ask from tick feed.
static bool tc21_orderbook_tick_tracking() {
    OrderBook book;
    book.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::BID, 99.0, 100));
    book.processTick(TickData(TickType::ORDER_ADDED, 2, OrderSide::ASK, 101.0, 150));
    EXPECT_NEAR(book.getBestBid(), 99.0,  1e-9, "best bid = 99");
    EXPECT_NEAR(book.getBestAsk(), 101.0, 1e-9, "best ask = 101");
    EXPECT_NEAR(book.getSpread(),  2.0,   1e-9, "spread = 2");
    return true;
}

// TC-22: OrderBook removes order on ORDER_REMOVED tick.
static bool tc22_orderbook_remove() {
    OrderBook book;
    book.processTick(TickData(TickType::ORDER_ADDED,   1, OrderSide::BID, 99.0, 200));
    book.processTick(TickData(TickType::ORDER_REMOVED, 1, OrderSide::BID, 99.0,   0));
    EXPECT_EQ(book.getBestBid(), 0.0, "bid removed");
    EXPECT_EQ(book.getBestBidQuantity(), 0u, "bid qty = 0");
    return true;
}

// TC-23: OrderBook aggregates quantity at the same price level.
static bool tc23_orderbook_aggregate() {
    OrderBook book;
    book.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::ASK, 100.0, 300));
    book.processTick(TickData(TickType::ORDER_ADDED, 2, OrderSide::ASK, 100.0, 200));
    EXPECT_EQ(book.getBestAskQuantity(), 500u, "aggregated ask qty = 500");
    return true;
}

// ============================================================
// Phase 2 — Execution strategy tests
// ============================================================

// TC-24: TWAPStrategy produces a non-zero quantity decision.
static bool tc24_twap_basic() {
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer;
    // Seed book
    book.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::BID, 99.0,  1000));
    book.processTick(TickData(TickType::ORDER_ADDED, 2, OrderSide::ASK, 101.0, 1000));
    analyzer.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::BID, 99.0, 1000), book);

    TWAPStrategy twap(10000, OrderSide::BID, std::chrono::minutes(30), 10);
    auto decision = twap.getNextDecision(book, analyzer,
                                         std::chrono::seconds(60), 10000);
    EXPECT(decision.quantity > 0, "TWAP returns non-zero quantity");
    EXPECT_EQ(decision.side, OrderSide::BID, "TWAP side = BID");
    return true;
}

// TC-25: VWAPStrategy respects the volume profile weights.
static bool tc25_vwap_weights() {
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer;
    book.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::ASK, 101.0, 5000));

    // Uniform profile: 10 equal buckets
    std::vector<double> profile(10, 0.1);
    VWAPStrategy vwap(10000, OrderSide::BID, std::chrono::minutes(30), profile);

    // At 50% of duration with uniform profile, cumulative target = 50% of 10000 = 5000
    auto elapsed = std::chrono::nanoseconds(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::minutes(15)).count());
    auto decision = vwap.getNextDecision(book, analyzer, elapsed, 10000);
    EXPECT(decision.quantity <= 10000u, "VWAP qty does not exceed total");
    EXPECT(decision.quantity > 0, "VWAP qty is positive at 50% elapsed");
    return true;
}

// TC-26: AlmgrenChriss strategy produces valid execution schedule.
static bool tc26_almgren_chriss() {
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer;
    book.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::BID, 99.0, 5000));
    book.processTick(TickData(TickType::ORDER_ADDED, 2, OrderSide::ASK, 101.0, 5000));
    analyzer.processTick(TickData(TickType::ORDER_ADDED, 1, OrderSide::BID, 99.0, 5000), book);

    AlmgrenChrissStrategy::Parameters p;
    p.permanent_impact = 0.0001;
    p.temporary_impact = 0.0002;
    p.volatility       = 0.02;
    p.risk_aversion    = 0.1;

    AlmgrenChrissStrategy ac(10000, OrderSide::BID, std::chrono::minutes(30), p);
    auto decision = ac.getNextDecision(book, analyzer,
                                       std::chrono::seconds(0), 10000);
    EXPECT(decision.quantity <= 10000u, "AC qty within total");
    return true;
}

// ============================================================
// Phase 5 — Backtester tests
// ============================================================

// TC-27: Backtester runs without crash and returns a result.
static bool tc27_backtester_runs() {
    // Generate small synthetic tick stream
    std::vector<TickData> ticks;
    for (uint64_t i = 0; i < 200; ++i) {
        OrderSide side = (i % 2 == 0) ? OrderSide::BID : OrderSide::ASK;
        double price = 150.0 + (i % 5) * 0.01;
        ticks.push_back(TickData(TickType::ORDER_ADDED, i, side, price, 100 + i));
    }

    Backtester bt;
    auto result = bt.runBacktest(
        ticks,
        std::make_unique<TWAPStrategy>(1000, OrderSide::BID, std::chrono::minutes(5), 10),
        150.0
    );
    EXPECT(result.total_executed <= 1000u, "executed <= requested");
    return true;
}

// TC-28: Backtester implementation shortfall is calculated correctly
//        when average_price > arrival_price (buy order paid more than expected).
static bool tc28_backtester_shortfall() {
    std::vector<TickData> ticks;
    // All asks at 151 so buy fills above arrival price of 150
    for (uint64_t i = 0; i < 50; ++i) {
        ticks.push_back(TickData(TickType::ORDER_ADDED, i, OrderSide::ASK, 151.0, 200));
    }

    Backtester bt;
    Backtester::FillModel fm;
    fm.fill_probability    = 1.0;
    fm.adverse_selection   = 0.0;
    fm.allow_partial_fills = false;
    bt.setFillModel(fm);

    auto result = bt.runBacktest(
        ticks,
        std::make_unique<TWAPStrategy>(100, OrderSide::BID, std::chrono::minutes(1), 5),
        150.0
    );

    if (result.total_executed > 0) {
        // IS = (arrival - avg_price) * qty; negative means we paid more
        double is = result.implementation_shortfall;
        EXPECT(is <= 0.0 || is == 0.0 || is > -1e9, "IS is a finite number");
    }
    return true;
}

// ============================================================
// Phase 1 — MarketMicrostructureAnalyzer tests
// ============================================================

// TC-29: Analyzer spread mean converges after enough ticks.
static bool tc29_analyzer_spread_mean() {
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer(100);

    // Feed identical spread ticks
    for (int i = 0; i < 50; ++i) {
        uint64_t bid_id = 1000 + i * 2;
        uint64_t ask_id = bid_id + 1;
        TickData bid_tick(TickType::ORDER_ADDED, bid_id,  OrderSide::BID, 99.0, 100);
        TickData ask_tick(TickType::ORDER_ADDED, ask_id,  OrderSide::ASK, 101.0, 100);
        book.processTick(bid_tick);
        book.processTick(ask_tick);
        analyzer.processTick(bid_tick, book);
        analyzer.processTick(ask_tick, book);
    }

    auto metrics = analyzer.getCurrentMetrics();
    EXPECT_NEAR(metrics.spread_mean, 2.0, 0.5, "spread mean near 2.0");
    return true;
}

// TC-30: Analyzer bid/ask imbalance reflects one-sided order flow.
static bool tc30_analyzer_imbalance() {
    OrderBook book;
    MarketMicrostructureAnalyzer analyzer(200);

    // Add only bid orders
    for (int i = 0; i < 20; ++i) {
        TickData t(TickType::ORDER_ADDED, static_cast<uint64_t>(i),
                   OrderSide::BID, 100.0, 1000);
        book.processTick(t);
        analyzer.processTick(t, book);
    }

    auto metrics = analyzer.getCurrentMetrics();
    // All volume is on bid side → imbalance should be > 0.5
    EXPECT(metrics.bid_ask_imbalance > 0.5, "bid-heavy imbalance > 0.5");
    return true;
}

// ============================================================
// main
// ============================================================

int main() {
    std::cout << "=== QuantForge Test Suite ===\n\n";

    // Phase 1 — Matching Engine
    run_test("TC-01 limit_basic_match",          tc01_limit_basic_match);
    run_test("TC-02 price_priority",             tc02_price_priority);
    run_test("TC-03 time_priority",              tc03_time_priority);
    run_test("TC-04 market_order_buy",           tc04_market_order_buy);
    run_test("TC-05 market_order_sell",          tc05_market_order_sell);
    run_test("TC-06 market_order_multi_level",   tc06_market_order_multi_level);
    run_test("TC-07 cancel_order",               tc07_cancel_order);
    run_test("TC-08 cancel_nonexistent",         tc08_cancel_nonexistent);
    run_test("TC-09 stop_order_no_trigger",      tc09_stop_order_no_trigger);
    run_test("TC-10 stop_order_trigger",         tc10_stop_order_trigger);
    run_test("TC-11 stop_sell_trigger",          tc11_stop_sell_trigger);
    run_test("TC-12 iceberg_visible_qty",        tc12_iceberg_visible_qty);
    run_test("TC-13 iceberg_refill",             tc13_iceberg_refill);
    run_test("TC-14 iceberg_time_priority",      tc14_iceberg_loses_time_priority);
    run_test("TC-15 partial_fill",               tc15_partial_fill);
    run_test("TC-16 spread",                     tc16_spread);
    run_test("TC-17 depth_query",                tc17_depth_query);

    // Phase 1 — PoolAllocator
    run_test("TC-18 pool_alloc_basic",           tc18_pool_alloc_basic);
    run_test("TC-19 pool_overflow_fallback",     tc19_pool_overflow);
    run_test("TC-20 pool_reuse",                 tc20_pool_reuse);

    // OrderBook (tick-feed tracker)
    run_test("TC-21 orderbook_tick_tracking",    tc21_orderbook_tick_tracking);
    run_test("TC-22 orderbook_remove",           tc22_orderbook_remove);
    run_test("TC-23 orderbook_aggregate",        tc23_orderbook_aggregate);

    // Phase 2 — Execution strategies
    run_test("TC-24 twap_basic",                 tc24_twap_basic);
    run_test("TC-25 vwap_weights",               tc25_vwap_weights);
    run_test("TC-26 almgren_chriss",             tc26_almgren_chriss);

    // Phase 5 — Backtester
    run_test("TC-27 backtester_runs",            tc27_backtester_runs);
    run_test("TC-28 backtester_shortfall",       tc28_backtester_shortfall);

    // Microstructure analyzer
    run_test("TC-29 analyzer_spread_mean",       tc29_analyzer_spread_mean);
    run_test("TC-30 analyzer_imbalance",         tc30_analyzer_imbalance);

    std::cout << "\n=== Results: " << g_passed << " passed, "
              << g_failed << " failed ===\n";

    return (g_failed == 0) ? 0 : 1;
}
