import {
  ApiResponse,
  BenchmarkJob,
  CompactionStats,
  EngineConfig,
  EngineEvent,
  EngineStats,
  EngineVersion,
  IntegrityStatus,
  LevelInfo,
  MemTableData,
  ScanResult,
  SSTableItem,
  SystemHealth,
  WalFile,
  WalRecord,
} from '@/types';

export const DEFAULT_API_BASE = 'http://localhost:8080';

export function getApiBaseUrl(): string {
  if (typeof window !== 'undefined') {
    const saved = localStorage.getItem('lsmforge_api_base');
    if (saved) return saved;
  }
  return process.env.NEXT_PUBLIC_API_URL || DEFAULT_API_BASE;
}

export function setApiBaseUrl(url: string) {
  if (typeof window !== 'undefined') {
    localStorage.setItem('lsmforge_api_base', url.replace(/\/+$/, ''));
  }
}

async function request<T>(endpoint: string, options: RequestInit = {}): Promise<T> {
  const base = getApiBaseUrl();
  const url = `${base}${endpoint}`;
  
  const headers = new Headers(options.headers || {});
  if (!headers.has('Accept')) {
    headers.set('Accept', 'application/json');
  }
  if (options.body && typeof options.body === 'string' && !headers.has('Content-Type')) {
    headers.set('Content-Type', 'application/json');
  }

  const res = await fetch(url, {
    ...options,
    headers,
    cache: 'no-store',
  });

  const json: ApiResponse<T> = await res.json();
  if (!res.ok || json.status === 'error') {
    const msg = json.error?.message || `Request failed with status ${res.status}`;
    throw new Error(msg);
  }

  return json.data as T;
}

export const api = {
  // Health & System
  getHealth: () => request<SystemHealth>('/api/v1/health'),
  getVersion: () => request<EngineVersion>('/api/v1/version'),
  getConfig: () => request<EngineConfig>('/api/v1/config'),
  getStats: () => request<EngineStats>('/api/v1/stats'),

  // Key-Value Operations
  getKey: (key: string) => request<{ key: string; value: string; found: boolean; latency_us: number }>(`/api/v1/keys/${encodeURIComponent(key)}`),
  putKey: (key: string, value: string) =>
    request<{ key: string; value_size: number; written: boolean; latency_us: number }>(`/api/v1/keys/${encodeURIComponent(key)}`, {
      method: 'PUT',
      body: JSON.stringify({ value }),
    }),
  deleteKey: (key: string) =>
    request<{ key: string; deleted: boolean; latency_us: number }>(`/api/v1/keys/${encodeURIComponent(key)}`, {
      method: 'DELETE',
    }),
  scan: (start?: string, end?: string, limit = 50) => {
    const params = new URLSearchParams();
    if (start) params.set('start', start);
    if (end) params.set('end', end);
    if (limit) params.set('limit', limit.toString());
    return request<ScanResult>(`/api/v1/scan?${params.toString()}`);
  },
  writeBatch: (operations: Array<{ type: 'put' | 'delete'; key: string; value?: string }>, sync = false) =>
    request<{ applied_count: number; latency_us: number }>('/api/v1/batches', {
      method: 'POST',
      body: JSON.stringify({ operations, sync }),
    }),

  // MemTables & Flushing
  getMemTables: () => request<MemTableData>('/api/v1/memtables'),
  flushMemTable: () => request<{ flushed: boolean; duration_ms: number }>('/api/v1/flush', { method: 'POST' }),
  getFlushes: () => request<{ total_flushes: number; total_duration_us: number; avg_duration_us: number }>('/api/v1/flushes'),

  // WAL
  getWalFiles: () => request<{ wal_files: WalFile[]; count: number }>('/api/v1/wal'),
  getWalRecords: (fileNumber: number) =>
    request<{ file_number: number; records: WalRecord[]; count: number }>(`/api/v1/wal/${fileNumber}/records`),

  // SSTables & Levels
  getSSTables: () => request<{ sstables: SSTableItem[]; count: number }>('/api/v1/sstables'),
  getLevels: () => request<{ levels: LevelInfo[]; total_storage_bytes: number }>('/api/v1/levels'),

  // Compaction
  triggerCompaction: () => request<{ compacted: boolean; duration_ms: number }>('/api/v1/compactions', { method: 'POST' }),
  getCompactionStats: () => request<CompactionStats>('/api/v1/compactions/stats'),

  // Recovery & Integrity
  getIntegrity: () => request<IntegrityStatus>('/api/v1/integrity'),
  getEvents: () => request<{ events: EngineEvent[]; count: number }>('/api/v1/events'),

  // Benchmarking
  dispatchBenchmark: (params: {
    workload: string;
    num_operations: number;
    value_size: number;
    read_ratio: number;
    sync: boolean;
  }) =>
    request<{ job_id: string; state: string }>('/api/v1/benchmarks', {
      method: 'POST',
      body: JSON.stringify(params),
    }),
  getBenchmarks: () => request<{ benchmarks: BenchmarkJob[]; count: number }>('/api/v1/benchmarks'),
  getBenchmark: (id: string) => request<BenchmarkJob>(`/api/v1/benchmarks/${id}`),
};
