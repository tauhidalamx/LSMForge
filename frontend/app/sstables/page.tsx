'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { SSTableItem, LevelInfo } from '@/types';
import { formatBytes, formatNumber } from '@/lib/utils';
import {
  Boxes,
  Layers,
  HardDrive,
  Filter,
  CheckCircle,
  AlertTriangle,
  Eye,
  X,
  FileCode,
  Tag,
} from 'lucide-react';

export default function SSTablesPage() {
  const [sstables, setSstables] = useState<SSTableItem[]>([]);
  const [levels, setLevels] = useState<LevelInfo[]>([]);
  const [selectedTable, setSelectedTable] = useState<SSTableItem | null>(null);
  const [loading, setLoading] = useState(true);

  const loadData = async () => {
    try {
      const [sRes, lRes] = await Promise.all([
        api.getSSTables().catch(() => ({ sstables: [], count: 0 })),
        api.getLevels().catch(() => ({ levels: [], total_storage_bytes: 0 })),
      ]);
      setSstables(sRes.sstables);
      setLevels(lRes.levels);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 4000);
    return () => clearInterval(interval);
  }, []);

  // Group SSTables by level (0 to 6)
  const levelLanes = [0, 1, 2, 3, 4, 5, 6].map((lvl) => {
    const tablesInLevel = sstables.filter((t) => t.level === lvl);
    const summary = levels.find((l) => l.level === lvl);
    return {
      level: lvl,
      tables: tablesInLevel,
      totalBytes: summary ? summary.total_bytes : tablesInLevel.reduce((a, b) => a + b.file_size, 0),
    };
  });

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="SSTable Explorer & Level Lanes"
        description="Immutable Sorted String Tables partitioned across LSM levels with prefix-delta indexing and Bloom filters"
        onRefresh={loadData}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Level Compaction Policy Banner */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-lg bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center">
              <Boxes className="w-5 h-5 text-emerald-400" />
            </div>
            <div>
              <div className="text-sm font-semibold text-white">Leveled Storage Architecture</div>
              <p className="text-xs text-zinc-400 font-mono mt-0.5">
                Level 0: Overlapping key ranges (compaction triggers at 4 files) • Levels 1+: Non-overlapping key ranges
              </p>
            </div>
          </div>
          <div className="flex items-center gap-4 text-xs font-mono text-zinc-400">
            <span className="text-emerald-400 font-bold">{sstables.length} Total SSTables</span>
            <span>Total Storage: {formatBytes(levels.reduce((a, b) => a + b.total_bytes, 0))}</span>
          </div>
        </div>

        {/* Level Lanes Container */}
        <div className="space-y-6">
          {levelLanes.map((lane) => (
            <div
              key={lane.level}
              className={`p-5 rounded-xl border transition-all ${
                lane.tables.length > 0
                  ? 'border-[#232a3e] bg-[#121622]'
                  : 'border-[#1b2130] bg-[#0c0f16]/60 opacity-70'
              }`}
            >
              <div className="flex items-center justify-between border-b border-[#232a3e] pb-3 mb-4">
                <div className="flex items-center gap-3">
                  <span
                    className={`px-2.5 py-1 rounded text-xs font-mono font-bold ${
                      lane.level === 0
                        ? 'bg-amber-950/80 text-amber-400 border border-amber-800'
                        : 'bg-emerald-950/80 text-emerald-400 border border-emerald-800'
                    }`}
                  >
                    Level {lane.level} {lane.level === 0 ? '(L0 Overlapping)' : '(Disjoint)'}
                  </span>
                  <span className="text-xs font-mono text-zinc-400">
                    {lane.tables.length} {lane.tables.length === 1 ? 'file' : 'files'} • {formatBytes(lane.totalBytes)}
                  </span>
                </div>
                {lane.level === 0 && (
                  <span className="text-[11px] font-mono text-amber-400 bg-amber-950/40 px-2 py-0.5 rounded border border-amber-800/60">
                    Trigger Threshold: 4 files
                  </span>
                )}
              </div>

              {/* Cards Grid */}
              {lane.tables.length > 0 ? (
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
                  {lane.tables.map((table) => (
                    <div
                      key={table.file_number}
                      onClick={() => setSelectedTable(table)}
                      className="p-4 rounded-lg border border-[#232a3e] bg-[#0e121b] hover:border-emerald-500/60 hover:bg-[#151a27] transition-all cursor-pointer group"
                    >
                      <div className="flex items-center justify-between text-xs font-mono mb-2">
                        <span className="font-bold text-white flex items-center gap-1.5 group-hover:text-emerald-400 transition-colors">
                          <FileCode className="w-3.5 h-3.5 text-emerald-400" />
                          #{table.file_number}.sst
                        </span>
                        <span className="text-zinc-400">{formatBytes(table.file_size)}</span>
                      </div>

                      <div className="space-y-1.5 text-[11px] font-mono text-zinc-400 mt-3 pt-2 border-t border-[#1e2536]">
                        <div className="flex items-center gap-1.5 truncate">
                          <Tag className="w-3 h-3 text-zinc-500 flex-shrink-0" />
                          <span className="text-zinc-500">Min:</span>
                          <span className="text-zinc-300 truncate">{table.smallest_key || '—'}</span>
                        </div>
                        <div className="flex items-center gap-1.5 truncate">
                          <Tag className="w-3 h-3 text-zinc-500 flex-shrink-0" />
                          <span className="text-zinc-500">Max:</span>
                          <span className="text-zinc-300 truncate">{table.largest_key || '—'}</span>
                        </div>
                      </div>

                      <div className="mt-3 flex items-center justify-between text-[10px] font-mono text-zinc-500 pt-2 border-t border-[#1e2536]">
                        <span className="text-emerald-400/80">Bloom: 10 bpk</span>
                        <span className="group-hover:text-cyan-400 transition-colors flex items-center gap-1">
                          <Eye className="w-3 h-3" /> Inspect
                        </span>
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="py-4 text-center text-xs font-mono text-zinc-600">
                  Level {lane.level} is empty. Compactions from Level {lane.level - 1} will populate this tier.
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      {/* SSTable Block Inspector Modal */}
      {selectedTable && (
        <div className="fixed inset-0 z-50 bg-black/60 backdrop-blur-sm flex justify-end">
          <div className="w-full max-w-lg bg-[#0f1420] border-l border-[#232a3e] p-6 flex flex-col justify-between overflow-y-auto">
            <div>
              <div className="flex items-center justify-between border-b border-[#232a3e] pb-4 mb-4">
                <div>
                  <span className="text-[10px] uppercase font-mono text-emerald-400 font-semibold">
                    Level {selectedTable.level} SSTable
                  </span>
                  <h3 className="text-lg font-bold text-white mt-1">sst/{selectedTable.file_number}.sst</h3>
                </div>
                <button
                  onClick={() => setSelectedTable(null)}
                  className="p-1.5 rounded-lg hover:bg-[#1e2536] text-zinc-400 hover:text-white"
                >
                  <X className="w-5 h-5" />
                </button>
              </div>

              {/* Physical Block Layout Spec */}
              <div className="space-y-4 font-mono text-xs">
                <div className="p-3 rounded-lg bg-[#141824] border border-[#232a3e]">
                  <span className="text-[10px] text-zinc-500 uppercase block mb-1">File Size on Disk</span>
                  <span className="text-base font-bold text-white">
                    {formatBytes(selectedTable.file_size)} ({formatNumber(selectedTable.file_size)} bytes)
                  </span>
                </div>

                <div className="p-3 rounded-lg bg-[#141824] border border-[#232a3e]">
                  <span className="text-[10px] text-zinc-500 uppercase block mb-1">Key Boundaries (Lexicographic)</span>
                  <div className="mt-1 space-y-1">
                    <div>
                      <span className="text-zinc-500">Smallest Key:</span>{' '}
                      <span className="text-cyan-300 font-semibold">{selectedTable.smallest_key}</span>
                    </div>
                    <div>
                      <span className="text-zinc-500">Largest Key:</span>{' '}
                      <span className="text-cyan-300 font-semibold">{selectedTable.largest_key}</span>
                    </div>
                  </div>
                </div>

                <div>
                  <span className="text-[11px] text-zinc-400 uppercase font-mono block mb-2">
                    Physical Internal Segments
                  </span>
                  <div className="space-y-2">
                    <div className="p-3 rounded bg-[#0b0e15] border border-[#1e2536]">
                      <div className="flex justify-between text-zinc-200 font-semibold">
                        <span>1. Data Blocks</span>
                        <span className="text-cyan-400">4KB Uncompressed</span>
                      </div>
                      <p className="text-[11px] text-zinc-400 mt-1">
                        Prefix-delta encoded key-value entries with 16-key restart interval for fast binary search.
                      </p>
                    </div>

                    <div className="p-3 rounded bg-[#0b0e15] border border-[#1e2536]">
                      <div className="flex justify-between text-zinc-200 font-semibold">
                        <span>2. Bloom Filter Block</span>
                        <span className="text-emerald-400">MurmurHash3</span>
                      </div>
                      <p className="text-[11px] text-zinc-400 mt-1">
                        10 bits per key configuration (~1% false positive probability). Avoids redundant I/O for missing keys.
                      </p>
                    </div>

                    <div className="p-3 rounded bg-[#0b0e15] border border-[#1e2536]">
                      <div className="flex justify-between text-zinc-200 font-semibold">
                        <span>3. Index Block</span>
                        <span className="text-amber-400">Sparse Index</span>
                      </div>
                      <p className="text-[11px] text-zinc-400 mt-1">
                        Block handles storing offset and length for each data block separator key.
                      </p>
                    </div>

                    <div className="p-3 rounded bg-[#0b0e15] border border-[#1e2536]">
                      <div className="flex justify-between text-zinc-200 font-semibold">
                        <span>4. Trailer Footer</span>
                        <span className="text-purple-400">48-byte Fixed</span>
                      </div>
                      <p className="text-[11px] text-zinc-400 mt-1">
                        Magic Number: <code className="text-purple-300 font-bold">0x4c534d466f726765</code> (LSMForge), metaindex handle, index handle.
                      </p>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            <div className="pt-6 border-t border-[#232a3e]">
              <button
                onClick={() => setSelectedTable(null)}
                className="w-full py-2 bg-[#1e2536] hover:bg-[#28324a] text-zinc-200 text-xs font-medium rounded-lg"
              >
                Close Inspector
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
