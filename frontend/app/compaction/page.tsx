'use client';

import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { CompactionStats, LevelInfo, SSTableItem } from '@/types';
import { formatBytes, formatLatency, formatNumber } from '@/lib/utils';
import {
  RefreshCw,
  Play,
  Layers,
  HardDrive,
  TrendingUp,
  Activity,
  CheckCircle2,
  AlertCircle,
  HelpCircle,
  ArrowRight,
  Boxes,
  Zap,
} from 'lucide-react';
import {
  AreaChart,
  Area,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';

export default function CompactionPage() {
  const [stats, setStats] = useState<CompactionStats | null>(null);
  const [levels, setLevels] = useState<LevelInfo[]>([]);
  const [sstables, setSstables] = useState<SSTableItem[]>([]);
  const [compacting, setCompacting] = useState(false);
  const [msg, setMsg] = useState<{ text: string; error?: boolean } | null>(null);
  const [loading, setLoading] = useState(true);

  const loadData = async () => {
    try {
      const [cStats, lRes, sRes] = await Promise.all([
        api.getCompactionStats().catch(() => null),
        api.getLevels().catch(() => ({ levels: [], total_storage_bytes: 0 })),
        api.getSSTables().catch(() => ({ sstables: [], count: 0 })),
      ]);
      setStats(cStats);
      setLevels(lRes.levels);
      setSstables(sRes.sstables);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 4000);
    return () => clearInterval(interval);
  }, []);

  const handleTriggerCompaction = async () => {
    setCompacting(true);
    setMsg(null);
    try {
      const res = await api.triggerCompaction();
      setMsg({ text: `🚀 Leveled compaction completed in ${res.duration_ms} ms! Obsolete keys & tombstones cleaned.` });
      loadData();
    } catch (e: unknown) {
      setMsg({ text: e instanceof Error ? e.message : 'Compaction failed', error: true });
    } finally {
      setCompacting(false);
      setTimeout(() => setMsg(null), 5000);
    }
  };

  // Stacked size amplification data by level (Image 3 inspired)
  const sizeAmpData = levels.map((lvl) => ({
    name: `Level ${lvl.level}`,
    sizeKB: Math.round(lvl.total_bytes / 1024),
    files: lvl.num_files,
  }));

  // Read amplification data (Number of SST files)
  const readAmpData = levels.map((lvl) => ({
    name: `L${lvl.level}`,
    sstCount: lvl.num_files,
  }));

  const totalFiles = sstables.length;
  const totalStorage = levels.reduce((a, b) => a + b.total_bytes, 0);

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Compaction & Garbage Collection Monitor"
        description="Leveled SSTable merge-sort, tombstone GC, user-key expansion, and write/size amplification metrics"
        onRefresh={loadData}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {msg && (
          <div
            className={`p-3.5 rounded-xl border text-xs font-mono flex items-center justify-between ${
              msg.error
                ? 'bg-rose-950/60 text-rose-300 border-rose-800'
                : 'bg-emerald-950/60 text-emerald-300 border-emerald-800'
            }`}
          >
            <span>{msg.text}</span>
          </div>
        )}

        {/* Top KPI Cards Styled Directly After YugabyteDB Compaction Dashboard (Image 3) */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* Storage Size Card */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] relative overflow-hidden">
            <span className="text-[11px] font-mono text-zinc-400 block uppercase">Total SST Size</span>
            <div className="text-3xl font-extrabold font-mono text-emerald-400 mt-2">
              {formatBytes(totalStorage)}
            </div>
            <div className="mt-2 text-xs font-mono text-zinc-500">
              Across {levels.length} active LSM tiers
            </div>
          </div>

          {/* SST File Number Card (Red/Green threshold style like Image 3) */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] relative overflow-hidden">
            <span className="text-[11px] font-mono text-zinc-400 block uppercase">SST File Count</span>
            <div
              className={`text-3xl font-extrabold font-mono mt-2 ${
                totalFiles > 12 ? 'text-rose-500' : 'text-emerald-400'
              }`}
            >
              {totalFiles}
            </div>
            <div className="mt-2 text-xs font-mono text-zinc-500">
              {levels.find((l) => l.level === 0)?.num_files || 0} files in Level 0 (trigger: 4)
            </div>
          </div>

          {/* Compaction Latency Card */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] relative overflow-hidden">
            <span className="text-[11px] font-mono text-zinc-400 block uppercase">Avg Compaction Time</span>
            <div className="text-3xl font-extrabold font-mono text-emerald-400 mt-2">
              {formatLatency(
                stats?.compactions_run ? stats.total_duration_us / stats.compactions_run : 0
              )}
            </div>
            <div className="mt-2 text-xs font-mono text-zinc-500">
              {stats?.compactions_run || 0} passes executed
            </div>
          </div>

          {/* Write Amplification Card */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] relative overflow-hidden">
            <span className="text-[11px] font-mono text-zinc-400 block uppercase">Write Amplification</span>
            <div className="text-3xl font-extrabold font-mono text-amber-400 mt-2">
              {stats?.write_amplification ? stats.write_amplification.toFixed(2) : '1.00'}x
            </div>
            <div className="mt-2 text-xs font-mono text-zinc-500">
              Read: {formatBytes(stats?.bytes_read || 0)} / Written: {formatBytes(stats?.bytes_written || 0)}
            </div>
          </div>
        </div>

        {/* Manual Trigger Compaction Banner */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-wrap items-center justify-between gap-4">
          <div>
            <div className="text-sm font-semibold text-white">Manual Leveled Compaction Trigger</div>
            <p className="text-xs text-zinc-400 mt-0.5">
              Force an immediate multi-way merge-sort of Level 0 files into Level 1, purging expired versions and tombstones
            </p>
          </div>
          <button
            onClick={handleTriggerCompaction}
            disabled={compacting}
            className="px-5 py-2.5 rounded-lg bg-cyan-600 hover:bg-cyan-500 text-white font-medium text-xs font-mono flex items-center gap-2 transition-all shadow-md shadow-cyan-900/40 disabled:opacity-50"
          >
            <Play className={`w-4 h-4 ${compacting ? 'animate-spin' : ''}`} />
            <span>{compacting ? 'Compacting Storage...' : 'Trigger Leveled Compaction'}</span>
          </button>
        </div>

        {/* Visual Charts Styled Directly After Image 3 */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Size Amplification Chart (Yellow/Green stacked style from Image 3) */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <div>
                <h3 className="text-sm font-semibold text-white">Size Amplification by Level</h3>
                <p className="text-xs text-zinc-400 mt-0.5">Stacked disk capacity across LSM-Tree tiers (KB)</p>
              </div>
              <span className="text-xs font-mono text-emerald-400">Leveled Storage</span>
            </div>
            <div className="h-64 w-full">
              {sizeAmpData.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <AreaChart data={sizeAmpData} margin={{ top: 10, right: 10, left: -10, bottom: 0 }}>
                    <defs>
                      <linearGradient id="sizeGradient" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="5%" stopColor="#10b981" stopOpacity={0.8} />
                        <stop offset="95%" stopColor="#059669" stopOpacity={0.1} />
                      </linearGradient>
                    </defs>
                    <XAxis dataKey="name" stroke="#64748b" fontSize={12} tickLine={false} />
                    <YAxis stroke="#64748b" fontSize={11} tickLine={false} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#0f1420', borderColor: '#232a3e', borderRadius: '8px', fontSize: '12px' }}
                      formatter={(val: any) => [`${val} KB`, 'Disk Footprint']}
                    />
                    <Area type="monotone" dataKey="sizeKB" stroke="#10b981" fillOpacity={1} fill="url(#sizeGradient)" />
                  </AreaChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex items-center justify-center text-zinc-500 text-xs">
                  No level data available yet.
                </div>
              )}
            </div>
          </div>

          {/* Read Amplification Chart (Number of SST files per level from Image 3) */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <div>
                <h3 className="text-sm font-semibold text-white">Read Amplification (SST File Count)</h3>
                <p className="text-xs text-zinc-400 mt-0.5">Candidate files inspected per point lookup</p>
              </div>
              <span className="text-xs font-mono text-amber-400">Target: 1 file/level (L1+)</span>
            </div>
            <div className="h-64 w-full">
              {readAmpData.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={readAmpData} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                    <XAxis dataKey="name" stroke="#64748b" fontSize={12} tickLine={false} />
                    <YAxis stroke="#64748b" fontSize={11} tickLine={false} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#0f1420', borderColor: '#232a3e', borderRadius: '8px', fontSize: '12px' }}
                      formatter={(val: any) => [`${val} files`, 'Candidate SSTables']}
                    />
                    <Bar dataKey="sstCount" fill="#f59e0b" radius={[4, 4, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex items-center justify-center text-zinc-500 text-xs">
                  No SSTables found.
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Humanized Plain English Explainer & Cross-Links */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] space-y-2">
            <div className="flex items-center gap-2 text-xs font-semibold text-white">
              <HelpCircle className="w-4 h-4 text-cyan-400" />
              <span>What is Write Amplification?</span>
            </div>
            <p className="text-[11px] text-zinc-400 leading-relaxed">
              When you write 1 MB of data, the database writes it to the WAL, flushes it to L0, and then compactor threads merge it into L1 and beyond.
              A write amplification of <strong>{stats?.write_amplification ? stats.write_amplification.toFixed(2) : '1.00'}x</strong> means that for every 1 byte requested by your application, {stats?.write_amplification ? stats.write_amplification.toFixed(2) : '1.00'} bytes of physical disk I/O took place. Lower is better!
            </p>
          </div>

          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-col justify-between">
            <div>
              <div className="flex items-center gap-2 text-xs font-semibold text-white mb-2">
                <Boxes className="w-4 h-4 text-emerald-400" />
                <span>Related Storage Inspectors</span>
              </div>
              <p className="text-[11px] text-zinc-400 leading-relaxed">
                Compaction directly transforms files across levels. You can view the actual SSTables and MemTable buffers that feed into this process.
              </p>
            </div>
            <div className="mt-3 flex items-center gap-4 text-xs font-mono">
              <Link href="/sstables" className="text-emerald-400 hover:underline flex items-center gap-1">
                <span>Inspect SSTable Lanes</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </Link>
              <Link href="/memtable" className="text-cyan-400 hover:underline flex items-center gap-1">
                <span>View Flush Buffer</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </Link>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
