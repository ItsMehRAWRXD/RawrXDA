/**
 * Per-workspace knowledge JSON under Electron userData/knowledge/<fp>.json.
 * @see docs/PROJECT_KNOWLEDGE_STORE.md
 */
'use strict';

const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');

const MAX_ARTIFACTS = 500;
const MAX_TEXT_CHARS = 500_000;

function workspaceFingerprint(projectRoot) {
  const norm = String(projectRoot || '')
    .replace(/\\/g, '/')
    .trim()
    .toLowerCase();
  return crypto.createHash('sha256').update(norm).digest('hex').slice(0, 16);
}

function emptyDoc() {
  return { version: 1, artifacts: [], signatureStats: {} };
}

function normalizeDoc(j) {
  const out = emptyDoc();
  if (!j || typeof j !== 'object') return out;
  if (Array.isArray(j.artifacts)) {
    out.artifacts = j.artifacts.filter(Boolean).slice(-MAX_ARTIFACTS);
  }
  if (j.signatureStats && typeof j.signatureStats === 'object') {
    out.signatureStats = { ...j.signatureStats };
  }
  return out;
}

class KnowledgeStore {
  /**
   * @param {string} userDataPath — `app.getPath('userData')`
   */
  constructor(userDataPath) {
    this.baseDir = path.join(userDataPath, 'knowledge');
  }

  async ensureDir() {
    await fs.mkdir(this.baseDir, { recursive: true });
  }

  /** @param {string} fp */
  storePath(fp) {
    return path.join(this.baseDir, `${fp}.json`);
  }

  /**
   * @param {string | null} projectRoot
   * @returns {Promise<{ fp: string, doc: ReturnType<typeof emptyDoc> }>}
   */
  async load(projectRoot) {
    const fp = workspaceFingerprint(projectRoot);
    const p = this.storePath(fp);
    try {
      const raw = await fs.readFile(p, 'utf8');
      const j = JSON.parse(raw);
      return { fp, doc: normalizeDoc(j) };
    } catch (e) {
      if (e && e.code === 'ENOENT') return { fp, doc: emptyDoc() };
      throw e;
    }
  }

  /**
   * @param {string} fp
   * @param {ReturnType<typeof emptyDoc>} doc
   */
  async save(fp, doc) {
    await this.ensureDir();
    const p = this.storePath(fp);
    const tmp = `${p}.tmp`;
    await fs.writeFile(tmp, JSON.stringify(doc, null, 2), 'utf8');
    await fs.rename(tmp, p);
  }

  /**
   * @param {string | null} projectRoot
   * @param {Record<string, unknown>} rec
   */
  async appendArtifact(projectRoot, rec) {
    const { fp, doc } = await this.load(projectRoot);
    const r = rec && typeof rec === 'object' ? rec : {};
    const entry = {
      at: new Date().toISOString(),
      kind: String(r.kind || 'unknown').slice(0, 128),
      path: String(r.path || '').slice(0, 512),
      text: typeof r.text === 'string' ? r.text.slice(0, MAX_TEXT_CHARS) : ''
    };
    doc.artifacts.push(entry);
    if (doc.artifacts.length > MAX_ARTIFACTS) {
      doc.artifacts = doc.artifacts.slice(-MAX_ARTIFACTS);
    }
    await this.save(fp, doc);
  }

  /**
   * @param {string | null} projectRoot
   * @param {string[]} signatures
   */
  async rankSignatures(projectRoot, signatures) {
    const { doc } = await this.load(projectRoot);
    const arr = Array.isArray(signatures) ? signatures : [];
    const ranked = arr.map((sig) => {
      const key = String(sig || '').slice(0, 400);
      const st = doc.signatureStats[key] || { ok: 0, fail: 0 };
      const score = st.ok * 2 - st.fail;
      return { signature: key, score, ok: st.ok, fail: st.fail };
    });
    ranked.sort((a, b) => b.score - a.score);
    return ranked;
  }

  /**
   * @param {string | null} projectRoot
   * @param {string} signature
   * @param {boolean} success
   */
  async recordSignatureOutcome(projectRoot, signature, success) {
    const { fp, doc } = await this.load(projectRoot);
    const key = String(signature || '').slice(0, 400);
    if (!doc.signatureStats[key]) doc.signatureStats[key] = { ok: 0, fail: 0 };
    if (success) doc.signatureStats[key].ok += 1;
    else doc.signatureStats[key].fail += 1;
    await this.save(fp, doc);
    return doc.signatureStats[key];
  }
}

/** @type {KnowledgeStore | null} */
let singleton = null;

/**
 * @param {import('electron').App} app
 * @returns {KnowledgeStore}
 */
function createKnowledgeStore(app) {
  if (!singleton) {
    singleton = new KnowledgeStore(app.getPath('userData'));
  }
  return singleton;
}

module.exports = { createKnowledgeStore, workspaceFingerprint };
