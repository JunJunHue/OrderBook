'use client';

import { useState, useEffect, useRef } from 'react';
import Link from 'next/link';

// ─── Types ────────────────────────────────────────────────────────────────────

type Module = 'orderbook' | 'history' | 'cpp' | 'vwap';

interface OBLevel { price: number; size: number; }

// ─── OrderBook Animation ──────────────────────────────────────────────────────

function OrderBookModule() {
  const [bids, setBids] = useState<OBLevel[]>([]);
  const [asks, setAsks] = useState<OBLevel[]>([]);
  const midRef = useRef(500.0);

  useEffect(() => {
    const interval = setInterval(() => {
      const mid = midRef.current + (Math.random() - 0.5) * 0.8;
      midRef.current = mid;
      setBids(Array.from({ length: 6 }, (_, i) => ({
        price: parseFloat((mid - 0.05 - i * 0.05).toFixed(2)),
        size: Math.round(Math.random() * 1800 + 100),
      })));
      setAsks(Array.from({ length: 6 }, (_, i) => ({
        price: parseFloat((mid + 0.05 + i * 0.05).toFixed(2)),
        size: Math.round(Math.random() * 1800 + 100),
      })));
    }, 600);
    return () => clearInterval(interval);
  }, []);

  const maxSize = Math.max(...bids.map(b => b.size), ...asks.map(a => a.size), 1);
  const spread = asks[0] && bids[0] ? (asks[0].price - bids[0].price).toFixed(2) : '—';

  return (
    <div className="max-w-4xl mx-auto p-6">
      <h2 className="font-display text-2xl text-[#00c8ff] mb-1 font-bold">What is an Order Book?</h2>
      <p className="text-[#8b949e] mb-6 text-sm">
        The real-time list of all outstanding buy and sell orders for an asset, organized by price level.
      </p>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-8">
        {/* Live order book */}
        <div className="bg-[#0d1117] rounded-lg border border-[#1e2832] overflow-hidden">
          <div className="px-4 py-2 border-b border-[#1e2832] text-[10px] text-[#8b949e] uppercase tracking-widest flex justify-between">
            <span>Live Simulation</span>
            <span className="flex items-center gap-1 text-[#00d97e]">
              <span className="w-1 h-1 rounded-full bg-[#00d97e] animate-pulse" />animating
            </span>
          </div>

          {/* Asks reversed */}
          <div className="flex flex-col-reverse">
            {asks.map((a, i) => (
              <div key={i} className="relative px-4 py-1.5 flex justify-between text-sm">
                <div className="absolute top-0 right-0 bottom-0 bg-[#ff335515]" style={{ width: `${(a.size / maxSize) * 100}%` }} />
                <span className="relative text-[#ff3355] font-bold">{a.price.toFixed(2)}</span>
                <span className="relative text-[#c9d1d9]">{a.size.toLocaleString()}</span>
              </div>
            ))}
          </div>

          {/* Spread row */}
          <div className="px-4 py-1.5 bg-[#0a0f14] border-y border-[#1e2832] flex justify-between text-[11px]">
            <span className="text-[#8b949e]">── SPREAD ──</span>
            <span className="text-[#f5a623] font-bold">${spread}</span>
          </div>

          {/* Bids */}
          {bids.map((b, i) => (
            <div key={i} className="relative px-4 py-1.5 flex justify-between text-sm">
              <div className="absolute top-0 left-0 bottom-0 bg-[#00d97e14]" style={{ width: `${(b.size / maxSize) * 100}%` }} />
              <span className="relative text-[#00d97e] font-bold">{b.price.toFixed(2)}</span>
              <span className="relative text-[#c9d1d9]">{b.size.toLocaleString()}</span>
            </div>
          ))}
        </div>

        {/* Explanation */}
        <div className="flex flex-col gap-4">
          {[
            { term: 'Bid', color: '#00d97e', desc: 'The highest price a buyer is willing to pay. Bids sit below the current price.' },
            { term: 'Ask', color: '#ff3355', desc: 'The lowest price a seller will accept. Asks sit above the current price.' },
            { term: 'Spread', color: '#f5a623', desc: 'The gap between best bid and best ask. This is the market maker\'s compensation for providing liquidity.' },
            { term: 'Match', color: '#00c8ff', desc: 'When a buy order price ≥ a sell order price, the engine executes a fill at the ask price (price-time priority).' },
          ].map(({ term, color, desc }) => (
            <div key={term} className="flex gap-3">
              <div className="w-1 rounded-full shrink-0 mt-1" style={{ backgroundColor: color, minHeight: '2rem' }} />
              <div>
                <span className="font-bold text-sm" style={{ color }}>{term}</span>
                <p className="text-[#8b949e] text-sm mt-0.5">{desc}</p>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="bg-[#0d1117] border border-[#1e2832] rounded-lg p-4 text-sm text-[#8b949e]">
        <span className="text-[#00c8ff] font-bold">Price-Time Priority: </span>
        Among orders at the same price level, the order that arrived <em className="text-[#e6edf3]">first</em> fills first.
        This is why high-frequency traders colocate their servers within feet of exchange matching engines — microseconds determine queue position.
      </div>
    </div>
  );
}

// ─── History of Quant ─────────────────────────────────────────────────────────

const TIMELINE = [
  {
    year: '1962',
    name: 'Ed Thorp',
    subtitle: 'Beat the Market',
    color: '#f5a623',
    desc: `Ed Thorp, a mathematics professor at MIT, published "Beat the Dealer" in 1962 — the first mathematically proven card counting system. He later applied the same probabilistic thinking to financial markets, discovering that warrants were systematically mispriced. His firm Princeton Newport Partners returned ~15% annually from 1969–1988 using delta-hedging techniques that predated Black-Scholes by nearly a decade.`,
    tag: 'FIRST QUANT FUND',
  },
  {
    year: '1973',
    name: 'Black-Scholes-Merton',
    subtitle: 'Options Pricing Formula',
    color: '#00c8ff',
    desc: `Fischer Black, Myron Scholes, and Robert Merton published the formula that would win a Nobel Prize in 1997 and create the modern derivatives market. The Black-Scholes equation — ∂V/∂t + ½σ²S²∂²V/∂S² + rS∂V/∂S − rV = 0 — gave traders a closed-form solution for pricing European options. Within a year, the CBOE's daily option volume increased 1000×.`,
    tag: 'NOBEL 1997',
  },
  {
    year: '1988',
    name: 'Renaissance Technologies',
    subtitle: 'The Medallion Fund',
    color: '#00d97e',
    desc: `Jim Simons, a former NSA codebreaker and mathematics department chair, assembled a team of physicists, cryptographers, and mathematicians — deliberately excluding finance professionals. The Medallion Fund returned ~66% annually before fees from 1988–2018, the greatest investment record in history. Their models discovered non-random patterns in market microstructure data invisible to traditional analysts.`,
    tag: '66% ANNUAL RETURN',
  },
  {
    year: '2020s',
    name: 'ML & LLM Quant',
    subtitle: 'Transformers & RL',
    color: '#a78bfa',
    desc: `Modern quant firms deploy transformer models fine-tuned on earnings call transcripts for sentiment alpha, reinforcement learning agents for optimal execution, and graph neural networks for cross-asset correlation modeling. Signals decay in days or weeks, requiring continuous retraining pipelines. The edge has shifted from having models to having better data infrastructure and lower latency inference.`,
    tag: 'CURRENT ERA',
  },
];

function HistoryModule() {
  const [expanded, setExpanded] = useState<number | null>(0);

  return (
    <div className="max-w-3xl mx-auto p-6">
      <h2 className="font-display text-2xl text-[#00c8ff] mb-1 font-bold">History of Quant</h2>
      <p className="text-[#8b949e] mb-8 text-sm">From card counting to transformers — six decades of mathematical edge.</p>

      <div className="relative">
        {/* Timeline line */}
        <div className="absolute left-[5.5rem] top-0 bottom-0 w-px bg-[#1e2832]" />

        <div className="flex flex-col gap-0">
          {TIMELINE.map((item, i) => (
            <div key={i} className="relative">
              {/* Node */}
              <div
                className="absolute left-[5.5rem] w-3 h-3 rounded-full border-2 border-[#080c10] -translate-x-1/2 mt-4 z-10"
                style={{ backgroundColor: item.color, top: 0 }}
              />

              <div className="pl-24 pb-6">
                <button
                  onClick={() => setExpanded(expanded === i ? null : i)}
                  className="w-full text-left group"
                >
                  <div className="flex items-start gap-3">
                    <div className="shrink-0 w-16 text-right">
                      <span className="text-[11px] font-bold" style={{ color: item.color }}>{item.year}</span>
                    </div>
                    <div className="flex-1">
                      <div className="flex items-center gap-2 mb-0.5">
                        <span className="font-display font-bold text-[#e6edf3] group-hover:text-[#00c8ff] transition-colors">
                          {item.name}
                        </span>
                        <span
                          className="text-[9px] uppercase tracking-widest px-1.5 py-0.5 rounded"
                          style={{ color: item.color, backgroundColor: `${item.color}18`, border: `1px solid ${item.color}44` }}
                        >
                          {item.tag}
                        </span>
                      </div>
                      <div className="text-[#8b949e] text-sm">{item.subtitle}</div>
                    </div>
                    <span className="text-[#8b949e] text-lg shrink-0 mt-0.5">
                      {expanded === i ? '−' : '+'}
                    </span>
                  </div>
                </button>

                {expanded === i && (
                  <div className="mt-3 pl-16 pr-4 text-sm text-[#8b949e] leading-relaxed border-l-2" style={{ borderColor: `${item.color}44` }}>
                    {item.desc}
                  </div>
                )}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

// ─── Why C++ ──────────────────────────────────────────────────────────────────

const BENCHMARKS = [
  { label: 'Python dict', ops: 5000, color: '#f5a623', reason: 'GIL, dynamic dispatch, GC pauses' },
  { label: 'C++ std::map', ops: 120000, color: '#00c8ff', reason: 'O(log n) tree traversal, heap allocations' },
  { label: 'C++ pool alloc', ops: 800000, color: '#38bdf8', reason: 'No malloc jitter — slab allocator' },
  { label: 'C++ SIMD + flat', ops: 10000000, color: '#00d97e', reason: 'Cache-aligned flat array + AVX2 vectorization' },
];
const MAX_OPS = 10000000;

function CppModule() {
  const [running, setRunning] = useState(false);
  const [progress, setProgress] = useState<number[]>([0, 0, 0, 0]);

  const runBenchmark = () => {
    if (running) return;
    setRunning(true);
    setProgress([0, 0, 0, 0]);
    BENCHMARKS.forEach((b, i) => {
      const delay = i * 400;
      const duration = 1000;
      const start = Date.now() + delay;
      setTimeout(() => {
        const tick = setInterval(() => {
          const elapsed = Date.now() - start;
          const p = Math.min(elapsed / duration, 1);
          setProgress(prev => {
            const next = [...prev];
            next[i] = p;
            return next;
          });
          if (p >= 1) {
            clearInterval(tick);
            if (i === BENCHMARKS.length - 1) setRunning(false);
          }
        }, 16);
      }, delay);
    });
  };

  return (
    <div className="max-w-3xl mx-auto p-6">
      <h2 className="font-display text-2xl text-[#00c8ff] mb-1 font-bold">Why C++?</h2>
      <p className="text-[#8b949e] mb-6 text-sm">
        Order matching is pure throughput. Every microsecond of latency is queue position lost. Here&apos;s what the numbers look like.
      </p>

      <div className="bg-[#0d1117] border border-[#1e2832] rounded-lg p-5 mb-6">
        <div className="flex justify-between items-center mb-5">
          <span className="text-[10px] text-[#8b949e] uppercase tracking-widest">Ticks Processed / Second</span>
          <button
            onClick={runBenchmark}
            disabled={running}
            className="px-4 py-1.5 text-[11px] font-bold uppercase tracking-widest rounded transition-all bg-[#00c8ff] text-[#080c10] hover:bg-[#33d4ff] disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {running ? 'Running…' : 'Run Benchmark'}
          </button>
        </div>

        <div className="flex flex-col gap-4">
          {BENCHMARKS.map((b, i) => {
            const animPct = (b.ops / MAX_OPS) * progress[i] * 100;
            const displayOps = Math.round(b.ops * progress[i]);
            return (
              <div key={i}>
                <div className="flex justify-between text-sm mb-1.5">
                  <span className="font-bold" style={{ color: b.color }}>{b.label}</span>
                  <span className="text-[#e6edf3] font-bold tabular-nums">
                    {displayOps.toLocaleString()} ops/s
                  </span>
                </div>
                <div className="h-5 bg-[#080c10] rounded overflow-hidden relative">
                  <div
                    className="absolute inset-y-0 left-0 rounded transition-all duration-100"
                    style={{
                      width: `${animPct}%`,
                      backgroundColor: b.color,
                      opacity: 0.85,
                    }}
                  />
                  <div className="absolute inset-0 flex items-center px-2">
                    <span className="text-[10px] text-[#8b949e]">{b.reason}</span>
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
        {[
          { title: 'Memory Layout', icon: '▦', desc: 'Pool allocators avoid malloc() jitter. Cache-aligned structs ensure hot data fits in L1 cache (32KB).' },
          { title: 'SIMD Intrinsics', icon: '⊞', desc: '__m256 AVX2 processes 4× doubles per cycle. Price comparison across 8 levels in a single instruction.' },
          { title: 'No GC Pauses', icon: '⏱', desc: 'Garbage collection can pause execution for 10–100ms. At 10M ops/sec, that is 100,000 missed ticks.' },
        ].map(({ title, icon, desc }) => (
          <div key={title} className="bg-[#0d1117] border border-[#1e2832] rounded-lg p-4">
            <div className="text-[#00c8ff] text-xl mb-2">{icon}</div>
            <div className="font-bold text-sm text-[#e6edf3] mb-1">{title}</div>
            <p className="text-[11px] text-[#8b949e] leading-relaxed">{desc}</p>
          </div>
        ))}
      </div>
    </div>
  );
}

// ─── VWAP Step-Through ────────────────────────────────────────────────────────

const VWAP_SCHEDULE = [
  { time: '9:30–10:00', pct: 0.15, shares: 1500, price: 874.50 },
  { time: '10:00–10:30', pct: 0.10, shares: 1000, price: 875.20 },
  { time: '10:30–11:00', pct: 0.08, shares: 800, price: 875.80 },
  { time: '11:00–11:30', pct: 0.07, shares: 700, price: 876.10 },
  { time: '11:30–12:00', pct: 0.06, shares: 600, price: 875.90 },
  { time: '12:00–12:30', pct: 0.05, shares: 500, price: 875.50 },
  { time: '12:30–13:00', pct: 0.05, shares: 500, price: 875.30 },
  { time: '13:00–13:30', pct: 0.06, shares: 600, price: 875.70 },
  { time: '13:30–14:00', pct: 0.07, shares: 700, price: 876.20 },
  { time: '14:00–14:30', pct: 0.08, shares: 800, price: 876.50 },
  { time: '14:30–15:00', pct: 0.10, shares: 1000, price: 876.80 },
  { time: '15:00–15:30', pct: 0.13, shares: 1300, price: 877.20 },
];
const TOTAL_SHARES = 10000;

function VwapModule() {
  const [step, setStep] = useState(-1);

  const executed = VWAP_SCHEDULE.slice(0, step + 1);
  const totalExecuted = executed.reduce((s, r) => s + r.shares, 0);
  const vwap = executed.length > 0
    ? executed.reduce((s, r) => s + r.shares * r.price, 0) / totalExecuted
    : 0;
  const current = VWAP_SCHEDULE[step];

  return (
    <div className="max-w-4xl mx-auto p-6">
      <h2 className="font-display text-2xl text-[#00c8ff] mb-1 font-bold">How Does VWAP Work?</h2>
      <p className="text-[#8b949e] mb-6 text-sm">
        Volume-Weighted Average Price slices a large order across the trading day, proportional to historical volume at each interval.
        Minimizes market impact by never representing more than ~15% of typical volume.
      </p>

      {/* Order summary */}
      <div className="grid grid-cols-4 gap-3 mb-6">
        {[
          { label: 'Total Order', value: `${TOTAL_SHARES.toLocaleString()} shares`, color: '#e6edf3' },
          { label: 'Executed', value: `${totalExecuted.toLocaleString()}`, color: '#00d97e' },
          { label: 'Remaining', value: `${(TOTAL_SHARES - totalExecuted).toLocaleString()}`, color: '#f5a623' },
          { label: 'VWAP', value: vwap > 0 ? `$${vwap.toFixed(2)}` : '—', color: '#00c8ff' },
        ].map(({ label, value, color }) => (
          <div key={label} className="bg-[#0d1117] border border-[#1e2832] rounded-lg p-3">
            <div className="text-[10px] text-[#8b949e] uppercase tracking-widest mb-1">{label}</div>
            <div className="text-lg font-bold font-display" style={{ color }}>{value}</div>
          </div>
        ))}
      </div>

      {/* Progress bar */}
      <div className="h-2 bg-[#1e2832] rounded-full mb-6 overflow-hidden">
        <div
          className="h-full bg-[#00d97e] transition-all duration-500 rounded-full"
          style={{ width: `${(totalExecuted / TOTAL_SHARES) * 100}%` }}
        />
      </div>

      {/* Schedule grid */}
      <div className="bg-[#0d1117] border border-[#1e2832] rounded-lg overflow-hidden mb-4">
        <div className="grid grid-cols-5 gap-0 text-[10px] text-[#8b949e] uppercase tracking-wider px-4 py-2 border-b border-[#1e2832]">
          <span>Interval</span>
          <span className="text-right">Vol%</span>
          <span className="text-right">Shares</span>
          <span className="text-right">Exec Price</span>
          <span className="text-right">Status</span>
        </div>
        {VWAP_SCHEDULE.map((row, i) => {
          const done = i <= step;
          const active = i === step;
          return (
            <div
              key={i}
              className={`grid grid-cols-5 gap-0 px-4 py-2 border-b border-[#1e283244] text-sm transition-colors ${
                active ? 'bg-[#00c8ff11]' : done ? 'opacity-60' : ''
              }`}
            >
              <span className={active ? 'text-[#00c8ff] font-bold' : done ? 'text-[#8b949e]' : 'text-[#e6edf3]'}>
                {row.time}
              </span>
              <span className="text-right text-[#8b949e]">{(row.pct * 100).toFixed(0)}%</span>
              <span className={`text-right font-bold ${done ? 'text-[#00d97e]' : 'text-[#8b949e]'}`}>
                {done ? row.shares.toLocaleString() : '—'}
              </span>
              <span className={`text-right ${done ? 'text-[#e6edf3]' : 'text-[#8b949e]'}`}>
                {done ? `$${row.price.toFixed(2)}` : '—'}
              </span>
              <span className={`text-right text-[10px] ${done ? 'text-[#00d97e]' : 'text-[#8b949e]'}`}>
                {done ? '✓ FILLED' : i === step + 1 ? '◎ NEXT' : '○ PENDING'}
              </span>
            </div>
          );
        })}
      </div>

      {/* Explanation of current step */}
      {current && (
        <div className="bg-[#0d1117] border border-[#00c8ff33] rounded-lg p-4 mb-4 text-sm">
          <span className="text-[#00c8ff] font-bold">Interval {step + 1} of {VWAP_SCHEDULE.length}: </span>
          <span className="text-[#8b949e]">
            Historical volume between {current.time} represents{' '}
            <span className="text-[#e6edf3] font-bold">{(current.pct * 100).toFixed(0)}%</span> of daily volume.
            Allocating <span className="text-[#00d97e] font-bold">{current.shares.toLocaleString()} shares</span>{' '}
            ({(current.pct * 100).toFixed(0)}% × {TOTAL_SHARES.toLocaleString()}) into this window.
            Executed at market price <span className="text-[#e6edf3] font-bold">${current.price.toFixed(2)}</span>.
          </span>
        </div>
      )}
      {step === -1 && (
        <div className="bg-[#0d1117] border border-[#1e2832] rounded-lg p-4 mb-4 text-sm text-[#8b949e]">
          Press <span className="text-[#00c8ff]">Step Forward</span> to simulate a 10,000-share NVDA order being executed via VWAP.
        </div>
      )}
      {step === VWAP_SCHEDULE.length - 1 && (
        <div className="bg-[#00d97e11] border border-[#00d97e33] rounded-lg p-4 mb-4 text-sm">
          <span className="text-[#00d97e] font-bold">Execution complete. </span>
          <span className="text-[#8b949e]">
            10,000 shares executed at VWAP <span className="text-[#00d97e] font-bold">${vwap.toFixed(2)}</span>.
            Arrival price was $874.50. Implementation shortfall:{' '}
            <span className="text-[#f5a623] font-bold">${((vwap - 874.50) * TOTAL_SHARES).toFixed(0)}</span> total.
          </span>
        </div>
      )}

      {/* Controls */}
      <div className="flex gap-2">
        <button
          onClick={() => setStep(s => Math.min(s + 1, VWAP_SCHEDULE.length - 1))}
          disabled={step >= VWAP_SCHEDULE.length - 1}
          className="px-5 py-2 bg-[#00c8ff] text-[#080c10] rounded font-bold text-sm uppercase tracking-widest hover:bg-[#33d4ff] disabled:opacity-40 disabled:cursor-not-allowed transition-all"
        >
          Step Forward ▶
        </button>
        <button
          onClick={() => setStep(-1)}
          className="px-5 py-2 border border-[#1e2832] text-[#8b949e] rounded text-sm uppercase tracking-widest hover:text-[#e6edf3] hover:border-[#8b949e] transition-all"
        >
          Reset
        </button>
      </div>
    </div>
  );
}

// ─── Main Learn Page ──────────────────────────────────────────────────────────

const MODULES: { id: Module; label: string }[] = [
  { id: 'orderbook', label: 'What is an Order Book?' },
  { id: 'history', label: 'History of Quant' },
  { id: 'cpp', label: 'Why C++?' },
  { id: 'vwap', label: 'How Does VWAP Work?' },
];

export default function Learn() {
  const [active, setActive] = useState<Module>('orderbook');

  return (
    <div className="min-h-screen bg-[#080c10] text-[#e6edf3] font-mono">
      {/* Nav */}
      <header className="flex items-center gap-4 px-4 h-9 border-b border-[#1e2832]">
        <span className="font-display text-[#00c8ff] text-sm font-bold tracking-[0.25em]">QUANTFORGE</span>
        <div className="w-px h-4 bg-[#1e2832]" />
        <Link href="/" className="text-[11px] text-[#8b949e] hover:text-[#00c8ff] transition-colors uppercase tracking-widest">
          Terminal
        </Link>
        <span className="text-[11px] text-[#e6edf3] uppercase tracking-widest">Learn</span>
      </header>

      {/* Module tabs */}
      <div className="border-b border-[#1e2832] flex overflow-x-auto">
        {MODULES.map(m => (
          <button
            key={m.id}
            onClick={() => setActive(m.id)}
            className={`px-5 py-3 text-[11px] uppercase tracking-widest whitespace-nowrap border-b-2 transition-all ${
              active === m.id
                ? 'border-[#00c8ff] text-[#00c8ff]'
                : 'border-transparent text-[#8b949e] hover:text-[#e6edf3]'
            }`}
          >
            {m.label}
          </button>
        ))}
      </div>

      {/* Content */}
      <div className="py-4">
        {active === 'orderbook' && <OrderBookModule />}
        {active === 'history' && <HistoryModule />}
        {active === 'cpp' && <CppModule />}
        {active === 'vwap' && <VwapModule />}
      </div>
    </div>
  );
}
