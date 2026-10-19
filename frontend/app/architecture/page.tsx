'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { EngineStats, MemTableData, LevelInfo } from '@/types';
import { formatBytes, formatNumber } from '@/lib/utils';
import {
  FileText,
  Layers,
  HardDrive,
  Filter,
  RefreshCw,
  Server,
  Database,
  ArrowRight,
  Shield,
  Search,
  CheckCircle2,
  Info,
} from 'lucide-react';

interface ArchNode {
  id: string;
  title: string;
  role: string;
  type: 'write' | 'read' | 'storage' | 'meta';
  cppFile: string;
  description: string;
  keyDetails: string[];
}

const ARCH_NODES: ArchNode[] = [
  {
    id: 'client_api',
    title: 'HTTP REST & Client API',
    role: 'API Entrypoint & Versioning',
    type: 'write',
    cppFile: 'server/src/http_server.cpp',
    description: 'High-throughput C++20 REST API powered by cpp-httplib, with JSON validation, CORS headers, SSE event streaming, and typed error responses.',
    keyDetails: ['REST v1 endpoints', 'Microsecond latency tracing', 'Batch framing & atomic submission', 'Binary key safe'],
  },
  {
    id: 'write_coordinator',
    title: 'Write Coordinator',
    role: 'Atomic Sequencing & WAL Append',
    type: 'write',
    cppFile: 'src/db/db_impl.cpp',
    description: 'Assigns monotonic SequenceNumbers to incoming Put, Delete, and Batch operations. Coordinates concurrent writers with single-mutex batch merging.',
    keyDetails: ['Monotonic 56-bit sequence numbering', 'Tombstone creation for deletes', 'Atomic WriteBatch application', 'Backpressure on memtable stalls'],
  },
  {
    id: 'wal',
    title: 'Write-Ahead Log (WAL)',
    role: 'Crash Durability & Replay',
    type: 'write',
    cppFile: 'src/wal/wal_writer.cpp',
    description: 'Sequential append-only log structured in 32KB blocks with 7-byte framing and Castagnoli CRC32C checksums to survive unbuffered system crashes.',
    keyDetails: ['32KB physical block chunks', 'FULL, FIRST, MIDDLE, LAST record types', 'macOS fcntl(F_FULLFSYNC) & Linux fdatasync', 'Crash recovery replay'],
  },
  {
    id: 'memtable',
    title: 'Active MemTable',
    role: 'In-Memory Sorted SkipList',
    type: 'write',
    cppFile: 'src/memtable/skiplist_memtable.cpp',
    description: 'Lock-free SkipList maintaining ordered keys with InternalKey encoding (UserKey + SequenceNumber + ValueType). Fast lock-free concurrent reads.',
    keyDetails: ['Lock-free skip list (p=0.25, max height 12)', 'Arena memory allocator', 'InternalKey ordering: newest version first', 'Rotates to immutable on 4MB limit'],
  },
  {
    id: 'immutable_mem',
    title: 'Immutable MemTables',
    role: 'Flush Buffer Queue',
    type: 'write',
    cppFile: 'src/db/db_impl.cpp',
    description: 'Frozen MemTables queued for background flushing into Level 0 SSTable files. Ensures client writes continue unblocked.',
    keyDetails: ['Read-only concurrent lookups', 'Serialized flush worker', 'Zero data loss on rotation', 'Direct conversion to SSTable format'],
  },
  {
    id: 'sstable_builder',
    title: 'SSTable Builder',
    role: 'Block & Filter Serialization',
    type: 'storage',
    cppFile: 'src/table/table_builder.cpp',
    description: 'Encodes sorted key-values with prefix-delta compression, generates 16-key restart points, builds MurmurHash3 Bloom filters, and formats index blocks.',
    keyDetails: ['Prefix-delta compressed 4KB data blocks', '16-key restart interval', 'MurmurHash3 Bloom filter block', '48-byte trailer footer (Magic: 0x4c534d466f726765)'],
  },
  {
    id: 'manifest',
    title: 'Manifest & VersionSet',
    role: 'LSM State Journal & MVCC',
    type: 'meta',
    cppFile: 'src/db/version_set.cpp',
    description: 'Atomic state transitions via VersionEdit records. Tracks live SSTable file numbers, levels, and key boundaries across crash reboots.',
    keyDetails: ['CURRENT pointer file protocol', 'VersionEdit delta log journal', 'Snapshot MVCC isolation', 'Atomic file installation'],
  },
  {
    id: 'sstables',
    title: 'SSTable Storage (L0 - L6)',
    role: 'Persistent Leveled Files',
    type: 'storage',
    cppFile: 'src/table/table_reader.cpp',
    description: 'Immutable on-disk files. Level 0 contains overlapping key ranges. Levels 1 through 6 guarantee disjoint, non-overlapping sorted partitions.',
    keyDetails: ['Level 0: 4-file compaction trigger', 'Level 1+: Strictly non-overlapping key ranges', 'Binary search over sparse index', 'Mmap & RandomAccessFile I/O'],
  },
  {
    id: 'bloom_filter',
    title: 'Bloom Filter Engine',
    role: 'I/O Avoidance for Point Lookups',
    type: 'read',
    cppFile: 'src/table/bloom_filter.cpp',
    description: 'Probabilistic set membership based on double-hashing MurmurHash3. Returns true negative with 100% certainty, bypassing disk reads for missing keys.',
    keyDetails: ['10 bits per key (~1% false positive rate)', 'Kirsch-Mitzenmacher hash optimization', 'Serialized in SSTable meta block', 'Pre-read in memory at open'],
  },
  {
    id: 'compaction',
    title: 'Leveled Compactor',
    role: 'Garbage Collection & Range Merging',
    type: 'storage',
    cppFile: 'src/db/compaction.cpp',
    description: 'Merges multi-level SSTables, purges stale sequence versions, drops tombstones when safe (IsBaseLevelForKey), and writes new compact files.',
    keyDetails: ['Multi-way merging iterator', 'User-key expansion across overlapping L0 files', 'Tombstone preservation until base level', 'Calculates write & size amplification'],
  },
];

export default function ArchitecturePage() {
  const [selectedNode, setSelectedNode] = useState<ArchNode>(ARCH_NODES[0]);
  const [stats, setStats] = useState<EngineStats | null>(null);
  const [memData, setMemData] = useState<MemTableData | null>(null);
  const [levels, setLevels] = useState<LevelInfo[]>([]);

  useEffect(() => {
    Promise.all([
      api.getStats().catch(() => null),
      api.getMemTables().catch(() => null),
      api.getLevels().then((r) => r.levels).catch(() => []),
    ]).then(([s, m, l]) => {
      setStats(s);
      setMemData(m);
      setLevels(l);
    });
  }, []);

  const getNodeMetrics = (nodeId: string) => {
    switch (nodeId) {
      case 'client_api':
        return `${formatNumber((stats?.operations.puts || 0) + (stats?.operations.gets || 0))} API requests served`;
      case 'write_coordinator':
        return `${formatNumber(stats?.operations.puts || 0)} PUT / ${formatNumber(stats?.operations.deletes || 0)} DELETE`;
      case 'wal':
        return `${formatBytes(stats?.storage.wal_bytes_written || 0)} written to disk`;
      case 'memtable':
        return `${formatBytes(memData?.active.memory_bytes || 0)} memory (${memData?.active.usage_percent.toFixed(1) || 0}% full)`;
      case 'immutable_mem':
        return `${memData?.immutable.count || 0} queued for disk flush`;
      case 'sstable_builder':
        return `${stats?.compaction.flushes || 0} tables built and sealed`;
      case 'manifest':
        return `VersionSet active (${stats?.storage.active_sstables || 0} live SSTables tracked)`;
      case 'sstables':
        return `${formatBytes(stats?.storage.total_sstable_bytes || 0)} across ${levels.length} levels`;
      case 'bloom_filter':
        return `${formatNumber(stats?.bloom_filter.useful_checks || 0)} disk reads saved`;
      case 'compaction':
        return `${stats?.compaction.compactions || 0} compaction cycles completed`;
      default:
        return 'Active';
    }
  };

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Interactive System Architecture"
        description="Comprehensive C++20 component topology, dataflow pipelines, and internal mechanics"
      />

      <div className="flex-1 p-6 grid grid-cols-1 lg:grid-cols-3 gap-6 overflow-y-auto">
        {/* Interactive Architecture Flow View (2 Columns) */}
        <div className="lg:col-span-2 space-y-6">
          {/* Write Pipeline Section */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <span className="text-xs font-semibold uppercase tracking-wider text-cyan-400">
                1. Write Path Architecture (Low-Latency Durability)
              </span>
              <span className="text-[11px] font-mono text-zinc-500">Atomic Sequencing</span>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
              {['client_api', 'write_coordinator', 'wal'].map((id) => {
                const node = ARCH_NODES.find((n) => n.id === id)!;
                const isSelected = selectedNode.id === id;
                return (
                  <button
                    key={id}
                    onClick={() => setSelectedNode(node)}
                    className={`text-left p-4 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-cyan-500 bg-cyan-950/30 shadow-md shadow-cyan-950/50'
                        : 'border-[#232a3e] bg-[#0e121b] hover:border-zinc-700'
                    }`}
                  >
                    <div className="text-xs font-semibold text-white">{node.title}</div>
                    <div className="text-[11px] text-zinc-400 mt-1">{node.role}</div>
                    <div className="mt-3 text-[10px] font-mono text-cyan-400 bg-cyan-950/60 px-2 py-0.5 rounded border border-cyan-800/60 inline-block">
                      {getNodeMetrics(id)}
                    </div>
                  </button>
                );
              })}
            </div>

            <div className="flex justify-center my-2 text-zinc-600">
              <ArrowRight className="w-5 h-5 rotate-90 sm:rotate-0" />
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
              {['memtable', 'immutable_mem'].map((id) => {
                const node = ARCH_NODES.find((n) => n.id === id)!;
                const isSelected = selectedNode.id === id;
                return (
                  <button
                    key={id}
                    onClick={() => setSelectedNode(node)}
                    className={`text-left p-4 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-cyan-500 bg-cyan-950/30 shadow-md shadow-cyan-950/50'
                        : 'border-[#232a3e] bg-[#0e121b] hover:border-zinc-700'
                    }`}
                  >
                    <div className="text-xs font-semibold text-white">{node.title}</div>
                    <div className="text-[11px] text-zinc-400 mt-1">{node.role}</div>
                    <div className="mt-3 text-[10px] font-mono text-emerald-400 bg-emerald-950/60 px-2 py-0.5 rounded border border-emerald-800/60 inline-block">
                      {getNodeMetrics(id)}
                    </div>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Storage & Compaction Pipeline Section */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <span className="text-xs font-semibold uppercase tracking-wider text-emerald-400">
                2. Storage & Leveled Compaction Path
              </span>
              <span className="text-[11px] font-mono text-zinc-500">Persistent Disk Files</span>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
              {['sstable_builder', 'sstables', 'compaction'].map((id) => {
                const node = ARCH_NODES.find((n) => n.id === id)!;
                const isSelected = selectedNode.id === id;
                return (
                  <button
                    key={id}
                    onClick={() => setSelectedNode(node)}
                    className={`text-left p-4 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-emerald-500 bg-emerald-950/30 shadow-md shadow-emerald-950/50'
                        : 'border-[#232a3e] bg-[#0e121b] hover:border-zinc-700'
                    }`}
                  >
                    <div className="text-xs font-semibold text-white">{node.title}</div>
                    <div className="text-[11px] text-zinc-400 mt-1">{node.role}</div>
                    <div className="mt-3 text-[10px] font-mono text-emerald-400 bg-emerald-950/60 px-2 py-0.5 rounded border border-emerald-800/60 inline-block">
                      {getNodeMetrics(id)}
                    </div>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Read Path & Metadata Section */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622]">
            <div className="flex items-center justify-between mb-4">
              <span className="text-xs font-semibold uppercase tracking-wider text-purple-400">
                3. Read Hierarchy & Manifest Metadata
              </span>
              <span className="text-[11px] font-mono text-zinc-500">Bloom Filter Avoidance</span>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
              {['bloom_filter', 'manifest'].map((id) => {
                const node = ARCH_NODES.find((n) => n.id === id)!;
                const isSelected = selectedNode.id === id;
                return (
                  <button
                    key={id}
                    onClick={() => setSelectedNode(node)}
                    className={`text-left p-4 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-purple-500 bg-purple-950/30 shadow-md shadow-purple-950/50'
                        : 'border-[#232a3e] bg-[#0e121b] hover:border-zinc-700'
                    }`}
                  >
                    <div className="text-xs font-semibold text-white">{node.title}</div>
                    <div className="text-[11px] text-zinc-400 mt-1">{node.role}</div>
                    <div className="mt-3 text-[10px] font-mono text-purple-400 bg-purple-950/60 px-2 py-0.5 rounded border border-purple-800/60 inline-block">
                      {getNodeMetrics(id)}
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        </div>

        {/* Selected Component Detail Drawer (1 Column) */}
        <div className="p-6 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between border-b border-[#232a3e] pb-4 mb-4">
              <div>
                <span className="text-[11px] uppercase font-mono tracking-wider text-zinc-400">Component Spec</span>
                <h3 className="text-lg font-bold text-white mt-1">{selectedNode.title}</h3>
                <span className="text-xs text-cyan-400">{selectedNode.role}</span>
              </div>
              <div className="p-2.5 rounded-lg bg-[#141824] border border-[#232a3e] text-cyan-400">
                <Info className="w-5 h-5" />
              </div>
            </div>

            {/* Source code file */}
            <div className="p-3 rounded-lg bg-[#141824] border border-[#232a3e] mb-4">
              <span className="text-[10px] text-zinc-500 uppercase font-mono block">C++20 Implementation File</span>
              <code className="text-xs font-mono text-emerald-400 font-semibold">{selectedNode.cppFile}</code>
            </div>

            {/* Description */}
            <div className="mb-5">
              <span className="text-[11px] text-zinc-400 uppercase font-mono block mb-1">Architecture Description</span>
              <p className="text-xs text-zinc-300 leading-relaxed">{selectedNode.description}</p>
            </div>

            {/* Key Mechanics */}
            <div>
              <span className="text-[11px] text-zinc-400 uppercase font-mono block mb-2">Key Guarantees & Mechanics</span>
              <ul className="space-y-2">
                {selectedNode.keyDetails.map((detail, idx) => (
                  <li key={idx} className="flex items-start gap-2 text-xs text-zinc-300">
                    <CheckCircle2 className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <span>{detail}</span>
                  </li>
                ))}
              </ul>
            </div>
          </div>

          {/* Live telemetry badge */}
          <div className="mt-6 pt-4 border-t border-[#232a3e]">
            <div className="text-[10px] text-zinc-500 uppercase font-mono">Current Live Metric</div>
            <div className="text-sm font-mono text-white font-bold mt-1">{getNodeMetrics(selectedNode.id)}</div>
          </div>
        </div>
      </div>
    </div>
  );
}
