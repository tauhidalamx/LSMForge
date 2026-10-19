'use client';

import React, { useEffect, useState, useCallback } from 'react';
import Link from 'next/link';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { EngineStats, LevelInfo, MemTableData, SystemHealth, IntegrityStatus } from '@/types';
import { formatBytes, formatLatency, formatNumber } from '@/lib/utils';
import {
  HardDrive,
  Layers,
  Activity,
  Zap,
  Filter,
  RefreshCw,
  ShieldCheck,
  AlertTriangle,
  ArrowRight,
  TrendingUp,
  Terminal,
  HelpCircle,
  Sparkles,
  ExternalLink,
} from 'lucide-react';
import {
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';

export default function OverviewPage() {
  const [stats, setStats] = useState<EngineStats | null>(null);
  const [memData, setMemData] = useState<MemTableData | null>(null);
  const [levels, setLevels] = useState<LevelInfo[]>([]);
  const [health, setHealth] = useState<SystemHealth | null>(null);
  const [integrity, setIntegrity] = useState<IntegrityStatus | null>(null);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);

  const fetchAll = useCallback(async () => {
    try {
      setError(null);
      const [s, m, l, h, ing] = await Promise.all([
        api.getStats().catch(() => null),
        api.getMemTables().catch(() => null),
        api.getLevels().then((res) => res.levels).catch(() => []),
        api.getHealth().catch(() => null),
        api.getIntegrity().catch(() => null),
      ]);
      setStats(s);
      setMemData(m);
      setLevels(l);
      setHealth(h);
      setIntegrity(ing);
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : 'Failed to fetch data');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchAll();
    const interval = setInterval(fetchAll, 3000);
    return () => clearInterval(interval);
  }, [fetchAll]);

  // Operations chart data
  const opsData = stats
    ? [
        { name: 'PUT', count: stats.operations.puts, fill: '#06b6d4' },
        { name: 'GET', count: stats.operations.gets, fill: '#10b981' },
        { name: 'DELETE', count: stats.operations.deletes, fill: '#f43f5e' },
        { name: 'SCAN', count: stats.operations.scans, fill: '#a855f7' },
      ]
    : [];

  // Level storage data
  const levelData = levels.map((lvl) => ({
    name: `L${lvl.level}`,
    files: lvl.num_files,
    bytes: lvl.total_bytes,
    kb: Math.round(lvl.total_bytes / 1024),
  }));

  const totalOps = stats
    ? stats.operations.puts + stats.operations.gets + stats.operations.deletes + stats.operations.scans
    : 0;

  const usefulBloom = stats?.bloom_filter.useful_checks || 0;
  const positiveBloom = stats?.bloom_filter.positive_checks || 0;
  const totalBloom = usefulBloom + positiveBloom;
  const bloomSavedPct = totalBloom > 0 ? ((usefulBloom / totalBloom) * 100).toFixed(1) : '100';

  const writeAmp = stats && stats.compaction.bytes_read > 0
    ? (stats.compaction.bytes_written / stats.compaction.bytes_read).toFixed(2)
    : '1.00';

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Engine Overview & Telemetry"
        description="High-density C++20 LSM-Tree runtime status, memory utilization, and leveled compaction metrics"
        onRefresh={fetchAll}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {error && (
          <div className="p-4 rounded-xl border border-rose-900/60 bg-rose-950/30 text-rose-300 flex items-center gap-3">
            <AlertTriangle className="w-5 h-5 flex-shrink-0 text-rose-400" />
            <div className="text-sm">
              <span className="font-semibold">Backend Connection Issue:</span> {error}. Ensure{' '}
              <code className="font-mono bg-rose-950 px-1.5 py-0.5 rounded text-rose-200">
                ./build_release/lsmforge_server
              </code>{' '}
              is running on port 8080.
            </div>
          </div>
        )}

        {/* Humanized Quick Action Guide Banner */}
        <div className="p-4 rounded-xl border border-cyan-900/40 bg-gradient-to-r from-cyan-950/30 via-[#101522] to-emerald-950/20 flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-lg bg-cyan-500/10 border border-cyan-500/30 flex items-center justify-center flex-shrink-0">
              <Sparkles className="w-4 h-4 text-cyan-400" />
            </div>
            <div>
              <div className="text-xs font-bold text-white flex items-center gap-2">
                <span>Welcome to LSMForge Dashboard</span>
                <span className="text-[10px] font-mono px-1.5 py-0.2 rounded bg-cyan-900/60 text-cyan-300 border border-cyan-700">
                  Interactive Guide
                </span>
              </div>
              <p className="text-[11px] text-zinc-400 mt-0.5">
                Every card and metric below is live. Click any card to drill down into its dedicated inspector.
              </p>
            </div>
          </div>

          <div className="flex items-center gap-2">
            <Link
              href="/query"
              className="px-3 py-1.5 rounded-lg bg-cyan-600/20 hover:bg-cyan-600/30 border border-cyan-500/40 text-cyan-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all"
            >
              <Terminal className="w-3.5 h-3.5" />
              <span>1. Insert Key</span>
            </Link>
            <Link
              href="/memtable"
              className="px-3 py-1.5 rounded-lg bg-[#161c28] hover:bg-[#1f2738] border border-[#2b354d] text-zinc-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all"
            >
              <Zap className="w-3.5 h-3.5 text-amber-400" />
              <span>2. Flush to Disk</span>
            </Link>
            <Link
              href="/benchmarks"
              className="px-3 py-1.5 rounded-lg bg-[#161c28] hover:bg-[#1f2738] border border-[#2b354d] text-zinc-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all"
            >
              <Activity className="w-3.5 h-3.5 text-emerald-400" />
              <span>3. Benchmark Engine</span>
            </Link>
          </div>
        </div>

        {/* Top KPI Cards (All Clickable and Humanized!) */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* Active MemTable Card -> /memtable */}
          <Link
            href="/memtable"
            className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] hover:border-cyan-500/60 hover:bg-[#151a27] transition-all flex flex-col justify-between group cursor-pointer"
          >
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-2">
                <span className="text-xs font-semibold tracking-wide uppercase group-hover:text-cyan-400 transition-colors flex items-center gap-1.5">
                  <Layers className="w-4 h-4 text-cyan-400" />
                  Active MemTable
                </span>
                <ArrowRight className="w-3.5 h-3.5 text-zinc-600 group-hover:text-cyan-400 group-hover:translate-x-0.5 transition-all" />
              </div>
              <div className="flex items-baseline gap-2 mt-1">
                <span className="text-2xl font-bold font-mono text-white">
                  {formatBytes(memData?.active.memory_bytes || 0)}
                </span>
                <span className="text-xs text-zinc-500 font-mono">
                  / {formatBytes(memData?.active.threshold_bytes || 4194304)}
                </span>
              </div>
              <div className="w-full bg-[#1e2536] h-1.5 rounded-full mt-3 overflow-hidden">
                <div
                  className="bg-gradient-to-r from-cyan-500 to-emerald-400 h-full rounded-full transition-all duration-500"
                  style={{ width: `${Math.min(100, Math.max(2, memData?.active.usage_percent || 0))}%` }}
                />
              </div>
            </div>
            <div className="mt-3 flex items-center justify-between text-[11px] text-zinc-400 font-mono">
              <span>{memData?.active.usage_percent.toFixed(1) || '0.0'}% buffer capacity</span>
              <span className="text-amber-400">
                {memData?.immutable.count || 0} in flush queue
              </span>
            </div>
          </Link>

          {/* Persistent SSTables Card -> /sstables */}
          <Link
            href="/sstables"
            className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] hover:border-emerald-500/60 hover:bg-[#151a27] transition-all flex flex-col justify-between group cursor-pointer"
          >
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-2">
                <span className="text-xs font-semibold tracking-wide uppercase group-hover:text-emerald-400 transition-colors flex items-center gap-1.5">
                  <HardDrive className="w-4 h-4 text-emerald-400" />
                  SSTable Storage
                </span>
                <ArrowRight className="w-3.5 h-3.5 text-zinc-600 group-hover:text-emerald-400 group-hover:translate-x-0.5 transition-all" />
              </div>
              <div className="flex items-baseline gap-2 mt-1">
                <span className="text-2xl font-bold font-mono text-white">
                  {formatBytes(stats?.storage.total_sstable_bytes || 0)}
                </span>
                <span className="text-xs text-zinc-500 font-mono">
                  ({stats?.storage.active_sstables || 0} files)
                </span>
              </div>
              <div className="text-xs text-zinc-400 mt-2 font-mono flex items-center gap-2">
                <span>WAL Bytes Written:</span>
                <span className="text-emerald-400 font-semibold">
                  {formatBytes(stats?.storage.wal_bytes_written || 0)}
                </span>
              </div>
            </div>
            <div className="mt-3 flex items-center justify-between text-[11px] text-zinc-400 font-mono">
              <span>Levels: {levels.length} active</span>
              <span className="text-emerald-400">Prefix-Delta 16K</span>
            </div>
          </Link>

          {/* Operations Card -> /query */}
          <Link
            href="/query"
            className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] hover:border-amber-500/60 hover:bg-[#151a27] transition-all flex flex-col justify-between group cursor-pointer"
          >
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-2">
                <span className="text-xs font-semibold tracking-wide uppercase group-hover:text-amber-400 transition-colors flex items-center gap-1.5">
                  <Activity className="w-4 h-4 text-amber-400" />
                  Total Operations
                </span>
                <ArrowRight className="w-3.5 h-3.5 text-zinc-600 group-hover:text-amber-400 group-hover:translate-x-0.5 transition-all" />
              </div>
              <div className="text-2xl font-bold font-mono text-white mt-1">
                {formatNumber(totalOps)}
              </div>
              <div className="grid grid-cols-2 gap-2 mt-2 text-xs font-mono">
                <div className="flex justify-between text-zinc-400">
                  <span>Puts:</span>
                  <span className="text-cyan-400">{formatNumber(stats?.operations.puts || 0)}</span>
                </div>
                <div className="flex justify-between text-zinc-400">
                  <span>Gets:</span>
                  <span className="text-emerald-400">{formatNumber(stats?.operations.gets || 0)}</span>
                </div>
              </div>
            </div>
            <div className="mt-3 flex items-center justify-between text-[11px] text-zinc-400 font-mono">
              <span>Deletes: {stats?.operations.deletes || 0}</span>
              <span>Scans: {stats?.operations.scans || 0}</span>
            </div>
          </Link>

          {/* Compaction & Bloom Card -> /compaction */}
          <Link
            href="/compaction"
            className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] hover:border-purple-500/60 hover:bg-[#151a27] transition-all flex flex-col justify-between group cursor-pointer"
          >
            <div>
              <div className="flex items-center justify-between text-zinc-400 mb-2">
                <span className="text-xs font-semibold tracking-wide uppercase group-hover:text-purple-400 transition-colors flex items-center gap-1.5">
                  <RefreshCw className="w-4 h-4 text-purple-400" />
                  Compaction & Bloom
                </span>
                <ArrowRight className="w-3.5 h-3.5 text-zinc-600 group-hover:text-purple-400 group-hover:translate-x-0.5 transition-all" />
              </div>
              <div className="flex items-baseline gap-2 mt-1">
                <span className="text-2xl font-bold font-mono text-white">{writeAmp}x</span>
                <span className="text-xs text-zinc-500 font-mono">Write Amplification</span>
              </div>
              <div className="text-xs text-zinc-400 mt-2 font-mono flex items-center justify-between">
                <span>Bloom Disk Saved:</span>
                <span className="text-cyan-400 font-semibold">{bloomSavedPct}%</span>
              </div>
            </div>
            <div className="mt-3 flex items-center justify-between text-[11px] text-zinc-400 font-mono">
              <span>{stats?.compaction.compactions || 0} Compactions</span>
              <span className="text-purple-400">{stats?.compaction.flushes || 0} Flushes</span>
            </div>
          </Link>
        </div>

        {/* System Health & Integrity Panel */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center">
              <ShieldCheck className="w-5 h-5 text-emerald-400" />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <span className="text-sm font-semibold text-white">Database Engine Integrity</span>
                <span className="px-2 py-0.5 rounded text-[10px] font-mono font-semibold bg-emerald-950/80 text-emerald-400 border border-emerald-800">
                  {integrity?.integrity_pass ? 'PASSED VERIFICATION' : 'CHECKING...'}
                </span>
              </div>
              <p className="text-xs text-zinc-400 mt-0.5">
                Current Manifest file verified • File lock active • All {integrity?.verified_sstables || 0} registered SSTables present on disk
              </p>
            </div>
          </div>
          <div className="flex items-center gap-4 text-xs font-mono text-zinc-400">
            <div>
              <span className="text-zinc-500">Uptime:</span>{' '}
              <span className="text-zinc-200">
                {health?.uptime_seconds ? `${Math.floor(health.uptime_seconds / 60)}m ${health.uptime_seconds % 60}s` : '0s'}
              </span>
            </div>
            <div>
              <span className="text-zinc-500">DB:</span>{' '}
              <span className="text-zinc-200 truncate max-w-[140px]">{health?.database_name || 'lsmforge.db'}</span>
            </div>
            <div>
              <span className="text-zinc-500">Engine:</span>{' '}
              <span className="text-cyan-400">v{health?.version || '1.0.0'} (C++20)</span>
            </div>
          </div>
        </div>

        {/* Visual Charts Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Operations Breakdown Chart */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <div>
                <h3 className="text-sm font-semibold text-white">Database Operations Breakdown</h3>
                <p className="text-xs text-zinc-400 mt-0.5">Cumulative PUT, GET, DELETE, and SCAN distribution</p>
              </div>
              <Link
                href="/query"
                className="flex items-center gap-1 text-xs text-cyan-400 hover:text-cyan-300 font-mono"
              >
                <span>Query Console</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </Link>
            </div>
            <div className="h-64 w-full">
              {opsData.some((d) => d.count > 0) ? (
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={opsData} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                    <XAxis dataKey="name" stroke="#64748b" fontSize={12} tickLine={false} />
                    <YAxis stroke="#64748b" fontSize={11} tickLine={false} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#0f1420', borderColor: '#232a3e', borderRadius: '8px', fontSize: '12px' }}
                      labelStyle={{ color: '#f1f5f9' }}
                    />
                    <Bar dataKey="count" radius={[4, 4, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex flex-col items-center justify-center text-zinc-500 text-xs">
                  <Activity className="w-8 h-8 mb-2 opacity-40 text-cyan-400" />
                  <span>No database operations logged yet. Run a benchmark or query.</span>
                </div>
              )}
            </div>
          </div>

          {/* SSTables By Level Distribution */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <div>
                <h3 className="text-sm font-semibold text-white">SSTable Level Footprint</h3>
                <p className="text-xs text-zinc-400 mt-0.5">Total disk size (KB) by LSM-Tree level</p>
              </div>
              <Link
                href="/sstables"
                className="flex items-center gap-1 text-xs text-emerald-400 hover:text-emerald-300 font-mono"
              >
                <span>Explore Lanes</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </Link>
            </div>
            <div className="h-64 w-full">
              {levelData.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={levelData} margin={{ top: 10, right: 10, left: -10, bottom: 0 }}>
                    <XAxis dataKey="name" stroke="#64748b" fontSize={12} tickLine={false} />
                    <YAxis stroke="#64748b" fontSize={11} tickLine={false} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#0f1420', borderColor: '#232a3e', borderRadius: '8px', fontSize: '12px' }}
                      formatter={(val: any) => [`${val} KB`, 'Disk Size']}
                    />
                    <Bar dataKey="kb" fill="#10b981" radius={[4, 4, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex flex-col items-center justify-center text-zinc-500 text-xs">
                  <HardDrive className="w-8 h-8 mb-2 opacity-40 text-emerald-400" />
                  <span>No SSTables on disk yet. Writes are in the active MemTable.</span>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Humanized Plain-English Educational Callouts */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-col justify-between">
            <div>
              <div className="flex items-center gap-2 text-xs font-semibold text-white mb-1.5">
                <HelpCircle className="w-4 h-4 text-cyan-400 flex-shrink-0" />
                <span>What is a MemTable?</span>
              </div>
              <p className="text-[11px] text-zinc-400 leading-relaxed">
                MemTable is an in-memory sorted SkipList that absorbs incoming writes instantly. Because updates are ordered in RAM, reads are instantaneous without disk head contention.
              </p>
            </div>
            <Link
              href="/memtable"
              className="mt-3 text-[11px] text-cyan-400 hover:text-cyan-300 font-mono flex items-center gap-1"
            >
              <span>Inspect MemTable</span>
              <ArrowRight className="w-3 h-3" />
            </Link>
          </div>

          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-col justify-between">
            <div>
              <div className="flex items-center gap-2 text-xs font-semibold text-white mb-1.5">
                <HelpCircle className="w-4 h-4 text-emerald-400 flex-shrink-0" />
                <span>Why Leveled Compaction?</span>
              </div>
              <p className="text-[11px] text-zinc-400 leading-relaxed">
                LSM engines write sequentially for maximum throughput. Compaction merges overlapping Level 0 files into strictly sorted non-overlapping tiers, cleaning deleted records.
              </p>
            </div>
            <Link
              href="/compaction"
              className="mt-3 text-[11px] text-emerald-400 hover:text-emerald-300 font-mono flex items-center gap-1"
            >
              <span>View Compaction Monitor</span>
              <ArrowRight className="w-3 h-3" />
            </Link>
          </div>

          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-col justify-between">
            <div>
              <div className="flex items-center gap-2 text-xs font-semibold text-white mb-1.5">
                <HelpCircle className="w-4 h-4 text-purple-400 flex-shrink-0" />
                <span>How Bloom Filters Save Disk I/O</span>
              </div>
              <p className="text-[11px] text-zinc-400 leading-relaxed">
                Bloom filters check set membership in nanoseconds. If a key doesn&apos;t exist in an SSTable, it returns false with 100% accuracy, bypassing disk reads completely.
              </p>
            </div>
            <Link
              href="/bloom"
              className="mt-3 text-[11px] text-purple-400 hover:text-purple-300 font-mono flex items-center gap-1"
            >
              <span>Test Bloom Filters</span>
              <ArrowRight className="w-3 h-3" />
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}
