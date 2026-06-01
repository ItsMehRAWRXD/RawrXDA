/**
 * SafetyInterceptor.ts
 * Day 7: Output Safety Interceptor with Sliding Window.
 *
 * Architecture
 * ───────────
 * LLMs stream tokens one at a time. A secret like `ghp_<36-chars>` can arrive
 * split across two or more tokens. A purely stateless, per-token scanner would
 * miss it. This class maintains a look-behind buffer (WINDOW_SIZE chars) so
 * every regex scan covers the trailing edge of already-emitted content plus the
 * new token, catching cross-token patterns.
 *
 * Cross-boundary redaction logic
 * ──────────────────────────────
 * When a match starts inside the look-behind (already emitted) and ends inside
 * the current token:
 *   - The violation is recorded (the UI can show the audit event retroactively).
 *   - The portion of the match that falls within the current token is replaced,
 *     so the dangerous suffix never reaches the DOM.
 *
 * This is the correct trade-off: we cannot un-emit look-behind content, but we
 * can always suppress the completing tail. In practice, credential patterns
 * concentrate their entropy in the suffix (AWS key IDs, GitHub PATs, etc.), so
 * tail-suppression is the highest-value redaction point.
 *
 * Modes
 * ─────
 * PASSTHROUGH: Scan and record violations; do NOT alter token content.
 *              Useful for auditing whether rules would trigger without affecting UX.
 * REDACT:      Replace matched spans with labelled placeholders (default).
 * BLOCK:       Return a sentinel on first violation; caller must stop the stream.
 */

import { SAFETY_RULES, SafetyRule, ViolationType, Severity } from './rules';
import { RESTRICTED_KEYWORDS } from './restricted_keywords';
import { TelemetrySink, TelemetrySeverity } from '../telemetry/TelemetrySink';

export type InterceptMode = 'PASSTHROUGH' | 'REDACT' | 'BLOCK';

export interface Violation {
  ruleId: string;
  type: ViolationType;
  severity: Severity;
  description: string;
  /** Epoch ms — correlatable with FaultSidecar.timestamp and headless_trace.log */
  timestamp: number;
  matchedText: string;
}

export interface InterceptResult {
  /** Sanitised token to append to the output buffer. */
  token: string;
  /** Highest-severity violation detected this call, if any. */
  violation?: Violation;
}

type ViolationCallback = (violation: Violation) => void;

const SEVERITY_ORDER: Record<Severity, number> = {
  LOW: 0,
  MEDIUM: 1,
  HIGH: 2,
  CRITICAL: 3,
};

export class SafetyInterceptor {
  /** Look-behind: trailing chars of already-emitted content. */
  private lookBehind = '';

  /**
   * How many chars of emitted history to retain for cross-token detection.
   * 50 chars catches secrets up to ~50 chars that straddle a token boundary,
   * which covers every pattern in rules.ts.
   */
  private readonly WINDOW_SIZE = 50;

  private mode: InterceptMode;
  private violationCallbacks: Set<ViolationCallback> = new Set();

  /**
   * Keyword rules compiled once at construction — never inside process().
   * Regex metacharacters in keyword strings are automatically escaped.
   */
  private readonly keywordRules: SafetyRule[];

  constructor(mode: InterceptMode = 'REDACT') {
    this.mode = mode;

    this.keywordRules = RESTRICTED_KEYWORDS
      .filter((kw) => kw.trim().length > 0)
      .map((kw) => ({
        id: `keyword:${kw}`,
        type: 'RESTRICTED' as ViolationType,
        severity: 'MEDIUM' as Severity,
        // Escape metacharacters; add word-boundary anchors where possible.
        pattern: new RegExp(kw.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi'),
        replacement: `[BLOCKED:KW]`,
        description: `Restricted keyword: "${kw}"`,
      }));
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  public setMode(mode: InterceptMode): void {
    this.mode = mode;
    void TelemetrySink.log(
      {
        type: 'MODE_CHANGE',
        severity: 'LOW',
      },
      mode
    );
  }

  public getMode(): InterceptMode {
    return this.mode;
  }

  /**
   * Subscribe to violation events.
   * Called for EVERY violation regardless of mode — including PASSTHROUGH.
   * This allows the SafetyAuditPanel to display what would have been redacted
   * even when the user has selected PASSTHROUGH mode.
   * Returns an unsubscribe function.
   */
  public onViolation(cb: ViolationCallback): () => void {
    this.violationCallbacks.add(cb);
    return () => {
      this.violationCallbacks.delete(cb);
    };
  }

  /**
   * Reset internal state between inference streams.
   * Must be called before each new stream to prevent look-behind
   * from a previous session contaminating the next.
   */
  public reset(): void {
    this.lookBehind = '';
  }

  /**
   * Process one incoming token.
   *
   * Returns the sanitised token to render and the highest-severity violation
   * detected (if any). When mode is BLOCK and a violation is found, the
   * returned token is the block sentinel — the caller should stop the stream.
   */
  public process(token: string): InterceptResult {
    if (!token) return { token };

    const allRules: SafetyRule[] = [...SAFETY_RULES, ...this.keywordRules];
    const scanWindow = this.lookBehind + token;
    const lookBehindLen = this.lookBehind.length;

    let firstViolation: Violation | undefined;
    let sanitizedToken = token;

    for (const rule of allRules) {
      // Reset global regex lastIndex before each scan — critical for /g patterns.
      rule.pattern.lastIndex = 0;

      const match = rule.pattern.exec(scanWindow);
      if (!match) continue;

      const violation: Violation = {
        ruleId: rule.id,
        type: rule.type,
        severity: rule.severity,
        description: rule.description,
        timestamp: Date.now(),
        matchedText: match[0],
      };

      // Track highest-severity violation for the return value.
      if (
        !firstViolation ||
        SEVERITY_ORDER[rule.severity] > SEVERITY_ORDER[firstViolation.severity]
      ) {
        firstViolation = violation;
      }

      // Emit to subscribers in all modes — including PASSTHROUGH.
      // This drives the SafetyAuditPanel even when content is not redacted.
      this.emitViolation(violation);
      void TelemetrySink.log(
        {
          type: 'SAFETY_HIT',
          severity: this.toTelemetrySeverity(violation.severity),
        },
        violation.matchedText
      );

      if (this.mode === 'BLOCK') {
        // Stop immediately; return sentinel so caller can halt the stream.
        void TelemetrySink.log(
          {
            type: 'STREAM_BLOCK',
            severity: this.toTelemetrySeverity(violation.severity),
          },
          violation.matchedText
        );
        this.advanceLookBehind(sanitizedToken);
        return { token: `[STREAM_BLOCKED:${rule.type}]`, violation: firstViolation };
      }

      if (this.mode === 'REDACT') {
        const matchStart = match.index;
        const matchEnd = match.index + match[0].length;

        if (matchStart >= lookBehindLen) {
          // Match is entirely within the current token — straightforward replacement.
          rule.pattern.lastIndex = 0;
          sanitizedToken = sanitizedToken.replace(rule.pattern, rule.replacement);
        } else if (matchEnd > lookBehindLen) {
          // Cross-boundary match: starts in look-behind, ends in current token.
          // We cannot un-emit look-behind content, but we suppress the tail
          // (the completing portion) so the dangerous suffix never reaches the DOM.
          const portionInToken = matchEnd - lookBehindLen;
          sanitizedToken = rule.replacement + sanitizedToken.slice(portionInToken);
        }
        // If matchEnd <= lookBehindLen: match is entirely in already-emitted
        // look-behind. Record the violation but nothing left to redact.
      }
      // PASSTHROUGH: violation recorded above; content unchanged.
    }

    this.advanceLookBehind(sanitizedToken);
    return { token: sanitizedToken, violation: firstViolation };
  }

  // ---------------------------------------------------------------------------
  // Private helpers
  // ---------------------------------------------------------------------------

  private advanceLookBehind(sanitizedToken: string): void {
    const combined = this.lookBehind + sanitizedToken;
    this.lookBehind = combined.slice(Math.max(0, combined.length - this.WINDOW_SIZE));
  }

  private emitViolation(violation: Violation): void {
    for (const cb of this.violationCallbacks) {
      try {
        cb(violation);
      } catch {
        // Isolate subscriber failures — never let an audit callback crash the hot path.
      }
    }
  }

  private toTelemetrySeverity(severity: Severity): TelemetrySeverity {
    if (severity === 'CRITICAL') {
      return 'CRITICAL';
    }

    if (severity === 'HIGH') {
      return 'HIGH';
    }

    return 'LOW';
  }
}

/** Singleton shared across the application. */
export const safetyInterceptor = new SafetyInterceptor('REDACT');
