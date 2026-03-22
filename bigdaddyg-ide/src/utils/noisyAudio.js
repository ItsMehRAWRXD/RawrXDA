/**
 * Web Audio UI bleeps for dock/palette/settings feedback.
 * Audio/UI only — does not alter inference routing, provider selection, or model loading (see IdeFeaturesContext + main process).
 * Default gains are moderate; `intensity: 'maximum'` adds an extra high blip (explicit opt-in). Fanfare preset is the loudest built-in.
 */

let audioCtx;
let audioUnavailableWarned = false;

function getCtx() {
  if (typeof window === 'undefined') return null;
  try {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    return audioCtx;
  } catch {
    if (!audioUnavailableWarned && typeof console !== 'undefined' && console.warn) {
      audioUnavailableWarned = true;
      console.warn(
        '[noise] Web Audio unavailable — UI sounds disabled. Click the window or check autoplay/OS audio, then retry an action.'
      );
    }
    return null;
  }
}

function beep(freq, durationMs = 90, gain = 0.09) {
  const a = getCtx();
  if (!a) return;
  try {
    if (a.state === 'suspended') a.resume();
    const o = a.createOscillator();
    const g = a.createGain();
    o.type = 'sine';
    o.frequency.setValueAtTime(freq, a.currentTime);
    g.gain.setValueAtTime(gain, a.currentTime);
    g.gain.exponentialRampToValueAtTime(0.0001, a.currentTime + durationMs / 1000);
    o.connect(g);
    g.connect(a.destination);
    o.start(a.currentTime);
    o.stop(a.currentTime + durationMs / 1000 + 0.02);
  } catch {
    /* ignore */
  }
}

const PRESETS = {
  pop: () => beep(523, 70),
  open: () => beep(392, 55),
  close: () => beep(262, 80),
  success: () => {
    beep(523, 50, 0.07);
    setTimeout(() => beep(659, 50, 0.06), 60);
    setTimeout(() => beep(784, 70, 0.05), 120);
  },
  warn: () => beep(180, 120, 0.12),
  error: () => {
    beep(140, 100, 0.1);
    setTimeout(() => beep(120, 120, 0.12), 90);
  },
  tick: () => beep(880, 35, 0.04),
  whoosh: () => beep(1200, 40, 0.03),
  chatSend: () => {
    beep(440, 40, 0.06);
    setTimeout(() => beep(554, 45, 0.05), 45);
  },
  palette: () => {
    beep(660, 30, 0.05);
    setTimeout(() => beep(990, 35, 0.04), 40);
  },
  /** Maximum noise mode */
  fanfare: () => {
    const seq = [523, 587, 659, 698, 784, 880, 988];
    seq.forEach((f, i) => setTimeout(() => beep(f, 55, 0.06), i * 55));
  }
};

export function playUiSound(kind, { intensity = 'normal' } = {}) {
  const fn = PRESETS[kind] || PRESETS.pop;
  fn();
  if (intensity === 'maximum' && kind !== 'fanfare') {
    setTimeout(() => beep(1200, 25, 0.02), 80);
  }
}

export function playNoiseCelebration() {
  PRESETS.fanfare();
}

export default { playUiSound, playNoiseCelebration };
