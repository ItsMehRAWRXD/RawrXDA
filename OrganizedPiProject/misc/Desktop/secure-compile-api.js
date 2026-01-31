'use strict';
const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs').promises;
const os = require('os');
const crypto = require('crypto');

const app = express();
app.use(express.json({ limit: '256kb' }));

const TMP = path.join(os.tmpdir(), 'rawrz-secure-builds');
const IMAGE = 'rawrz-secure-compiler:1.0';

function hmac(data, secret) {
  return secret ? crypto.createHmac('sha256', secret).update(data).digest('hex') : null;
}

app.post('/compile', async (req, res) => {
  try {
    const { language, filename, source, timeout = 10, signSecret } = req.body || {};
    const allow = new Set(['c','cpp','java','node','python']);
    if (!allow.has(String(language))) return res.status(400).json({ error: 'Unsupported language' });
    if (!filename || !/^[\w.\-]{1,128}$/.test(filename)) return res.status(400).json({ error: 'Bad filename' });
    if (typeof source !== 'string' || source.length === 0) return res.status(400).json({ error: 'Empty source' });

    const jobId = crypto.randomBytes(8).toString('hex');
    const jobDir = path.join(TMP, jobId);
    const srcDir = path.join(jobDir, 'src');
    const outDir = path.join(jobDir, 'out');

    await fs.mkdir(srcDir, { recursive: true });
    await fs.writeFile(path.join(srcDir, filename), source, 'utf8');

    // spawn docker run with heavy restrictions
    const args = [
      'run', '--rm',
      '--network=none',
      '--cpus=1', '--memory=512m',
      '--pids-limit=128',
      '--read-only',
      '--cap-drop=ALL',
      '--security-opt', 'no-new-privileges:true',
      '-u', '1000:1000',
      // mount job dirs
      '-v', `${srcDir}:/work/src:ro`,
      '-v', `${outDir}:/work/out:rw`,
      IMAGE,
      'bash','-lc',
      // exec sandbox-run inside
      `LANGUAGE=${language} ENTRY=${filename} TIMEOUT=${Number(timeout)||10} sandbox-run`
    ];

    await fs.mkdir(outDir, { recursive: true });

    const child = spawn('docker', args, { stdio: ['ignore','pipe','pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => stdout += d.toString());
    child.stderr.on('data', d => stderr += d.toString());

    const code = await new Promise(resolve => child.on('close', resolve));

    // Collect outputs
    let artifacts = [];
    try {
      const files = await fs.readdir(outDir);
      for (const f of files) {
        const p = path.join(outDir, f);
        const stat = await fs.stat(p);
        if (stat.isFile()) {
          const data = await fs.readFile(p);
          // return small files inline (<= 2MB), else just list name + sha
          if (data.length <= 2 * 1024 * 1024) {
            artifacts.push({ name: f, size: data.length, base64: data.toString('base64') });
          } else {
            const sha = crypto.createHash('sha256').update(data).digest('hex');
            artifacts.push({ name: f, size: data.length, sha256: sha, note: 'too large to inline' });
          }
        }
      }
    } catch (_) {}

    const payload = {
      ok: code === 0,
      code,
      stdout,
      stderr,
      artifacts
    };
    const signature = hmac(JSON.stringify(payload), signSecret);

    // Minimal scrubbing: don't leak absolute paths in logs
    payload.stdout = (payload.stdout || '').replaceAll(jobDir, '<job>');
    payload.stderr = (payload.stderr || '').replaceAll(jobDir, '<job>');

    res.json(signature ? { ...payload, signature } : payload);

    // erase job dir
    setTimeout(() => fs.rm(jobDir, { recursive: true, force: true }).catch(()=>{}), 10_000);

  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/lint', async (req, res) => {
  try {
    const { files, signSecret, timeout = 15 } = req.body || {};
    if (!Array.isArray(files) || files.length === 0) return res.status(400).json({ error: 'files[] required' });

    const jobId = crypto.randomBytes(8).toString('hex');
    const jobDir = path.join(TMP, jobId);
    const srcDir = path.join(jobDir, 'src');
    const outDir = path.join(jobDir, 'out');
    await fs.mkdir(srcDir, { recursive: true });
    await fs.mkdir(outDir, { recursive: true });

    // write files: [{path:"relative/path.ext", content:"..."}]
    for (const f of files) {
      if (!f.path || typeof f.path !== 'string' || f.path.includes('..')) continue;
      const fp = path.join(srcDir, f.path);
      await fs.mkdir(path.dirname(fp), { recursive: true });
      await fs.writeFile(fp, f.content ?? '', 'utf8');
    }

    const args = [
      'run','--rm','--network=none',
      '--cpus=1','--memory=512m','--pids-limit=128',
      '--read-only','--cap-drop=ALL','--security-opt','no-new-privileges:true',
      '-u','1000:1000',
      '-v', `${srcDir}:/work/src:ro`,
      '-v', `${outDir}:/work/out:rw`,
      IMAGE,
      'bash','-lc',
      `LANGUAGE=lint TIMEOUT=${Number(timeout)||15} sandbox-run`
    ];

    const child = spawn('docker', args, { stdio: ['ignore','pipe','pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => stdout += d.toString());
    child.stderr.on('data', d => stderr += d.toString());
    const code = await new Promise(r => child.on('close', r));

    // collect lint artifacts
    const artifacts = {};
    for (const name of ['eslint.json','flake8.txt','phpcs.txt','lint.json','build.log']) {
      try {
        const buf = await fs.readFile(path.join(outDir, name));
        artifacts[name] = buf.toString('utf8');
      } catch (_) {}
    }

    const payload = { ok: code === 0, code, stdout, stderr, artifacts };
    const signature = signSecret ? hmac(JSON.stringify(payload), signSecret) : null;
    if (signature) payload.signature = signature;

    // scrub absolute paths
    for (const k of ['stdout','stderr']) if (payload[k]) payload[k] = payload[k].replaceAll(jobDir, '<job>');

    res.json(payload);
    setTimeout(() => fs.rm(jobDir, { recursive: true, force: true }).catch(()=>{}), 10_000);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.listen(4040, () => console.log('[secure-compile] listening on :4040'));
