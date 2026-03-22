import React, { useEffect, useMemo, useRef, useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';

/**
 * VS Code / Cursor–style command palette: fuzzy filter, keyboard nav, categories.
 */
function fuzzyScore(query, text) {
  if (!query) return 1;
  const q = query.toLowerCase();
  const t = text.toLowerCase();
  let qi = 0;
  let score = 0;
  for (let i = 0; i < t.length && qi < q.length; i++) {
    if (t[i] === q[qi]) {
      score += 2;
      qi++;
    }
  }
  if (qi < q.length) return 0;
  return score;
}

const CommandPalette = ({ open, onClose, commands }) => {
  const [query, setQuery] = useState('');
  const [sel, setSel] = useState(0);
  const inputRef = useRef(null);
  const { playUiSound, pushToast, setStatusLine, settings, celebrate, noisyLog, modules } = useIdeFeatures();

  const gatedOpen = open && modules?.commandPalette !== false;

  const registryCommands = useMemo(() => {
    const list = commands || [];
    return list.filter((c) => {
      const req = c.requiresModule;
      if (!req) return true;
      return modules?.[req] !== false;
    });
  }, [commands, modules]);

  const filtered = useMemo(() => {
    const scored = registryCommands
      .map((c) => ({ c, s: fuzzyScore(query, `${c.label} ${c.category || ''}`) }))
      .filter((x) => x.s > 0 || !query.trim())
      .sort((a, b) => b.s - a.s || a.c.label.localeCompare(b.c.label));
    return scored.map((x) => x.c);
  }, [registryCommands, query]);

  useEffect(() => {
    if (gatedOpen) {
      setQuery('');
      setSel(0);
      setTimeout(() => inputRef.current?.focus(), 50);
    }
  }, [gatedOpen]);

  useEffect(() => {
    setSel(0);
  }, [query]);

  const run = (cmd) => {
    const max = settings.noiseIntensity === 'maximum';
    try {
      if (cmd?.action) cmd.action();
    } catch (err) {
      playUiSound('error');
      pushToast({
        title: 'Palette',
        message: err?.message ? String(err.message) : 'Command failed — check devtools console.',
        variant: 'error',
        durationMs: 4500
      });
      setStatusLine(`palette: error — ${cmd?.label || 'command'}`);
      noisyLog('[palette]', 'action error', cmd?.id, err?.message);
      onClose();
      return;
    }
    playUiSound(max ? 'success' : 'tick');
    setStatusLine(`palette: ran — ${cmd?.label || 'command'}`);
    noisyLog('[palette]', 'ran', cmd?.id || cmd?.label);
    if (settings.noisyToasts && max) {
      pushToast({ title: 'Palette', message: cmd?.label || 'Command', variant: 'success', durationMs: 1400 });
    }
    if (max) celebrate();
    onClose();
  };

  const onKeyDown = (e) => {
    if (e.key === 'Escape') {
      e.preventDefault();
      onClose();
      return;
    }
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      setSel((i) => Math.min(i + 1, filtered.length - 1));
    }
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      setSel((i) => Math.max(i - 1, 0));
    }
    if (e.key === 'Enter' && filtered[sel]) {
      e.preventDefault();
      run(filtered[sel]);
    }
  };

  if (modules?.commandPalette === false) {
    return null;
  }

  return (
    <AnimatePresence>
      {gatedOpen && (
        <motion.div
          className="fixed inset-0 z-[200] flex items-start justify-center pt-[12vh] px-4 bg-black/50"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onMouseDown={(e) => e.target === e.currentTarget && onClose()}
        >
          <motion.div
            role="dialog"
            aria-label="Command palette"
            className="w-full max-w-xl rounded-lg border border-gray-600 bg-ide-sidebar shadow-2xl overflow-hidden"
            initial={{ opacity: 0, y: -12 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -12 }}
            onKeyDown={onKeyDown}
          >
            <input
              ref={inputRef}
              type="text"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              placeholder="Type a command…"
              aria-label="Filter commands"
              title={`M01 — Allow-listed actions only; no shell. M12 ${MINIMALISTIC_DOC}`}
              className={`w-full bg-ide-bg border-0 border-b border-gray-700 px-4 py-3 text-sm text-white placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-ide-accent/40 ${focusVisibleRing}`}
            />
            <ul role="listbox" aria-label="Commands" className="max-h-72 overflow-y-auto py-1">
              {filtered.length === 0 && (
                <li className="px-4 py-3 text-sm text-gray-500" role="presentation">
                  {registryCommands.length === 0
                    ? 'No commands available — enable the relevant modules (e.g. Command palette, test runner) in the Modules tab.'
                    : 'No matching commands — try a shorter query, remove typos, or press Esc to close.'}
                </li>
              )}
              {filtered.map((cmd, i) => (
                <li key={cmd.id} role="presentation">
                  <button
                    type="button"
                    role="option"
                    aria-selected={i === sel}
                    onClick={() => run(cmd)}
                    className={`w-full text-left px-4 py-2.5 text-sm flex justify-between gap-2 ${focusVisibleRing} ${
                      i === sel ? 'bg-ide-accent/25 text-white' : 'text-gray-200 hover:bg-gray-700/50'
                    }`}
                  >
                    <span>{cmd.label}</span>
                    <span className="text-xs text-gray-500 shrink-0">
                      {cmd.category}
                      {cmd.accelerator ? ` · ${cmd.accelerator}` : ''}
                    </span>
                  </button>
                </li>
              ))}
            </ul>
            <div
              className="px-4 py-2 border-t border-gray-700 text-[10px] text-gray-500 flex flex-col gap-1 sm:flex-row sm:justify-between sm:items-center"
              title="M01 — Runs registered allow-listed commands only; does not execute shell or arbitrary strings. M06 — Commands mirror App.js paletteCommands / menu IPC ids."
            >
              <span>↑↓ navigate · ↵ run · Esc close (clears filter when reopened)</span>
              <span>M04 — Ctrl+Shift+P · ide:action command-palette · labels match View menu</span>
            </div>
            <MinimalSurfaceM814Footer
              surfaceId="command-palette"
              offlineHint="Palette works offline; some opened targets need backends."
              docPath={MINIMALISTIC_DOC}
              m13Hint="Dev: noisyLog('[palette]', …) on run/error."
              className="px-3 pb-2"
            >
              <div className="flex flex-wrap gap-1 px-1 pb-1">
                <CopySupportLineButton
                  getText={() =>
                    `[palette] matches=${filtered.length} sel=${sel} q=${String(query).slice(0, 48)}`
                  }
                  label="Copy support line"
                  className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
                  onCopied={() => {
                    pushToast({ title: '[palette]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
                    setStatusLine('[palette] support line copied');
                  }}
                  onFailed={() =>
                    pushToast({ title: '[palette]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
                  }
                />
              </div>
            </MinimalSurfaceM814Footer>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default CommandPalette;
