export interface ApiResponse<T> {
  status: 'ok' | 'error';
  data?: T;
  error?: {
    code: string;
    message: string;
  };
  request_id?: string;
  timestamp?: number;
}

export interface SystemHealth {
  engine: string;
  version: string;
  uptime_seconds: number;
  database_open: boolean;
  database_name: string;
}

export interface EngineVersion {
  engine: string;
  version: string;
  cxx_standard: string;
  compiler: string;
  platform: string;
  capabilities: string[];
}

export interface EngineConfig {
  write_buffer_size: number;
  max_open_files: number;
  block_size: number;
  block_restart_interval: number;
  bloom_bits_per_key: number;
  paranoid_checks: boolean;
  max_background_jobs: number;
  level0_file_num_compaction_trigger: number;
  level0_slowdown_writes_trigger: number;
  level0_stop_writes_trigger: number;
  wal_sync_policy: string;
}

export interface EngineStats {
  operations: {
    puts: number;
    gets: number;
    deletes: number;
    scans: number;
  };
  storage: {
    wal_bytes_written: number;
    memtable_bytes: number;
    memtable_threshold: number;
    immutable_memtables: number;
    active_sstables: number;
    total_sstable_bytes: number;
  };
  bloom_filter: {
    useful_checks: number;
    positive_checks: number;
  };
  compaction: {
    flushes: number;
    flush_duration_us: number;
    compactions: number;
    compaction_duration_us: number;
    bytes_read: number;
    bytes_written: number;
  };
  errors: {
    write_errors: number;
    read_errors: number;
  };
}

export interface KeyRecord {
  key: string;
  value: string;
}

export interface ScanResult {
  items: KeyRecord[];
  count: number;
  has_more: boolean;
  latency_us: number;
}

export interface MemTableData {
  active: {
    memory_bytes: number;
    threshold_bytes: number;
    usage_percent: number;
    data_structure: string;
  };
  immutable: {
    count: number;
    flushing: boolean;
  };
}

export interface WalFile {
  file_number: number;
  filename: string;
  file_size: number;
  is_active: boolean;
}

export interface WalRecord {
  record_index: number;
  sequence_start?: number;
  batch_count?: number;
  payload_size: number;
  checksum_valid: boolean;
}

export interface SSTableItem {
  file_number: number;
  filename: string;
  level: number;
  file_size: number;
  smallest_key: string;
  largest_key: string;
}

export interface LevelInfo {
  level: number;
  num_files: number;
  total_bytes: number;
  is_l0: boolean;
}

export interface CompactionStats {
  compactions_run: number;
  total_duration_us: number;
  bytes_read: number;
  bytes_written: number;
  write_amplification: number;
}

export interface IntegrityStatus {
  status: string;
  current_file_present: boolean;
  lock_file_active: boolean;
  registered_sstables: number;
  verified_sstables: number;
  integrity_pass: boolean;
}

export interface EngineEvent {
  id: string;
  type: string;
  message: string;
  component: string;
  severity: 'info' | 'warn' | 'error';
  timestamp: number;
}

export interface BenchmarkJob {
  id: string;
  workload: string;
  state: 'queued' | 'running' | 'completed' | 'failed';
  num_operations: number;
  value_size?: number;
  read_ratio?: number;
  ops_per_sec: number;
  mb_per_sec?: number;
  p50_us: number;
  p90_us?: number;
  p95_us: number;
  p99_us: number;
  duration_sec: number;
  progress_percent: number;
  latency_samples_us?: number[];
  started_at?: number;
  completed_at?: number;
}
