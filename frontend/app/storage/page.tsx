'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { SSTableItem, WalFile, MemTableData, LevelInfo } from '@/types';
import { formatBytes, formatNumber } from '@/lib/utils';
import {
  HardDrive,
  FileText,
  Boxes,
  Layers,
  Search,
  CheckCircle,
  FileCode,
  Shield,
  Eye,
  X,
} from 'lucide-react';

interface StorageFile {
  id: string;
  name: string;
  type: 'wal' | 'sstable' | 'manifest' | 'memtable' | 'lock';
  sizeBytes: number;
  level?: number;
  keyRange?: { start: string; end: string };
  isActive?: boolean;
  meta?: Record<string, any>;
}

export default function StorageExplorerPage() {
  const [filterType, setFilterType] = useState<string>('all');
  const [searchQuery, setSearchQuery] = useState<string>('');
  const [selectedFile, setSelectedFile] = useState<StorageFile | null>(null);

  const [walFiles, setWalFiles] = useState<WalFile[]>([]);
  const [sstables, setSstables] = useState<SSTableItem[]>([]);
  const [memData, setMemData] = useState<MemTableData | null>(null);
  const [levels, setLevels] = useState<LevelInfo[]>([]);
  const [loading, setLoading] = useState(true);

  const loadData = async () => {
    try {
      const [wRes, sRes, mRes, lRes] = await Promise.all([
        api.getWalFiles().catch(() => ({ wal_files: [], count: 0 })),
        api.getSSTables().catch(() => ({ sstables: [], count: 0 })),
        api.getMemTables().catch(() => null),
        api.getLevels().catch(() => ({ levels: [], total_storage_bytes: 0 })),
      ]);
      setWalFiles(wRes.wal_files);
      setSstables(sRes.sstables);
      setMemData(mRes);
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

  // Assemble files list
  const files: StorageFile[] = [];

  // Active MemTable
  if (memData) {
    files.push({
      id: 'memtable-active',
      name: 'Active MemTable (In-Memory SkipList)',
      type: 'memtable',
      sizeBytes: memData.active.memory_bytes,
      isActive: true,
      meta: {
        structure: memData.active.data_structure,
        capacity: memData.active.threshold_bytes,
        usage: `${memData.active.usage_percent.toFixed(1)}%`,
      },
    });

    if (memData.immutable.count > 0) {
      files.push({
        id: 'memtable-immutable',
        name: `Immutable MemTable Queue (${memData.immutable.count} buffers)`,
        type: 'memtable',
        sizeBytes: memData.immutable.count * 4194304,
        isActive: false,
        meta: {
          status: 'Flushing to Level 0 SSTable',
        },
      });
    }
  }

  // WAL Files
  walFiles.forEach((wf) => {
    files.push({
      id: `wal-${wf.file_number}`,
      name: wf.filename,
      type: 'wal',
      sizeBytes: wf.file_size,
      isActive: wf.is_active,
      meta: {
        file_number: wf.file_number,
        framing: '32KB CRC32C Blocks',
      },
    });
  });

  // Manifest files
  files.push({
    id: 'meta-current',
    name: 'CURRENT',
    type: 'manifest',
    sizeBytes: 16,
    isActive: true,
    meta: {
      role: 'Points to active MANIFEST journal',
    },
  });

  files.push({
    id: 'meta-lock',
    name: 'LOCK',
    type: 'lock',
    sizeBytes: 0,
    isActive: true,
    meta: {
      role: 'Advisory single-process file lock (fcntl F_SETLK)',
    },
  });

  // SSTables
  sstables.forEach((st) => {
    files.push({
      id: `sst-${st.file_number}`,
      name: st.filename,
      type: 'sstable',
      sizeBytes: st.file_size,
      level: st.level,
      keyRange: { start: st.smallest_key, end: st.largest_key },
      meta: {
        file_number: st.file_number,
        level: st.level,
        block_size: '4KB',
        bloom_filter: 'MurmurHash3 (10 bits/key)',
      },
    });
  });

  // Filter & Search
  const filteredFiles = files.filter((f) => {
    if (filterType !== 'all' && f.type !== filterType) return false;
    if (searchQuery && !f.name.toLowerCase().includes(searchQuery.toLowerCase())) return false;
    return true;
  });

  const totalBytes = files.reduce((acc, f) => acc + f.sizeBytes, 0);

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Storage Explorer"
        description="Physical filesystem inspection of WAL logs, SSTables, MemTables, and database journals"
        onRefresh={loadData}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Storage Summary Bar */}
        <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
          <div className="flex items-center justify-between text-xs mb-2">
            <span className="font-semibold text-white tracking-wide uppercase">Total Database Footprint</span>
            <span className="font-mono text-cyan-400 font-bold">{formatBytes(totalBytes)}</span>
          </div>

          <div className="w-full h-3 rounded-full bg-[#1b2130] flex overflow-hidden">
            {/* SSTables percentage */}
            <div
              className="bg-emerald-500 h-full"
              style={{
                width: `${
                  totalBytes > 0
                    ? (files.filter((f) => f.type === 'sstable').reduce((a, b) => a + b.sizeBytes, 0) / totalBytes) * 100
                    : 0
                }%`,
              }}
              title="SSTables"
            />
            {/* WAL percentage */}
            <div
              className="bg-amber-500 h-full"
              style={{
                width: `${
                  totalBytes > 0
                    ? (files.filter((f) => f.type === 'wal').reduce((a, b) => a + b.sizeBytes, 0) / totalBytes) * 100
                    : 0
                }%`,
              }}
              title="WAL"
            />
            {/* MemTable percentage */}
            <div
              className="bg-cyan-500 h-full"
              style={{
                width: `${
                  totalBytes > 0
                    ? (files.filter((f) => f.type === 'memtable').reduce((a, b) => a + b.sizeBytes, 0) / totalBytes) * 100
                    : 0
                }%`,
              }}
              title="MemTable"
            />
          </div>

          <div className="flex items-center gap-6 mt-3 text-xs font-mono text-zinc-400">
            <div className="flex items-center gap-2">
              <span className="w-2.5 h-2.5 rounded bg-emerald-500" />
              <span>SSTables ({sstables.length} files)</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2.5 h-2.5 rounded bg-amber-500" />
              <span>WAL ({walFiles.length} logs)</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2.5 h-2.5 rounded bg-cyan-500" />
              <span>MemTable ({formatBytes(memData?.active.memory_bytes || 0)})</span>
            </div>
          </div>
        </div>

        {/* Filter & Search Bar */}
        <div className="flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-1.5 p-1 rounded-lg bg-[#121622] border border-[#232a3e]">
            {['all', 'sstable', 'wal', 'memtable', 'manifest'].map((t) => (
              <button
                key={t}
                onClick={() => setFilterType(t)}
                className={`px-3 py-1.5 rounded-md text-xs font-medium uppercase tracking-wider transition-all ${
                  filterType === t
                    ? 'bg-cyan-600/20 text-cyan-400 border border-cyan-500/30'
                    : 'text-zinc-400 hover:text-white'
                }`}
              >
                {t}
              </button>
            ))}
          </div>

          <div className="relative">
            <Search className="w-4 h-4 text-zinc-500 absolute left-3 top-2.5" />
            <input
              type="text"
              placeholder="Search file name or ID..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="pl-9 pr-4 py-1.5 bg-[#121622] border border-[#232a3e] rounded-lg text-xs text-zinc-200 placeholder-zinc-500 focus:outline-none focus:border-cyan-500 w-64"
            />
          </div>
        </div>

        {/* Storage Files Table */}
        <div className="rounded-xl border border-[#232a3e] bg-[#121622] overflow-hidden">
          <table className="w-full text-left text-xs">
            <thead className="bg-[#0b0e15] border-b border-[#232a3e] text-zinc-400 font-mono">
              <tr>
                <th className="py-3 px-4">Filename / Item</th>
                <th className="py-3 px-4">Type</th>
                <th className="py-3 px-4">Size</th>
                <th className="py-3 px-4">Level / Target</th>
                <th className="py-3 px-4">Key Range</th>
                <th className="py-3 px-4 text-right">Action</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1e2536] font-mono">
              {filteredFiles.map((file) => (
                <tr
                  key={file.id}
                  className="hover:bg-[#161c2b] transition-colors cursor-pointer"
                  onClick={() => setSelectedFile(file)}
                >
                  <td className="py-3 px-4 flex items-center gap-2">
                    {file.type === 'wal' && <FileText className="w-4 h-4 text-amber-400" />}
                    {file.type === 'sstable' && <Boxes className="w-4 h-4 text-emerald-400" />}
                    {file.type === 'memtable' && <Layers className="w-4 h-4 text-cyan-400" />}
                    {file.type === 'manifest' && <FileCode className="w-4 h-4 text-purple-400" />}
                    {file.type === 'lock' && <Shield className="w-4 h-4 text-zinc-400" />}
                    <span className="font-semibold text-white">{file.name}</span>
                    {file.isActive && (
                      <span className="px-1.5 py-0.2 rounded text-[9px] bg-emerald-950 text-emerald-400 border border-emerald-800">
                        ACTIVE
                      </span>
                    )}
                  </td>
                  <td className="py-3 px-4">
                    <span className="uppercase text-[10px] px-2 py-0.5 rounded bg-[#1b2130] text-zinc-300">
                      {file.type}
                    </span>
                  </td>
                  <td className="py-3 px-4 text-zinc-300">{formatBytes(file.sizeBytes)}</td>
                  <td className="py-3 px-4 text-zinc-400">
                    {file.level !== undefined ? (
                      <span className="text-emerald-400 font-bold">L{file.level}</span>
                    ) : (
                      '—'
                    )}
                  </td>
                  <td className="py-3 px-4 text-zinc-400 max-w-xs truncate">
                    {file.keyRange ? `[${file.keyRange.start} ... ${file.keyRange.end}]` : '—'}
                  </td>
                  <td className="py-3 px-4 text-right">
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setSelectedFile(file);
                      }}
                      className="p-1 rounded hover:bg-[#232a3e] text-zinc-400 hover:text-cyan-400"
                    >
                      <Eye className="w-4 h-4" />
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      {/* File Detail Drawer Modal */}
      {selectedFile && (
        <div className="fixed inset-0 z-50 bg-black/60 backdrop-blur-sm flex justify-end">
          <div className="w-full max-w-md bg-[#0f1420] border-l border-[#232a3e] p-6 flex flex-col justify-between overflow-y-auto">
            <div>
              <div className="flex items-center justify-between border-b border-[#232a3e] pb-4 mb-4">
                <div>
                  <span className="text-[10px] uppercase font-mono text-zinc-500">File Metadata</span>
                  <h3 className="text-base font-bold text-white mt-1 break-all">{selectedFile.name}</h3>
                </div>
                <button
                  onClick={() => setSelectedFile(null)}
                  className="p-1.5 rounded-lg hover:bg-[#1e2536] text-zinc-400 hover:text-white"
                >
                  <X className="w-5 h-5" />
                </button>
              </div>

              <div className="space-y-4 font-mono text-xs">
                <div>
                  <span className="text-zinc-500 text-[11px] block">Item Type:</span>
                  <span className="text-cyan-400 uppercase font-semibold">{selectedFile.type}</span>
                </div>

                <div>
                  <span className="text-zinc-500 text-[11px] block">Exact File Size:</span>
                  <span className="text-white">{formatNumber(selectedFile.sizeBytes)} bytes ({formatBytes(selectedFile.sizeBytes)})</span>
                </div>

                {selectedFile.level !== undefined && (
                  <div>
                    <span className="text-zinc-500 text-[11px] block">LSM-Tree Level:</span>
                    <span className="text-emerald-400 font-semibold">Level {selectedFile.level}</span>
                  </div>
                )}

                {selectedFile.keyRange && (
                  <div>
                    <span className="text-zinc-500 text-[11px] block">Indexed Key Range:</span>
                    <div className="p-2 rounded bg-[#161c28] border border-[#232a3e] text-[11px] mt-1 space-y-1">
                      <div><span className="text-zinc-500">Smallest:</span> {selectedFile.keyRange.start}</div>
                      <div><span className="text-zinc-500">Largest:</span> {selectedFile.keyRange.end}</div>
                    </div>
                  </div>
                )}

                {selectedFile.meta && (
                  <div>
                    <span className="text-zinc-500 text-[11px] block mb-1">Extended Properties:</span>
                    <pre className="p-3 rounded bg-[#141824] border border-[#232a3e] text-zinc-300 text-[11px] overflow-x-auto">
                      {JSON.stringify(selectedFile.meta, null, 2)}
                    </pre>
                  </div>
                )}
              </div>
            </div>

            <div className="pt-6 border-t border-[#232a3e]">
              <button
                onClick={() => setSelectedFile(null)}
                className="w-full py-2 bg-[#1e2536] hover:bg-[#28324a] text-zinc-200 text-xs font-medium rounded-lg"
              >
                Close Drawer
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
