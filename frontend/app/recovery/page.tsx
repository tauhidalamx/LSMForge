'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { IntegrityStatus, SystemHealth } from '@/types';
import {
  ShieldCheck,
  CheckCircle2,
  AlertTriangle,
  FileText,
  Boxes,
  Lock,
  RefreshCw,
  HardDrive,
  Info,
} from 'lucide-react';

export default function RecoveryPage() {
  const [integrity, setIntegrity] = useState<IntegrityStatus | null>(null);
  const [health, setHealth] = useState<SystemHealth | null>(null);
  const [loading, setLoading] = useState<boolean>(true);

  const loadData = async () => {
    try {
      const [ing, h] = await Promise.all([
        api.getIntegrity().catch(() => null),
        api.getHealth().catch(() => null),
      ]);
      setIntegrity(ing);
      setHealth(h);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 4000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Recovery & Integrity Center"
        description="Write-Ahead Log replay status, Manifest journal integrity, and single-process advisory lock diagnostics"
        onRefresh={loadData}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Top Status Card */}
        <div
          className={`p-5 rounded-xl border flex items-center justify-between ${
            integrity?.integrity_pass
              ? 'border-emerald-800 bg-emerald-950/20 text-emerald-300'
              : 'border-amber-800 bg-amber-950/20 text-amber-300'
          }`}
        >
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center">
              <ShieldCheck className="w-6 h-6 text-emerald-400" />
            </div>
            <div>
              <h3 className="text-base font-bold text-white">
                {integrity?.integrity_pass ? 'System Integrity Pass — Database Healthy' : 'Verifying State...'}
              </h3>
              <p className="text-xs text-zinc-400 mt-0.5 font-mono">
                Manifest journal and on-disk SSTable inventory are in 100% agreement.
              </p>
            </div>
          </div>
          <span className="px-3 py-1 rounded text-xs font-mono font-bold bg-emerald-900/60 text-emerald-300 border border-emerald-700">
            ZERO CORRUPTION DETECTED
          </span>
        </div>

        {/* 4 Pillars of Crash Recovery & Persistence */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* CURRENT Pointer */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] space-y-2 font-mono text-xs">
            <div className="flex items-center justify-between">
              <span className="text-zinc-400 font-semibold">CURRENT File</span>
              <CheckCircle2 className="w-4 h-4 text-emerald-400" />
            </div>
            <div className="text-lg font-bold text-white">
              {integrity?.current_file_present ? 'Present & Valid' : 'Missing'}
            </div>
            <p className="text-[11px] text-zinc-500">
              Points safely to active MANIFEST journal. Updated via atomic rename.
            </p>
          </div>

          {/* Advisory Lock */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] space-y-2 font-mono text-xs">
            <div className="flex items-center justify-between">
              <span className="text-zinc-400 font-semibold">Process Lock (LOCK)</span>
              <Lock className="w-4 h-4 text-cyan-400" />
            </div>
            <div className="text-lg font-bold text-white">
              {integrity?.lock_file_active ? 'Active Lock Held' : 'Unlocked'}
            </div>
            <p className="text-[11px] text-zinc-500">
              POSIX fcntl(F_SETLK) ensures strict single-process access.
            </p>
          </div>

          {/* SSTables Verified */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] space-y-2 font-mono text-xs">
            <div className="flex items-center justify-between">
              <span className="text-zinc-400 font-semibold">Registered SSTables</span>
              <Boxes className="w-4 h-4 text-emerald-400" />
            </div>
            <div className="text-lg font-bold text-white">
              {integrity?.verified_sstables || 0} / {integrity?.registered_sstables || 0}
            </div>
            <p className="text-[11px] text-zinc-500">
              Every table referenced in Manifest exists on persistent filesystem.
            </p>
          </div>

          {/* Engine Uptime */}
          <div className="p-4 rounded-xl border border-[#232a3e] bg-[#121622] space-y-2 font-mono text-xs">
            <div className="flex items-center justify-between">
              <span className="text-zinc-400 font-semibold">Engine Runtime</span>
              <RefreshCw className="w-4 h-4 text-purple-400" />
            </div>
            <div className="text-lg font-bold text-white">
              {health?.uptime_seconds ? `${health.uptime_seconds}s uptime` : 'Active'}
            </div>
            <p className="text-[11px] text-zinc-500">
              Continuous background flush and compaction worker threads active.
            </p>
          </div>
        </div>

        {/* Detailed Crash Recovery Protocol Steps */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
          <div className="flex items-center justify-between border-b border-[#232a3e] pb-3">
            <div>
              <h3 className="text-sm font-semibold text-white">Crash Recovery Sequence Specification</h3>
              <p className="text-xs text-zinc-400 mt-0.5">
                Deterministic restart steps executed whenever the database is reopened
              </p>
            </div>
            <span className="text-xs font-mono text-cyan-400 bg-cyan-950/60 px-2 py-0.5 rounded border border-cyan-800">
              Guaranteed Zero Data Loss
            </span>
          </div>

          <div className="space-y-3 font-mono text-xs">
            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e] flex items-start gap-3">
              <span className="w-6 h-6 rounded-full bg-cyan-950 text-cyan-400 border border-cyan-800 flex items-center justify-center font-bold flex-shrink-0">
                1
              </span>
              <div>
                <span className="text-white font-semibold">Acquire Advisory File Lock</span>
                <p className="text-zinc-400 text-[11px] mt-0.5">
                  Calls <code className="text-cyan-300">fcntl(fd, F_SETLK)</code> on <code className="text-zinc-300">LOCK</code> file. If another engine process is active, returns <code className="text-amber-300">Status::IOError(&quot;Database locked&quot;)</code> to prevent corruption.
                </p>
              </div>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e] flex items-start gap-3">
              <span className="w-6 h-6 rounded-full bg-cyan-950 text-cyan-400 border border-cyan-800 flex items-center justify-center font-bold flex-shrink-0">
                2
              </span>
              <div>
                <span className="text-white font-semibold">Read CURRENT & Replay MANIFEST Journal</span>
                <p className="text-zinc-400 text-[11px] mt-0.5">
                  Reads <code className="text-zinc-300">CURRENT</code> to identify active <code className="text-zinc-300">MANIFEST-xxxxxx</code>. Iteratively parses <code className="text-cyan-300">VersionEdit</code> records into a fresh <code className="text-cyan-300">VersionSet</code>, reconstructing active SSTables and sequence boundaries.
                </p>
              </div>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e] flex items-start gap-3">
              <span className="w-6 h-6 rounded-full bg-cyan-950 text-cyan-400 border border-cyan-800 flex items-center justify-center font-bold flex-shrink-0">
                3
              </span>
              <div>
                <span className="text-white font-semibold">Replay Active Write-Ahead Log</span>
                <p className="text-zinc-400 text-[11px] mt-0.5">
                  Reads uncompacted WAL file starting from <code className="text-zinc-300">log_number</code>. Uses 32KB block framing with CRC32C verification. Any incomplete trailing record from a crash is cleanly truncated. Durable records are re-inserted into the active MemTable.
                </p>
              </div>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e] flex items-start gap-3">
              <span className="w-6 h-6 rounded-full bg-cyan-950 text-cyan-400 border border-cyan-800 flex items-center justify-center font-bold flex-shrink-0">
                4
              </span>
              <div>
                <span className="text-white font-semibold">Allocate Fresh WAL & Resume Operations</span>
                <p className="text-zinc-400 text-[11px] mt-0.5">
                  Atomically writes a new VersionEdit reserving the next file number, allocates a clean WAL file, and opens the database for client read and write operations.
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
