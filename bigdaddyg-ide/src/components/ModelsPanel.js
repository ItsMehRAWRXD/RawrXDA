import React from 'react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  createFileByteSource,
  createUrlByteSource,
  formatBytes,
  formatCount,
  loadGgufModelSummary
} from '../utils/ggufStreamingLoader';
import {
  focusVisibleRing,
  MinimalSurfaceM814Footer,
  MINIMALISTIC_DOC,
  CopySupportLineButton
} from '../utils/minimalisticM08M14';

const LAST_REMOTE_URL_KEY = 'rawrxd.ide.models.lastRemoteUrl';
const GGUF_MANIFEST_OK_KEY = 'rawrxd.ide.ggufLocalManifestOk';
const GGUF_LAST_LABEL_KEY = 'rawrxd.ide.ggufLastModelLabel';

function compareBigIntDescending(left, right) {
  if (left === right) return 0;
  return left > right ? -1 : 1;
}

function formatMetadataValue(value) {
  if (value == null) return 'null';
  if (typeof value === 'string') return value;
  if (typeof value === 'number') return Number.isInteger(value) ? value.toLocaleString() : value.toFixed(4);
  if (typeof value === 'boolean') return value ? 'true' : 'false';
  if (Array.isArray(value)) return value.join(', ');
  if (value.kind === 'array') {
    const preview = Array.isArray(value.preview) ? value.preview.join(', ') : '';
    return `${value.valueType}[${value.length}] ${preview}${value.truncated ? ', ...' : ''}`;
  }
  return JSON.stringify(value);
}

function StatCard({ label, value, tone = 'blue' }) {
  const tones = {
    blue: 'border-cyan-500/30 bg-cyan-500/10 text-cyan-100',
    amber: 'border-amber-500/30 bg-amber-500/10 text-amber-100',
    green: 'border-emerald-500/30 bg-emerald-500/10 text-emerald-100',
    rose: 'border-rose-500/30 bg-rose-500/10 text-rose-100'
  };

  return (
    <div className={`rounded-xl border px-3 py-2 ${tones[tone] || tones.blue}`}>
      <div className="text-[10px] uppercase tracking-[0.24em] text-white/60">{label}</div>
      <div className="mt-1 text-sm font-semibold text-white">{value}</div>
    </div>
  );
}

const ModelsPanel = () => {
  const { setStatusLine, pushToast, noisyLog, noisyLogVerbose, playUiSound, settings } = useIdeFeatures();
  const [mode, setMode] = React.useState('file');
  const [fileInputKey, setFileInputKey] = React.useState(0);
  const [selectedFile, setSelectedFile] = React.useState(null);
  const [remoteUrl, setRemoteUrl] = React.useState(() => {
    try {
      return localStorage.getItem(LAST_REMOTE_URL_KEY) || '';
    } catch {
      return '';
    }
  });
  const [isLoading, setIsLoading] = React.useState(false);
  const [error, setError] = React.useState('');
  const [progress, setProgress] = React.useState({
    phase: 'idle',
    percent: 0,
    detail: 'Attach a local GGUF or a byte-range URL to stream the manifest.'
  });
  const [result, setResult] = React.useState(null);
  const requestIdRef = React.useRef(0);

  React.useEffect(() => {
    try {
      localStorage.setItem(LAST_REMOTE_URL_KEY, remoteUrl);
    } catch {
      /* ignore */
    }
  }, [remoteUrl]);

  React.useEffect(() => {
    noisyLog('[models]', 'panel ready');
    let lastLabel = '';
    try {
      lastLabel = localStorage.getItem(GGUF_LAST_LABEL_KEY) || '';
    } catch {
      /* ignore */
    }
    setStatusLine(lastLabel.trim() ? `Models: last manifest ${lastLabel.trim()}` : 'Models: attach a GGUF source');
  }, [noisyLog, setStatusLine]);

  const loadSource = async () => {
    if (isLoading) return;
    if (mode === 'file' && !selectedFile) {
      setError('Pick a GGUF file first.');
      return;
    }
    if (mode === 'url' && !remoteUrl.trim()) {
      setError('Enter a byte-range URL first.');
      return;
    }

    const requestId = requestIdRef.current + 1;
    requestIdRef.current = requestId;
    setIsLoading(true);
    setError('');
    setResult(null);
    playUiSound('tick');
    setStatusLine('Models: streaming GGUF manifest...');
    noisyLog('[models]', 'load start', mode);

    try {
      const source = mode === 'file' ? createFileByteSource(selectedFile) : await createUrlByteSource(remoteUrl);
      const summary = await loadGgufModelSummary(source, {
        onProgress(update) {
          if (requestIdRef.current !== requestId) return;
          setProgress({
            phase: update.phase,
            percent: update.percent,
            detail: update.detail || 'Streaming manifest'
          });
          noisyLogVerbose('models', 'progress', update.phase, update.percent);
        }
      });

      if (requestIdRef.current !== requestId) return;
      setResult(summary);
      setProgress({ phase: 'ready', percent: 100, detail: `${summary.overview.modelName} manifest ready.` });
      const warnLine =
        Array.isArray(summary.warnings) && summary.warnings.length > 0 ? String(summary.warnings[0]) : '';
      setStatusLine(
        warnLine
          ? `Models: ${summary.overview.modelName} — ${warnLine}`
          : `Models: ${summary.overview.modelName} ready`
      );
      pushToast({
        title: warnLine ? 'GGUF loaded (check warnings)' : 'GGUF loaded',
        message: warnLine
          ? `${summary.overview.modelName} — ${warnLine}`
          : `${summary.overview.modelName} parsed with ${formatCount(summary.header.tensorCount)} tensors.`,
        variant: warnLine ? 'warn' : 'success',
        durationMs: warnLine ? 5200 : 3200
      });
      noisyLog('[models]', 'manifest ok', summary.overview.modelName, summary.header.tensorCount, 'tensors');
      try {
        if (mode === 'file' && selectedFile) {
          localStorage.setItem(GGUF_MANIFEST_OK_KEY, '1');
        }
        localStorage.setItem(GGUF_LAST_LABEL_KEY, String(summary.overview.modelName || summary.source?.name || ''));
      } catch {
        /* ignore */
      }
    } catch (loadError) {
      if (requestIdRef.current !== requestId) return;
      let message = loadError?.message || 'Failed to stream GGUF manifest';
      if (/GGUF|magic|range|truncat/i.test(message)) {
        message = `${message} If this is a remote URL, confirm byte-range (206) responses and CORS; for local files, verify the download is complete.`;
      }
      setError(message);
      setProgress({ phase: 'error', percent: 0, detail: message });
      setStatusLine(`Models: ${message}`);
      pushToast({
        title: 'GGUF load failed',
        message: `${message} — Check path, CORS (remote), or try a smaller local file.`,
        variant: 'error',
        durationMs: 4200
      });
      noisyLog('[models]', 'manifest failed', message);
      playUiSound('error');
    } finally {
      if (requestIdRef.current === requestId) {
        setIsLoading(false);
      }
    }
  };

  const resetState = () => {
    requestIdRef.current += 1;
    setIsLoading(false);
    setError('');
    setResult(null);
    setSelectedFile(null);
    setFileInputKey((k) => k + 1);
    try {
      localStorage.removeItem(LAST_REMOTE_URL_KEY);
    } catch {
      /* ignore */
    }
    setRemoteUrl('');
    setProgress({
      phase: 'idle',
      percent: 0,
      detail: 'M03 — Pick a local .gguf or a byte-range URL, then Load manifest.'
    });
    setStatusLine('Models: reset — pick a source');
    noisyLog('[models]', 'picker reset (local file + saved URL cleared)');
  };

  const dominantTensors = React.useMemo(() => {
    if (!result?.tensors) return [];
    return [...result.tensors]
      .sort((left, right) => compareBigIntDescending(left.approxBytes, right.approxBytes))
      .slice(0, 12);
  }, [result]);

  const tensorTypeEntries = React.useMemo(() => {
    if (!result?.tensorTypeHistogram) return [];
    return Object.entries(result.tensorTypeHistogram).sort((left, right) => right[1] - left[1]);
  }, [result]);

  const metadataPreview = React.useMemo(() => result?.metadata?.slice(0, 14) || [], [result]);

  const verboseModels = settings.accessibilityVerboseLocalModelHints !== false;

  const idlePreAiGgufHint = React.useMemo(() => {
    if (result || isLoading || progress.phase !== 'idle' || error) return '';
    try {
      const prev = localStorage.getItem(GGUF_LAST_LABEL_KEY);
      return prev && String(prev).trim()
        ? ` Pre-AI hint: last GGUF label in this shell was "${String(prev).trim()}".`
        : '';
    } catch {
      return '';
    }
  }, [result, isLoading, progress.phase, error]);

  return (
    <div
      className="flex h-full min-h-0 flex-col bg-ide-sidebar text-white"
      role="region"
      aria-labelledby="rawrxd-models-heading"
      {...(verboseModels ? { 'aria-describedby': 'rawrxd-models-gguf-hint' } : {})}
    >
      {verboseModels ? (
        <p id="rawrxd-models-gguf-hint" className="sr-only">
          Autonomous workspace model registry: stream G G U F manifest and tensor index from a local file or byte-range U R L.
          Feeds agentic context for RawrXD; full weight execution stays on native or HTTP provider lanes you configure elsewhere.
        </p>
      ) : null}
      <div className="border-b border-gray-700 px-4 py-3">
        <div className="flex items-center justify-between gap-3">
          <div>
            <h2
              id="rawrxd-models-heading"
              className="text-sm font-semibold tracking-wide"
              title="Streams GGUF manifest and tensor directory in-browser. Does not download the full weight blob without you choosing a source; remote URLs need CORS + byte ranges."
            >
              Model Streamer
            </h2>
            <p className="mt-1 text-[11px] text-gray-400">
              Pure browser GGUF loader that streams headers, metadata, and tensor tables without a native bridge.
            </p>
          </div>
          <span className="rounded-full border border-cyan-500/40 bg-cyan-500/10 px-2 py-1 text-[10px] font-semibold uppercase tracking-[0.22em] text-cyan-200">
            WebASM
          </span>
        </div>
      </div>

      <div className="flex-1 space-y-4 overflow-y-auto px-4 py-4">
        {!result && !isLoading && progress.phase === 'idle' && !error ? (
          <p className="text-[11px] text-gray-500 border border-gray-700/80 rounded-lg px-3 py-2 bg-gray-950/40">
            M03 — No manifest loaded. Next: choose Local file or Remote URL, attach a source, then Load manifest.
            {idlePreAiGgufHint}
          </p>
        ) : null}
        <section
          className="rounded-2xl border border-gray-700 bg-gray-900/60 p-4 shadow-[0_18px_60px_rgba(0,0,0,0.25)]"
          aria-label="GGUF source"
          {...(verboseModels ? { 'aria-describedby': 'rawrxd-models-gguf-hint' } : {})}
        >
          <div className="flex gap-2 rounded-xl border border-gray-700 bg-gray-950/70 p-1 text-xs" role="tablist" aria-label="GGUF source type">
            <button
              type="button"
              role="tab"
              aria-selected={mode === 'file'}
              onClick={() => setMode('file')}
              title="Use a file from disk (Electron file picker). Streams manifest chunks only."
              className={`flex-1 rounded-lg px-3 py-2 transition ${focusVisibleRing} ${
                mode === 'file' ? 'bg-cyan-500/20 text-white' : 'text-gray-400 hover:bg-gray-800 hover:text-white'
              }`}
            >
              Local file
            </button>
            <button
              type="button"
              role="tab"
              aria-selected={mode === 'url'}
              onClick={() => setMode('url')}
              title="HTTP(S) URL with Range support and CORS. Does not bypass your network policy."
              className={`flex-1 rounded-lg px-3 py-2 transition ${focusVisibleRing} ${
                mode === 'url' ? 'bg-cyan-500/20 text-white' : 'text-gray-400 hover:bg-gray-800 hover:text-white'
              }`}
            >
              Remote URL
            </button>
          </div>

          <div className="mt-4 space-y-3">
            {mode === 'file' ? (
              <label className="block rounded-xl border border-dashed border-gray-600 bg-gray-950/60 px-3 py-4 text-xs text-gray-300">
                <div className="font-semibold text-white">Select a local GGUF</div>
                <div className="mt-1 text-gray-400">The loader streams only the manifest and tensor directory, not the full weight blob.</div>
                <input
                  key={fileInputKey}
                  type="file"
                  accept=".gguf"
                  className="mt-3 block w-full text-xs text-gray-400 file:mr-3 file:rounded-lg file:border-0 file:bg-cyan-500/20 file:px-3 file:py-2 file:text-cyan-100"
                  onChange={(event) => {
                    const nextFile = event.target.files?.[0] || null;
                    setSelectedFile(nextFile);
                    setError('');
                  }}
                />
                <div className="mt-2 text-[11px] text-gray-500">
                  {selectedFile ? `${selectedFile.name} · ${formatBytes(selectedFile.size)}` : 'No file selected'}
                </div>
              </label>
            ) : (
              <label className="block text-xs text-gray-300">
                <div className="mb-2 font-semibold text-white">Byte-range GGUF URL</div>
                <input
                  type="url"
                  value={remoteUrl}
                  onChange={(event) => {
                    setRemoteUrl(event.target.value);
                    setError('');
                  }}
                  placeholder="https://host/models/model.gguf"
                  className="w-full rounded-xl border border-gray-600 bg-gray-950/70 px-3 py-2 text-sm text-white outline-none transition focus:border-cyan-400"
                />
                <div className="mt-2 text-[11px] text-gray-500">
                  Remote sources must expose CORS and accept byte ranges.
                </div>
              </label>
            )}

            <div className="flex gap-2">
              <button
                type="button"
                onClick={loadSource}
                disabled={isLoading}
                title="Streams tensor table and metadata. Does not run inference in this panel."
                className={`flex-1 rounded-xl bg-cyan-500 px-3 py-2 text-xs font-semibold text-slate-950 transition hover:bg-cyan-400 disabled:cursor-not-allowed disabled:bg-gray-700 disabled:text-gray-400 ${focusVisibleRing}`}
              >
                {isLoading ? 'Streaming manifest…' : 'Load manifest'}
              </button>
              <button
                type="button"
                onClick={resetState}
                title="Clears result, file pick, and saved remote URL in localStorage."
                className={`rounded-xl border border-gray-600 px-3 py-2 text-xs text-gray-300 transition hover:border-gray-400 hover:text-white ${focusVisibleRing}`}
              >
                Reset
              </button>
            </div>
          </div>
        </section>

        <section className="rounded-2xl border border-gray-700 bg-gradient-to-br from-slate-950 via-slate-900 to-cyan-950/60 p-4">
          <div className="flex items-center justify-between gap-3">
            <div>
              <div className="text-[10px] uppercase tracking-[0.26em] text-cyan-200/70">Stream phase</div>
              <div className="mt-1 text-sm font-semibold text-white">{progress.phase}</div>
            </div>
            <div className="text-right text-xs text-cyan-100">{progress.percent}%</div>
          </div>
          <div className="mt-3 h-2 overflow-hidden rounded-full bg-black/40">
            <div
              className="h-full rounded-full bg-gradient-to-r from-cyan-400 via-emerald-300 to-cyan-100 transition-all duration-300"
              style={{ width: `${progress.percent}%` }}
            />
          </div>
          <div className="mt-3 text-xs text-gray-300">{progress.detail}</div>
          {error && (
            <div className="mt-3 rounded-xl border border-rose-500/30 bg-rose-500/10 px-3 py-2 text-xs text-rose-200 space-y-1">
              <div>{error}</div>
              <div className="text-[10px] text-rose-300/90">Next: fix path or URL, confirm CORS/Range for remote, or Reset and retry.</div>
            </div>
          )}
        </section>

        {result && (
          <>
            <section className="grid grid-cols-2 gap-3">
              <StatCard label="Model" value={result.overview.modelName} tone="blue" />
              <StatCard label="Architecture" value={result.overview.architecture} tone="amber" />
              <StatCard label="Tensors" value={formatCount(result.header.tensorCount)} tone="green" />
              <StatCard label="Manifest bytes" value={formatBytes(result.stats.scannedBytes)} tone="rose" />
            </section>

            <section className="rounded-2xl border border-gray-700 bg-gray-900/60 p-4">
              <div className="flex items-center justify-between gap-3">
                <div>
                  <h3 className="text-sm font-semibold text-white">Model profile</h3>
                  <p className="mt-1 text-[11px] text-gray-400">Mirrors the Win32 loader surface: identity, alignment, context, and tensor catalog.</p>
                </div>
                <div className="text-[11px] text-gray-500">{result.source.kind === 'url' ? 'Remote stream' : 'Local stream'}</div>
              </div>
              <div className="mt-4 grid grid-cols-2 gap-3 text-xs text-gray-300">
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">File size</div>
                  <div className="mt-1 text-white">{formatBytes(result.source.size)}</div>
                </div>
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">GGUF version</div>
                  <div className="mt-1 text-white">v{result.header.version}</div>
                </div>
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">Metadata entries</div>
                  <div className="mt-1 text-white">{formatCount(result.header.metadataCount)}</div>
                </div>
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">Alignment</div>
                  <div className="mt-1 text-white">{formatCount(result.header.alignment)} bytes</div>
                </div>
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">Context length</div>
                  <div className="mt-1 text-white">{result.overview.contextLength ? formatCount(result.overview.contextLength) : 'n/a'}</div>
                </div>
                <div className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                  <div className="text-gray-500">Embedding length</div>
                  <div className="mt-1 text-white">{result.overview.embeddingLength ? formatCount(result.overview.embeddingLength) : 'n/a'}</div>
                </div>
              </div>
              {result.warnings.length > 0 && (
                <div className="mt-4 rounded-xl border border-amber-500/30 bg-amber-500/10 px-3 py-2 text-xs text-amber-200">
                  {result.warnings.join(' | ')}
                </div>
              )}
            </section>

            <section className="rounded-2xl border border-gray-700 bg-gray-900/60 p-4">
              <div className="flex items-center justify-between gap-3">
                <h3 className="text-sm font-semibold text-white">Tensor mix</h3>
                <div className="text-[11px] text-gray-500">Largest tensor regions by streamed directory offset</div>
              </div>
              <div className="mt-3 flex flex-wrap gap-2">
                {tensorTypeEntries.map(([type, count]) => (
                  <span
                    key={type}
                    className="rounded-full border border-cyan-500/20 bg-cyan-500/10 px-2 py-1 text-[11px] text-cyan-100"
                  >
                    {type} · {formatCount(count)}
                  </span>
                ))}
              </div>
              <div className="mt-4 overflow-hidden rounded-xl border border-gray-700">
                <table className="min-w-full divide-y divide-gray-700 text-left text-xs">
                  <thead className="bg-gray-950/80 text-gray-400">
                    <tr>
                      <th className="px-3 py-2 font-medium">Tensor</th>
                      <th className="px-3 py-2 font-medium">Shape</th>
                      <th className="px-3 py-2 font-medium">Type</th>
                      <th className="px-3 py-2 font-medium">Region</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-800 bg-gray-900/50">
                    {dominantTensors.map((tensor) => (
                      <tr key={tensor.name}>
                        <td className="px-3 py-2 font-mono text-[11px] text-gray-200">{tensor.name}</td>
                        <td className="px-3 py-2 text-gray-300">{tensor.dimensions.join(' x ')}</td>
                        <td className="px-3 py-2 text-cyan-200">{tensor.tensorType}</td>
                        <td className="px-3 py-2 text-gray-300">{formatBytes(tensor.approxBytes)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </section>

            <section className="rounded-2xl border border-gray-700 bg-gray-900/60 p-4">
              <div className="flex items-center justify-between gap-3">
                <h3 className="text-sm font-semibold text-white">Metadata preview</h3>
                <div className="text-[11px] text-gray-500">First {metadataPreview.length} entries from the streamed KV store</div>
              </div>
              <div className="mt-4 space-y-2">
                {metadataPreview.map((entry) => (
                  <div key={entry.key} className="rounded-xl border border-gray-700 bg-black/20 px-3 py-2">
                    <div className="flex items-center justify-between gap-3">
                      <div className="font-mono text-[11px] text-cyan-100">{entry.key}</div>
                      <div className="text-[10px] uppercase tracking-[0.22em] text-gray-500">{entry.type}</div>
                    </div>
                    <div className="mt-1 break-words text-xs text-gray-300">{formatMetadataValue(entry.value)}</div>
                  </div>
                ))}
              </div>
            </section>
          </>
        )}
      </div>
      <MinimalSurfaceM814Footer
        surfaceId="models"
        offlineHint="Local GGUF file mode works offline; remote URL needs network + CORS."
        docPath="docs/GGUF_PRODUCTION_DEPTH.md"
        m13Hint={`Models: status lines + noisyLog; also ${MINIMALISTIC_DOC}.`}
        className="px-3 border-t border-gray-800/80 shrink-0"
      >
        <div className="flex flex-wrap gap-1 pb-1">
          <CopySupportLineButton
            getText={() => `[models] phase=${progress.phase} err=${error ? String(error).slice(0, 120) : 'none'}`}
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[models]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('Models: support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[models]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
};

export default ModelsPanel;