/**
 * Streams GGUF header/metadata/tensor table from a file or ranged URL source (manifest slice).
 * Parses structure for inspection only — does not load full tensor weights into a runtime or run inference.
 *
 * Quick verify: pick a small .gguf → `loadGgufModelSummary` → expect overview + tensor list; on failure check path/URL and Range support.
 */
const GGUF_VALUE_TYPES = {
  0: 'uint8',
  1: 'int8',
  2: 'uint16',
  3: 'int16',
  4: 'uint32',
  5: 'int32',
  6: 'float32',
  7: 'bool',
  8: 'string',
  9: 'array',
  10: 'uint64',
  11: 'int64',
  12: 'float64'
};

const FIXED_WIDTH_TYPES = {
  0: 1,
  1: 1,
  2: 2,
  3: 2,
  4: 4,
  5: 4,
  6: 4,
  7: 1,
  10: 8,
  11: 8,
  12: 8
};

const GGML_TENSOR_TYPES = {
  0: 'F32',
  1: 'F16',
  2: 'Q4_0',
  3: 'Q4_1',
  6: 'Q5_0',
  7: 'Q5_1',
  8: 'Q8_0',
  9: 'Q8_1',
  10: 'Q2_K',
  11: 'Q3_K',
  12: 'Q4_K',
  13: 'Q5_K',
  14: 'Q6_K',
  15: 'Q8_K',
  16: 'IQ2_XXS',
  17: 'IQ2_XS',
  18: 'IQ3_XXS',
  19: 'IQ1_S',
  20: 'IQ4_NL',
  21: 'IQ3_S',
  22: 'IQ2_S',
  23: 'IQ4_XS',
  24: 'I8',
  25: 'I16',
  26: 'I32',
  27: 'I64',
  28: 'F64',
  29: 'IQ1_M',
  30: 'BF16',
  34: 'TQ1_0',
  35: 'TQ2_0'
};

const UTF8 = new TextDecoder('utf-8');
const MAX_SAFE_BIGINT = BigInt(Number.MAX_SAFE_INTEGER);

function readBigUint64(view, offset = 0) {
  if (typeof view.getBigUint64 === 'function') {
    return view.getBigUint64(offset, true);
  }
  const low = BigInt(view.getUint32(offset, true));
  const high = BigInt(view.getUint32(offset + 4, true));
  return (high << 32n) | low;
}

function readBigInt64(view, offset = 0) {
  if (typeof view.getBigInt64 === 'function') {
    return view.getBigInt64(offset, true);
  }
  const value = readBigUint64(view, offset);
  return value > 0x7fffffffffffffffn ? value - 0x10000000000000000n : value;
}

function toDisplayNumber(value) {
  if (typeof value !== 'bigint') return value;
  return value <= MAX_SAFE_BIGINT ? Number(value) : value.toString();
}

function toBigInt(value) {
  if (typeof value === 'bigint') return value;
  if (typeof value === 'number') return BigInt(value);
  return BigInt(String(value));
}

function alignTo(value, alignment) {
  const current = toBigInt(value);
  const size = BigInt(alignment || 32);
  if (size <= 0n) return current;
  return ((current + size - 1n) / size) * size;
}

function truncateText(value, maxLength = 160) {
  if (typeof value !== 'string') return value;
  if (value.length <= maxLength) return value;
  return `${value.slice(0, maxLength)}...`;
}

function normalizeMetadataValue(value) {
  if (typeof value === 'bigint') return toDisplayNumber(value);
  if (Array.isArray(value)) return value.map(normalizeMetadataValue);
  if (value && typeof value === 'object') {
    return Object.fromEntries(
      Object.entries(value).map(([key, inner]) => [key, normalizeMetadataValue(inner)])
    );
  }
  return value;
}

class SequentialByteReader {
  constructor(source, { windowSize = 1024 * 1024 } = {}) {
    this.source = source;
    this.windowSize = windowSize;
    this.cursor = 0;
    this.bufferStart = 0;
    this.buffer = new Uint8Array(0);
  }

  async ensure(size) {
    const localOffset = this.cursor - this.bufferStart;
    if (localOffset >= 0 && localOffset + size <= this.buffer.length) {
      return;
    }
    const remaining = Math.max(0, this.source.size - this.cursor);
    const nextSize = Math.min(remaining, Math.max(this.windowSize, size));
    this.bufferStart = this.cursor;
    this.buffer = nextSize > 0 ? await this.source.read(this.cursor, nextSize) : new Uint8Array(0);
  }

  async readBytes(length) {
    if (length === 0) return new Uint8Array(0);
    const output = new Uint8Array(length);
    let written = 0;
    while (written < length) {
      await this.ensure(1);
      const localOffset = this.cursor - this.bufferStart;
      const available = Math.min(length - written, this.buffer.length - localOffset);
      if (available <= 0) {
        throw new Error(
          'Unexpected end of source while streaming GGUF bytes. Try another file or reconnect; for URLs confirm Range requests work.'
        );
      }
      output.set(this.buffer.subarray(localOffset, localOffset + available), written);
      this.cursor += available;
      written += available;
    }
    return output;
  }

  async readView(length) {
    const bytes = await this.readBytes(length);
    return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  }

  async readUint8() {
    const view = await this.readView(1);
    return view.getUint8(0);
  }

  async readInt8() {
    const view = await this.readView(1);
    return view.getInt8(0);
  }

  async readUint16() {
    const view = await this.readView(2);
    return view.getUint16(0, true);
  }

  async readInt16() {
    const view = await this.readView(2);
    return view.getInt16(0, true);
  }

  async readUint32() {
    const view = await this.readView(4);
    return view.getUint32(0, true);
  }

  async readInt32() {
    const view = await this.readView(4);
    return view.getInt32(0, true);
  }

  async readFloat32() {
    const view = await this.readView(4);
    return view.getFloat32(0, true);
  }

  async readFloat64() {
    const view = await this.readView(8);
    return view.getFloat64(0, true);
  }

  async readUint64() {
    const view = await this.readView(8);
    return readBigUint64(view, 0);
  }

  async readInt64() {
    const view = await this.readView(8);
    return readBigInt64(view, 0);
  }

  async readString({ previewBytes = Infinity } = {}) {
    const byteLength = await this.readUint64();
    const length = toBigInt(byteLength);
    const previewLength = Number(length > BigInt(previewBytes) ? BigInt(previewBytes) : length);
    const preview = UTF8.decode(await this.readBytes(previewLength));
    const remaining = length - BigInt(previewLength);
    if (remaining > 0n) {
      this.skip(Number(remaining));
      return `${preview}...`;
    }
    return preview;
  }

  skip(length) {
    this.cursor += length;
  }
}

function notifyProgress(onProgress, phase, reader, source, detail) {
  if (!onProgress) return;
  const scanned = Math.min(reader.cursor, source.size);
  const percent = source.size > 0 ? Math.min(99, Math.round((scanned / source.size) * 100)) : 0;
  onProgress({ phase, percent, scannedBytes: scanned, totalBytes: source.size, detail });
}

async function readScalarMetadataValue(reader, valueType) {
  switch (valueType) {
    case 0:
      return reader.readUint8();
    case 1:
      return reader.readInt8();
    case 2:
      return reader.readUint16();
    case 3:
      return reader.readInt16();
    case 4:
      return reader.readUint32();
    case 5:
      return reader.readInt32();
    case 6:
      return reader.readFloat32();
    case 7:
      return (await reader.readUint8()) !== 0;
    case 8:
      return reader.readString({ previewBytes: 256 });
    case 10:
      return toDisplayNumber(await reader.readUint64());
    case 11:
      return toDisplayNumber(await reader.readInt64());
    case 12:
      return reader.readFloat64();
    default:
      throw new Error(`Unsupported GGUF metadata value type ${valueType}`);
  }
}

async function skipMetadataValue(reader, valueType) {
  if (valueType === 8) {
    const length = await reader.readUint64();
    reader.skip(Number(length));
    return;
  }
  if (valueType === 9) {
    const nestedType = await reader.readUint32();
    const length = await reader.readUint64();
    const count = Number(length);
    if (FIXED_WIDTH_TYPES[nestedType]) {
      reader.skip(count * FIXED_WIDTH_TYPES[nestedType]);
      return;
    }
    for (let index = 0; index < count; index += 1) {
      await skipMetadataValue(reader, nestedType);
    }
    return;
  }
  const width = FIXED_WIDTH_TYPES[valueType];
  if (!width) {
    throw new Error(
      `Cannot skip unsupported GGUF metadata value type ${valueType}. Try another model file or update the loader for this GGUF version.`
    );
  }
  reader.skip(width);
}

async function readMetadataValue(reader, valueType, options) {
  if (valueType !== 9) {
    return normalizeMetadataValue(await readScalarMetadataValue(reader, valueType));
  }

  const nestedType = await reader.readUint32();
  const length = Number(await reader.readUint64());
  const previewLimit = options.arrayPreviewLimit ?? 6;
  const preview = [];
  const count = Math.min(length, previewLimit);
  for (let index = 0; index < count; index += 1) {
    preview.push(normalizeMetadataValue(await readMetadataValue(reader, nestedType, options)));
  }
  for (let index = count; index < length; index += 1) {
    await skipMetadataValue(reader, nestedType);
  }

  return {
    kind: 'array',
    valueType: GGUF_VALUE_TYPES[nestedType] || `type-${nestedType}`,
    length,
    preview,
    truncated: length > count
  };
}

function getMetadataNumber(metadataMap, keys) {
  for (const key of keys) {
    const value = metadataMap[key];
    if (typeof value === 'number' && Number.isFinite(value)) return value;
    if (typeof value === 'string' && /^\d+$/.test(value)) return Number(value);
  }
  return null;
}

function getMetadataString(metadataMap, keys) {
  for (const key of keys) {
    const value = metadataMap[key];
    if (typeof value === 'string' && value.trim()) return value;
  }
  return null;
}

function compareBigIntDescending(left, right) {
  if (left === right) return 0;
  return left > right ? -1 : 1;
}

export function formatBytes(value) {
  const numeric = typeof value === 'bigint' ? Number(value <= MAX_SAFE_BIGINT ? value : MAX_SAFE_BIGINT) : value;
  if (!Number.isFinite(numeric) || numeric < 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let current = numeric;
  let unit = 0;
  while (current >= 1024 && unit < units.length - 1) {
    current /= 1024;
    unit += 1;
  }
  const precision = unit === 0 ? 0 : current >= 100 ? 0 : current >= 10 ? 1 : 2;
  return `${current.toFixed(precision)} ${units[unit]}`;
}

export function formatCount(value) {
  if (typeof value === 'bigint') return value.toString();
  if (typeof value === 'number') return value.toLocaleString();
  return String(value ?? '0');
}

export function createFileByteSource(file) {
  return {
    kind: 'file',
    name: file.name,
    label: file.name,
    size: file.size,
    async read(offset, length) {
      const next = file.slice(offset, offset + length);
      return new Uint8Array(await next.arrayBuffer());
    }
  };
}

export async function createUrlByteSource(url) {
  const normalizedUrl = url.trim();
  const headResponse = await fetch(normalizedUrl, { method: 'HEAD' });
  if (!headResponse.ok) {
    throw new Error(`HEAD probe failed with status ${headResponse.status}`);
  }

  const headerLength = Number(headResponse.headers.get('content-length'));
  const acceptRanges = headResponse.headers.get('accept-ranges') || '';
  if (!Number.isFinite(headerLength) || headerLength <= 0) {
    throw new Error('Remote source did not provide a content length');
  }
  if (!/bytes/i.test(acceptRanges)) {
    throw new Error('Remote source must support byte-range requests for streaming loads');
  }

  return {
    kind: 'url',
    name: normalizedUrl,
    label: normalizedUrl,
    size: headerLength,
    async read(offset, length) {
      const end = Math.min(headerLength - 1, offset + length - 1);
      const response = await fetch(normalizedUrl, {
        headers: {
          Range: `bytes=${offset}-${end}`
        }
      });
      if (!(response.ok || response.status === 206)) {
        throw new Error(
          `Range request failed with status ${response.status}. Retry or switch to a local .gguf if the host blocks partial content.`
        );
      }
      return new Uint8Array(await response.arrayBuffer());
    }
  };
}

export async function loadGgufModelSummary(source, options = {}) {
  const reader = new SequentialByteReader(source, { windowSize: options.windowSize ?? 1024 * 1024 });
  const warnings = [];
  const metadataEntries = [];
  const metadataMap = {};
  const tensorTypeHistogram = {};

  notifyProgress(options.onProgress, 'header', reader, source, 'Reading GGUF header');

  const magic = UTF8.decode(await reader.readBytes(4));
  if (magic !== 'GGUF') {
    throw new Error(
      `Expected GGUF magic, found ${magic || 'unknown bytes'}. Pick a valid .gguf file or confirm the download completed.`
    );
  }

  const version = await reader.readUint32();
  const tensorCount = Number(await reader.readUint64());
  const metadataCount = Number(await reader.readUint64());

  if (version < 2 || version > 3) {
    warnings.push(`GGUF version ${version} is outside the common v2-v3 range`);
  }

  for (let index = 0; index < metadataCount; index += 1) {
    const key = await reader.readString({ previewBytes: 256 });
    const valueType = await reader.readUint32();
    const value = await readMetadataValue(reader, valueType, {
      arrayPreviewLimit: options.arrayPreviewLimit ?? 6
    });
    const normalizedValue = normalizeMetadataValue(value);

    metadataEntries.push({
      key,
      type: GGUF_VALUE_TYPES[valueType] || `type-${valueType}`,
      value: normalizedValue
    });
    metadataMap[key] = normalizedValue;

    if ((index + 1) % 8 === 0 || index === metadataCount - 1) {
      notifyProgress(
        options.onProgress,
        'metadata',
        reader,
        source,
        `Metadata ${index + 1}/${metadataCount}`
      );
    }
  }

  const tensors = [];
  for (let index = 0; index < tensorCount; index += 1) {
    const name = await reader.readString({ previewBytes: 256 });
    const dimensionCount = await reader.readUint32();
    const dimensions = [];
    for (let axis = 0; axis < dimensionCount; axis += 1) {
      dimensions.push(toDisplayNumber(await reader.readUint64()));
    }
    const tensorTypeCode = await reader.readUint32();
    const relativeOffset = await reader.readUint64();
    const elementCount = dimensions.reduce((product, dimension) => product * toBigInt(dimension), 1n);
    const typeName = GGML_TENSOR_TYPES[tensorTypeCode] || `type-${tensorTypeCode}`;

    tensors.push({
      name,
      dimensions,
      dimensionCount,
      elementCount,
      tensorTypeCode,
      tensorType: typeName,
      relativeOffset
    });
    tensorTypeHistogram[typeName] = (tensorTypeHistogram[typeName] || 0) + 1;

    if ((index + 1) % 16 === 0 || index === tensorCount - 1) {
      notifyProgress(options.onProgress, 'tensors', reader, source, `Tensor ${index + 1}/${tensorCount}`);
    }
  }

  const alignment = getMetadataNumber(metadataMap, ['general.alignment']) || 32;
  const dataOffset = alignTo(reader.cursor, alignment);
  const sortedByOffset = [...tensors].sort((left, right) => compareBigIntDescending(right.relativeOffset, left.relativeOffset));
  const totalDataBytes = BigInt(Math.max(0, source.size - Number(dataOffset)));

  for (let index = 0; index < sortedByOffset.length; index += 1) {
    const current = sortedByOffset[index];
    const next = sortedByOffset[index + 1];
    const endOffset = next ? next.relativeOffset : totalDataBytes;
    current.approxBytes = endOffset > current.relativeOffset ? endOffset - current.relativeOffset : 0n;
    current.absoluteOffset = dataOffset + current.relativeOffset;
  }

  let sumApproxBytes = 0n;
  for (const t of tensors) {
    sumApproxBytes += t.approxBytes;
  }
  if (tensorCount > 0 && sumApproxBytes > totalDataBytes) {
    warnings.push(
      `Tensor layout spans ~${formatBytes(sumApproxBytes)} but the weight region is ~${formatBytes(
        totalDataBytes
      )} (${tensorCount.toLocaleString()} tensors in header) — file may be truncated or misaligned.`
    );
  }

  const architecture = getMetadataString(metadataMap, ['general.architecture']) || 'unknown';
  const overview = {
    modelName: getMetadataString(metadataMap, ['general.name', 'tokenizer.ggml.model']) || source.name,
    architecture,
    alignment,
    contextLength: getMetadataNumber(metadataMap, [`${architecture}.context_length`, 'general.context_length', 'n_ctx']),
    embeddingLength: getMetadataNumber(metadataMap, [`${architecture}.embedding_length`, 'general.embedding_length', 'n_embd']),
    blockCount: getMetadataNumber(metadataMap, [`${architecture}.block_count`, 'general.block_count', 'n_layer']),
    headCount: getMetadataNumber(metadataMap, [`${architecture}.attention.head_count`, 'general.attention.head_count']),
    vocabularySize: getMetadataNumber(metadataMap, ['tokenizer.ggml.tokens', 'general.vocab_size', 'n_vocab'])
  };

  notifyProgress(options.onProgress, 'ready', { cursor: source.size }, source, 'Streamed GGUF manifest ready');

  return {
    source: {
      kind: source.kind,
      name: source.name,
      label: source.label,
      size: source.size
    },
    header: {
      magic,
      version,
      tensorCount,
      metadataCount,
      alignment,
      dataOffset
    },
    overview,
    metadata: metadataEntries,
    metadataMap,
    tensors,
    tensorTypeHistogram,
    warnings,
    stats: {
      scannedBytes: reader.cursor,
      fileSize: source.size,
      dataBytes: totalDataBytes
    }
  };
}