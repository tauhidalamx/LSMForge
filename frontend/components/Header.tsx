'use client';

import React, { useState } from 'react';
import { RefreshCw, Play, Zap, Terminal, ChevronRight, Home, HelpCircle } from 'lucide-react';
import { api } from '@/lib/api';
import Link from 'next/link';
import { usePathname } from 'next/navigation';

interface HeaderProps {
  title: string;
  description?: string;
  onRefresh?: () => void;
  isRefreshing?: boolean;
}

export function Header({ title, description, onRefresh, isRefreshing }: HeaderProps) {
  const pathname = usePathname();
  const [flushing, setFlushing] = useState(false);
  const [compacting, setCompacting] = useState(false);
  const [msg, setMsg] = useState<{ text: string; error?: boolean } | null>(null);

  const handleFlush = async () => {
    setFlushing(true);
    setMsg(null);
    try {
      const res = await api.flushMemTable();
      setMsg({ text: `✨ Active MemTable persisted to Level 0 SSTable in ${res.duration_ms} ms!` });
      if (onRefresh) onRefresh();
    } catch (e: unknown) {
      setMsg({ text: e instanceof Error ? e.message : 'Flush failed', error: true });
    } finally {
      setFlushing(false);
      setTimeout(() => setMsg(null), 5000);
    }
  };

  const handleCompact = async () => {
    setCompacting(true);
    setMsg(null);
    try {
      const res = await api.triggerCompaction();
      setMsg({ text: `🚀 Leveled Compaction finished in ${res.duration_ms} ms! Obsolete keys & tombstones cleaned.` });
      if (onRefresh) onRefresh();
    } catch (e: unknown) {
      setMsg({ text: e instanceof Error ? e.message : 'Compaction failed', error: true });
    } finally {
      setCompacting(false);
      setTimeout(() => setMsg(null), 5000);
    }
  };

  // Humanized breadcrumb navigation
  const pathSegment = pathname.replace(/^\//, '');
  const breadcrumbName = pathSegment ? pathSegment.charAt(0).toUpperCase() + pathSegment.slice(1) : 'Overview';

  return (
    <header className="h-16 border-b border-[#232a3e] bg-[#0b0e15]/90 backdrop-blur-md px-6 flex items-center justify-between sticky top-0 z-30">
      <div className="flex flex-col justify-center">
        {/* Humanized Breadcrumbs */}
        <div className="flex items-center gap-1.5 text-[11px] font-mono text-zinc-400 mb-0.5">
          <Link href="/" className="hover:text-cyan-400 transition-colors flex items-center gap-1">
            <Home className="w-3 h-3 text-zinc-500" />
            <span>Dashboard</span>
          </Link>
          {pathSegment && (
            <>
              <ChevronRight className="w-3 h-3 text-zinc-600" />
              <span className="text-cyan-400 font-semibold">{breadcrumbName}</span>
            </>
          )}
        </div>
        <h1 className="text-base font-bold text-white tracking-tight flex items-center gap-2">
          {title}
        </h1>
      </div>

      <div className="flex items-center gap-3">
        {msg && (
          <div
            className={`text-xs px-3.5 py-1.5 rounded-lg border transition-all animate-fade-in font-mono shadow-md ${
              msg.error
                ? 'bg-rose-950/70 text-rose-300 border-rose-800'
                : 'bg-emerald-950/70 text-emerald-300 border-emerald-800'
            }`}
          >
            {msg.text}
          </div>
        )}

        {/* Quick action: Flush */}
        <button
          onClick={handleFlush}
          disabled={flushing}
          className="flex items-center gap-1.5 text-xs font-mono font-medium px-3 py-1.5 rounded-lg bg-[#161c28] hover:bg-[#1f2738] text-zinc-200 border border-[#2b354d] hover:border-amber-500/50 transition-all disabled:opacity-50 group"
          title="Flush active in-memory SkipList MemTable into a persistent Level 0 SSTable on disk"
        >
          <Zap className={`w-3.5 h-3.5 text-amber-400 group-hover:scale-110 transition-transform ${flushing ? 'animate-spin' : ''}`} />
          <span>{flushing ? 'Flushing...' : 'Flush MemTable'}</span>
        </button>

        {/* Quick action: Compact */}
        <button
          onClick={handleCompact}
          disabled={compacting}
          className="flex items-center gap-1.5 text-xs font-mono font-medium px-3 py-1.5 rounded-lg bg-[#161c28] hover:bg-[#1f2738] text-zinc-200 border border-[#2b354d] hover:border-cyan-500/50 transition-all disabled:opacity-50 group"
          title="Run a background leveled compaction pass to merge L0 files into L1 and garbage collect tombstones"
        >
          <Play className={`w-3.5 h-3.5 text-cyan-400 group-hover:scale-110 transition-transform ${compacting ? 'animate-spin' : ''}`} />
          <span>{compacting ? 'Compacting...' : 'Compact DB'}</span>
        </button>

        {/* Quick link: Query Console */}
        <Link
          href="/query"
          className="flex items-center gap-1.5 text-xs font-mono font-medium px-3 py-1.5 rounded-lg bg-cyan-600/20 hover:bg-cyan-600/30 text-cyan-300 border border-cyan-500/30 hover:border-cyan-400 transition-all"
        >
          <Terminal className="w-3.5 h-3.5 text-cyan-400" />
          <span>Query Console</span>
        </Link>

        {onRefresh && (
          <button
            onClick={onRefresh}
            disabled={isRefreshing}
            className="p-2 rounded-lg hover:bg-[#161c28] text-zinc-400 hover:text-white border border-transparent hover:border-[#2b354d] transition-all"
            title="Refresh live telemetry from C++ storage engine"
          >
            <RefreshCw className={`w-4 h-4 ${isRefreshing ? 'animate-spin text-cyan-400' : ''}`} />
          </button>
        )}
      </div>
    </header>
  );
}
