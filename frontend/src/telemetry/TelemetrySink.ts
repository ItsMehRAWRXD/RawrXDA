import { auditLogService } from './AuditLogService';

/**
 * TelemetrySink.ts
 * Day 10 operational telemetry sink with privacy-safe correlation.
 *
 * Design notes:
 * - Never logs raw prompt/text.
 * - Uses HMAC-SHA256 (Web Crypto API) over a content fragment so events can be
 *   correlated without exposing sensitive source text.
 * - Secret should come from runtime env/config in production.
 */

export type TelemetryEventType =
  | 'SAFETY_HIT'
  | 'STREAM_BLOCK'
  | 'MODE_CHANGE'
  | 'TOOL_EXECUTION_SUCCESS'
  | 'TOOL_PENDING_APPROVAL'
  | 'USER_APPROVED'
  | 'USER_DENIED'
  | 'ENGINE_PAUSED'
  | 'ENGINE_RESUMED'
  | 'TOOL_WRITE_FILE_SUCCESS';
export type TelemetrySeverity = 'LOW' | 'HIGH' | 'CRITICAL';

export interface TelemetryEvent {
  type: TelemetryEventType;
  severity: TelemetrySeverity;
  hashedPrompt: string;
  timestamp: number;
}

export class TelemetrySink {
  // In production, inject via environment or secure runtime config.
  private static readonly SECRET = 'ide-telemetry-salt';

  private static async hmacSha256Hex(message: string, secret: string): Promise<string> {
    const subtle = globalThis.crypto?.subtle;
    if (!subtle) {
      // Fail closed to privacy: avoid logging any unhashed source-derived text.
      throw new Error('WebCrypto subtle API unavailable');
    }

    const enc = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      enc.encode(secret),
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']
    );

    const signature = await subtle.sign('HMAC', key, enc.encode(message));
    const bytes = new Uint8Array(signature);
    return Array.from(bytes)
      .map((b) => b.toString(16).padStart(2, '0'))
      .join('');
  }

  public static async log(
    event: Omit<TelemetryEvent, 'hashedPrompt' | 'timestamp'>,
    rawPrompt: string
  ): Promise<void> {
    try {
      const hashedPrompt = await this.hmacSha256Hex(rawPrompt, this.SECRET);
      const payload: TelemetryEvent = {
        ...event,
        hashedPrompt,
        timestamp: Date.now(),
      };

      auditLogService.record({
        kind: 'TELEMETRY',
        telemetryType: payload.type,
        telemetrySeverity: payload.severity,
        hashedPrompt: payload.hashedPrompt,
        timestamp: payload.timestamp,
      });

      // Placeholder sink: console buffer. Replace with HTTPS endpoint in production.
      // eslint-disable-next-line no-console
      console.debug('[TELEMETRY]', JSON.stringify(payload));
    } catch {
      // Keep telemetry non-blocking and fail-safe.
    }
  }
}
