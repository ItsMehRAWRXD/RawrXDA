import React from 'react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import { extractSymbolsFromText } from '../utils/textSymbolIndex';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';

/**
 * Text-based symbol index from the active editor buffer (regex / line-oriented).
 * Not LSP, not binary disassembly, not xref resolution.
 *
 * @param {{ path?: string, content?: string } | null | undefined} [activeFile]
 */
const SymbolsPanel = ({ activeFile }) => {
  const { setStatusLine, pushToast, playUiSound, noisyLog } = useIdeFeatures();
  const [symbols, setSymbols] = React.useState([]);
  const [loading, setLoading] = React.useState(false);
  const [loadError, setLoadError] = React.useState('');

  const activePath = activeFile?.path || '';
  const activeContent = activeFile?.content ?? '';
  const hasActiveBuffer = Boolean(activePath || activeContent.length > 0);
  const canIndex = activeContent.length > 0;

  const clearPanel = () => {
    setSymbols([]);
    setLoadError('');
    noisyLog('[symbols] panel cleared');
    setStatusLine('Symbols: cleared');
    playUiSound('tick');
  };

  const indexActiveFile = () => {
    setLoading(true);
    setLoadError('');
    playUiSound('tick');
    noisyLog('[symbols] index active buffer', activePath || '(untitled)');
    setStatusLine('Symbols: indexing active file…');

    try {
      const list = extractSymbolsFromText(activeContent, activePath, activeFile?.language || '');
      setSymbols(list);
      setLoading(false);
      setStatusLine(`Symbols: ${list.length} from text index`);
      pushToast({
        title: 'Symbols',
        message:
          list.length === 0
            ? 'No matching patterns in this buffer (try a .js/.ts file or check line-oriented rules).'
            : `Indexed ${list.length} symbol(s) from editor text — not from LSP or binaries.`,
        variant: 'info',
        durationMs: 3200
      });
    } catch (e) {
      const msg = e?.message || 'index failed';
      setLoadError(msg);
      setSymbols([]);
      setLoading(false);
      setStatusLine(`Symbols: error — ${msg}`);
    }
  };

  return (
    <div className="flex h-full min-h-0 flex-col bg-ide-sidebar text-white">
      <div
        className="border-b border-gray-700 px-3 py-2"
        title="Line-oriented regex index of the open editor buffer. Not LSP, not disassembly, not xref resolution. M06 — Read-only navigation default."
      >
        <h3 className="text-sm font-semibold">Symbols &amp; xrefs</h3>
        <p className="text-[10px] text-gray-500 mt-0.5">Text index from editor · line numbers only</p>
        <p className="text-[10px] text-gray-500 mt-0.5">
          Dock tab · <kbd className="px-1 bg-gray-800 rounded text-[9px]">Ctrl+Shift+Y</kbd>
        </p>
      </div>
      <div className="p-3 space-y-2 text-sm flex-1 min-h-0 flex flex-col">
        <div className="flex gap-2">
          <button
            type="button"
            onClick={indexActiveFile}
            disabled={loading || !canIndex}
            className={`flex-1 py-2 rounded bg-cyan-600/30 border border-cyan-500/40 text-cyan-100 text-xs font-medium hover:bg-cyan-600/50 disabled:opacity-50 ${focusVisibleRing}`}
            title={
              canIndex
                ? 'Run line-oriented patterns on the active file content (capped at 500 matches).'
                : 'Open a file with content in the editor first.'
            }
          >
            {loading ? 'Working…' : 'Index active file'}
          </button>
          <button
            type="button"
            onClick={clearPanel}
            disabled={loading}
            className={`px-2 py-2 rounded border border-gray-600 text-gray-400 text-xs hover:bg-gray-800 ${focusVisibleRing}`}
            title="Clear list and saved legacy label"
          >
            Reset
          </button>
        </div>

        {!canIndex ? (
          <div className="text-[11px] text-gray-500 border border-dashed border-gray-700 rounded p-3">
            <p className="text-gray-400 mb-1 font-medium">Nothing to index yet</p>
            <p>
              Open a file in the editor and ensure it has content. This panel scans the buffer with simple patterns — it does
              not read from disk on its own and does not run a language server.
            </p>
            {activePath ? (
              <p className="mt-2 text-gray-500 font-mono text-[10px] break-all" title={activePath}>
                Current file path has no buffer text: {activePath}
              </p>
            ) : null}
          </div>
        ) : (
          <p className="text-[10px] text-gray-500">
            Active:{' '}
            <span className="text-gray-400 font-mono truncate block" title={activePath || '(buffer)'}>
              {activePath || '(unsaved buffer)'}
            </span>
          </p>
        )}

        {loadError ? (
          <div className="text-[11px] text-rose-300 bg-rose-950/40 border border-rose-800/50 rounded px-2 py-2" role="alert">
            {loadError}
          </div>
        ) : null}

        <div className="flex-1 min-h-0 flex flex-col">
          {!symbols.length && !loading && hasActiveBuffer && canIndex ? (
            <div className="text-[11px] text-gray-500 border border-dashed border-gray-700 rounded p-3 mt-1">
              <p className="text-gray-400 mb-1 font-medium">No symbols listed</p>
              <p>Click &quot;Index active file&quot; to scan the editor buffer, or open a different source file.</p>
            </div>
          ) : null}
          {symbols.length > 0 ? (
            <ul className="mt-1 space-y-1 flex-1 overflow-y-auto text-xs font-mono text-gray-200">
              {symbols.map((s, idx) => (
                <li
                  key={`${s.line}-${s.name}-${s.kind}-${idx}`}
                  className="flex justify-between gap-2 border-b border-gray-800/80 pb-1"
                  title={`${s.kind} ${s.name} at line ${s.line} (text index)`}
                >
                  <span className="text-rose-200/90 truncate">{s.name}</span>
                  <span className="text-gray-500 shrink-0">{s.kind}</span>
                  <span className="text-cyan-200/80 shrink-0">L{s.line}</span>
                </li>
              ))}
            </ul>
          ) : null}
        </div>
      </div>
      <MinimalSurfaceM814Footer
        surfaceId="symbols"
        offlineHint="Text index runs in the renderer; knowledge append needs Electron IPC when used."
        docPath="docs/RE_MVP_SYMBOLS_XREFS.md"
        m13Hint={`Dev: noisyLog('[symbols]', …). Also ${MINIMALISTIC_DOC}.`}
        className="px-2"
      >
        <div className="flex flex-wrap gap-1 px-1 pb-1">
          <CopySupportLineButton
            getText={() =>
              `[symbols] count=${symbols.length} path=${(activePath || 'none').slice(0, 120)} err=${loadError ? '1' : '0'}`
            }
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[symbols]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('[symbols] support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[symbols]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
};

export default SymbolsPanel;
