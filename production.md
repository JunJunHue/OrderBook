# QuantForge — C++ Orderbook Simulator for AI Infrastructure Stocks

A production roadmap for building a high-performance orderbook simulator covering C++ systems engineering, ML trading strategies, educational UI, and backtesting analytics.

---

## Build Order

1. C++ matching engine (orders matching + fill logging)
2. Data store
3. Python/ML bridge
4. UI + educational layer
5. Polish + benchmarking

**Recommended timeline:** 12 weeks
- Weeks 1–3: C++ core engine
- Weeks 4–5: ML strategies
- Weeks 6–7: Backtesting
- Weeks 8–10: UI + educational layer
- Weeks 11–12: Polish and benchmarking

---

## Phase 1 — C++ Core Engine

The resume centerpiece. Proves understanding of low-latency systems.

### Goal
A price-time priority matching engine in C++17/20.

### Core Data Structure
- Base: `std::map<price, queue<Order>>` for bid and ask sides
- Optimize to: lock-free skip list or flat array with pointer arithmetic
- Target: sub-microsecond match times

### C++ Concepts to Implement
| Concept | Purpose |
|---|---|
| Memory layout + cache locality | `std::deque` vs pool allocator — avoid cache miss latency |
| Lock-free programming | `std::atomic` + CAS loops |
| SIMD intrinsics | `__m256` AVX2 for batch price comparisons |
| `constexpr` + template metaprogramming | Eliminate runtime overhead |
| Custom memory pool allocators | Avoid `malloc` jitter during order matching |
| `perf` + `valgrind` profiling | Measure and prove speedups with hard numbers |

### Order Types
- **Market orders** — fill immediately at best price
- **Limit orders** — rest in book until matched
- **Stop orders** — trigger at threshold price
- **Iceberg orders** — hidden quantity, partial display

### Deliverable
Matching engine that logs fills, with benchmarks showing latency improvements at each optimization step.

---

## Phase 2 — ML Trading Strategies

Goal: apply models, not build them from scratch. Each strategy is a separate module.

### Strategies

**VWAP Execution**
Slice a large order across time, weighted by predicted volume. Classic institutional algorithm.

**Momentum Strategy**
Buy NVDA if its 5-min return exceeds a threshold. Signal from a logistic regression model trained on historical tick data.

**Mean Reversion**
If CEG deviates >2σ from its 20-day mean, bet on return. Pairs with nuclear/power theme in the AI infrastructure ticker list.

**LSTM Price Predictor**
- PyTorch model (Python side) outputting a 1-minute price direction signal
- C++ engine consumes signal via shared-memory buffer or ZeroMQ socket
- The C++/Python bridge itself is an interview talking point

**Sentiment Overlay**
Weight signals by "AI demand news" sentiment scores. Can mock with a lookup table keyed to ticker list.

### Bridge Options
- `pybind11` — tighter integration, single process
- Shared memory — lowest latency IPC
- ZeroMQ socket — cleanest separation, easiest to demo

---

## Phase 3 — Educational Layer

The differentiator. Most quant projects are raw engines — wrapping in a teaching UI demonstrates product thinking.

### Modules

**"What is an Order Book?"**
Animated SVG: bids stacking below asks, live spread visualization that updates as orders are submitted.

**"History of Quant"**
Interactive timeline:
- Ed Thorp's card counting
- Black-Scholes (1973)
- Renaissance Technologies
- Modern ML quant

Each node expands with a 2-paragraph explainer.

**"Why C++?"**
Side-by-side benchmark: Python orderbook mock vs C++ engine. Target: show 100x–1000x latency difference. Concrete and visual.

**"How Does My VWAP Algo Work?"**
Step-through debugger mode: user places a 10,000-share order, watches it get sliced across simulated time with annotated reasoning at each step.

---

## Phase 4 — Sandbox Trading Interface

A Bloomberg-lite user-facing trading terminal.

### Features
- **Order book ladder** — live bids/asks updating from simulated feed
- **Order ticket** — toggle market/limit, set quantity and price, submit
- **Position blotter** — current holdings, average cost, unrealized PnL
- **Strategy autopilot toggle** — hands control to ML engine
- **Performance panel** — fills/second, average slippage, session Sharpe ratio

### Tech Stack Options
| Option | Notes |
|---|---|
| C++ → WebSocket → React/TypeScript | More impressive on a resume, broader audience |
| C++ → Qt GUI | Stays in C++, faster to build locally |

Recommended: WebSocket approach for resume impact.

---

## Phase 5 — Backtesting & Analytics

Closes the loop. Makes this a real research platform.

### Data
- Source: Yahoo Finance or Alpha Vantage API (free OHLCV)
- Tickers: AI infrastructure stocks (NVDA, CEG, and others from your theme)
- Period: 2020–2024 (captures COVID crash, AI boom, rate hike cycle)

### Output Metrics
- Sharpe ratio
- Max drawdown
- CAGR
- Win rate
- Average holding period

### Visualizations
- Equity curves per strategy
- Head-to-head strategy comparison plots

---

## Tickers (AI Infrastructure Theme)
- **NVDA** — GPU compute
- **CEG** — nuclear power (AI data center energy)
- Additional: AMD, MSFT, GOOGL, META (expand as needed)

---

## Key Interview Talking Points
1. Benchmarked latency improvements at each C++ optimization step (pool allocator, SIMD, lock-free)
2. C++/Python bridge design decision (pybind11 vs shared memory vs ZeroMQ)
3. Educational layer as a product decision, not just a demo
4. Backtesting over a period that includes multiple distinct market regimes
