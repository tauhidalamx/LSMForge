'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api, getApiBaseUrl, setApiBaseUrl } from '@/lib/api';
import { EngineConfig, EngineVersion } from '@/types';
import { formatBytes } from '@/lib/utils';
import {
  Settings as SettingsIcon,
  Server,
  Cpu,
  CheckCircle2,
  Save,
  RefreshCw,
  HardDrive,
  Sliders,
  Shield,
} from 'lucide-react';

export default function SettingsPage() {
  const [apiUrl, setApiUrl] = useState<string>('');
  const [testResult, setTestResult] = useState<{ ok: boolean; message: string } | null>(null);
  const [testing, setTesting] = useState<boolean>(false);
  const [config, setConfig] = useState<EngineConfig | null>(null);
  const [version, setVersion] = useState<EngineVersion | null>(null);
  const [loading, setLoading] = useState<boolean>(true);

  useEffect(() => {
    setApiUrl(getApiBaseUrl());
    Promise.all([api.getConfig().catch(() => null), api.getVersion().catch(() => null)])
      .then(([c, v]) => {
        setConfig(c);
        setVersion(v);
      })
      .finally(() => setLoading(false));
  }, []);

  const handleSaveApiUrl = async (e: React.FormEvent) => {
    e.preventDefault();
    setTesting(true);
    setTestResult(null);
    try {
      setApiBaseUrl(apiUrl.trim());
      const health = await api.getHealth();
      setTestResult({
        ok: true,
        message: `Successfully connected to ${health.engine} v${health.version} (${health.database_name})`,
      });
      const [c, v] = await Promise.all([api.getConfig(), api.getVersion()]);
      setConfig(c);
      setVersion(v);
    } catch (err: unknown) {
      setTestResult({
        ok: false,
        message: err instanceof Error ? err.message : 'Connection failed',
      });
    } finally {
      setTesting(false);
    }
  };

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Settings & Runtime Options"
        description="Backend HTTP connectivity parameters, C++20 engine build specifications, and LSM-Tree options"
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Backend API Connection Form */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
          <div className="flex items-center gap-2 border-b border-[#232a3e] pb-3">
            <Server className="w-5 h-5 text-cyan-400" />
            <h3 className="text-sm font-semibold text-white">Backend HTTP Service Connection</h3>
          </div>

          <form onSubmit={handleSaveApiUrl} className="space-y-4 max-w-xl">
            <div className="space-y-1 text-xs font-mono">
              <label className="text-zinc-400">REST API Base URL:</label>
              <div className="flex gap-2">
                <input
                  type="text"
                  value={apiUrl}
                  onChange={(e) => setApiUrl(e.target.value)}
                  placeholder="http://localhost:8080"
                  className="flex-1 px-3 py-2 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white font-mono text-xs focus:outline-none focus:border-cyan-500"
                />
                <button
                  type="submit"
                  disabled={testing}
                  className="px-4 py-2 bg-cyan-600 hover:bg-cyan-500 text-white font-medium text-xs rounded-lg flex items-center gap-1.5 transition-all font-mono disabled:opacity-50"
                >
                  <Save className="w-3.5 h-3.5" />
                  <span>{testing ? 'Testing...' : 'Test & Save'}</span>
                </button>
              </div>
              <span className="text-[10px] text-zinc-500">
                Default: http://localhost:8080 (binds to lsmforge_server executable)
              </span>
            </div>

            {testResult && (
              <div
                className={`p-3 rounded-lg border text-xs font-mono flex items-center gap-2 ${
                  testResult.ok
                    ? 'bg-emerald-950/60 text-emerald-300 border-emerald-800'
                    : 'bg-rose-950/60 text-rose-300 border-rose-800'
                }`}
              >
                <CheckCircle2 className="w-4 h-4 flex-shrink-0" />
                <span>{testResult.message}</span>
              </div>
            )}
          </form>
        </div>

        {/* C++ Engine Build & Compiler Metadata */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
          <div className="flex items-center gap-2 border-b border-[#232a3e] pb-3">
            <Cpu className="w-5 h-5 text-emerald-400" />
            <h3 className="text-sm font-semibold text-white">C++20 Engine Runtime & Compiler Profile</h3>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 font-mono text-xs">
            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-[11px] text-zinc-500 block">C++ Standard</span>
              <span className="text-white font-bold mt-1 block">{version?.cxx_standard || 'C++20'}</span>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-[11px] text-zinc-500 block">Host Compiler</span>
              <span className="text-emerald-400 font-bold mt-1 block truncate" title={version?.compiler}>
                {version?.compiler || 'Apple Clang'}
              </span>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-[11px] text-zinc-500 block">Target Platform</span>
              <span className="text-cyan-400 font-bold mt-1 block">{version?.platform || 'macOS (Darwin)'}</span>
            </div>

            <div className="p-3.5 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-[11px] text-zinc-500 block">Engine Version</span>
              <span className="text-purple-400 font-bold mt-1 block">v{version?.version || '1.0.0'}</span>
            </div>
          </div>

          {/* Capabilities Badges */}
          <div className="pt-2">
            <span className="text-zinc-500 text-[11px] font-mono block mb-2">Compiled Capabilities:</span>
            <div className="flex flex-wrap gap-2 font-mono text-xs">
              {(version?.capabilities || [
                'wal_framing_crc32c',
                'lockfree_skiplist_memtable',
                'prefix_delta_sstable',
                'murmur3_bloom_filters',
                'leveled_compaction',
                'mvcc_snapshots',
                'atomic_write_batches',
                'crash_recovery',
              ]).map((cap) => (
                <span
                  key={cap}
                  className="px-2.5 py-1 rounded bg-[#0e121b] text-zinc-300 border border-[#232a3e] text-[11px]"
                >
                  ✓ {cap}
                </span>
              ))}
            </div>
          </div>
        </div>

        {/* Database Options Inspection */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
          <div className="flex items-center gap-2 border-b border-[#232a3e] pb-3">
            <Sliders className="w-5 h-5 text-amber-400" />
            <h3 className="text-sm font-semibold text-white">Live LSMForge Options (`Options` Struct)</h3>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4 font-mono text-xs">
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">write_buffer_size:</span>
              <span className="text-white font-bold">{formatBytes(config?.write_buffer_size || 4194304)}</span>
            </div>
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">block_size:</span>
              <span className="text-white font-bold">{formatBytes(config?.block_size || 4096)}</span>
            </div>
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">block_restart_interval:</span>
              <span className="text-white font-bold">{config?.block_restart_interval || 16} keys</span>
            </div>
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">bloom_bits_per_key:</span>
              <span className="text-emerald-400 font-bold">{config?.bloom_bits_per_key || 10} bits</span>
            </div>
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">level0_compaction_trigger:</span>
              <span className="text-amber-400 font-bold">{config?.level0_file_num_compaction_trigger || 4} files</span>
            </div>
            <div className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e]">
              <span className="text-zinc-500 block">wal_sync_policy:</span>
              <span className="text-cyan-400 font-bold">{config?.wal_sync_policy || 'sync_periodic'}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
