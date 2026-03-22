import React, { useMemo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import { focusVisibleRing } from '../utils/minimalisticM08M14';
import {
  ACTION_ORDER,
  formatShortcutDisplayFixed,
  SHORTCUT_STATUS_HINTS
} from '../utils/keyboardShortcuts';

/**
 * Toast stack + scrolling status bar — “noisy” IDE chrome.
 */
const NoisyLayer = () => {
  const {
    toasts,
    dismissToast,
    statusLine,
    settings,
    accessibilityReducedMotionEffective,
    shortcuts,
    modules
  } = useIdeFeatures();
  const max = settings.noiseIntensity === 'maximum';
  const reduce = accessibilityReducedMotionEffective;

  const keysHint = useMemo(() => {
    const order = ACTION_ORDER.filter(
      (id) => id !== 'commandPalette' || modules?.commandPalette !== false
    ).slice(0, 3);
    return order
      .map((id) => {
        const combo = formatShortcutDisplayFixed(shortcuts[id]);
        const tag = SHORTCUT_STATUS_HINTS[id] || id;
        return `${combo} ${tag}`;
      })
      .join(' · ');
  }, [shortcuts, modules]);

  return (
    <>
      <div
        className={`fixed bottom-0 left-0 right-0 z-[150] pointer-events-none flex flex-col items-end gap-2 p-3 ${
          max ? 'pb-10' : 'pb-14'
        }`}
        style={{ paddingRight: 'max(12px, env(safe-area-inset-right))' }}
      >
        <AnimatePresence>
          {toasts.map((t) => (
            <motion.div
              key={t.id}
              layout={!reduce}
              initial={reduce ? { opacity: 1 } : { opacity: 0, x: 80, scale: 0.92 }}
              animate={{
                opacity: 1,
                x: 0,
                scale: 1,
                boxShadow:
                  reduce || !max
                    ? '0 8px 32px rgba(0,0,0,0.4)'
                    : ['0 0 0 0 rgba(59,130,246,0)', '0 0 24px 4px rgba(59,130,246,0.35)', '0 0 0 0 rgba(59,130,246,0)']
              }}
              exit={reduce ? { opacity: 0 } : { opacity: 0, x: 120, scale: 0.9 }}
              transition={
                reduce ? { duration: 0 } : max ? { duration: 0.35 } : { type: 'spring', stiffness: 420, damping: 28 }
              }
              className={`pointer-events-auto max-w-md rounded-lg border px-4 py-3 text-sm shadow-xl ${
                t.variant === 'error'
                  ? 'bg-red-950/95 border-red-500 text-red-100'
                  : t.variant === 'warn'
                  ? 'bg-amber-950/95 border-amber-500 text-amber-100'
                  : t.variant === 'success'
                  ? 'bg-emerald-950/95 border-emerald-500 text-emerald-100'
                  : 'bg-gray-900/95 border-cyan-500/60 text-cyan-50'
              }`}
            >
              <div className="flex justify-between gap-3 items-start">
                <div>
                  {t.title && <div className="font-bold text-xs uppercase tracking-wider mb-1 opacity-80">{t.title}</div>}
                  <div className="whitespace-pre-wrap break-words">{t.message}</div>
                </div>
                <button
                  type="button"
                  onClick={() => dismissToast(t.id)}
                  className={`text-gray-400 hover:text-white shrink-0 text-lg leading-none rounded px-0.5 ${focusVisibleRing}`}
                  aria-label="Dismiss toast"
                >
                  ×
                </button>
              </div>
            </motion.div>
          ))}
        </AnimatePresence>
      </div>

      <div
        role="status"
        aria-live="polite"
        aria-relevant="text"
        title="Renderer status and toasts; does not alter main-process inference or agent policy. MAX noise toggles in Settings › Noise."
        className={`fixed bottom-0 left-0 right-0 z-[140] h-7 flex items-center px-3 text-[11px] font-mono border-t overflow-hidden ${
          max && !reduce
            ? 'bg-gradient-to-r from-violet-950 via-cyan-950 to-fuchsia-950 border-fuchsia-500/50 text-fuchsia-100 animate-pulse'
            : 'bg-gray-950 border-gray-700 text-cyan-300/90'
        }`}
      >
        <motion.div
          className="flex items-center gap-6 whitespace-nowrap"
          animate={max && !reduce ? { x: [0, -400] } : {}}
          transition={max && !reduce ? { duration: 12, repeat: Infinity, ease: 'linear' } : {}}
        >
          <span className="text-amber-400 font-bold">● RAWRXD</span>
          <span>
            {statusLine || `Idle · ${keysHint}`}
          </span>
          {max && (
            <>
              <span className="text-fuchsia-300">MAXIMUM NOISE MODE</span>
              <span>🔊 sounds on</span>
              <span>✨ toasts on</span>
              <span>⚡ chrome FX</span>
            </>
          )}
        </motion.div>
      </div>
    </>
  );
};

export default NoisyLayer;
