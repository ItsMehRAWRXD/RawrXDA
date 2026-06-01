/**
 * rules.ts
 * Safety rule definitions for the Output Safety Interceptor.
 *
 * ALL RegExp objects are compiled here at module load time and reused
 * across every token. Never construct a RegExp inside process().
 */

export type ViolationType = 'SECRET_LEAK' | 'PII' | 'RESTRICTED';
export type Severity = 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';

export interface SafetyRule {
  readonly id: string;
  readonly type: ViolationType;
  readonly severity: Severity;
  /** Pre-compiled pattern. Must use the 'g' flag for exec() / replace() loops. */
  readonly pattern: RegExp;
  readonly replacement: string;
  readonly description: string;
}

// ---------------------------------------------------------------------------
// SECRET_LEAK — credentials and tokens that must never reach the DOM.
// ---------------------------------------------------------------------------

const SECRET_RULES: SafetyRule[] = [
  {
    id: 'openai-key',
    type: 'SECRET_LEAK',
    severity: 'CRITICAL',
    pattern: /sk-[a-zA-Z0-9]{20,}/g,
    replacement: '[REDACTED:API_KEY]',
    description: 'OpenAI / generic sk- API key',
  },
  {
    id: 'aws-access-key',
    type: 'SECRET_LEAK',
    severity: 'CRITICAL',
    pattern: /AKIA[0-9A-Z]{16}/g,
    replacement: '[REDACTED:AWS_KEY]',
    description: 'AWS access key ID (AKIA…)',
  },
  {
    id: 'github-pat',
    type: 'SECRET_LEAK',
    severity: 'CRITICAL',
    pattern: /ghp_[a-zA-Z0-9]{36}/g,
    replacement: '[REDACTED:GH_TOKEN]',
    description: 'GitHub personal access token (ghp_…)',
  },
  {
    id: 'bearer-token',
    type: 'SECRET_LEAK',
    severity: 'HIGH',
    // Negative look-behind avoids matching "Bearer [REDACTED:…]" recursively.
    pattern: /Bearer\s+(?!\[REDACTED)[a-zA-Z0-9\-._~+/]{20,}/g,
    replacement: 'Bearer [REDACTED:TOKEN]',
    description: 'Generic HTTP Bearer token',
  },
  {
    id: 'private-key-header',
    type: 'SECRET_LEAK',
    severity: 'CRITICAL',
    pattern: /-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/g,
    replacement: '[REDACTED:PRIVATE_KEY_HEADER]',
    description: 'PEM private key header',
  },
];

// ---------------------------------------------------------------------------
// PII — personally identifiable information.
// ---------------------------------------------------------------------------

const PII_RULES: SafetyRule[] = [
  {
    id: 'email',
    type: 'PII',
    severity: 'MEDIUM',
    pattern: /[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}/g,
    replacement: '[REDACTED:EMAIL]',
    description: 'Email address',
  },
  {
    id: 'ipv4',
    type: 'PII',
    severity: 'LOW',
    pattern: /\b(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\b/g,
    replacement: '[REDACTED:IP]',
    description: 'IPv4 address',
  },
  {
    id: 'ssn',
    type: 'PII',
    severity: 'CRITICAL',
    pattern: /\b\d{3}-\d{2}-\d{4}\b/g,
    replacement: '[REDACTED:SSN]',
    description: 'US Social Security Number',
  },
  {
    id: 'credit-card',
    type: 'PII',
    severity: 'CRITICAL',
    // Luhn-adjacent: 13-19 digit groups separated by spaces or dashes.
    pattern: /\b(?:\d[ \-]?){13,18}\d\b/g,
    replacement: '[REDACTED:CARD]',
    description: 'Possible credit card number',
  },
];

// ---------------------------------------------------------------------------
// RESTRICTED — dangerous code patterns.
// ---------------------------------------------------------------------------

const RESTRICTED_RULES: SafetyRule[] = [
  {
    id: 'eval-call',
    type: 'RESTRICTED',
    severity: 'HIGH',
    pattern: /\beval\s*\(/g,
    replacement: '[BLOCKED:eval(]',
    description: 'eval() — common code injection vector',
  },
  {
    id: 'os-system',
    type: 'RESTRICTED',
    severity: 'HIGH',
    pattern: /\bos\.system\s*\(/g,
    replacement: '[BLOCKED:os.system(]',
    description: 'os.system() shell execution',
  },
  {
    id: 'subprocess-shell-true',
    type: 'RESTRICTED',
    severity: 'HIGH',
    pattern: /subprocess\.\w+\([^)]*shell\s*=\s*True/g,
    replacement: '[BLOCKED:subprocess(shell=True)]',
    description: 'subprocess with shell=True',
  },
  {
    id: 'powershell-encoded',
    type: 'RESTRICTED',
    severity: 'HIGH',
    pattern: /powershell\s+.*?-[Ee]nc(?:odedCommand)?\s+[A-Za-z0-9+/=]{20,}/g,
    replacement: '[BLOCKED:PS_ENCODED_CMD]',
    description: 'PowerShell encoded command (common obfuscation technique)',
  },
];

// ---------------------------------------------------------------------------
// Exported flat list — order matters: CRITICAL rules run first.
// ---------------------------------------------------------------------------

export const SAFETY_RULES: readonly SafetyRule[] = [
  ...SECRET_RULES,
  ...PII_RULES,
  ...RESTRICTED_RULES,
];
