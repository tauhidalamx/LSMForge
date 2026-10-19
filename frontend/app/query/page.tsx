'use client';

import React, { useState } from 'react';
import Link from 'next/link';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import {
  Terminal,
  Play,
  Copy,
  Check,
  Clock,
  History,
  Trash2,
  Database,
  ArrowRight,
  Sparkles,
  HelpCircle,
  FileText,
  Layers,
} from 'lucide-react';

type OperationType = 'GET' | 'PUT' | 'DELETE' | 'SCAN' | 'BATCH';

interface QueryHistoryItem {
  id: string;
  type: OperationType;
  key?: string;
  timestamp: number;
  durationUs: number;
  status: 'ok' | 'error';
  summary: string;
}

const PRESETS = [
  {
    name: '👤 Alice Profile (PUT)',
    type: 'PUT' as OperationType,
    key: 'user:1001',
    value: '{"name": "Alice Chen", "role": "Site Reliability Engineer", "status": "active"}',
    desc: 'Persists user profile directly to WAL and MemTable',
  },
  {
    name: '🔍 Find Alice (GET)',
    type: 'GET' as OperationType,
    key: 'user:1001',
    value: '',
    desc: 'Retrieves user:1001 with microsecond latency',
  },
  {
    name: '🛒 Order Item (PUT)',
    type: 'PUT' as OperationType,
    key: 'order:9042',
    value: '{"item": "NVMe Storage 2TB", "price": 149.99, "currency": "USD"}',
    desc: 'Inserts transaction record with CRC32C framing',
  },
  {
    name: '📑 Scan Users (SCAN)',
    type: 'SCAN' as OperationType,
    scanStart: 'user:',
    scanEnd: 'user:~',
    scanLimit: 25,
    desc: 'Lexicographical range scan across in-memory and on-disk tiers',
  },
  {
    name: '🗑️ Deletion Tombstone (DELETE)',
    type: 'DELETE' as OperationType,
    key: 'user:1001',
    value: '',
    desc: 'Appends a deletion tombstone preventing resurrected reads',
  },
];

export default function QueryConsolePage() {
  const [opType, setOpType] = useState<OperationType>('GET');
  const [key, setKey] = useState<string>('user:1001');
  const [value, setValue] = useState<string>('{"name": "Alice Chen", "role": "Site Reliability Engineer", "status": "active"}');
  const [scanStart, setScanStart] = useState<string>('user:');
  const [scanEnd, setScanEnd] = useState<string>('');
  const [scanLimit, setScanLimit] = useState<number>(20);
  const [batchPayload, setBatchPayload] = useState<string>(
    JSON.stringify(
      {
        sync: false,
        operations: [
          { type: 'put', key: 'user:1002', value: '{"name": "Bob", "dept": "Infra"}' },
          { type: 'put', key: 'user:1003', value: '{"name": "Charlie", "dept": "Security"}' },
          { type: 'delete', key: 'user:retired' },
        ],
      },
      null,
      2
    )
  );

  const [executing, setExecuting] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [durationUs, setDurationUs] = useState<number | null>(null);
  const [copied, setCopied] = useState(false);
  const [history, setHistory] = useState<QueryHistoryItem[]>([]);

  const handleApplyPreset = (p: typeof PRESETS[0]) => {
    setOpType(p.type);
    if (p.key) setKey(p.key);
    if (p.value) setValue(p.value);
    if (p.scanStart) setScanStart(p.scanStart);
    if (p.scanEnd) setScanEnd(p.scanEnd);
    if (p.scanLimit) setScanLimit(p.scanLimit);
  };

  const handleExecute = async () => {
    setExecuting(true);
    setResult(null);
    setDurationUs(null);

    const tStart = performance.now();
    try {
      let res: any;
      let dur = 0;

      if (opType === 'GET') {
        if (!key.trim()) throw new Error('Key cannot be empty');
        res = await api.getKey(key.trim());
        dur = res.latency_us;
      } else if (opType === 'PUT') {
        if (!key.trim()) throw new Error('Key cannot be empty');
        res = await api.putKey(key.trim(), value);
        dur = res.latency_us;
      } else if (opType === 'DELETE') {
        if (!key.trim()) throw new Error('Key cannot be empty');
        res = await api.deleteKey(key.trim());
        dur = res.latency_us;
      } else if (opType === 'SCAN') {
        res = await api.scan(scanStart, scanEnd, scanLimit);
        dur = res.latency_us;
      } else if (opType === 'BATCH') {
        const parsed = JSON.parse(batchPayload);
        res = await api.writeBatch(parsed.operations, parsed.sync || false);
        dur = res.latency_us;
      }

      setDurationUs(dur || Math.round((performance.now() - tStart) * 1000));
      setResult(res);

      setHistory((prev) => [
        {
          id: Math.random().toString(),
          type: opType,
          key: opType !== 'BATCH' ? key : 'batch',
          timestamp: Date.now(),
          durationUs: dur,
          status: 'ok',
          summary: opType === 'SCAN' ? `${res.count} items found` : 'Success',
        },
        ...prev.slice(0, 19),
      ]);
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : 'Execution error';
      setResult({ error: msg });
      setDurationUs(Math.round((performance.now() - tStart) * 1000));
      setHistory((prev) => [
        {
          id: Math.random().toString(),
          type: opType,
          key: opType !== 'BATCH' ? key : 'batch',
          timestamp: Date.now(),
          durationUs: Math.round((performance.now() - tStart) * 1000),
          status: 'error',
          summary: msg,
        },
        ...prev.slice(0, 19),
      ]);
    } finally {
      setExecuting(false);
    }
  };

  const handleCopy = () => {
    if (!result) return;
    navigator.clipboard.writeText(JSON.stringify(result, null, 2));
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Interactive Query Console"
        description="Point lookups, atomic batch writes, deletion tombstones, and lexicographical range scans"
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Humanized Guided Presets Bar */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#10131c]">
          <div className="flex items-center gap-2 mb-2 text-xs font-semibold text-white">
            <Sparkles className="w-4 h-4 text-cyan-400" />
            <span>Humanized Quick Presets (Click to Load)</span>
          </div>
          <div className="flex flex-wrap gap-2">
            {PRESETS.map((p, idx) => (
              <button
                key={idx}
                onClick={() => handleApplyPreset(p)}
                className="px-3 py-1.5 rounded-lg bg-[#161c28] hover:bg-[#1f2738] border border-[#2b354d] hover:border-cyan-500/50 text-xs font-mono text-zinc-300 hover:text-white transition-all text-left"
                title={p.desc}
              >
                <span>{p.name}</span>
              </button>
            ))}
          </div>
        </div>

        {/* 2 Column Layout: Form & Results */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Left 2 Cols: Form Input & Response Console */}
          <div className="lg:col-span-2 space-y-6">
            {/* Operation Selector & Inputs */}
            <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
              <div className="flex flex-wrap items-center justify-between gap-3 border-b border-[#232a3e] pb-3">
                <div className="flex items-center gap-1.5 p-1 rounded-lg bg-[#0b0e15] border border-[#232a3e]">
                  {(['GET', 'PUT', 'DELETE', 'SCAN', 'BATCH'] as OperationType[]).map((t) => (
                    <button
                      key={t}
                      onClick={() => setOpType(t)}
                      className={`px-3 py-1.5 rounded-md text-xs font-mono font-bold transition-all ${
                        opType === t
                          ? t === 'DELETE'
                            ? 'bg-rose-950 text-rose-400 border border-rose-800'
                            : 'bg-cyan-600/20 text-cyan-400 border border-cyan-500/30'
                          : 'text-zinc-400 hover:text-white'
                      }`}
                    >
                      {t}
                    </button>
                  ))}
                </div>

                <button
                  onClick={handleExecute}
                  disabled={executing}
                  className="px-5 py-2 rounded-lg bg-cyan-600 hover:bg-cyan-500 text-white font-medium text-xs font-mono flex items-center gap-2 transition-all shadow-md shadow-cyan-900/40 disabled:opacity-50"
                >
                  <Play className={`w-4 h-4 ${executing ? 'animate-spin' : ''}`} />
                  <span>{executing ? 'Executing...' : `Execute ${opType}`}</span>
                </button>
              </div>

              {/* Dynamic Inputs Based on opType */}
              {opType !== 'SCAN' && opType !== 'BATCH' && (
                <div className="space-y-3 font-mono text-xs">
                  <div>
                    <div className="flex justify-between items-center mb-1">
                      <label className="text-zinc-400">Key (UserKey String):</label>
                      <span className="text-[10px] text-zinc-500">Binary-safe key identifier</span>
                    </div>
                    <input
                      type="text"
                      value={key}
                      onChange={(e) => setKey(e.target.value)}
                      placeholder="Enter key (e.g. user:1001)..."
                      className="w-full px-3 py-2 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white focus:outline-none focus:border-cyan-500 font-semibold"
                    />
                  </div>

                  {opType === 'PUT' && (
                    <div>
                      <div className="flex justify-between items-center mb-1">
                        <label className="text-zinc-400">Value Payload (String or JSON):</label>
                        <span className="text-[10px] text-zinc-500">{value.length} bytes</span>
                      </div>
                      <textarea
                        rows={4}
                        value={value}
                        onChange={(e) => setValue(e.target.value)}
                        placeholder="Enter value payload..."
                        className="w-full p-3 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white font-mono focus:outline-none focus:border-cyan-500"
                      />
                    </div>
                  )}

                  {opType === 'DELETE' && (
                    <div className="p-3 rounded-lg bg-rose-950/30 border border-rose-900/60 text-[11px] text-rose-300">
                      ℹ️ Executing DELETE appends an immutable deletion tombstone with a new sequence number.
                      Previous values remain safely suppressed without resurrecting.
                    </div>
                  )}
                </div>
              )}

              {opType === 'SCAN' && (
                <div className="grid grid-cols-1 sm:grid-cols-3 gap-3 font-mono text-xs">
                  <div>
                    <label className="text-zinc-400 block mb-1">Start Key (Inclusive):</label>
                    <input
                      type="text"
                      value={scanStart}
                      onChange={(e) => setScanStart(e.target.value)}
                      placeholder="e.g. user:1000"
                      className="w-full px-3 py-2 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white focus:outline-none focus:border-cyan-500"
                    />
                  </div>
                  <div>
                    <label className="text-zinc-400 block mb-1">End Key (Optional):</label>
                    <input
                      type="text"
                      value={scanEnd}
                      onChange={(e) => setScanEnd(e.target.value)}
                      placeholder="e.g. user:9999"
                      className="w-full px-3 py-2 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white focus:outline-none focus:border-cyan-500"
                    />
                  </div>
                  <div>
                    <label className="text-zinc-400 block mb-1">Limit (Max 1000):</label>
                    <input
                      type="number"
                      value={scanLimit}
                      onChange={(e) => setScanLimit(parseInt(e.target.value) || 20)}
                      className="w-full px-3 py-2 bg-[#0e121b] border border-[#232a3e] rounded-lg text-white focus:outline-none focus:border-cyan-500"
                    />
                  </div>
                </div>
              )}

              {opType === 'BATCH' && (
                <div className="font-mono text-xs">
                  <div className="flex justify-between items-center mb-1">
                    <label className="text-zinc-400">Atomic WriteBatch Specification (JSON):</label>
                    <span className="text-[10px] text-emerald-400 font-semibold">Atomic All-or-Nothing</span>
                  </div>
                  <textarea
                    rows={7}
                    value={batchPayload}
                    onChange={(e) => setBatchPayload(e.target.value)}
                    className="w-full p-3 bg-[#0e121b] border border-[#232a3e] rounded-lg text-emerald-300 font-mono focus:outline-none focus:border-cyan-500"
                  />
                </div>
              )}
            </div>

            {/* Result Console Display */}
            <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between mb-3 border-b border-[#232a3e] pb-3">
                  <div className="flex items-center gap-2">
                    <Terminal className="w-4 h-4 text-cyan-400" />
                    <span className="text-xs font-semibold text-white">Engine Response Output</span>
                    {durationUs !== null && (
                      <span className="text-[11px] font-mono text-cyan-400 bg-cyan-950/60 px-2 py-0.5 rounded border border-cyan-800">
                        {durationUs} μs latency
                      </span>
                    )}
                  </div>

                  {result && (
                    <button
                      onClick={handleCopy}
                      className="flex items-center gap-1 text-xs font-mono text-zinc-400 hover:text-white px-2 py-1 rounded bg-[#1e2536] hover:bg-[#28324a]"
                    >
                      {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
                      <span>{copied ? 'Copied' : 'Copy'}</span>
                    </button>
                  )}
                </div>

                <pre className="p-4 rounded-lg bg-[#0b0e15] border border-[#1e2536] font-mono text-xs text-zinc-200 overflow-x-auto min-h-[160px] max-h-[380px]">
                  {result ? (
                    JSON.stringify(result, null, 2)
                  ) : (
                    <span className="text-zinc-600">// Ready. Select a preset or press &apos;Execute {opType}&apos; to run against database.</span>
                  )}
                </pre>
              </div>

              {/* Cross-page routing shortcuts */}
              {result && !result.error && (
                <div className="mt-4 pt-3 border-t border-[#232a3e] flex items-center justify-between text-[11px] font-mono">
                  <span className="text-emerald-400">✓ Persisted to engine state</span>
                  <div className="flex items-center gap-3">
                    <Link href="/memtable" className="text-cyan-400 hover:underline flex items-center gap-1">
                      <Layers className="w-3 h-3" />
                      <span>Inspect in MemTable</span>
                    </Link>
                    <Link href="/wal" className="text-amber-400 hover:underline flex items-center gap-1">
                      <FileText className="w-3 h-3" />
                      <span>Inspect WAL</span>
                    </Link>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Right Col: Query History */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between border-b border-[#232a3e] pb-3 mb-4">
                <div className="flex items-center gap-2 text-xs font-semibold text-white">
                  <History className="w-4 h-4 text-zinc-400" />
                  <span>Console History</span>
                </div>
                {history.length > 0 && (
                  <button
                    onClick={() => setHistory([])}
                    className="text-[10px] text-zinc-500 hover:text-rose-400 font-mono flex items-center gap-1"
                  >
                    <Trash2 className="w-3 h-3" /> Clear
                  </button>
                )}
              </div>

              <div className="space-y-2 overflow-y-auto max-h-[580px]">
                {history.length > 0 ? (
                  history.map((item) => (
                    <div
                      key={item.id}
                      className="p-3 rounded-lg bg-[#0e121b] border border-[#232a3e] font-mono text-xs space-y-1"
                    >
                      <div className="flex items-center justify-between">
                        <span
                          className={`text-[10px] font-bold px-1.5 py-0.2 rounded ${
                            item.type === 'DELETE'
                              ? 'bg-rose-950 text-rose-400 border border-rose-800'
                              : 'bg-cyan-950 text-cyan-400 border border-cyan-800'
                          }`}
                        >
                          {item.type}
                        </span>
                        <span className="text-[11px] text-zinc-400">{item.durationUs} μs</span>
                      </div>
                      <div className="text-zinc-200 truncate font-semibold">{item.key}</div>
                      <div className="text-[11px] text-zinc-500 truncate">{item.summary}</div>
                    </div>
                  ))
                ) : (
                  <div className="py-16 text-center text-xs text-zinc-600 font-mono">
                    No operations recorded yet.
                  </div>
                )}
              </div>
            </div>

            <div className="pt-4 border-t border-[#232a3e] text-[11px] font-mono text-zinc-500">
              Atomic batch guarantee active
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
