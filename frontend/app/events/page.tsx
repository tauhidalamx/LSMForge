'use client';

import React, { useState, useEffect } from 'react';
import { Header } from '@/components/Header';
import { api, getApiBaseUrl } from '@/lib/api';
import { EngineEvent } from '@/types';
import { formatTimeAgo } from '@/lib/utils';
import {
  Bell,
  Search,
  Filter,
  CheckCircle2,
  AlertTriangle,
  AlertCircle,
  Clock,
  Play,
  Pause,
  Trash2,
  Radio,
} from 'lucide-react';

export default function EventsPage() {
  const [events, setEvents] = useState<EngineEvent[]>([]);
  const [filterSeverity, setFilterSeverity] = useState<string>('all');
  const [search, setSearch] = useState<string>('');
  const [isPaused, setIsPaused] = useState<boolean>(false);
  const [loading, setLoading] = useState<boolean>(true);
  const [sseConnected, setSseConnected] = useState<boolean>(false);

  const fetchEvents = async () => {
    if (isPaused) return;
    try {
      const res = await api.getEvents();
      setEvents(res.events.reverse());
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchEvents();

    // Setup Server-Sent Events (SSE) listener
    let evtSource: EventSource | null = null;
    try {
      evtSource = new EventSource(`${getApiBaseUrl()}/api/v1/events/stream`);
      evtSource.onopen = () => setSseConnected(true);
      evtSource.onerror = () => setSseConnected(false);
      evtSource.addEventListener('heartbeat', () => {
        // SSE heartbeat confirms live socket
        setSseConnected(true);
      });
    } catch {
      setSseConnected(false);
    }

    const interval = setInterval(fetchEvents, 2500);
    return () => {
      clearInterval(interval);
      if (evtSource) evtSource.close();
    };
  }, [isPaused]);

  const filteredEvents = events.filter((ev) => {
    if (filterSeverity !== 'all' && ev.severity !== filterSeverity) return false;
    if (search && !ev.message.toLowerCase().includes(search.toLowerCase()) && !ev.type.toLowerCase().includes(search.toLowerCase()))
      return false;
    return true;
  });

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <Header
        title="Engine Logs & Events"
        description="Chronological event journal for WAL rotations, MemTable flushes, compactions, and diagnostic warnings"
        onRefresh={fetchEvents}
        isRefreshing={loading}
      />

      <div className="flex-1 p-6 space-y-6 overflow-y-auto">
        {/* Controls Bar */}
        <div className="flex flex-wrap items-center justify-between gap-4 p-4 rounded-xl border border-[#232a3e] bg-[#121622]">
          <div className="flex items-center gap-2">
            <div className="flex items-center gap-1.5 p-1 rounded-lg bg-[#0b0e15] border border-[#232a3e]">
              {['all', 'info', 'warn', 'error'].map((s) => (
                <button
                  key={s}
                  onClick={() => setFilterSeverity(s)}
                  className={`px-3 py-1 rounded-md text-xs font-mono uppercase tracking-wider transition-all ${
                    filterSeverity === s
                      ? s === 'error'
                        ? 'bg-rose-950 text-rose-400 border border-rose-800'
                        : 'bg-cyan-600/20 text-cyan-400 border border-cyan-500/30'
                      : 'text-zinc-400 hover:text-white'
                  }`}
                >
                  {s}
                </button>
              ))}
            </div>

            <button
              onClick={() => setIsPaused(!isPaused)}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg border text-xs font-mono transition-all ${
                isPaused
                  ? 'bg-amber-950/60 border-amber-800 text-amber-300'
                  : 'bg-[#181d2a] border-[#2b354d] text-zinc-300 hover:text-white'
              }`}
            >
              {isPaused ? <Play className="w-3.5 h-3.5 text-amber-400" /> : <Pause className="w-3.5 h-3.5 text-zinc-400" />}
              <span>{isPaused ? 'Resume Feed' : 'Pause'}</span>
            </button>
          </div>

          <div className="flex items-center gap-4">
            <div className="flex items-center gap-2 text-xs font-mono">
              <Radio className={`w-3.5 h-3.5 ${sseConnected ? 'text-emerald-400 animate-pulse' : 'text-zinc-600'}`} />
              <span className={sseConnected ? 'text-emerald-400' : 'text-zinc-500'}>
                {sseConnected ? 'SSE Live Stream Active' : 'Polling (2.5s)'}
              </span>
            </div>

            <div className="relative">
              <Search className="w-4 h-4 text-zinc-500 absolute left-3 top-2" />
              <input
                type="text"
                placeholder="Search log events..."
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                className="pl-9 pr-3 py-1 bg-[#0b0e15] border border-[#232a3e] rounded-lg text-xs font-mono text-white placeholder-zinc-500 focus:outline-none focus:border-cyan-500 w-56"
              />
            </div>
          </div>
        </div>

        {/* Events Table Container */}
        <div className="rounded-xl border border-[#232a3e] bg-[#121622] overflow-hidden">
          <table className="w-full text-left text-xs font-mono">
            <thead className="bg-[#0b0e15] border-b border-[#232a3e] text-zinc-400">
              <tr>
                <th className="py-3 px-4 w-28">Severity</th>
                <th className="py-3 px-4 w-32">Component</th>
                <th className="py-3 px-4 w-36">Event Type</th>
                <th className="py-3 px-4">Message</th>
                <th className="py-3 px-4 w-32 text-right">Time</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1e2536]">
              {filteredEvents.length > 0 ? (
                filteredEvents.map((ev) => (
                  <tr key={ev.id} className="hover:bg-[#161c28] transition-colors">
                    <td className="py-2.5 px-4">
                      <span
                        className={`inline-flex items-center gap-1 text-[10px] uppercase font-bold px-2 py-0.5 rounded border ${
                          ev.severity === 'error'
                            ? 'bg-rose-950/80 text-rose-400 border-rose-800'
                            : ev.severity === 'warn'
                            ? 'bg-amber-950/80 text-amber-400 border-amber-800'
                            : 'bg-cyan-950/80 text-cyan-400 border-cyan-800'
                        }`}
                      >
                        {ev.severity}
                      </span>
                    </td>
                    <td className="py-2.5 px-4 text-zinc-300 font-semibold">{ev.component}</td>
                    <td className="py-2.5 px-4 text-cyan-300">{ev.type}</td>
                    <td className="py-2.5 px-4 text-zinc-200">{ev.message}</td>
                    <td className="py-2.5 px-4 text-right text-zinc-500">
                      {formatTimeAgo(ev.timestamp)}
                    </td>
                  </tr>
                ))
              ) : (
                <tr>
                  <td colSpan={5} className="py-12 text-center text-zinc-500">
                    No matching events logged yet. Operations will stream here in real time.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
