'use strict';

const crypto = require('crypto');
const dns = require('dns').promises;
const { exec } = require('child_process');

class RawrZStandalone {
  async hash(input, algorithm = 'sha256', save = false, extension) {
    const h = crypto.createHash(algorithm).update(String(input), 'utf8').digest('hex');
    return { algorithm, hash: h, saved: Boolean(save), extension: extension || null };
  }

  // AES: supports aes-256-gcm / aes-128-cbc
  async encrypt(algorithm, input, extension) {
    const text = Buffer.from(String(input), 'utf8');

    if (algorithm.toLowerCase() === 'aes-256-gcm') {
      const key = crypto.randomBytes(32);
      const iv = crypto.randomBytes(12);
      const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
      const enc = Buffer.concat([cipher.update(text), cipher.final()]);
      const tag = cipher.getAuthTag();
      return {
        algorithm,
        ciphertext: enc.toString('base64'),
        key: key.toString('base64'),
        iv: iv.toString('base64'),
        authTag: tag.toString('base64'),
        extension: extension || null
      };
    }

    if (algorithm.toLowerCase() === 'aes-128-cbc') {
      const key = crypto.randomBytes(16);
      const iv = crypto.randomBytes(16);
      const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);
      const enc = Buffer.concat([cipher.update(text), cipher.final()]);
      return {
        algorithm,
        ciphertext: enc.toString('base64'),
        key: key.toString('base64'),
        iv: iv.toString('base64'),
        extension: extension || null
      };
    }

    throw new Error('Unsupported encryption algorithm in stub: ' + algorithm);
  }

  async decrypt(algorithm, input, keyB64, ivB64, extension) {
    const key = Buffer.from(String(keyB64 || ''), 'base64');
    const iv = Buffer.from(String(ivB64 || ''), 'base64');
    const data = Buffer.from(String(input || ''), 'base64');

    if (algorithm.toLowerCase() === 'aes-256-gcm') {
      if (!key.length || !iv.length) throw new Error('Key/IV required for AES-GCM');
      const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
      // Best-effort: try to peel trailing 16 bytes as authTag
      if (data.length > 16) {
        const tag = data.slice(-16);
        const body = data.slice(0, -16);
        decipher.setAuthTag(tag);
        const plain = Buffer.concat([decipher.update(body), decipher.final()]);
        return { algorithm, plaintext: plain.toString('utf8'), extension: extension || null };
      }
      // If authTag wasn't appended, try anyway (will throw if invalid)
      const plain = Buffer.concat([decipher.update(data), decipher.final()]);
      return { algorithm, plaintext: plain.toString('utf8'), extension: extension || null };
    }

    if (algorithm.toLowerCase() === 'aes-128-cbc') {
      if (!key.length || !iv.length) throw new Error('Key/IV required for AES-CBC');
      const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
      const dec = Buffer.concat([decipher.update(data), decipher.final()]);
      return { algorithm, plaintext: dec.toString('utf8'), extension: extension || null };
    }

    throw new Error('Unsupported decryption algorithm in stub: ' + algorithm);
  }

  async dnsLookup(hostname) {
    const [addr] = await dns.lookup(hostname, { all: true });
    return { hostname, address: addr?.address || null, family: addr?.family || null };
  }

  async ping(host, _privileged = false) {
    // Cross-platform best-effort ping (no root required)
    const countFlag = process.platform === 'win32' ? '-n' : '-c';
    const cmd = 'ping ' + countFlag + ' 1 ' + host;
    return new Promise((resolve) => {
      exec(cmd, { timeout: 4000 }, (err, stdout, stderr) => {
        resolve({
          host,
          ok: !err,
          rtt: /time[=<]s*d+(.d+)?s*ms/i.test(stdout) ? stdout.match(/time[=<]s*([d.]+)s*ms/i)[1] + ' ms' : null,
          raw: (stdout || stderr || '').slice(0, 1000)
        });
      });
    });
  }
}

module.exports = RawrZStandalone;
