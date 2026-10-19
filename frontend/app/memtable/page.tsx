'use client';

import React, { useState, useEffect, useCallback } from 'react';
import Link from 'next/link';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { MemTableData, ScanResult } from '@/types';
import { formatBytes, formatLatency, formatNumber } from '@/lib/utils';
import {
  Layers,
  Zap,
  RefreshCw,
  HardDrive,
  Database,
  ArrowRight,
  Search,
  CheckCircle,
  HelpCircle,
  FileText,
  Boxes,
} from 'lucide-react';

export default function MemTablePage() {
  const [memData, setMemData] = useState<MemTableData | null>(null);
  const [flushStats, setFlushStats] = useState<{ total_flushes: number; total_duration_us: number; avg_duration_us: number } | null>(null);
  const [scanResult, setScanResult] = useState<ScanResult | null>(null);
  const [flushing, setFlushing] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const loadAll = useCallback(async () => {
    try {
      const [m, f, s] = await Promise.all([
        api.getMemTables().catch(() => null),
        api.getFlushes().catch(() => null),
        api.scan('', '', 50).catch(() => null),
      ]);
      setMemData(m);
      setFlushStats(f);
      setScanResult(s);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadAll();
    const interval = setInterval(loadAll, 3000);
    return () => clearInterval(interval);
  }, [loadAll]);

  const handleFlush = async () => {
    setFlushing(true);
    setMsg(null);
    try {
      const res = await api.flushMemTable();
      setMsg(`✨ MemTable flushed successfully to Level 0 SSTable in ${res.duration_ms} ms!`);
      loadAll();
    } catch (e: unknown) {
      setMsg(e instanceof Error ? e.message : 'Flush failed');
    } finally {
      setFlushing(false);
      setTimeout(() => setMsg(null), 5000);
    }
  };

  const usagePercent = memData?.active.usage_percent || 0;

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="MemTable & SkipList Buffer"
        description="In-memory sorted SkipList buffer, Arena memory consumption, and immutable flush queue"
        onRefresh={loadAll}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {msg && (
          <div className="p-3.5 rounded-xl border border-emerald-800 bg-emerald-950/40 text-emerald-300 text-xs font-mono flex items-center justify-between">
            <span>{msg}</span>
          </div>
        )}

        {/* Humanized Top Explainer */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-lg bg-cyan-500/10 border border-cyan-500/30 flex items-center justify-center flex-shrink-0">
              <Layers className="w-5 h-5 text-cyan-400" />
            </div>
            <div>
              <div className="text-sm font-semibold text-white">The Front Line of LSM Storage</div>
              <p className="text-xs text-zinc-400 mt-0.5">
                Every write is ordered inside this lock-free memory SkipList. When capacity reaches 4MB, it freezes and serializes into Level 0.
              </p>
            </div>
          </div>
          <div className="flex items-center gap-3 font-mono text-xs">
            <Link href="/wal" className="text-amber-400 hover:underline flex items-center gap-1">
              <FileText className="w-3.5 h-3.5" />
              <span>Inspect Companion WAL Log</span>
            </Link>
          </div>
        </div>

        {/* MemTable Top Status Cards */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          {/* Active SkipList Card */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-3">
                <span className="text-xs font-semibold uppercase tracking-wider text-cyan-400">
                  Active SkipList MemTable
                </span>
                <Layers className="w-4 h-4 text-cyan-400" />
              </div>
              <div className="flex items-baseline gap-2">
                <span className="text-3xl font-bold font-mono text-white">
                  {formatBytes(memData?.active.memory_bytes || 0)}
                </span>
                <span className="text-xs text-zinc-500 font-mono">
                  / {formatBytes(memData?.active.threshold_bytes || 4194304)}
                </span>
              </div>
              {/* Capacity Progress Bar */}
              <div className="w-full bg-[#1e2536] h-2 rounded-full mt-4 overflow-hidden">
                <div
                  className="bg-gradient-to-r from-cyan-500 to-emerald-400 h-full rounded-full transition-all duration-300"
                  style={{ width: `${Math.min(100, Math.max(3, usagePercent))}%` }}
                />
              </div>
            </div>

            <div className="mt-5 space-y-1.5 text-xs font-mono text-zinc-400">
              <div className="flex justify-between">
                <span>Threshold:</span>
                <span className="text-white">{formatBytes(memData?.active.threshold_bytes || 4194304)}</span>
              </div>
              <div className="flex justify-between">
                <span>Buffer Fill:</span>
                <span className="text-cyan-400 font-semibold">{usagePercent.toFixed(1)}%</span>
              </div>
              <div className="flex justify-between">
                <span>Structure:</span>
                <span className="text-zinc-300">Lock-free SkipList</span>
              </div>
            </div>
          </div>

          {/* Immutable Queue Card */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-3">
                <span className="text-xs font-semibold uppercase tracking-wider text-amber-400">
                  Immutable Flush Queue
                </span>
                <HardDrive className="w-4 h-4 text-amber-400" />
              </div>
              <div className="text-3xl font-bold font-mono text-white">
                {memData?.immutable.count || 0}
              </div>
              <p className="text-xs text-zinc-400 mt-2">
                {memData?.immutable.flushing
                  ? 'A background thread is currently serializing frozen MemTable into an L0 SSTable.'
                  : 'No active MemTable flushes pending. Ready for immediate writes.'}
              </p>
            </div>

            <div className="mt-5 pt-4 border-t border-[#232a3e]">
              <button
                onClick={handleFlush}
                disabled={flushing}
                className="w-full py-2.5 rounded-lg bg-cyan-600 hover:bg-cyan-500 text-white font-medium text-xs flex items-center justify-center gap-2 transition-all shadow-md shadow-cyan-900/40 disabled:opacity-50"
              >
                <Zap className={`w-4 h-4 ${flushing ? 'animate-spin' : ''}`} />
                <span>{flushing ? 'Flushing MemTable to Disk...' : 'Flush Active MemTable Now'}</span>
              </button>
            </div>
          </div>

          {/* Flush History & Stats Card */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-3">
                <span className="text-xs font-semibold uppercase tracking-wider text-purple-400">
                  Flush Durability History
                </span>
                <RefreshCw className="w-4 h-4 text-purple-400" />
              </div>
              <div className="text-3xl font-bold font-mono text-white">
                {flushStats?.total_flushes || 0}
              </div>
              <span className="text-xs text-zinc-500 font-mono">Total Completed Flushes</span>
            </div>

            <div className="mt-5 space-y-1.5 text-xs font-mono text-zinc-400">
              <div className="flex justify-between">
                <span>Total Flush Time:</span>
                <span className="text-white">{formatLatency(flushStats?.total_duration_us || 0)}</span>
              </div>
              <div className="flex justify-between">
                <span>Average Flush Duration:</span>
                <span className="text-purple-400 font-semibold">
                  {formatLatency(flushStats?.avg_duration_us || 0)}
                </span>
              </div>
              <div className="flex justify-between">
                <span>Disk Format:</span>
                <span className="text-emerald-400">L0 SSTable</span>
              </div>
            </div>
          </div>
        </div>

        {/* Live Sorted MemTable Entries Scan */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
          <div className="flex items-center justify-between mb-4">
            <div>
              <h3 className="text-sm font-semibold text-white">Live In-Memory Key Preview</h3>
              <p className="text-xs text-zinc-400 mt-0.5">
                First 50 lexicographically sorted keys currently accessible in the LSM-Tree
              </p>
            </div>
            <Link
              href="/query"
              className="text-xs font-mono text-cyan-400 hover:underline flex items-center gap-1"
            >
              <span>Add Records in Query Console</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </Link>
          </div>

          <div className="rounded-lg border border-[#232a3e] bg-[#0e121b] overflow-hidden">
            <table className="w-full text-left text-xs font-mono">
              <thead className="bg-[#0b0e15] border-b border-[#232a3e] text-zinc-400">
                <tr>
                  <th className="py-2.5 px-4 w-12 text-zinc-500">#</th>
                  <th className="py-2.5 px-4">Key (UserKey)</th>
                  <th className="py-2.5 px-4">Value Content</th>
                  <th className="py-2.5 px-4 w-24 text-right">Size</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-[#1e2536]">
                {scanResult?.items && scanResult.items.length > 0 ? (
                  scanResult.items.map((item, idx) => (
                    <tr key={idx} className="hover:bg-[#161c28]">
                      <td className="py-2 px-4 text-zinc-500">{idx + 1}</td>
                      <td className="py-2 px-4 text-cyan-300 font-semibold">{item.key}</td>
                      <td className="py-2 px-4 text-zinc-300 max-w-md truncate">
                        {item.value}
                      </td>
                      <td className="py-2 px-4 text-right text-zinc-500">{item.value.length} B</td>
                    </tr>
                  ))
                ) : (
                  <tr>
                    <td colSpan={4} className="py-8 text-center text-zinc-500">
                      No records in database. Use Query Console or run a benchmark to insert keys.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>
  );
}
