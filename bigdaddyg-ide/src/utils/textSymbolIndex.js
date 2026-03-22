/**
 * Line-oriented symbol extraction from source text (no LSP, no AST).
 *
 * Limitations (documented):
 * - Does not parse AST; regexes can false-positive inside string literals or templates.
 * - Whole-line comment skip is naive (`//`, `#`, `*`, `/*`); block comments spanning lines may still match inner lines.
 * - Capped at MAX_SYMBOLS (500); order is first-seen in file.
 */

const MAX_SYMBOLS = 500;

function extFromPath(filePath) {
  const m = String(filePath || '')
    .toLowerCase()
    .match(/\.([a-z0-9]+)$/);
  return m ? m[1] : '';
}

/** @param {string} line */
function skipCommentOnlyLine(line) {
  const t = line.trimStart();
  if (!t) return true;
  if (t.startsWith('//')) return true;
  if (t.startsWith('#')) return true;
  if (t.startsWith('/*') && t.endsWith('*/')) return true;
  if (t.startsWith('*')) return true;
  return false;
}

/**
 * @param {string} line
 * @param {{ re: RegExp, kind: string, group?: number }[]} rules
 * @returns {{ name: string, kind: string } | null}
 */
function firstMatch(line, rules) {
  for (const { re, kind, group = 1 } of rules) {
    const m = line.match(re);
    if (m && m[group]) {
      return { name: m[group], kind };
    }
  }
  return null;
}

const jsTsRules = [
  { re: /export\s+async\s+function\s+(\w+)/, kind: 'function' },
  { re: /export\s+function\s+(\w+)/, kind: 'function' },
  { re: /export\s+class\s+(\w+)/, kind: 'class' },
  { re: /export\s+interface\s+(\w+)/, kind: 'interface' },
  { re: /export\s+type\s+(\w+)\s*=/, kind: 'type' },
  { re: /export\s+const\s+(\w+)\s*=\s*(?:async\s*)?\(/, kind: 'function' },
  { re: /export\s+let\s+(\w+)\s*=\s*(?:async\s*)?\(/, kind: 'function' },
  { re: /function\s+(\w+)\s*[<(]/, kind: 'function' },
  { re: /class\s+(\w+)/, kind: 'class' },
  { re: /interface\s+(\w+)/, kind: 'interface' },
  { re: /\btype\s+(\w+)\s*=/, kind: 'type' },
  { re: /const\s+(\w+)\s*=\s*(?:async\s*)?\(/, kind: 'function' },
  { re: /let\s+(\w+)\s*=\s*(?:async\s*)?\(/, kind: 'function' },
  { re: /var\s+(\w+)\s*=\s*(?:async\s*)?\(/, kind: 'function' }
];

const pyRules = [
  { re: /^\s*async\s+def\s+(\w+)\s*\(/, kind: 'function' },
  { re: /^\s*def\s+(\w+)\s*\(/, kind: 'function' },
  { re: /^\s*class\s+(\w+)\s*[:(]/, kind: 'class' }
];

const csRules = [
  { re: /\bclass\s+(\w+)\b/, kind: 'class' },
  { re: /\binterface\s+(\w+)\b/, kind: 'interface' },
  { re: /\bstruct\s+(\w+)\b/, kind: 'struct' },
  { re: /\benum\s+(\w+)\b/, kind: 'enum' },
  { re: /\bdelegate\s+.+?\s+(\w+)\s*\(/, kind: 'delegate' },
  {
    re: /\b(?:public|private|protected|internal)\s+(?:static\s+|virtual\s+|override\s+|abstract\s+|async\s+)*(?:void|[\w.<>,\s\[\]?]+)\s+(\w+)\s*\(/,
    kind: 'method'
  }
];

const rsRules = [
  { re: /^\s*(?:pub\s+)?fn\s+(\w+)\s*\(/, kind: 'function' },
  { re: /^\s*(?:pub\s+)?struct\s+(\w+)\b/, kind: 'struct' },
  { re: /^\s*(?:pub\s+)?enum\s+(\w+)\b/, kind: 'enum' },
  { re: /^\s*(?:pub\s+)?trait\s+(\w+)\b/, kind: 'trait' },
  { re: /^\s*(?:pub\s+)?impl\s+(?:[\w:]+\s+for\s+)?(\w+)\b/, kind: 'impl' },
  { re: /^\s*type\s+(\w+)\s*=/, kind: 'type' }
];

const goRules = [
  { re: /^\s*func\s+(?:\([^)]*\)\s*)?(\w+)\s*\(/, kind: 'function' },
  { re: /^\s*type\s+(\w+)\s+interface\s*\{/, kind: 'interface' },
  { re: /^\s*type\s+(\w+)\s+struct\s*\{/, kind: 'struct' }
];

function rulesForExt(ext) {
  const jsLike = new Set(['js', 'jsx', 'mjs', 'cjs', 'ts', 'tsx']);
  if (jsLike.has(ext)) return jsTsRules;
  if (ext === 'py') return pyRules;
  if (ext === 'cs') return csRules;
  if (ext === 'rs') return rsRules;
  if (ext === 'go') return goRules;
  return jsTsRules;
}

/** Monaco language id → rules bucket (before AI / LSP: better than pathless buffers). */
function rulesForLanguageId(id) {
  const lid = String(id || '').toLowerCase();
  if (/^(javascript|javascriptreact|typescript|typescriptreact|jsx|tsx)$/.test(lid)) return jsTsRules;
  if (lid === 'python') return pyRules;
  if (lid === 'csharp') return csRules;
  if (lid === 'rust') return rsRules;
  if (lid === 'go') return goRules;
  return null;
}

/**
 * @param {string} text
 * @param {string} [filePath]
 * @param {string} [monacoLanguageId] e.g. typescript, python — used when path has no extension
 * @returns {{ name: string, kind: string, line: number }[]}
 */
export function extractSymbolsFromText(text, filePath = '', monacoLanguageId = '') {
  if (text == null || text === '') return [];
  const byLang = rulesForLanguageId(monacoLanguageId);
  const rules = byLang || rulesForExt(extFromPath(filePath));
  const lines = String(text).split(/\r?\n/);
  const out = [];
  for (let i = 0; i < lines.length && out.length < MAX_SYMBOLS; i++) {
    const line = lines[i];
    if (skipCommentOnlyLine(line)) continue;
    const hit = firstMatch(line, rules);
    if (hit) {
      out.push({ name: hit.name, kind: hit.kind, line: i + 1 });
    }
  }
  return out;
}

export { MAX_SYMBOLS };
