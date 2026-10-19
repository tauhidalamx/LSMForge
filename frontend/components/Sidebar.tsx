'use client';

import React, { useEffect, useState } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import {
  LayoutDashboard,
  Network,
  HardDrive,
  Layers,
  FileText,
  Boxes,
  Filter,
  RefreshCw,
  Terminal,
  Activity,
  ShieldCheck,
  Bell,
  Settings,
  Database,
  Flame,
} from 'lucide-react';
import { api } from '@/lib/api';

const NAV_ITEMS = [
  { href: '/', label: 'Overview', icon: LayoutDashboard },
  { href: '/architecture', label: 'Architecture', icon: Network },
  { href: '/storage', label: 'Storage Explorer', icon: HardDrive },
  { href: '/memtable', label: 'MemTable', icon: Layers },
  { href: '/wal', label: 'WAL Inspector', icon: FileText },
  { href: '/sstables', label: 'SSTables', icon: Boxes },
  { href: '/bloom', label: 'Bloom Filters', icon: Filter },
  { href: '/compaction', label: 'Compaction', icon: RefreshCw },
  { href: '/query', label: 'Query Console', icon: Terminal },
  { href: '/benchmarks', label: 'Benchmarks', icon: Activity },
  { href: '/recovery', label: 'Recovery & Integrity', icon: ShieldCheck },
  { href: '/events', label: 'Logs & Events', icon: Bell },
  { href: '/settings', label: 'Settings', icon: Settings },
];

export function Sidebar() {
  const pathname = usePathname();
  const [online, setOnline] = useState<boolean | null>(null);
  const [dbName, setDbName] = useState<string>('');

  useEffect(() => {
    let mounted = true;
    const check = async () => {
      try {
        const h = await api.getHealth();
        if (mounted) {
          setOnline(h.database_open);
          setDbName(h.database_name || 'lsmforge.db');
        }
      } catch {
        if (mounted) {
          setOnline(false);
        }
      }
    };
    check();
    const interval = setInterval(check, 5000);
    return () => {
      mounted = false;
      clearInterval(interval);
    };
  }, []);

  return (
    <aside className="w-64 flex-shrink-0 bg-[#0e121b] border-r border-[#232a3e] flex flex-col h-screen sticky top-0">
      {/* Brand Header */}
      <div className="h-16 flex items-center gap-3 px-5 border-b border-[#232a3e]">
        <div className="w-9 h-9 rounded-lg bg-gradient-to-tr from-cyan-600 to-emerald-500 flex items-center justify-center shadow-lg shadow-cyan-900/40">
          <Flame className="w-5 h-5 text-white" />
        </div>
        <div>
          <div className="flex items-center gap-2">
            <span className="font-bold text-base tracking-wide text-white">LSMForge</span>
            <span className="text-[10px] uppercase font-mono px-1.5 py-0.5 rounded bg-cyan-950/80 text-cyan-400 border border-cyan-800/60 font-semibold">
              C++20
            </span>
          </div>
          <div className="flex items-center gap-1.5 mt-0.5">
            <span
              className={`w-2 h-2 rounded-full ${
                online === true
                  ? 'bg-emerald-400 animate-pulse shadow-sm shadow-emerald-400/50'
                  : online === false
                  ? 'bg-rose-500'
                  : 'bg-zinc-500'
              }`}
            />
            <span className="text-[11px] text-zinc-400 font-mono">
              {online === true ? 'ENGINE ONLINE' : online === false ? 'OFFLINE' : 'CONNECTING...'}
            </span>
          </div>
        </div>
      </div>

      {/* Navigation Links */}
      <nav className="flex-1 overflow-y-auto px-3 py-4 space-y-1">
        <div className="px-2 pb-2 text-[11px] font-semibold tracking-wider text-zinc-500 uppercase">
          Engine Navigation
        </div>
        {NAV_ITEMS.map((item) => {
          const Icon = item.icon;
          const isActive = pathname === item.href;
          return (
            <Link
              key={item.href}
              href={item.href}
              className={`flex items-center gap-3 px-3 py-2 rounded-lg text-sm font-medium transition-all ${
                isActive
                  ? 'bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 shadow-sm'
                  : 'text-zinc-400 hover:text-zinc-200 hover:bg-[#141824]'
              }`}
            >
              <Icon
                className={`w-4 h-4 transition-colors ${
                  isActive ? 'text-cyan-400' : 'text-zinc-500 group-hover:text-zinc-300'
                }`}
              />
              <span>{item.label}</span>
            </Link>
          );
        })}
      </nav>

      {/* Footer Info */}
      <div className="p-4 border-t border-[#232a3e] bg-[#090c13]/60">
        <div className="flex items-center justify-between text-xs text-zinc-400 mb-1">
          <span className="flex items-center gap-1.5 font-mono text-[11px]">
            <Database className="w-3.5 h-3.5 text-zinc-500" />
            <span className="truncate max-w-[130px]" title={dbName}>
              {dbName || 'Engine'}
            </span>
          </span>
          <span className="text-[10px] text-zinc-500 font-mono">v1.0.0</span>
        </div>
        <div className="text-[10px] text-zinc-500 font-mono flex items-center justify-between">
          <span>Port :8080</span>
          <span className="text-emerald-400">Leveled Compaction</span>
        </div>
      </div>
    </aside>
  );
}
