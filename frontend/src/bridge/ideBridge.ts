export interface IDEBridge {
  restartEngine: () => Promise<boolean>;
  getSystemLogs: () => Promise<string[]>;
}

// Global IPC bridge exposed by Electron/Tauri preload scripts.
// Falls back to the mock HTTP backend during development.
declare global {
  interface Window {
    ideBridge?: {
      restartEngine: () => Promise<void>;
    };
  }
}

const CONTROL_ENDPOINT = 'http://localhost:11435/control/restart';
const LOGS_ENDPOINT = 'http://localhost:11435/system/logs';

export const ideBridge: IDEBridge = {
  restartEngine: async () => {
    // Prefer the native host bridge (Electron ipcRenderer / Tauri invoke).
    if (window.ideBridge?.restartEngine) {
      try {
        await window.ideBridge.restartEngine();
        return true;
      } catch {
        return false;
      }
    }

    // Dev fallback: ask the mock backend to reset its state machine.
    try {
      const response = await fetch(CONTROL_ENDPOINT, { method: 'POST' });
      return response.ok;
    } catch {
      return false;
    }
  },

  getSystemLogs: async () => {
    try {
      const response = await fetch(LOGS_ENDPOINT);
      if (!response.ok) {
        return [];
      }

      const payload = (await response.json()) as { logs?: string[] };
      return Array.isArray(payload.logs) ? payload.logs : [];
    } catch {
      return [];
    }
  },
};