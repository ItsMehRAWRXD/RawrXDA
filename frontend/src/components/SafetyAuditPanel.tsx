/**
 * SafetyAuditPanel.tsx
 * Day 7: Real-time display of SafetyInterceptor violation events.
 *
 * Subscribes to the safetyInterceptor singleton's violation stream.
 * In PASSTHROUGH mode violations are shown with a "would-redact" label so
 * the operator can verify rule coverage without affecting the user's output.
 */

import React, { useEffect, useRef, useState } from 'react';
import {
  safetyInterceptor,
  SafetyInterceptor,
  Violation,
  InterceptMode,
} from '../safety/SafetyInterceptor';

const MAX_LOG_ENTRIES = 50;

const SEVERITY_CLASS: Record<string, string> = {
  LOW: 'safety-sev-low',
  MEDIUM: 'safety-sev-medium',
  HIGH: 'safety-sev-high',
  CRITICAL: 'safety-sev-critical',
};

const TYPE_CLASS: Record<string, string> = {
  SECRET_LEAK: 'safety-type-secret',
  PII: 'safety-type-pii',
  RESTRICTED: 'safety-type-restricted',
};

interface BenchmarkRow {
  mode: 'BASELINE' | InterceptMode;
  elapsedMs: number;
  processedTokens: number;
  tokensPerSec: number;
  violationCount: number;
  blockedCount: number;
  overheadPct: number;
}

interface BenchmarkExport {
  schemaVersion: 'day8.safety-benchmark.v1';
  generatedAtEpochMs: number;
  loops: number;
  rows: BenchmarkRow[];
}

const BENCH_LOOPS = 8;

const buildBenchmarkTokens = (): string[] => {
  // Includes split secrets and restricted patterns across token boundaries.
  const corpus = [
    'plain text ',
    'normal code ',
    'email: ',
    'alice@example.com ',
    'start key ',
    'sk-',
    '12345678901234567890 ',
    'aws ',
    'AKIA',
    'ABCD1234EFGH5678 ',
    'github ',
    'ghp_',
    '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ',
    'danger ',
    'os.',
    'system(',
    '"echo hi") ',
    'tail ',
  ];

  const out: string[] = [];
  for (let i = 0; i < 1200; i += 1) {
    for (const token of corpus) {
      out.push(token);
    }
  }
  return out;
};

const nowMs = (): number => {
  if (typeof performance !== 'undefined' && typeof performance.now === 'function') {
    return performance.now();
  }
  return Date.now();
};

export const SafetyAuditPanel: React.FC = () => {
  const [violations, setViolations] = useState<Violation[]>([]);
  const [mode, setMode] = useState<InterceptMode>(() => safetyInterceptor.getMode());
  const [benchmarkRows, setBenchmarkRows] = useState<BenchmarkRow[]>([]);
  const [isBenchmarking, setIsBenchmarking] = useState(false);
  const mountedRef = useRef(true);

  useEffect(() => {
    const unsubscribe = safetyInterceptor.onViolation((v) => {
      if (!mountedRef.current) return;
      setViolations((prev) => [v, ...prev].slice(0, MAX_LOG_ENTRIES));
    });

    return () => {
      mountedRef.current = false;
      unsubscribe();
    };
  }, []);

  const handleModeChange = (next: InterceptMode) => {
    safetyInterceptor.setMode(next);
    setMode(next);
  };

  // Aggregate counts per type for the summary chips.
  const counts = violations.reduce<Record<string, number>>((acc, v) => {
    acc[v.type] = (acc[v.type] ?? 0) + 1;
    return acc;
  }, {});

  const isPassthrough = mode === 'PASSTHROUGH';

  const runBenchmark = () => {
    setIsBenchmarking(true);

    // Yield first so the button state updates before CPU-heavy loop work.
    setTimeout(() => {
      const tokens = buildBenchmarkTokens();
      const baselineTokenCount = BENCH_LOOPS * tokens.length;

      let sink = '';
      const baselineStart = nowMs();
      for (let loop = 0; loop < BENCH_LOOPS; loop += 1) {
        for (const token of tokens) {
          sink += token;
        }
      }
      // Prevent engines from aggressively dead-code eliminating the loop.
      if (sink.length === Number.MIN_SAFE_INTEGER) {
        console.log('unreachable');
      }
      const baselineElapsed = Math.max(0.001, nowMs() - baselineStart);
      const baselineNsPerToken = (baselineElapsed * 1_000_000) / baselineTokenCount;

      const runMode = (benchMode: InterceptMode): BenchmarkRow => {
        const interceptor = new SafetyInterceptor(benchMode);
        let processedTokens = 0;
        let violationCount = 0;
        let blockedCount = 0;

        const start = nowMs();
        for (let loop = 0; loop < BENCH_LOOPS; loop += 1) {
          interceptor.reset();
          for (const token of tokens) {
            const result = interceptor.process(token);
            processedTokens += 1;
            if (result.violation) {
              violationCount += 1;
            }
            if (result.token.startsWith('[STREAM_BLOCKED:')) {
              blockedCount += 1;
              // Mirror integration behavior: stop stream on first blocked token.
              break;
            }
          }
        }

        const elapsedMs = Math.max(0.001, nowMs() - start);
        const tokensPerSec = processedTokens / (elapsedMs / 1000);
        const nsPerToken = (elapsedMs * 1_000_000) / Math.max(1, processedTokens);
        const overheadPct = ((nsPerToken - baselineNsPerToken) / baselineNsPerToken) * 100;

        return {
          mode: benchMode,
          elapsedMs,
          processedTokens,
          tokensPerSec,
          violationCount,
          blockedCount,
          overheadPct,
        };
      };

      const baselineRow: BenchmarkRow = {
        mode: 'BASELINE',
        elapsedMs: baselineElapsed,
        processedTokens: baselineTokenCount,
        tokensPerSec: baselineTokenCount / (baselineElapsed / 1000),
        violationCount: 0,
        blockedCount: 0,
        overheadPct: 0,
      };

      const rows: BenchmarkRow[] = [
        baselineRow,
        runMode('PASSTHROUGH'),
        runMode('REDACT'),
        runMode('BLOCK'),
      ];

      if (mountedRef.current) {
        setBenchmarkRows(rows);
        setIsBenchmarking(false);
      }
    }, 0);
  };

  const exportBenchmarkJson = () => {
    if (benchmarkRows.length === 0) {
      return;
    }

    const payload: BenchmarkExport = {
      schemaVersion: 'day8.safety-benchmark.v1',
      generatedAtEpochMs: Date.now(),
      loops: BENCH_LOOPS,
      rows: benchmarkRows,
    };

    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `safety-benchmark-${payload.generatedAtEpochMs}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="safety-audit-panel">
      {/* Header */}
      <div className="safety-audit-header">
        <span className="safety-audit-title">Output Safety</span>
        <span className={`safety-mode-badge safety-mode-${mode.toLowerCase()}`}>
          {mode}
        </span>
      </div>

      {/* Mode toggle */}
      <div className="safety-mode-controls" role="group" aria-label="Safety mode">
        {(['PASSTHROUGH', 'REDACT', 'BLOCK'] as InterceptMode[]).map((m) => (
          <button
            key={m}
            className={`safety-mode-btn${mode === m ? ' active' : ''}`}
            onClick={() => handleModeChange(m)}
            title={
              m === 'PASSTHROUGH'
                ? 'Log violations without altering output'
                : m === 'REDACT'
                  ? 'Replace violations with [REDACTED] placeholders'
                  : 'Stop the stream on first violation'
            }
          >
            {m}
          </button>
        ))}
      </div>

      {/* Benchmark */}
      <div className="safety-benchmark">
        <div className="safety-bench-actions">
          <button
            className="safety-bench-run"
            onClick={runBenchmark}
            disabled={isBenchmarking}
            title="Run deterministic safety overhead benchmark"
          >
            {isBenchmarking ? 'Benchmarking...' : 'Run Benchmark'}
          </button>
          <button
            className="safety-bench-export"
            onClick={exportBenchmarkJson}
            disabled={isBenchmarking || benchmarkRows.length === 0}
            title="Export benchmark rows as JSON for CI budget gates"
          >
            Export JSON
          </button>
        </div>

        {benchmarkRows.length > 0 && (
          <div className="safety-bench-table" aria-label="Safety benchmark results">
            <div className="safety-bench-row safety-bench-head">
              <span>Mode</span>
              <span>TPS</span>
              <span>Overhead</span>
              <span>Blocked</span>
            </div>
            {benchmarkRows.map((row) => (
              <div key={row.mode} className="safety-bench-row">
                <span>{row.mode}</span>
                <span>{Math.round(row.tokensPerSec).toLocaleString()}</span>
                <span>{row.overheadPct >= 0 ? '+' : ''}{row.overheadPct.toFixed(1)}%</span>
                <span>{row.blockedCount}</span>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Count summary chips */}
      {violations.length > 0 && (
        <div className="safety-counts">
          {Object.entries(counts).map(([type, count]) => (
            <span key={type} className={`safety-count-chip ${TYPE_CLASS[type] ?? ''}`}>
              {type} ×{count}
            </span>
          ))}
        </div>
      )}

      {/* Audit log */}
      <div className="safety-audit-log" aria-live="polite" aria-label="Safety audit log">
        {violations.length === 0 ? (
          <div className="safety-audit-empty">No violations detected</div>
        ) : (
          violations.map((v, i) => (
            <div
              key={`${v.timestamp}-${i}`}
              className={`safety-audit-entry ${SEVERITY_CLASS[v.severity] ?? ''}`}
            >
              <span className={`safety-entry-type ${TYPE_CLASS[v.type] ?? ''}`}>
                {v.type}
              </span>
              <span className="safety-entry-sev">{v.severity}</span>
              <span className="safety-entry-desc">{v.description}</span>
              {isPassthrough && (
                <span className="safety-entry-passthrough">would-redact</span>
              )}
              <span className="safety-entry-time">
                {new Date(v.timestamp).toLocaleTimeString()}
              </span>
            </div>
          ))
        )}
      </div>
    </div>
  );
};
