'use client';

import { useState, useEffect, useRef } from 'react';
import Link from 'next/link';

// ─── Types ────────────────────────────────────────────────────────────────────

interface Level {
  price: number;
  size: number;
  count: number;
}

interface Fill {
  id: number;
  price: number;
  size: number;
  side: 'buy' | 'sell';
  ticker: string;
}

interface Position {
  qty: number;
  avgCost: number;
}

// ─── Constants ────────────────────────────────────────────────────────────────

const BASE: Record<string, number> = {
  NVDA: 875.50,
  CEG: 234.80,
  AMD: 168.25,
  MSFT: 417.30,
  META: 512.75,
  GOOGL: 176.40,
};
const TICKERS = Object.keys(BASE);

// ─── Helpers ──────────────────────────────────────────────────────────────────

const f2 = (n: number) => n.toFixed(2);
const fK = (n: number) => n >= 1000 ? `${(n / 1000).toFixed(1)}K` : String(n);

function makeBook(mid: number): { bids: Level[]; asks: Level[] } {
  const tick = mid > 200 ? 0.05 : 0.02;
  const bestAsk = Math.round((mid + tick) / tick) * tick;
  const bestBid = bestAsk - tick * 2;
  const bids: Level[] = [];
  const asks: Level[] = [];
  for (let i = 0; i < 12; i++) {
    const decay = 1 / (1 + i * 0.2);
    bids.push({
      price: parseFloat((bestBid - i * tick).toFixed(2)),
      size: Math.round((Math.random() * 2000 + 150) * decay),
      count: Math.floor(Math.random() * 7) + 1,
    });
    asks.push({
      price: parseFloat((bestAsk + i * tick).toFixed(2)),
      size: Math.round((Math.random() * 2000 + 150) * decay),
      count: Math.floor(Math.random() * 7) + 1,
    });
  }
  return { bids, asks };
}

function nextPrice(price: number, base: number): number {
  return price + (base - price) * 0.003 + (Math.random() - 0.5) * price * 0.0009;
}

// ─── Component ────────────────────────────────────────────────────────────────

export default function Terminal() {
  const [sym, setSym] = useState('NVDA');
  const [price, setPrice] = useState(BASE['NVDA']);
  const [bids, setBids] = useState<Level[]>([]);
  const [asks, setAsks] = useState<Level[]>([]);
  const [hist, setHist] = useState<number[]>(() => Array(80).fill(BASE['NVDA']));
  const [fills, setFills] = useState<Fill[]>([]);
  const [pos, setPos] = useState<Record<string, Position>>({});
  const [oType, setOType] = useState<'market' | 'limit'>('limit');
  const [oSide, setOSide] = useState<'buy' | 'sell'>('buy');
  const [oQty, setOQty] = useState('100');
  const [oLx, setOLx] = useState('');
  const [strategy, setStrategy] = useState('vwap');
  const [autoOn, setAutoOn] = useState(false);
  const [fillCount, setFillCount] = useState(0);
  const [fps, setFps] = useState(0);

  const priceRef = useRef(BASE['NVDA']);
  const symRef = useRef('NVDA');
  const fpsRef = useRef(0);

  useEffect(() => {
    symRef.current = sym;
    priceRef.current = BASE[sym];
    setPrice(BASE[sym]);
    setHist(Array(80).fill(BASE[sym]));
    const book = makeBook(BASE[sym]);
    setBids(book.bids);
    setAsks(book.asks);
  }, [sym]);

  useEffect(() => {
    const sim = setInterval(() => {
      const p = nextPrice(priceRef.current, BASE[symRef.current]);
      priceRef.current = p;
      setPrice(p);
      setHist(h => [...h.slice(1), p]);
      const book = makeBook(p);
      setBids(book.bids);
      setAsks(book.asks);
      if (Math.random() < 0.3) {
        const s: 'buy' | 'sell' = Math.random() < 0.5 ? 'buy' : 'sell';
        const fp = s === 'buy' ? book.asks[0].price : book.bids[0].price;
        setFills(f => [
          { id: Date.now() + Math.random(), price: fp, size: Math.round(Math.random() * 800 + 50), side: s, ticker: symRef.current },
          ...f.slice(0, 59),
        ]);
        fpsRef.current++;
        setFillCount(c => c + 1);
      }
    }, 150);
    const counter = setInterval(() => {
      setFps(fpsRef.current);
      fpsRef.current = 0;
    }, 1000);
    return () => { clearInterval(sim); clearInterval(counter); };
  }, []);

  const submitOrder = () => {
    const qty = Math.max(1, parseInt(oQty) || 100);
    const execPrice =
      oType === 'market'
        ? oSide === 'buy' ? (asks[0]?.price ?? price) : (bids[0]?.price ?? price)
        : parseFloat(oLx) || price;
    const slip = oType === 'market' ? Math.random() * 0.015 * price / 100 : 0;
    const ep = oSide === 'buy' ? execPrice + slip : execPrice - slip;
    setFills(f => [{ id: Date.now(), price: ep, size: qty, side: oSide, ticker: sym }, ...f.slice(0, 59)]);
    setPos(p => {
      const cur = p[sym] ?? { qty: 0, avgCost: 0 };
      if (oSide === 'buy') {
        const nq = cur.qty + qty;
        return { ...p, [sym]: { qty: nq, avgCost: nq === 0 ? ep : (cur.qty * cur.avgCost + qty * ep) / nq } };
      }
      return { ...p, [sym]: { qty: cur.qty - qty, avgCost: cur.avgCost } };
    });
    setFillCount(c => c + 1);
  };

  const chartPath = () => {
    if (hist.length < 2) return '';
    const W = 300, H = 56;
    const lo = Math.min(...hist), hi = Math.max(...hist), range = hi - lo || 1;
    const pts = hist.map((p, i) => `${((i / (hist.length - 1)) * W).toFixed(1)},${(H - ((p - lo) / range) * H).toFixed(1)}`);
    return pts.map((pt, i) => `${i === 0 ? 'M' : 'L'} ${pt}`).join(' ');
  };

  const areaPath = () => {
    if (hist.length < 2) return '';
    const W = 300, H = 56;
    const lo = Math.min(...hist), hi = Math.max(...hist), range = hi - lo || 1;
    const pts = hist.map((p, i) => `${((i / (hist.length - 1)) * W).toFixed(1)},${(H - ((p - lo) / range) * H).toFixed(1)}`);
    return `M ${pts.join(' L ')} L ${W},${H} L 0,${H} Z`;
  };

  const up = hist.length >= 2 && hist[hist.length - 1] >= hist[hist.length - 2];
  const lineColor = up ? '#00d97e' : '#ff3355';
  const spread = asks[0] && bids[0] ? (asks[0].price - bids[0].price).toFixed(2) : '--';
  const maxB = Math.max(...bids.map(b => b.size), 1);
  const maxA = Math.max(...asks.map(a => a.size), 1);
  const posArr = Object.entries(pos).filter(([, v]) => v.qty !== 0);
  const sessPnl = posArr.reduce((s, [t, p]) => s + p.qty * ((t === sym ? price : BASE[t]) - p.avgCost), 0);
  const priceDelta = price - (hist[0] || price);
  const priceDeltaPct = ((priceDelta / (hist[0] || price)) * 100).toFixed(3);

  return (
    <div className="h-screen flex flex-col bg-[#080c10] text-[#e6edf3] font-mono overflow-hidden text-xs select-none">

      {/* ── NAV ──────────────────────────────────────────────────────────── */}
      <header className="flex items-center gap-3 px-4 h-9 border-b border-[#1e2832] shrink-0">
        <span className="font-display text-[#00c8ff] text-sm font-bold tracking-[0.25em]">QUANTFORGE</span>
        <div className="w-px h-4 bg-[#1e2832]" />
        <div className="flex gap-0.5">
          {TICKERS.map(t => (
            <button
              key={t}
              onClick={() => setSym(t)}
              className={`px-2 py-0.5 rounded text-[11px] font-bold transition-all ${
                sym === t
                  ? 'bg-[#00c8ff] text-[#080c10]'
                  : 'text-[#8b949e] hover:text-[#e6edf3]'
              }`}
            >
              {t}
            </button>
          ))}
        </div>
        <div className="ml-auto flex items-center gap-4">
          <Link href="/learn" className="text-[11px] text-[#8b949e] hover:text-[#00c8ff] transition-colors uppercase tracking-widest">
            Learn
          </Link>
          <span className="flex items-center gap-1.5 text-[11px] text-[#00d97e]">
            <span className="w-1.5 h-1.5 rounded-full bg-[#00d97e] animate-pulse" />
            LIVE
          </span>
        </div>
      </header>

      {/* ── MAIN ─────────────────────────────────────────────────────────── */}
      <div className="flex flex-1 overflow-hidden">

        {/* ORDER BOOK */}
        <aside className="w-[232px] border-r border-[#1e2832] flex flex-col shrink-0">
          <div className="px-3 py-1.5 flex justify-between text-[10px] text-[#8b949e] uppercase tracking-widest border-b border-[#1e2832] shrink-0">
            <span>Depth</span>
            <span className="text-[#f5a623]">{sym}</span>
          </div>
          <div className="px-2 flex justify-between text-[9px] text-[#8b949e] uppercase tracking-wider py-1 border-b border-[#1e283244] shrink-0">
            <span className="w-16">Price</span>
            <span className="w-12 text-right">Size</span>
            <span className="w-6 text-right">N</span>
          </div>

          {/* Asks — best ask at bottom, worst at top */}
          <div className="flex-1 flex flex-col-reverse overflow-hidden">
            {asks.map((a, i) => (
              <div key={i} className="relative flex justify-between px-2 py-[3px] hover:bg-[#ff335508]">
                <div
                  className="absolute top-0 right-0 bottom-0 bg-[#ff335515]"
                  style={{ width: `${(a.size / maxA) * 100}%` }}
                />
                <span className="relative text-[#ff3355] w-16">{f2(a.price)}</span>
                <span className="relative text-[#c9d1d9] w-12 text-right">{fK(a.size)}</span>
                <span className="relative text-[#8b949e] w-6 text-right">{a.count}</span>
              </div>
            ))}
          </div>

          {/* Spread */}
          <div className="px-3 py-1.5 flex justify-between border-y border-[#1e2832] text-[10px] shrink-0">
            <span className="text-[#8b949e]">SPREAD</span>
            <span className="text-[#f5a623] font-bold">${spread}</span>
          </div>

          {/* Bids */}
          <div className="flex-1 overflow-hidden">
            {bids.map((b, i) => (
              <div key={i} className="relative flex justify-between px-2 py-[3px] hover:bg-[#00d97e08]">
                <div
                  className="absolute top-0 left-0 bottom-0 bg-[#00d97e14]"
                  style={{ width: `${(b.size / maxB) * 100}%` }}
                />
                <span className="relative text-[#00d97e] w-16">{f2(b.price)}</span>
                <span className="relative text-[#c9d1d9] w-12 text-right">{fK(b.size)}</span>
                <span className="relative text-[#8b949e] w-6 text-right">{b.count}</span>
              </div>
            ))}
          </div>
        </aside>

        {/* CENTER */}
        <main className="flex-1 flex flex-col overflow-hidden min-w-0">
          {/* Price + Chart */}
          <div className="p-4 border-b border-[#1e2832] flex items-start gap-6 shrink-0">
            <div className="shrink-0">
              <div className="text-[10px] text-[#8b949e] uppercase tracking-widest mb-0.5">{sym} / USD</div>
              <div className={`text-4xl font-bold tracking-tight font-display ${up ? 'text-[#00d97e]' : 'text-[#ff3355]'}`}>
                ${f2(price)}
              </div>
              <div className={`text-[11px] mt-0.5 ${up ? 'text-[#00d97e]' : 'text-[#ff3355]'}`}>
                {up ? '▲' : '▼'} {priceDelta >= 0 ? '+' : ''}{f2(priceDelta)} ({priceDelta >= 0 ? '+' : ''}{priceDeltaPct}%)
              </div>
            </div>
            <div className="flex-1 min-w-0 mt-1">
              <svg width="100%" height="60" viewBox="0 0 300 60" preserveAspectRatio="none">
                <defs>
                  <linearGradient id="chartFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor={lineColor} stopOpacity="0.2" />
                    <stop offset="100%" stopColor={lineColor} stopOpacity="0" />
                  </linearGradient>
                </defs>
                <path d={areaPath()} fill="url(#chartFill)" />
                <path d={chartPath()} fill="none" stroke={lineColor} strokeWidth="1.5" />
              </svg>
            </div>
          </div>

          {/* Order Ticket */}
          <div className="p-4 overflow-y-auto flex-1">
            <div className="text-[10px] text-[#8b949e] uppercase tracking-widest mb-3">Place Order</div>

            {/* Market / Limit toggle */}
            <div className="flex gap-0.5 mb-3 bg-[#0d1117] rounded p-0.5">
              {(['market', 'limit'] as const).map(t => (
                <button
                  key={t}
                  onClick={() => setOType(t)}
                  className={`flex-1 py-1.5 rounded text-[10px] font-bold uppercase tracking-widest transition-all ${
                    oType === t ? 'bg-[#1e2832] text-[#e6edf3]' : 'text-[#8b949e] hover:text-[#e6edf3]'
                  }`}
                >
                  {t}
                </button>
              ))}
            </div>

            {/* Buy / Sell */}
            <div className="grid grid-cols-2 gap-1 mb-3">
              <button
                onClick={() => setOSide('buy')}
                className={`py-2 rounded font-bold uppercase tracking-wider transition-all ${
                  oSide === 'buy' ? 'bg-[#00d97e] text-[#080c10]' : 'border border-[#00d97e33] text-[#00d97e] hover:border-[#00d97e77]'
                }`}
              >
                Buy
              </button>
              <button
                onClick={() => setOSide('sell')}
                className={`py-2 rounded font-bold uppercase tracking-wider transition-all ${
                  oSide === 'sell' ? 'bg-[#ff3355] text-white' : 'border border-[#ff335533] text-[#ff3355] hover:border-[#ff335577]'
                }`}
              >
                Sell
              </button>
            </div>

            {/* Qty */}
            <div className="mb-2">
              <label className="text-[10px] text-[#8b949e] uppercase tracking-wider">Qty (shares)</label>
              <input
                type="number"
                value={oQty}
                onChange={e => setOQty(e.target.value)}
                className="mt-1 w-full bg-[#0d1117] border border-[#1e2832] rounded px-3 py-2 text-[#e6edf3] focus:outline-none focus:border-[#00c8ff] transition-colors"
              />
            </div>

            {/* Limit price */}
            {oType === 'limit' && (
              <div className="mb-3">
                <label className="text-[10px] text-[#8b949e] uppercase tracking-wider">Limit Price</label>
                <input
                  type="number"
                  step="0.01"
                  value={oLx}
                  onChange={e => setOLx(e.target.value)}
                  placeholder={f2(price)}
                  className="mt-1 w-full bg-[#0d1117] border border-[#1e2832] rounded px-3 py-2 text-[#e6edf3] focus:outline-none focus:border-[#00c8ff] transition-colors placeholder:text-[#8b949e]"
                />
              </div>
            )}

            {/* Submit */}
            <button
              onClick={submitOrder}
              className={`w-full py-2.5 rounded font-bold uppercase tracking-wider transition-all text-[11px] ${
                oSide === 'buy'
                  ? 'bg-[#00d97e] text-[#080c10] hover:bg-[#00ff99]'
                  : 'bg-[#ff3355] text-white hover:bg-[#ff5577]'
              }`}
            >
              {oType === 'market' ? `Market ${oSide}` : `Limit ${oSide}`}
            </button>

            {/* Strategy Autopilot */}
            <div className="mt-5 pt-4 border-t border-[#1e2832]">
              <div className="text-[10px] text-[#8b949e] uppercase tracking-widest mb-2">Strategy Autopilot</div>
              <div className="grid grid-cols-3 gap-1 mb-2">
                {[['twap', 'TWAP'], ['vwap', 'VWAP'], ['ac', 'A-C']].map(([val, label]) => (
                  <button
                    key={val}
                    onClick={() => setStrategy(val)}
                    className={`py-1.5 rounded text-[10px] uppercase tracking-wider transition-all ${
                      strategy === val
                        ? 'bg-[#0d1117] text-[#00c8ff] border border-[#00c8ff44]'
                        : 'text-[#8b949e] hover:text-[#e6edf3] border border-transparent'
                    }`}
                  >
                    {label}
                  </button>
                ))}
              </div>
              <button
                onClick={() => setAutoOn(a => !a)}
                className={`w-full py-2 rounded font-bold uppercase tracking-widest text-[11px] transition-all ${
                  autoOn
                    ? 'bg-[#f5a623] text-[#080c10]'
                    : 'border border-[#f5a62344] text-[#f5a623] hover:bg-[#f5a62311]'
                }`}
              >
                {autoOn ? `◉ ${strategy.toUpperCase()} ACTIVE` : '◎ ENGAGE AUTOPILOT'}
              </button>
            </div>
          </div>
        </main>

        {/* RIGHT — Positions + Metrics */}
        <aside className="w-[216px] border-l border-[#1e2832] flex flex-col shrink-0">
          <div className="px-3 py-1.5 text-[10px] text-[#8b949e] uppercase tracking-widest border-b border-[#1e2832] shrink-0">
            Positions
          </div>
          <div className="flex-1 overflow-y-auto">
            {posArr.length === 0 ? (
              <p className="px-3 py-4 text-[11px] text-[#8b949e] italic">No open positions</p>
            ) : (
              posArr.map(([t, p]) => {
                const cp = t === sym ? price : BASE[t];
                const pnl = p.qty * (cp - p.avgCost);
                const pct = ((cp - p.avgCost) / p.avgCost) * 100;
                return (
                  <div key={t} className="px-3 py-2 border-b border-[#1e283255]">
                    <div className="flex justify-between mb-0.5">
                      <span className="text-[#00c8ff] font-bold">{t}</span>
                      <span className={pnl >= 0 ? 'text-[#00d97e]' : 'text-[#ff3355]'}>
                        {pnl >= 0 ? '+' : ''}{f2(pnl)}
                      </span>
                    </div>
                    <div className="flex justify-between text-[10px] text-[#8b949e]">
                      <span>{p.qty > 0 ? 'LONG' : 'SHORT'} {Math.abs(p.qty)}</span>
                      <span className={pnl >= 0 ? 'text-[#00d97e]' : 'text-[#ff3355]'}>
                        {pct >= 0 ? '+' : ''}{pct.toFixed(2)}%
                      </span>
                    </div>
                    <div className="text-[10px] text-[#8b949e]">avg ${f2(p.avgCost)}</div>
                  </div>
                );
              })
            )}
          </div>

          {/* Performance Metrics */}
          <div className="border-t border-[#1e2832] shrink-0">
            <div className="px-3 py-1.5 text-[10px] text-[#8b949e] uppercase tracking-widest border-b border-[#1e2832]">
              Performance
            </div>
            {[
              { label: 'Fills/sec', value: fps.toString(), color: undefined },
              { label: 'Session PnL', value: `${sessPnl >= 0 ? '+' : ''}$${f2(sessPnl)}`, color: sessPnl >= 0 ? '#00d97e' : '#ff3355' },
              { label: 'Total Fills', value: fillCount.toString(), color: undefined },
              { label: 'Best Bid', value: `$${f2(bids[0]?.price ?? 0)}`, color: '#00d97e' },
              { label: 'Best Ask', value: `$${f2(asks[0]?.price ?? 0)}`, color: '#ff3355' },
              { label: 'Spread', value: `$${spread}`, color: '#f5a623' },
            ].map(({ label, value, color }) => (
              <div key={label} className="px-3 py-1.5 flex justify-between border-b border-[#1e283244]">
                <span className="text-[#8b949e]">{label}</span>
                <span style={{ color: color ?? '#e6edf3' }}>{value}</span>
              </div>
            ))}
          </div>
        </aside>
      </div>

      {/* ── TAPE ─────────────────────────────────────────────────────────── */}
      <footer className="h-7 border-t border-[#1e2832] flex items-center shrink-0 overflow-hidden">
        <div className="px-3 text-[10px] text-[#8b949e] uppercase tracking-widest border-r border-[#1e2832] h-full flex items-center shrink-0">
          Tape
        </div>
        <div className="flex-1 overflow-hidden">
          <div className="tape-inner">
            {[...fills, ...fills].map((f, i) => (
              <span
                key={`${f.id}-${i}`}
                className={`text-[11px] shrink-0 ${f.side === 'buy' ? 'text-[#00d97e]' : 'text-[#ff3355]'}`}
              >
                {f.ticker} {f.side === 'buy' ? '▲' : '▼'} {f.size} @ ${f2(f.price)}
              </span>
            ))}
          </div>
        </div>
      </footer>
    </div>
  );
}
