/**
 * restricted_keywords.ts
 * High-maintenance keyword lists kept separate from the core regex rules.
 * Populate RESTRICTED_KEYWORDS with any terms your deployment policy prohibits.
 * Each entry is escaped and compiled into a case-insensitive RegExp at runtime.
 *
 * NOTE: This file intentionally ships empty. Fill it according to your
 * organisation's content policy before deploying to production.
 */

/**
 * Plain-text keywords to block. Regex metacharacters are automatically escaped.
 * Example: ['rm -rf', '__import__', 'DROP TABLE']
 */
export const RESTRICTED_KEYWORDS: string[] = [
  // Add deployment-specific restricted terms here.
];
