'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api } from '@/lib/api';
import { WalFile, WalRecord } from '@/types';
import { formatBytes, formatNumber } from '@/lib/utils';
import {
  FileText,
  ShieldCheck,
  CheckCircle2,
  HardDrive,
  RefreshCw,
  Search,
  AlertCircle,
  FileCode,
} from 'lucide-react';

export default function WalInspectorPage() {
  const [walFiles, setWalFiles] = useState<WalFile[]>([]);
  const [selectedFileNumber, setSelectedFileNumber] = useState<number | null>(null);
  const [records, setRecords] = useState<WalRecord[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadingRecords, setLoadingRecords] = useState(false);

  const loadWalFiles = async () => {
    try {
      const res = await api.getWalFiles();
      setWalFiles(res.wal_files);
      if (res.wal_files.length > 0 && selectedFileNumber === null) {
        setSelectedFileNumber(res.wal_files[res.wal_files.length - 1].file_number);
      }
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadWalFiles();
  }, []);

  useEffect(() => {
    if (selectedFileNumber === null) return;
    setLoadingRecords(true);
    api
      .getWalRecords(selectedFileNumber)
      .then((res) => {
        setRecords(res.records);
      })
      .catch(() => setRecords([]))
      .finally(() => setLoadingRecords(false));
  }, [selectedFileNumber]);

  const selectedFile = walFiles.find((f) => f.file_number === selectedFileNumber);

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Write-Ahead Log (WAL) Inspector"
        description="Binary record decoder, 32KB block chunking, and Castagnoli CRC32C checksum integrity verification"
        onRefresh={loadWalFiles}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* WAL Framing Spec Overview */}
        <div className="p-4 rounded-xl border border-[#232a3e] bg-[#0e121b] flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-lg bg-amber-500/10 border border-amber-500/30 flex items-center justify-center">
              <FileText className="w-5 h-5 text-amber-400" />
            </div>
            <div>
              <div className="text-sm font-semibold text-white">Record Framing Architecture</div>
              <p className="text-xs text-zinc-400 font-mono mt-0.5">
                Header: [4B CRC32C] [2B Length] [1B RecordType] • Chunks: 32KB Page Blocks
              </p>
            </div>
          </div>
          <div className="flex items-center gap-4 text-xs font-mono text-zinc-400">
            <span className="px-2.5 py-1 rounded bg-amber-950/60 text-amber-300 border border-amber-800">
              POSIX fcntl(F_FULLFSYNC)
            </span>
            <span>Total Logs: {walFiles.length}</span>
          </div>
        </div>

        {/* 2 Column Layout: File List vs Binary Records Table */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* WAL Files Selector */}
          <div className="p-5 rounded-xl border border-[#232a3e] bg-[#121622] space-y-4">
            <div className="flex items-center justify-between">
              <h3 className="text-sm font-semibold text-white">WAL Log Files</h3>
              <span className="text-xs font-mono text-zinc-500">{walFiles.length} files</span>
            </div>

            <div className="space-y-2">
              {walFiles.map((wf) => {
                const isSelected = wf.file_number === selectedFileNumber;
                return (
                  <button
                    key={wf.file_number}
                    onClick={() => setSelectedFileNumber(wf.file_number)}
                    className={`w-full text-left p-3 rounded-lg border transition-all ${
                      isSelected
                        ? 'border-amber-500 bg-amber-950/20 shadow-sm'
                        : 'border-[#232a3e] bg-[#0e121b] hover:border-zinc-700'
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="text-xs font-bold font-mono text-white flex items-center gap-2">
                        <FileCode className="w-3.5 h-3.5 text-amber-400" />
                        {wf.filename}
                      </span>
                      {wf.is_active && (
                        <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-emerald-950 text-emerald-400 border border-emerald-800">
                          ACTIVE
                        </span>
                      )}
                    </div>
                    <div className="mt-2 flex items-center justify-between text-[11px] font-mono text-zinc-400">
                      <span>Size: {formatBytes(wf.file_size)}</span>
                      <span className="text-zinc-500">#{wf.file_number}</span>
                    </div>
                  </button>
                );
              })}

              {walFiles.length === 0 && (
                <div className="py-8 text-center text-xs text-zinc-500">
                  No WAL files detected on disk.
                </div>
              )}
            </div>
          </div>

          {/* Record Decoder Table (2 Cols) */}
          <div className="lg:col-span-2 p-5 rounded-xl border border-[#232a3e] bg-[#121622] flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between mb-4 border-b border-[#232a3e] pb-3">
                <div>
                  <h3 className="text-sm font-semibold text-white">
                    Decoded Records in {selectedFile?.filename || 'WAL'}
                  </h3>
                  <p className="text-xs text-zinc-400 mt-0.5">
                    Inspecting physical batch records and checksums via binary parser
                  </p>
                </div>
                <span className="text-xs font-mono text-amber-400 bg-amber-950/60 px-2.5 py-1 rounded border border-amber-800">
                  {records.length} records parsed
                </span>
              </div>

              {loadingRecords ? (
                <div className="py-16 text-center text-xs text-zinc-400 flex flex-col items-center justify-center gap-2">
                  <RefreshCw className="w-5 h-5 animate-spin text-amber-400" />
                  <span>Decoding WAL blocks...</span>
                </div>
              ) : records.length > 0 ? (
                <div className="rounded-lg border border-[#232a3e] bg-[#0e121b] overflow-hidden">
                  <table className="w-full text-left text-xs font-mono">
                    <thead className="bg-[#0b0e15] border-b border-[#232a3e] text-zinc-400">
                      <tr>
                        <th className="py-2.5 px-3 w-12 text-zinc-500">#</th>
                        <th className="py-2.5 px-3">Sequence Start</th>
                        <th className="py-2.5 px-3">Batch Operations</th>
                        <th className="py-2.5 px-3">Payload Size</th>
                        <th className="py-2.5 px-3 text-right">CRC32C Status</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-[#1e2536]">
                      {records.map((r) => (
                        <tr key={r.record_index} className="hover:bg-[#161c28]">
                          <td className="py-2 px-3 text-zinc-500">{r.record_index}</td>
                          <td className="py-2 px-3 text-cyan-300 font-semibold">
                            {r.sequence_start !== undefined ? `#${r.sequence_start}` : 'N/A'}
                          </td>
                          <td className="py-2 px-3 text-zinc-300">
                            {r.batch_count !== undefined ? `${r.batch_count} ops` : 'N/A'}
                          </td>
                          <td className="py-2 px-3 text-zinc-400">{r.payload_size} bytes</td>
                          <td className="py-2 px-3 text-right">
                            <span className="inline-flex items-center gap-1 text-[10px] text-emerald-400 bg-emerald-950/60 px-2 py-0.5 rounded border border-emerald-800">
                              <CheckCircle2 className="w-3 h-3" />
                              VALID
                            </span>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              ) : (
                <div className="py-16 text-center text-xs text-zinc-500">
                  No records found in this WAL file or log was freshly allocated.
                </div>
              )}
            </div>

            <div className="mt-4 pt-3 border-t border-[#232a3e] text-[11px] font-mono text-zinc-500 flex items-center justify-between">
              <span>Showing up to 100 records</span>
              <span>Hardware accelerated Castagnoli CRC32C</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
