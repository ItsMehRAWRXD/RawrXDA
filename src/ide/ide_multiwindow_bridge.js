/**
 * RawrXD MultiWindow Kernel — JavaScript Bridge
 *
 * Exposes the MASM64 task scheduler to the HTML IDE frontend via
 * native messaging (postMessage to C++ host) or WebSocket.
 *
 * This module is loaded by the HTML IDE shell and provides:
 *   - Window creation/destruction
 *   - Task submission with priority & dependency
 *   - Swarm broadcast
 *   - Chain-of-Thought pipelines
 *   - Real-time stats polling
 *   - Replay log access
 *   - Chat command integration (/swarm, /cot, /window, /stats)
 */

// ═══════════════════════════════════════════════════════════════════════════
// Constants (must match ASM/C header)
// ═══════════════════════════════════════════════════════════════════════════

const MW_TASK_TYPES = Object.freeze({
  CHAT: 0,
  AUDIT: 1,
  COT: 2,
  SWARM: 3,
  CODE_EDIT: 4,
  TERMINAL: 5,
  FILE_BROWSE: 6,
  MODEL_MANAGE: 7,
  PERF_MONITOR: 8,
  HOTPATCH: 9,
  REVERSE_ENG: 10,
  DEBUG: 11,
  BENCHMARK: 12,
  COMPILE: 13,
  DEPLOY: 14,
  CUSTOM: 15
});

const MW_PRIORITIES = Object.freeze({
  CRITICAL: 0,
  HIGH: 1,
  NORMAL: 2,
  LOW: 3,
  BACKGROUND: 4
});

const MW_STATES = Object.freeze({
  IDLE: 0,
  QUEUED: 1,
  RUNNING: 2,
  PAUSED: 3,
  COMPLETE: 4,
  FAILED: 5,
  CANCELLED: 6
});

const MW_MSG_TYPES = Object.freeze({
  TASK_SUBMIT: 1,
  TASK_CANCEL: 2,
  TASK_STATUS: 3,
  TASK_RESULT: 4,
  WINDOW_FOCUS: 5,
  WINDOW_RESIZE: 6,
  WINDOW_CLOSE: 7,
  MODEL_SWITCH: 8,
  SWARM_BROADCAST: 9,
  COT_CHAIN: 10,
  AUDIT_REQUEST: 11,
  HOTPATCH_APPLY: 12,
  PERF_SNAPSHOT: 13,
  STREAM_CHUNK: 14,
  HEARTBEAT: 15,
  SHUTDOWN: 16
});

// ═══════════════════════════════════════════════════════════════════════════
// Native call transport (postMessage → C++ host → kernel DLL)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Send a command to the native C++ host and return the result.
 * The host is expected to:
 *   1. Parse the JSON message
 *   2. Call the corresponding kernel API
 *   3. Post back the result via window.chrome.webview.postMessage
 *
 * @param {string} method  Kernel API method name
 * @param {object} params  Parameters object
 * @returns {Promise<object>} Result from the native host
 */
function nativeCall(method, params) {
  return new Promise((resolve, reject) => {
    const id = Math.random().toString(36).substring(2, 10);

    const handler = (event) => {
      let data;
      try {
        data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
      } catch { return; }

      if (data.id !== id) return;
      window.removeEventListener('message', handler);

      if (data.error) {
        reject(new Error(data.error));
      } else {
        resolve(data.result);
      }
    };

    window.addEventListener('message', handler);

    const msg = JSON.stringify({ id, method, params });

    // WebView2 path (Edge Chromium)
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(msg);
    }
    // Fallback: localhost WebSocket
    else if (window._mwSocket && window._mwSocket.readyState === 1) {
      window._mwSocket.send(msg);
    }
    // Fallback: direct window postMessage (for testing in iframe)
    else {
      window.postMessage(msg, '*');
    }

    // Timeout after 10 seconds
    setTimeout(() => {
      window.removeEventListener('message', handler);
      reject(new Error(`nativeCall timeout: ${method}`));
    }, 10000);
  });
}

// ═══════════════════════════════════════════════════════════════════════════
// MultiWindowBridge — Main API class
// ═══════════════════════════════════════════════════════════════════════════

class MultiWindowBridge {
  constructor() {
    /** @type {Map<number, {id:number, type:number, title:string}>} */
    this.windows = new Map();

    /** @type {Map<number, {windowId:number, type:number, state:number}>} */
    this.tasks = new Map();

    /** @type {boolean} */
    this.initialized = false;

    /** @type {number|null} */
    this._statsPollInterval = null;

    /** @type {function|null} */
    this.onStatsUpdate = null;

    /** @type {function|null} */
    this.onTaskComplete = null;
  }

  // ── Lifecycle ──────────────────────────────────────────────────────

  /**
   * Initialize the kernel via native host.
   * @param {number} workerCount  0 = auto
   * @returns {Promise<boolean>}
   */
  async init(workerCount = 0) {
    try {
      const result = await nativeCall('KernelInit', { workers: workerCount });
      this.initialized = result && result.success;
      if (this.initialized) {
        console.log('[MultiWindowBridge] Kernel initialized');
      }
      return this.initialized;
    } catch (e) {
      console.error('[MultiWindowBridge] Init failed:', e);
      return false;
    }
  }

  /**
   * Shutdown the kernel.
   */
  async shutdown() {
    this.stopStatsPolling();
    if (!this.initialized) return;
    await nativeCall('KernelShutdown', {});
    this.initialized = false;
    this.windows.clear();
    this.tasks.clear();
    console.log('[MultiWindowBridge] Kernel shut down');
  }

  // ── Window Management ──────────────────────────────────────────────

  /**
   * Create a new IDE window instance.
   * @param {string} typeName  One of: chat, audit, cot, swarm, editor, terminal, file
   * @param {number} x
   * @param {number} y
   * @param {number} w
   * @param {number} h
   * @returns {Promise<{id:number, type:number, title:string}>}
   */
  async createWindow(typeName = 'chat', x = 100, y = 100, w = 1200, h = 800) {
    const typeCode = this._mapWindowType(typeName);
    const result = await nativeCall('RegisterWindow', {
      type: typeCode, x, y, width: w, height: h
    });

    if (!result || !result.id) {
      throw new Error('Window registration failed');
    }

    const win = {
      id: result.id,
      type: typeCode,
      title: `RawrXD_${typeName}_${result.id}`
    };
    this.windows.set(win.id, win);
    return win;
  }

  /**
   * Destroy a window.
   * @param {number} windowId
   * @returns {Promise<boolean>}
   */
  async destroyWindow(windowId) {
    const result = await nativeCall('UnregisterWindow', { windowId });
    this.windows.delete(windowId);
    return result && result.success;
  }

  // ── Task Submission ────────────────────────────────────────────────

  /**
   * Submit a task to a specific window.
   * @param {number} windowId
   * @param {object} options
   * @param {string} options.type      Task type name (see MW_TASK_TYPES)
   * @param {string} options.priority  Priority name (see MW_PRIORITIES)
   * @param {number} options.modelId   Model ID (default 0)
   * @param {number} options.dependsOn Dependency task ID (default 0)
   * @returns {Promise<number>} task_id
   */
  async submitTask(windowId, options = {}) {
    const taskType = this._mapTaskType(options.type || 'CHAT');
    const priority = this._mapPriority(options.priority || 'NORMAL');

    const result = await nativeCall('SubmitTask', {
      windowId,
      taskType,
      priority,
      modelId: options.modelId || 0,
      dependsOn: options.dependsOn || 0
    });

    const taskId = result.taskId;
    if (taskId) {
      this.tasks.set(taskId, {
        windowId,
        type: taskType,
        state: MW_STATES.QUEUED,
        ...options
      });
    }
    return taskId;
  }

  /**
   * Cancel a task.
   * @param {number} taskId
   * @returns {Promise<boolean>}
   */
  async cancelTask(taskId) {
    const result = await nativeCall('CancelTask', { taskId });
    if (result && result.success) {
      const task = this.tasks.get(taskId);
      if (task) task.state = MW_STATES.CANCELLED;
    }
    return result && result.success;
  }

  // ── Swarm ──────────────────────────────────────────────────────────

  /**
   * Broadcast a task to multiple windows simultaneously.
   * @param {string} taskType  Task type name
   * @param {*} payload        Serializable payload
   * @param {number} targetCount  How many windows to target
   * @returns {Promise<number>} Number of windows that received the task
   */
  async swarmBroadcast(taskType, payload, targetCount) {
    const payloadStr = typeof payload === 'string' ? payload : JSON.stringify(payload);
    const result = await nativeCall('SwarmBroadcast', {
      taskType: this._mapTaskType(taskType),
      payload: payloadStr,
      payloadSize: payloadStr.length,
      modelCount: targetCount || this.windows.size
    });
    return result.dispatched || 0;
  }

  // ── Chain of Thought ───────────────────────────────────────────────

  /**
   * Create a sequential CoT pipeline in a window.
   * @param {number} windowId
   * @param {string[]} stepNames  Array of callback identifiers
   * @returns {Promise<number>} First task_id in the chain
   */
  async createCoTChain(windowId, stepNames) {
    const result = await nativeCall('ChainOfThought', {
      windowId,
      stepCount: stepNames.length,
      callbacks: stepNames
    });
    return result.firstTaskId || 0;
  }

  // ── IPC ────────────────────────────────────────────────────────────

  /**
   * Send an IPC message between windows.
   * @param {number} srcWindow
   * @param {number} dstWindow  0 = broadcast
   * @param {string} msgType    Message type name
   * @param {*} payload
   * @returns {Promise<boolean>}
   */
  async sendIPC(srcWindow, dstWindow, msgType, payload) {
    const payloadStr = typeof payload === 'string' ? payload : JSON.stringify(payload);
    const result = await nativeCall('SendIPCMessage', {
      msgType: MW_MSG_TYPES[msgType] || 15,
      srcWindow,
      dstWindow,
      payload: payloadStr,
      payloadSize: payloadStr.length
    });
    return result && result.success;
  }

  // ── Stats ──────────────────────────────────────────────────────────

  /**
   * Get current kernel stats.
   * @returns {Promise<object>}
   */
  async getStats() {
    return nativeCall('GetKernelStats', {});
  }

  /**
   * Start polling stats at an interval.
   * @param {number} intervalMs
   */
  startStatsPolling(intervalMs = 1000) {
    this.stopStatsPolling();
    this._statsPollInterval = setInterval(async () => {
      try {
        const stats = await this.getStats();
        if (this.onStatsUpdate) this.onStatsUpdate(stats);
      } catch (e) {
        console.warn('[MultiWindowBridge] Stats poll error:', e);
      }
    }, intervalMs);
  }

  stopStatsPolling() {
    if (this._statsPollInterval) {
      clearInterval(this._statsPollInterval);
      this._statsPollInterval = null;
    }
  }

  // ── Replay Log ─────────────────────────────────────────────────────

  /**
   * Read the deterministic replay log from shared memory.
   * @param {number} maxEntries
   * @returns {Promise<object[]>}
   */
  async getReplayLog(maxEntries = 1000) {
    return nativeCall('ReadReplayLog', { maxEntries });
  }

  // ── Chat Command Integration ───────────────────────────────────────

  /**
   * Execute a slash command from the chat input.
   * Supported: /swarm, /cot, /window, /stats, /cancel, /shutdown
   *
   * @param {string} command  The command (with leading /)
   * @param {string[]} args   Arguments
   * @returns {Promise<string>} Human-readable result message
   */
  async executeCommand(command, args) {
    switch (command) {
      case '/swarm': {
        const taskType = args[0] || 'COMPILE';
        const count = parseInt(args[1]) || this.windows.size;
        const dispatched = await this.swarmBroadcast(taskType, null, count);
        return `Swarm: dispatched to ${dispatched} windows`;
      }

      case '/cot': {
        const windowId = parseInt(args[0]);
        const steps = args.slice(1);
        if (!windowId || steps.length === 0) {
          return 'Usage: /cot <windowId> <step1> <step2> ...';
        }
        const firstId = await this.createCoTChain(windowId, steps);
        return firstId
          ? `CoT chain started (first task: ${firstId}, ${steps.length} steps)`
          : 'CoT chain creation failed';
      }

      case '/window': {
        const [type, x, y, w, h] = args;
        const win = await this.createWindow(
          type || 'chat',
          parseInt(x) || 100,
          parseInt(y) || 100,
          parseInt(w) || 1200,
          parseInt(h) || 800
        );
        return `Window created: id=${win.id} type=${win.title}`;
      }

      case '/stats': {
        const stats = await this.getStats();
        return [
          `Windows: ${stats.activeWindows}`,
          `Tasks: ${stats.activeTasks}`,
          `Processed: ${stats.totalProcessed}`,
          `Failed: ${stats.totalFailed}`,
          `Uptime: ${stats.uptimeMs}ms`,
          `Workers: ${stats.workerCount}`,
          `Queue: [${(stats.queueDepth || []).join(', ')}]`
        ].join(' | ');
      }

      case '/cancel': {
        const taskId = parseInt(args[0]);
        if (!taskId) return 'Usage: /cancel <taskId>';
        const ok = await this.cancelTask(taskId);
        return ok ? `Task ${taskId} cancelled` : `Failed to cancel task ${taskId}`;
      }

      case '/shutdown': {
        await this.shutdown();
        return 'Kernel shut down';
      }

      case '/replay': {
        const entries = await this.getReplayLog(parseInt(args[0]) || 50);
        if (!entries || entries.length === 0) return 'No replay entries';
        return `Replay: ${entries.length} entries retrieved`;
      }

      default:
        return `Unknown kernel command: ${command}`;
    }
  }

  // ── Private helpers ────────────────────────────────────────────────

  _mapWindowType(name) {
    const map = {
      'chat': MW_TASK_TYPES.CHAT,
      'audit': MW_TASK_TYPES.AUDIT,
      'cot': MW_TASK_TYPES.COT,
      'swarm': MW_TASK_TYPES.SWARM,
      'editor': MW_TASK_TYPES.CODE_EDIT,
      'terminal': MW_TASK_TYPES.TERMINAL,
      'file': MW_TASK_TYPES.FILE_BROWSE,
      'model': MW_TASK_TYPES.MODEL_MANAGE,
      'perf': MW_TASK_TYPES.PERF_MONITOR,
      'benchmark': MW_TASK_TYPES.BENCHMARK,
      'compile': MW_TASK_TYPES.COMPILE,
      'deploy': MW_TASK_TYPES.DEPLOY
    };
    return map[name.toLowerCase()] ?? MW_TASK_TYPES.CHAT;
  }

  _mapTaskType(name) {
    const upper = name.toUpperCase();
    return MW_TASK_TYPES[upper] ?? MW_TASK_TYPES.CUSTOM;
  }

  _mapPriority(name) {
    const upper = name.toUpperCase();
    return MW_PRIORITIES[upper] ?? MW_PRIORITIES.NORMAL;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Global singleton
// ═══════════════════════════════════════════════════════════════════════════

const mwKernel = new MultiWindowBridge();

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { MultiWindowBridge, mwKernel, MW_TASK_TYPES, MW_PRIORITIES, MW_STATES, MW_MSG_TYPES };
}
