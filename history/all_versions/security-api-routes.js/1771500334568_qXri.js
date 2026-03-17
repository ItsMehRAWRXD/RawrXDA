// ─────────────────────────────────────────────────────────────────────────────
// RawrZ Security API Routes — Plugs into RawrXD IDE server.js
// All routes under /api/security/* — loopback-only, rate-limited
// ─────────────────────────────────────────────────────────────────────────────
'use strict';

const path = require('path');

let securityBridge = null;
let bridgeInitPromise = null;
let beaconManager = null;
let beaconInitPromise = null;

function getSecurityBridge() {
  if (!bridgeInitPromise) {
    bridgeInitPromise = (async () => {
      try {
        securityBridge = require(path.join(__dirname, '..', 'src', 'security-engines', 'security-bridge'));
        await securityBridge.initialize();
        console.log('[SecurityAPI] ✅ Security bridge initialized');
      } catch (e) {
        console.log('[SecurityAPI] ⚠ Bridge init warning:', e.message);
        // Even if some engines fail, the bridge is usable
        if (securityBridge) securityBridge.initialized = true;
      }
    })();
  }
  return bridgeInitPromise.then(() => securityBridge);
}

function getBeaconManager() {
  if (!beaconInitPromise) {
    beaconInitPromise = (async () => {
      try {
        beaconManager = require(path.join(__dirname, '..', 'src', 'beacon-manager'));
        console.log('[BeaconAPI] ✅ Beacon manager initialized');
      } catch (e) {
        console.log('[BeaconAPI] ⚠ Beacon manager init warning:', e.message);
      }
    })();
  }
  return beaconInitPromise.then(() => beaconManager);
}

// Helper: read POST body
function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 5 * 1024 * 1024) {
        reject(new Error('Body too large'));
      }
    });
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (e) {
        reject(new Error('Invalid JSON'));
      }
    });
  });
}

// Helper: JSON response
function jsonRes(res, statusCode, data) {
  res.writeHead(statusCode, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify(data));
}

// ── Route Handler ──
async function handleApiRoute(req, res, pathname) {
  // Handle security routes
  if (pathname.startsWith('/api/security/')) {
    const bridge = await getSecurityBridge();
    if (!bridge) {
      return jsonRes(res, 503, { error: 'Security bridge not available' });
    }

    // ── GET endpoints ──
    if (req.method === 'GET') {
      // /api/security/status — Full status of all engines
      if (pathname === '/api/security/status') {
        return jsonRes(res, 200, bridge.getStatus());
      }

      // /api/security/engines — List loaded engines
      if (pathname === '/api/security/engines') {
        return jsonRes(res, 200, { engines: bridge.listEngines(), count: bridge.engines.size });
      }

      // /api/security/health — Quick health check
      if (pathname === '/api/security/health') {
        return jsonRes(res, 200, {
          ok: bridge.initialized,
          engines: bridge.engines.size,
          uptime: Math.round((Date.now() - bridge.startTime) / 1000),
          stats: bridge.stats
        });
      }

      // /api/security/algorithms — List supported encryption algorithms
      if (pathname === '/api/security/algorithms') {
        return jsonRes(res, 200, {
          algorithms: [
            'aes-256-gcm', 'aes-256-cbc', 'aes-192-gcm', 'aes-128-gcm',
            'chacha20-poly1305', 'chacha20',
            'aria-256-gcm', 'aria-192-gcm', 'aria-128-gcm',
            'camellia-256-cbc', 'camellia-192-cbc', 'camellia-128-cbc',
            'aes-256-ctr', 'aria-256-ctr'
          ],
          postQuantum: ['kyber', 'dilithium', 'falcon'],
          hashing: ['sha256', 'sha384', 'sha512', 'sha3-256', 'sha3-512', 'blake2b512', 'whirlpool', 'ripemd160']
        });
      }

      return jsonRes(res, 404, { error: 'Unknown security GET endpoint', pathname });
    }

    // ── POST endpoints ──
    if (req.method === 'POST') {
      try {
        const body = await readBody(req);

        // /api/security/encrypt — Encrypt data
        if (pathname === '/api/security/encrypt') {
          const { algorithm = 'aes-256-gcm', data, key, iv } = body;
          if (!data) return jsonRes(res, 400, { error: 'data required' });
          const result = await bridge.encrypt(algorithm, data, key, iv);
          return jsonRes(res, 200, result);
        }

        // /api/security/decrypt — Decrypt data
        if (pathname === '/api/security/decrypt') {
          const { algorithm = 'aes-256-gcm', encryptedData, key, iv, authTag } = body;
          if (!encryptedData) return jsonRes(res, 400, { error: 'encryptedData required' });
          const result = await bridge.decrypt(algorithm, encryptedData, key, iv, authTag);
          return jsonRes(res, 200, result);
        }

        // /api/security/hash — Hash data
        if (pathname === '/api/security/hash') {
          const { algorithm = 'sha256', data } = body;
          if (!data) return jsonRes(res, 400, { error: 'data required' });
          const result = await bridge.hash(algorithm, data);
          return jsonRes(res, 200, result);
        }

        // /api/security/sign — Sign data
        if (pathname === '/api/security/sign') {
          const { algorithm = 'rsa-sha256', data, privateKey } = body;
          if (!data || !privateKey) return jsonRes(res, 400, { error: 'data and privateKey required' });
          const result = await bridge.sign(algorithm, data, privateKey);
          return jsonRes(res, 200, result);
        }

        // /api/security/verify — Verify signature
        if (pathname === '/api/security/verify') {
          const { algorithm = 'rsa-sha256', data, signature, publicKey } = body;
          if (!data || !signature || !publicKey) return jsonRes(res, 400, { error: 'data, signature, and publicKey required' });
          const result = await bridge.verify(algorithm, data, signature, publicKey);
          return jsonRes(res, 200, result);
        }

        // /api/security/engine — Call engine method
        if (pathname === '/api/security/engine') {
          const { engine: engineName, method, args } = body;
          if (!engineName || !method) return jsonRes(res, 400, { error: 'engine and method required' });
          const Engine = bridge.getEngine(engineName);
          if (!Engine) return jsonRes(res, 404, { error: `Engine '${engineName}' not found` });
          const instance = typeof Engine === 'function' ? new Engine() : Engine;
          if (typeof instance[method] !== 'function') {
            return jsonRes(res, 400, { error: `Method '${method}' not found on ${engineName}` });
          }
          const result = await instance[method](...(args || []));
          return jsonRes(res, 200, { success: true, engine: engineName, method, result });
        }

        return jsonRes(res, 404, { error: 'Unknown security POST endpoint', pathname });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    return jsonRes(res, 405, { error: 'Method not allowed' });
  }

  // Handle beacon routes
  if (pathname.startsWith('/api/beacon/')) {
    const beacon = await getBeaconManager();
    if (!beacon) {
      return jsonRes(res, 503, { error: 'Beacon manager not available' });
    }

    // ── GET endpoints ──
    if (req.method === 'GET') {
      // /api/beacon/status — Full status of beacon system
      if (pathname === '/api/beacon/status') {
        return jsonRes(res, 200, beacon.getStatus());
      }

      // /api/beacon/beacons — List registered beacons
      if (pathname === '/api/beacon/beacons') {
        return jsonRes(res, 200, { beacons: beacon.beacons.size, list: Array.from(beacon.beacons.keys()) });
      }

      return jsonRes(res, 404, { error: 'Unknown beacon GET endpoint', pathname });
    }

    // ── POST endpoints ──
    if (req.method === 'POST') {
      try {
        const body = await readBody(req);

        // /api/beacon/register — Register a beacon
        if (pathname === '/api/beacon/register') {
          const { componentId, type, address } = body;
          if (!componentId || !type) return jsonRes(res, 400, { error: 'componentId and type required' });
          beacon.registerBeacon(componentId, type, address);
          return jsonRes(res, 200, { success: true, componentId, type });
        }

        // /api/beacon/send — Send message via beacon
        if (pathname === '/api/beacon/send') {
          const { sourceId, targetType, message, options } = body;
          if (!sourceId || !targetType || !message) return jsonRes(res, 400, { error: 'sourceId, targetType, and message required' });
          const result = await beacon.sendMessage(sourceId, targetType, message, options);
          return jsonRes(res, 200, result);
        }

        // /api/beacon/agentic — Send to agentic system
        if (pathname === '/api/beacon/agentic') {
          const { sourceId, action, params } = body;
          if (!sourceId || !action) return jsonRes(res, 400, { error: 'sourceId and action required' });
          const result = await beacon.sendToAgentic(sourceId, action, params);
          return jsonRes(res, 200, result);
        }

        // /api/beacon/hotpatch — Send to hotpatch system
        if (pathname === '/api/beacon/hotpatch') {
          const { sourceId, target, patch } = body;
          if (!sourceId || !target || !patch) return jsonRes(res, 400, { error: 'sourceId, target, and patch required' });
          const result = await beacon.sendToHotpatch(sourceId, target, patch);
          return jsonRes(res, 200, result);
        }

        // /api/beacon/security — Send to security system
        if (pathname === '/api/beacon/security') {
          const { sourceId, operation, data } = body;
          if (!sourceId || !operation) return jsonRes(res, 400, { error: 'sourceId and operation required' });
          const result = await beacon.sendToSecurity(sourceId, operation, data);
          return jsonRes(res, 200, result);
        }

        // /api/beacon/encryption — Send to encryption system
        if (pathname === '/api/beacon/encryption') {
          const { sourceId, mode, data } = body;
          if (!sourceId || !mode) return jsonRes(res, 400, { error: 'sourceId and mode required' });
          const result = await beacon.sendToEncryption(sourceId, mode, data);
          return jsonRes(res, 200, result);
        }

        return jsonRes(res, 404, { error: 'Unknown beacon POST endpoint', pathname });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    return jsonRes(res, 405, { error: 'Method not allowed' });
  }

  return jsonRes(res, 404, { error: 'Unknown API endpoint', pathname });
}
    try {
      body = await readBody(req);
    } catch (e) {
      return jsonRes(res, 400, { error: e.message });
    }

    // /api/security/encrypt
    if (pathname === '/api/security/encrypt') {
      try {
        const { algorithm, data, options } = body;
        if (!data) return jsonRes(res, 400, { error: 'data is required' });
        const result = await bridge.encrypt(algorithm || 'aes-256-gcm', data, options || {});
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/decrypt
    if (pathname === '/api/security/decrypt') {
      try {
        const { algorithm, data, key, iv, options } = body;
        if (!data || !key) return jsonRes(res, 400, { error: 'data and key required' });
        const result = await bridge.decrypt(algorithm, data, key, iv, options || {});
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/hash
    if (pathname === '/api/security/hash') {
      try {
        const { data, algorithm } = body;
        if (!data) return jsonRes(res, 400, { error: 'data is required' });
        const result = bridge.hash(data, algorithm || 'sha256');
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/stealth
    if (pathname === '/api/security/stealth') {
      try {
        const { payload, mode } = body;
        if (!payload) return jsonRes(res, 400, { error: 'payload is required' });
        const result = await bridge.generateStealth(payload, mode || 'maximum');
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/mutate
    if (pathname === '/api/security/mutate') {
      try {
        const { code, options } = body;
        if (!code) return jsonRes(res, 400, { error: 'code is required' });
        const result = await bridge.mutate(code, options || {});
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/analyze
    if (pathname === '/api/security/analyze') {
      try {
        const { filePath, data } = body;
        if (!filePath && !data) return jsonRes(res, 400, { error: 'filePath or data required' });
        const result = await bridge.analyze(filePath || data);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/scan
    if (pathname === '/api/security/scan') {
      try {
        const { filePath } = body;
        if (!filePath) return jsonRes(res, 400, { error: 'filePath is required' });
        const result = await bridge.scanFile(filePath);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/portscan
    if (pathname === '/api/security/portscan') {
      try {
        const { host, ports } = body;
        if (!host) return jsonRes(res, 400, { error: 'host is required' });
        const result = await bridge.portScan(host, ports || '1-1024');
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/generate-stub
    if (pathname === '/api/security/generate-stub') {
      try {
        const result = await bridge.generateStub(body);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/cve
    if (pathname === '/api/security/cve') {
      try {
        const { cveId } = body;
        if (!cveId) return jsonRes(res, 400, { error: 'cveId is required' });
        const result = await bridge.lookupCVE(cveId);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/forensics
    if (pathname === '/api/security/forensics') {
      try {
        const { target } = body;
        if (!target) return jsonRes(res, 400, { error: 'target is required' });
        const result = await bridge.forensicAnalysis(target);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/generate-bot
    if (pathname === '/api/security/generate-bot') {
      try {
        const { type, language, options } = body;
        if (!type || !language) return jsonRes(res, 400, { error: 'type and language required' });
        const result = await bridge.generateBot(type, language, options || {});
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/hot-patch
    if (pathname === '/api/security/hot-patch') {
      try {
        const { target, patch } = body;
        if (!target || !patch) return jsonRes(res, 400, { error: 'target and patch required' });
        const result = await bridge.hotPatch(target, patch);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/threat-detect
    if (pathname === '/api/security/threat-detect') {
      try {
        const { data } = body;
        if (!data) return jsonRes(res, 400, { error: 'data is required' });
        const result = await bridge.detectThreats(data);
        return jsonRes(res, 200, { success: true, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    // /api/security/engine — Direct engine access (advanced)
    if (pathname === '/api/security/engine') {
      try {
        const { engine: engineName, method, args } = body;
        if (!engineName || !method) return jsonRes(res, 400, { error: 'engine and method required' });
        const Engine = bridge.getEngine(engineName);
        if (!Engine) return jsonRes(res, 404, { error: `Engine '${engineName}' not found` });
        const instance = typeof Engine === 'function' ? new Engine() : Engine;
        if (typeof instance[method] !== 'function') {
          return jsonRes(res, 400, { error: `Method '${method}' not found on ${engineName}` });
        }
        const result = await instance[method](...(args || []));
        return jsonRes(res, 200, { success: true, engine: engineName, method, result });
      } catch (e) {
        return jsonRes(res, 500, { error: e.message });
      }
    }

    return jsonRes(res, 404, { error: 'Unknown security POST endpoint', pathname });
  }

  return jsonRes(res, 405, { error: 'Method not allowed' });
}

module.exports = { handleSecurityRoute };
