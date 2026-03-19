import type { Metadata } from 'next';
import { JetBrains_Mono, Syne } from 'next/font/google';
import './globals.css';

const jetbrains = JetBrains_Mono({
  subsets: ['latin'],
  variable: '--font-mono',
});

const syne = Syne({
  subsets: ['latin'],
  variable: '--font-display',
});

export const metadata: Metadata = {
  title: 'QuantForge — C++ Orderbook Simulator',
  description: 'High-performance orderbook simulator for AI infrastructure stocks. TWAP, VWAP, Almgren-Chriss execution strategies.',
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body className={`${jetbrains.variable} ${syne.variable} font-mono antialiased`}>
        {children}
      </body>
    </html>
  );
}
