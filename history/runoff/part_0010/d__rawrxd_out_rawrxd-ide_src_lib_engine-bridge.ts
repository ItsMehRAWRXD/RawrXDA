import { create } from 'zustand'

// Types for Engine Integration
export interface EngineState {
  isConnected: boolean;
  memoryTier: string;
  memoryUsage: number;
  memoryCapacity: number;
  activePlugins: string[];
  loadedModels: string[];
  logs: string[];
  ws: WebSocket | null;

  // Actions
  connect: () => Promise<void>;
  disconnect: () => void;
  executeCommand: (cmd: string) => Promise<string>;
  loadPlugin: (path: string) => Promise<boolean>;
  setMemoryTier: (tier: string) => Promise<boolean>;
  addLog: (msg: string) => void;
}

// Tier capacity map in tokens
const TIER_CAPACITY: Record<string, number> = {
  TIER_4K: 4096,
  TIER_32K: 32768,
  TIER_64K: 65536,
  TIER_128K: 131072,
  TIER_1M: 1048576,
};

export const useEngineStore = create<EngineState>((set, get) => ({
  isConnected: false,
  memoryTier: 'TIER_4K',
  memoryUsage: 0,
  memoryCapacity: 4096,
  activePlugins: [],
  loadedModels: [],
  logs: [],
  ws: null,

  addLog: (msg: string) => {
    set(state => ({ logs: [...state.logs.slice(-500), msg] }));
  },

  connect: async () => {
    const state = get();
    // Close existing connection
    if (state.ws) {
      state.ws.close();
    }

    // Determine WebSocket URL from current location
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    try {
      const ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        set({ isConnected: true, ws });
        get().addLog('[System] WebSocket connected to RawrXD Engine');
        // Request initial status
        ws.send(JSON.stringify({ type: 'status' }));
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          switch (data.type) {
            case 'status':
              set({
                memoryUsage: data.memoryUsage ?? get().memoryUsage,
                memoryTier: data.memoryTier ?? get().memoryTier,
                memoryCapacity: data.memoryCapacity ?? get().memoryCapacity,
                loadedModels: data.loadedModels ?? get().loadedModels,
              });
              break;
            case 'log':
              get().addLog(data.message);
              break;
            case 'plugin_loaded':
              set(s => ({
                activePlugins: [...s.activePlugins, data.name],
              }));
              get().addLog(`[Plugin] Loaded: ${data.name}`);
              break;
            case 'command_result':
              get().addLog(data.result);
              break;
            default:
              get().addLog(`[WS] ${JSON.stringify(data)}`);
          }
        } catch {
          get().addLog(`[WS] ${event.data}`);
        }
      };

      ws.onclose = () => {
        set({ isConnected: false, ws: null });
        get().addLog('[System] WebSocket disconnected');
        // Auto-reconnect after 3 seconds
        setTimeout(() => {
          if (!get().isConnected) {
            get().addLog('[System] Attempting reconnect...');
            get().connect();
          }
        }, 3000);
      };

      ws.onerror = (err) => {
        get().addLog('[System] WebSocket error — falling back to HTTP polling');
        // Fall back to HTTP-based status polling
        set({ isConnected: false, ws: null });
        // Still try to connect via HTTP
        try {
          fetch('/status').then(r => r.json()).then(data => {
            if (data.ready) {
              set({ isConnected: true });
              get().addLog('[System] Connected via HTTP polling fallback');
            }
          }).catch(() => { });
        } catch { }
      };
    } catch (err) {
      get().addLog(`[System] Connection failed: ${err}`);
      set({ isConnected: false });
    }
  },

  disconnect: () => {
    const { ws } = get();
    if (ws) {
      ws.close(1000, 'User disconnected');
    }
    set({ isConnected: false, ws: null });
    get().addLog('[System] Disconnected from RawrXD Engine');
  },

  executeCommand: async (cmd: string) => {
    const { ws, isConnected } = get();
    get().addLog(`> ${cmd}`);

    // Try WebSocket first
    if (ws && ws.readyState === WebSocket.OPEN) {
      return new Promise<string>((resolve) => {
        const handler = (event: MessageEvent) => {
          try {
            const data = JSON.parse(event.data);
            if (data.type === 'command_result') {
              ws.removeEventListener('message', handler);
              get().addLog(data.result);
              resolve(data.result);
            }
          } catch { }
        };
        ws.addEventListener('message', handler);
        ws.send(JSON.stringify({ type: 'command', command: cmd }));
        // Timeout after 10s
        setTimeout(() => {
          ws.removeEventListener('message', handler);
          resolve('[Timeout] No response after 10s');
        }, 10000);
      });
    }

    // Fallback to HTTP
    try {
      const response = await fetch('/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command: cmd }),
      });
      if (response.ok) {
        const data = await response.json();
        const result = data.result || data.output || JSON.stringify(data);
        get().addLog(result);
        return result;
      } else {
        const errMsg = `[Error] HTTP ${response.status}: ${response.statusText}`;
        get().addLog(errMsg);
        return errMsg;
      }
    } catch (err) {
      const errMsg = `[Error] ${err}`;
      get().addLog(errMsg);
      return errMsg;
    }
  },

  loadPlugin: async (path: string) => {
    const { ws } = get();
    const pluginName = path.split('/').pop() || path;

    // Try WebSocket
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'load_plugin', path }));
    }

    // Also try HTTP
    try {
      const response = await fetch('/api/plugins/load', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path }),
      });
      if (response.ok) {
        set(state => ({
          activePlugins: [...state.activePlugins, pluginName],
        }));
        get().addLog(`[Plugin] Loaded: ${pluginName}`);
        return true;
      } else {
        get().addLog(`[Plugin] Failed to load: ${pluginName} (HTTP ${response.status})`);
        return false;
      }
    } catch (err) {
      // Local-only fallback for offline mode
      set(state => ({
        activePlugins: [...state.activePlugins, pluginName],
      }));
      get().addLog(`[Plugin] Loaded locally (offline): ${pluginName}`);
      return true;
    }
  },

  setMemoryTier: async (tier: string) => {
    const capacity = TIER_CAPACITY[tier] ?? 4096;

    // Try HTTP endpoint
    try {
      const response = await fetch('/api/memory/set_tier', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tier, capacity }),
      });
      if (response.ok) {
        const data = await response.json();
        set({
          memoryTier: tier,
          memoryCapacity: data.capacity ?? capacity,
          memoryUsage: data.usage ?? get().memoryUsage,
        });
        get().addLog(`[Memory] Tier set to ${tier} (${capacity} tokens)`);
        return true;
      }
    } catch { }

    // Fallback: set locally
    set({ memoryTier: tier, memoryCapacity: capacity });
    get().addLog(`[Memory] Tier set locally to ${tier} (${capacity} tokens)`);
    return true;
  }
}));
