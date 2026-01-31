export interface GenerationChunk { token: string; isFinal?: boolean }

export interface ParseResult<T> { items: T[]; remaining: string }

export function parseSse(buffer: string): ParseResult<GenerationChunk> {
  const parts = buffer.split('\n\n');
  const tokens: GenerationChunk[] = [];
  let consumed = 0;
  for (let i = 0; i < parts.length - 1; i++) {
    const block = parts[i];
    const lines = block.split('\n');
    for (const l of lines) {
      if (l.startsWith('data:')) {
        const payload = l.slice(5).trim();
        if (payload === '[DONE]') {
          tokens.push({ token: '', isFinal: true });
          continue;
        }
        try {
          const json = JSON.parse(payload);
          const t = extractTokenFromJson(json);
          if (t) tokens.push({ token: t });
        } catch { }
      }
    }
    consumed += block.length + 2;
  }
  return { items: tokens, remaining: buffer.slice(consumed) };
}

export function parseNdjson(buffer: string): ParseResult<GenerationChunk> {
  const lines = buffer.split('\n');
  const tokens: GenerationChunk[] = [];
  let consumed = 0;
  for (let i = 0; i < lines.length - 1; i++) {
    const line = lines[i].trim();
    if (!line) { consumed += lines[i].length + 1; continue; }
    try {
      const json = JSON.parse(line);
      const t = extractTokenFromJson(json);
      if (t) tokens.push({ token: t });
      consumed += lines[i].length + 1;
    } catch {
      break;
    }
  }
  return { items: tokens, remaining: buffer.slice(consumed) };
}

export function parseJsonSequence(buffer: string): ParseResult<GenerationChunk> {
  const tokens: GenerationChunk[] = [];
  let idx = 0;
  while (idx < buffer.length) {
    const start = buffer.indexOf('{', idx);
    if (start === -1) break;
    let depth = 0;
    let end = -1;
    for (let i = start; i < buffer.length; i++) {
      const c = buffer[i];
      if (c === '{') depth++;
      else if (c === '}') {
        depth--;
        if (depth === 0) { end = i; break; }
      }
    }
    if (end === -1) break;
    const slice = buffer.slice(start, end + 1);
    try {
      const json = JSON.parse(slice);
      const t = extractTokenFromJson(json);
      if (t) tokens.push({ token: t });
      idx = end + 1;
    } catch {
      break;
    }
  }
  return { items: tokens, remaining: buffer.slice(idx) };
}

export function extractTokenFromJson(obj: any): string | null {
  if (!obj || typeof obj !== 'object') return null;

  // Generic: direct token field
  if (typeof obj.token === 'string') return obj.token;

  // OpenAI / OpenAI-compatible: choices[].delta.content or choices[].text
  if (obj.choices && Array.isArray(obj.choices)) {
    const c = obj.choices[0];
    if (c?.delta?.content != null) return String(c.delta.content);
    if (c?.text != null) return String(c.text);
    if (c?.message?.content != null) return String(c.message.content);
  }

  // Anthropic streaming: content_block_delta.delta.text or delta.text
  if (obj.type === 'content_block_delta' && obj.delta?.text != null) return String(obj.delta.text);
  if (obj.delta?.text != null) return String(obj.delta.text);

  // Mistral: choices[].delta.content (handled above) or output field
  if (typeof obj.output === 'string') return obj.output;

  // Together / generic: outputs[].text
  if (Array.isArray(obj.outputs) && obj.outputs[0]?.text != null) return String(obj.outputs[0].text);

  // Ollama: response field
  if (typeof obj.response === 'string') return obj.response;

  // Generic fallback fields
  if (typeof obj.delta === 'string') return obj.delta;
  if (obj.message?.content != null) return String(obj.message.content);
  if (typeof obj.content === 'string') return obj.content;
  if (typeof obj.text === 'string') return obj.text;

  return null;
}

/** Fallback: emit raw text when JSON parsing fails (useful for plain-text / malformed streams) */
export function extractRawTextFallback(buffer: string): ParseResult<GenerationChunk> {
  // Emit any printable text accumulated so far as a single token; caller clears buffer
  const trimmed = buffer.replace(/[\x00-\x08\x0b\x0c\x0e-\x1f]/g, '');
  if (trimmed.length > 0) {
    return { items: [{ token: trimmed }], remaining: '' };
  }
  return { items: [], remaining: '' };
}

export class StreamTokenizer {
  private buffer = '';
  constructor(private readonly mode: 'sse' | 'ndjson' | 'json') { }
  push(chunk: Uint8Array): GenerationChunk[] {
    this.buffer += new TextDecoder().decode(chunk);
    let parsed: ParseResult<GenerationChunk>;
    if (this.mode === 'sse') parsed = parseSse(this.buffer);
    else if (this.mode === 'ndjson') parsed = parseNdjson(this.buffer);
    else parsed = parseJsonSequence(this.buffer);
    this.buffer = parsed.remaining;
    return parsed.items;
  }
  flushFinal(): GenerationChunk[] {
    const out: GenerationChunk[] = [];
    if (this.buffer.trim().length) {
      const r = this.mode === 'sse' ? parseSse(this.buffer) : this.mode === 'ndjson' ? parseNdjson(this.buffer) : parseJsonSequence(this.buffer);
      out.push(...r.items);
      this.buffer = r.remaining;
    }
    out.push({ token: '', isFinal: true });
    return out;
  }
}
