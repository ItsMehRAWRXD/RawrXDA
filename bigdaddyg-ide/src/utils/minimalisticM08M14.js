/**
 * Optional shared UI bits (copy button, focus ring, optional footnote block).
 * **Primary contract** for shell polish: M01–M07 in `docs/MINIMALISTIC_7_ENHANCEMENTS.md` (repo root).
 * The footnote below is extra UI copy only — not a separate ship gate.
 */
import React from 'react';

import { MINIMALISTIC_DOC } from './workspacePathUtils';

export {
  MINIMALISTIC_DOC,
  workspaceFolderBasename,
  workspaceRelativePath
} from './workspacePathUtils';

/** Tailwind: visible keyboard focus without altering layout. */
export const focusVisibleRing =
  'focus:outline-none focus-visible:ring-2 focus-visible:ring-ide-accent/70 focus-visible:ring-offset-2 focus-visible:ring-offset-gray-900';

/** M11 — optional build-time gate (CRA/Vite inject at bundle time). */
export const EXPERIMENTAL_UI_ENV = 'REACT_APP_RAWRXD_EXPERIMENTAL';

export function readExperimentalUiFlag() {
  try {
    const v = String(process.env[EXPERIMENTAL_UI_ENV] || '').trim();
    return v === '1' || v === 'true';
  } catch {
    return false;
  }
}

export function copyTextToClipboard(text, { onDone, onFail } = {}) {
  const t = String(text || '').trim();
  if (!t) {
    onFail?.(new Error('empty'));
    return;
  }
  if (navigator.clipboard?.writeText) {
    navigator.clipboard.writeText(t).then(() => onDone?.()).catch((e) => onFail?.(e));
  } else {
    onFail?.(new Error('no clipboard'));
  }
}

/**
 * M10 — one-line diagnostic copy (caller must avoid secrets in getText).
 */
export function CopySupportLineButton({
  getText,
  label = 'Copy support line',
  className = '',
  title = 'M10 — copies a single grep-friendly line (no full dumps).',
  onCopied,
  onFailed
}) {
  const handle = () => {
    copyTextToClipboard(typeof getText === 'function' ? getText() : getText, {
      onDone: onCopied,
      onFail: onFailed
    });
  };
  return (
    <button type="button" onClick={handle} className={className} title={title}>
      {label}
    </button>
  );
}

/**
 * M08–M14 footnote — wording grounded in `docs/MINIMALISTIC_7_ENHANCEMENTS.md`.
 * M11: IdeFeaturesContext (runtime) + optional REACT_APP_RAWRXD_EXPERIMENTAL (build).
 */
export function MinimalM08M14Footnote({ offlineHint, docPath = MINIMALISTIC_DOC, surfaceId = 'surface', m13Hint }) {
  const gate = readExperimentalUiFlag() ? 'on' : 'off';
  const m13 =
    m13Hint ||
    'Dev: IdeFeaturesContext noisyConsole + noisyLogVerbose (Settings › Noise) when enabled or MAX noise.';
  return (
    <p
      className="text-[9px] leading-snug text-gray-600 border-t border-gray-800/60 pt-1.5 mt-1.5"
      title={`M08–M14 (modify-only); ${surfaceId} — see ${MINIMALISTIC_DOC}`}
    >
      <span className="text-gray-500 font-medium">M08</span> Tab / Enter; palette{' '}
      <kbd className="px-0.5 rounded bg-gray-800 text-[8px]">Ctrl+Shift+P</kbd>.{' '}
      <span className="text-gray-500 font-medium">M09</span> {offlineHint}{' '}
      <span className="text-gray-500 font-medium">M10</span> Use copy buttons where shown.{' '}
      <span className="text-gray-500 font-medium">M11</span>{' '}
      <code className="text-[8px] text-cyan-800/90">IdeFeaturesContext</code> +{' '}
      <code className="text-[8px] text-cyan-800/90">{EXPERIMENTAL_UI_ENV}</code>={gate}.{' '}
      <span className="text-gray-500 font-medium">M12</span>{' '}
      <span className="text-cyan-700/80 break-all">{docPath}</span>.{' '}
      <span className="text-gray-500 font-medium">M13</span> {m13}{' '}
      <span className="text-gray-500 font-medium">M14</span> No secrets in tooltips; relative paths in UI.
    </p>
  );
}

export function MinimalSurfaceM814Footer({ surfaceId, offlineHint, docPath, m13Hint, children, className = '' }) {
  return (
    <div className={`mt-auto pt-2 shrink-0 border-t border-gray-800/40 px-1 ${className}`.trim()}>
      {children}
      <MinimalM08M14Footnote surfaceId={surfaceId} offlineHint={offlineHint} docPath={docPath} m13Hint={m13Hint} />
    </div>
  );
}
