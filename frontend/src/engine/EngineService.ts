/**
 * EngineService.ts
 * Framework-agnostic TypeScript service for UI-engine communication.
 * Implements 1Hz polling, sidecar fallback, and Engine-as-Authority governance.
 */

export interface EngineStatus {
  session_id: string;
  status_seq: number;
  loader_context: {
    state: 0 | 1 | 2 | 3; // IDLE | LOADING | READY | FAULT
    suggested_action: string;
    can_retry: boolean;
    retry_budget_rem: number;
    terminal_fault: boolean;
    fault_class: string;
  };
  last_error_tag?: string;
  recommended_model?: string;
}

export interface ModelInfo {
  id: string;
  name: string;
  is_loaded: boolean;
  vram_req_mb: number;
}

export interface ListDirEntry {
  name: string;
  type: 'file' | 'dir';
}

export interface ListDirResult {
  path: string;
  entries: ListDirEntry[];
}

export interface ReadFileResult {
  path: string;
  content: string;
  start_line: number;
  end_line: number;
  total_lines: number;
}

export interface SearchCodeMatch {
  path: string;
  line_number: number;
  line: string;
}

export interface SearchCodeResult {
  query: string;
  matches: SearchCodeMatch[];
  truncated: boolean;
}

export interface WriteFileResult {
  path: string;
  bytes_written: number;
  appended: boolean;
}

export interface FaultSidecar {
  session_id: string;
  status_seq: number;
  fault_class: string;
  suggested_action: string;
  process_id: number;
  active_model: string;
  source: 'ENGINE_REPORTED' | 'DEAD_MAN_SWITCH';
  /** Epoch ms when the fault was observed — used for log correlation. */
  timestamp: number;
}

export type EngineStateCode = 'IDLE' | 'LOADING' | 'READY' | 'FAULT' | 'UNKNOWN';

type StatusCallback = (status: EngineStatus) => void;
type TokenCallback = (token: string, tokenCount: number) => void;
type CompletionCallback = (summary: { tokenCount: number; elapsedMs: number; tokensPerSec: number }) => void;
type StreamErrorCallback = (errorMessage: string) => void;
type PollFailureCallback = (errorMessage: string) => void;
type HeartbeatLossCallback = (errorMessage: string) => void;

export class EngineService {
  private endpoint: string = 'http://localhost:11435';
  private pollInterval: number = 1000; // 1Hz
  private pollTimeoutMs: number = 2500;
  private pollingHandle: ReturnType<typeof setInterval> | null = null;
  private lastStatus: EngineStatus | null = null;
  private lastStatusAtEpochMs: number = 0;
  private statusCallbacks: Set<StatusCallback> = new Set();
  private pollFailureCallbacks: Set<PollFailureCallback> = new Set();
  private heartbeatLossCallbacks: Set<HeartbeatLossCallback> = new Set();
  private activeInferenceSource: EventSource | null = null;
  private streamClosedByClient: boolean = false;
  private inferenceReconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private heartbeatLossRaised: boolean = false;

  /**
   * Start polling the engine /status endpoint at 1Hz.
   */
  public startPolling(): void {
    if (this.pollingHandle) {
      console.warn('EngineService: Polling already active');
      return;
    }

    void this.pollStatus();

    this.pollingHandle = setInterval(async () => {
      await this.pollStatus();
    }, this.pollInterval);

    console.log('EngineService: Polling started');
  }

  /**
   * Stop polling.
   */
  public stopPolling(): void {
    if (this.pollingHandle) {
      clearInterval(this.pollingHandle);
      this.pollingHandle = null;
      console.log('EngineService: Polling stopped');
    }
  }

  /**
   * Subscribe to status changes.
   */
  public onStatusChange(callback: StatusCallback): () => void {
    this.statusCallbacks.add(callback);
    return () => {
      this.statusCallbacks.delete(callback);
    };
  }

  /**
   * Subscribe to poll failures for resilience handling.
   */
  public onPollFailure(callback: PollFailureCallback): () => void {
    this.pollFailureCallbacks.add(callback);
    return () => {
      this.pollFailureCallbacks.delete(callback);
    };
  }

  /**
   * Subscribe to dead-man switch events when heartbeat is lost.
   */
  public onHeartbeatLoss(callback: HeartbeatLossCallback): () => void {
    this.heartbeatLossCallbacks.add(callback);
    return () => {
      this.heartbeatLossCallbacks.delete(callback);
    };
  }

  /**
   * Query current status (cached).
   */
  public getStatus(): EngineStatus | null {
    return this.lastStatus;
  }

  /**
   * Query current state code.
   */
  public getStateCode(): EngineStateCode {
    if (!this.lastStatus) return 'UNKNOWN';
    const stateMap = ['IDLE', 'LOADING', 'READY', 'FAULT'] as const;
    return stateMap[this.lastStatus.loader_context.state] || 'UNKNOWN';
  }

  /**
   * Query if engine is ready.
   */
  public isReady(): boolean {
    return this.lastStatus?.loader_context?.state === 2;
  }

  /**
   * Query if engine is in fault.
   */
  public isFaulted(): boolean {
    return this.lastStatus?.loader_context?.state === 3;
  }

  /**
   * Get suggested action from engine policy.
   */
  public getSuggestedAction(): string {
    return this.lastStatus?.loader_context?.suggested_action || 'UNKNOWN';
  }

  /**
   * Get recommended model from engine.
   */
  public getRecommendedModel(): string {
    return this.lastStatus?.recommended_model || 'default';
  }

  /**
   * Get session ID for identity validation.
   */
  public getSessionId(): string {
    return this.lastStatus?.session_id || 'UNKNOWN';
  }

  /**
   * Get status sequence for freshness validation.
   */
  public getStatusSeq(): number {
    return this.lastStatus?.status_seq || 0;
  }

  /**
   * Milliseconds since the last successful status update.
   */
  public getStatusAgeMs(): number {
    if (!this.lastStatusAtEpochMs) {
      return Number.POSITIVE_INFINITY;
    }

    return Date.now() - this.lastStatusAtEpochMs;
  }

  /**
   * Fetch the list of available model IDs from the engine.
   * Returns the `available` array from GET /models.
   */
  public async fetchAvailableModels(): Promise<string[]> {
    const response = await this.fetchWithTimeout(`${this.endpoint}/models`, this.pollTimeoutMs);
    if (!response.ok) {
      return [];
    }

    const data = (await response.json()) as { available?: string[] };
    return Array.isArray(data.available) ? data.available : [];
  }

  /**
   * Request a model load. Engine is authoritative; it transitions to
   * LOADING then READY — the UI must observe the polling loop for confirmation.
   *
   * Governance: throws if the engine is currently LOADING or FAULT.
   */
  public async requestModelLoad(modelId: string): Promise<void> {
    const code = this.getStateCode();
    if (code === 'LOADING' || code === 'FAULT') {
      throw new Error(`Governance Violation: Cannot switch model during ${code}.`);
    }

    const response = await fetch(`${this.endpoint}/model/select`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model: modelId }),
    });

    if (!response.ok) {
      throw new Error(`POST /model/select HTTP ${response.status}`);
    }
  }

  /**
   * Pause agentic execution lane at the backend control plane.
   */
  public async pauseAgenticExecution(): Promise<void> {
    const response = await fetch(`${this.endpoint}/control/pause`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    });

    if (!response.ok) {
      throw new Error(`POST /control/pause HTTP ${response.status}`);
    }
  }

  /**
   * Resume agentic execution lane at the backend control plane.
   */
  public async resumeAgenticExecution(): Promise<void> {
    const response = await fetch(`${this.endpoint}/control/resume`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    });

    if (!response.ok) {
      throw new Error(`POST /control/resume HTTP ${response.status}`);
    }
  }

  /**
   * Read-only tool adapter: list directory entries from backend sandbox.
   */
  public async toolListDir(path: string): Promise<ListDirResult> {
    const encodedPath = encodeURIComponent(path);
    const response = await this.fetchWithTimeout(
      `${this.endpoint}/tool/list_dir?path=${encodedPath}`,
      this.pollTimeoutMs
    );
    if (!response.ok) {
      throw new Error(`GET /tool/list_dir HTTP ${response.status}`);
    }

    return (await response.json()) as ListDirResult;
  }

  /**
   * Read-only tool adapter: read file content from backend sandbox.
   */
  public async toolReadFile(path: string, startLine = 1, endLine = 200): Promise<ReadFileResult> {
    const encodedPath = encodeURIComponent(path);
    const response = await this.fetchWithTimeout(
      `${this.endpoint}/tool/read_file?path=${encodedPath}&start=${startLine}&end=${endLine}`,
      this.pollTimeoutMs
    );
    if (!response.ok) {
      throw new Error(`GET /tool/read_file HTTP ${response.status}`);
    }

    return (await response.json()) as ReadFileResult;
  }

  /**
   * Read-only tool adapter: search source code in backend sandbox.
   */
  public async toolSearchCode(query: string): Promise<SearchCodeResult> {
    const encodedQuery = encodeURIComponent(query);
    const response = await this.fetchWithTimeout(
      `${this.endpoint}/tool/search_code?query=${encodedQuery}`,
      this.pollTimeoutMs
    );
    if (!response.ok) {
      throw new Error(`GET /tool/search_code HTTP ${response.status}`);
    }

    return (await response.json()) as SearchCodeResult;
  }

  /**
   * Medium-risk tool adapter: write file content within backend workspace sandbox.
   */
  public async toolWriteFile(
    path: string,
    content: string,
    append = false,
    createDirs = true
  ): Promise<WriteFileResult> {
    const response = await fetch(`${this.endpoint}/tool/write_file`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path, content, append, create_dirs: createDirs }),
    });

    if (!response.ok) {
      throw new Error(`POST /tool/write_file HTTP ${response.status}`);
    }

    return (await response.json()) as WriteFileResult;
  }

  /**
   * Stream inference tokens from engine endpoint.
   * Server emits SSE-style lines in response body: data: { ...json... }
   */
  public async startInferenceStream(
    prompt: string,
    onToken: TokenCallback,
    onComplete: CompletionCallback,
    onError: StreamErrorCallback
  ): Promise<void> {
    if (!this.isReady()) {
      onError('Engine is not in READY state.');
      return;
    }

    this.stopInferenceStream();
    this.streamClosedByClient = false;

    const params = new URLSearchParams({
      prompt,
      session_id: this.getSessionId(),
      status_seq: String(this.getStatusSeq()),
    });

    const maxReconnectAttempts = 8;
    let reconnectAttempts = 0;
    let streamFinished = false;

    const scheduleReconnect = (finalErrorMessage: string) => {
      if (this.streamClosedByClient || streamFinished) {
        return;
      }

      if (reconnectAttempts >= maxReconnectAttempts) {
        onError(finalErrorMessage);
        this.stopInferenceStream();
        return;
      }

      reconnectAttempts += 1;
      if (this.inferenceReconnectTimer) {
        clearTimeout(this.inferenceReconnectTimer);
      }

      this.inferenceReconnectTimer = setTimeout(() => {
        this.inferenceReconnectTimer = null;

        if (this.streamClosedByClient || streamFinished) {
          return;
        }

        // Engine remains authoritative: only reconnect while READY.
        if (!this.isReady()) {
          scheduleReconnect(finalErrorMessage);
          return;
        }

        connect();
      }, 300);
    };

    const connect = () => {
      const eventSource = new EventSource(`${this.endpoint}/inference/sse?${params.toString()}`);
      this.activeInferenceSource = eventSource;

      eventSource.onmessage = (event: MessageEvent<string>) => {
        try {
          const eventObj = JSON.parse(event.data) as {
            type: 'token' | 'done' | 'error';
            token?: string;
            token_count?: number;
            elapsed_ms?: number;
            tokens_per_sec?: number;
            message?: string;
          };

          if (eventObj.type === 'token') {
            onToken(eventObj.token || '', eventObj.token_count || 0);
            return;
          }

          if (eventObj.type === 'done') {
            streamFinished = true;
            onComplete({
              tokenCount: eventObj.token_count || 0,
              elapsedMs: eventObj.elapsed_ms || 0,
              tokensPerSec: eventObj.tokens_per_sec || 0,
            });
            this.stopInferenceStream();
            return;
          }

          if (eventObj.type === 'error') {
            const message = eventObj.message || 'Stream error received from engine.';

            if (message.startsWith('ENGINE_NOT_READY')) {
              eventSource.close();
              scheduleReconnect('Inference stream disconnected.');
              return;
            }

            streamFinished = true;
            onError(message);
            this.stopInferenceStream();
          }
        } catch {
          streamFinished = true;
          onError('Malformed stream event from engine.');
          this.stopInferenceStream();
        }
      };

      eventSource.onerror = () => {
        if (this.streamClosedByClient || streamFinished) {
          return;
        }

        eventSource.close();
        scheduleReconnect('Inference stream disconnected.');
      };
    };

    connect();
  }

  /**
   * Abort active inference stream, if present.
   */
  public stopInferenceStream(): void {
    this.streamClosedByClient = true;
    if (this.inferenceReconnectTimer) {
      clearTimeout(this.inferenceReconnectTimer);
      this.inferenceReconnectTimer = null;
    }
    if (this.activeInferenceSource) {
      this.activeInferenceSource.close();
      this.activeInferenceSource = null;
    }
  }

  /**
   * Poll the /status endpoint.
   */
  private async pollStatus(): Promise<void> {
    const statusAgeMs = this.getStatusAgeMs();
    if (statusAgeMs >= this.pollTimeoutMs && !this.heartbeatLossRaised) {
      const heartbeatMessage = `HEARTBEAT_LOSS: Engine unresponsive for ${statusAgeMs} ms`;
      this.heartbeatLossRaised = true;
      this.heartbeatLossCallbacks.forEach((cb) => {
        try {
          cb(heartbeatMessage);
        } catch (callbackErr) {
          console.error('EngineService: Heartbeat loss callback error', callbackErr);
        }
      });
    }

    try {
      const response = await this.fetchWithTimeout(`${this.endpoint}/status`, this.pollTimeoutMs);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const data: EngineStatus = await response.json();
      this.lastStatus = data;
      this.lastStatusAtEpochMs = Date.now();
      this.heartbeatLossRaised = false;

      // Notify all subscribers
      this.statusCallbacks.forEach((cb) => {
        try {
          cb(data);
        } catch (err) {
          console.error('EngineService: Callback error', err);
        }
      });
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Unknown poll failure';
      console.warn('EngineService: Poll failed', err);
      this.pollFailureCallbacks.forEach((cb) => {
        try {
          cb(errorMessage);
        } catch (callbackErr) {
          console.error('EngineService: Poll failure callback error', callbackErr);
        }
      });
    }
  }

  private async fetchWithTimeout(resource: string, timeoutMs: number): Promise<Response> {
    const controller = new AbortController();
    const timeoutHandle = setTimeout(() => controller.abort(), timeoutMs);

    try {
      return await fetch(resource, { signal: controller.signal });
    } catch (err) {
      if (err instanceof DOMException && err.name === 'AbortError') {
        throw new Error(`HTTP timeout after ${timeoutMs} ms`);
      }

      throw err;
    } finally {
      clearTimeout(timeoutHandle);
    }
  }

}

/**
 * Singleton instance for global access.
 */
export const engineService = new EngineService();
