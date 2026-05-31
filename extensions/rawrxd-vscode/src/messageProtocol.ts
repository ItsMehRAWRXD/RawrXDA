/**
 * messageProtocol.ts
 * Type-safe message passing between WebView <-> Extension <-> ASM
 */

export type MessageDirection = 'webview-to-extension' | 'extension-to-webview';

export interface RawrXDMessage<T = unknown> {
    type: string;
    payload: T;
    id: string;  
    timestamp: number;
}

export interface ComposerInitPayload {
    targetFiles: string[];
    prompt: string;
    contextRange?: { start: number; end: number };
}

export interface EditPlanPayload {
    transactionId: string;
    files: FileEditPlan[];
    dependencies: DependencyEdge[];
    estimatedTimeMs: number;
}

export interface FileEditPlan {
    path: string;
    original: string;
    modified: string;
    diff: string;
    heatmap: HeatmapData;
    validationStatus: 'pending' | 'valid' | 'invalid';
}

export interface HeatmapData {
    width: number;
    height: number;
    tokens: TokenAttention[];
    rawBuffer?: Uint8ClampedArray;  
}

export interface TokenAttention {
    x: number;
    y: number;
    attention: number;
    token: string;
    line: number;
    column: number;
}

export interface DependencyEdge {
    from: string;
    to: string;
    type: 'include' | 'call' | 'inheritance' | 'data';
}

export interface ApplyPayload {
    transactionId: string;
    mode: 'atomic' | 'best-effort';
}

export interface RollbackPayload {
    transactionId: string;
}

export interface TransactionStatusPayload {
    transactionId: string;
    status: 'planning' | 'validating' | 'writing' | 'committed' | 'rolled_back' | 'partial';
    appliedFiles: string[];
    failedFiles: Array<{ path: string; error: string }>;
    rollbackAvailable: boolean;
    timing: {
        totalMs: number;
        validationMs: number;
        writeMs: number;
    };
}
