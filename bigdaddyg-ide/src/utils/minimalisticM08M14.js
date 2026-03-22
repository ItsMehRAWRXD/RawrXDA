/**
 * Shared shell UI: focus ring, one-line clipboard helper, optional footer note.
 * Extra reference: `docs/MINIMALISTIC_7_ENHANCEMENTS.md` (exported as MINIMALISTIC_DOC).
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

/** Optional build-time gate (CRA/Vite inject at bundle time). */
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

/** One-line diagnostic copy (caller must avoid secrets in getText). */
export function CopySupportLineButton({
  getText,
  label = 'Copy support line',
  className = '',
  title = 'Copy one short support line (no full dumps or secrets).',
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

export function MinimalM08M14Footnote({ offlineHint, docPath = MINIMALISTIC_DOC, surfaceId = 'surface', m13Hint }) {
  const gate = readExperimentalUiFlag() ? 'on' : 'off';
  const devExtra =
    m13Hint ||
    'Verbose dev logs: Settings › Noise (console + tagged lines when enabled or MAX noise).';
  return (
    <p
      className="text-[9px] leading-snug text-gray-600 border-t border-gray-800/60 pt-1.5 mt-1.5"
      title={`${surfaceId} · ${docPath}`}
    >
      <kbd className="px-0.5 rounded bg-gray-800 text-[8px]">Ctrl+Shift+P</kbd> opens the command palette. {offlineHint}{' '}
      Use copy buttons for a single-line diagnostic (no secrets). Experimental UI flag{' '}
      <code className="text-[8px] text-cyan-800/90">{EXPERIMENTAL_UI_ENV}</code>={gate}. {devExtra}{' '}
      <span className="text-cyan-700/80 break-all">{docPath}</span>
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
