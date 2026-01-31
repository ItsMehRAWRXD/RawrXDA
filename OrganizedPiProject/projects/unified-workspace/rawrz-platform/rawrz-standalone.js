// Minimal drop-in: hashing + AES + DNS + ping wrappers used by server.js
'use strict';

const crypto = require('crypto');
const dns = require('dns').promises;
const { exec } = require('child_process');
const os = require('os');

class RawrZStandalone {
  async hash(input, algorithm = 'sha256', save = false, extension) {
    const h = crypto.createHash(algorithm).update(String(input), 'utf8').digest('hex');
    return { algorithm, hash: h, saved: Boolean(save), extension: extension || null };
  }

  // AES: supports aes-256-gcm / aes-128-cbc (as used by server.js)
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

    throw new Error(`Unsupported encryption algorithm in stub: ${algorithm}`);
  }

  async decrypt(algorithm, input, keyB64, ivB64, extension) {
    const key = Buffer.from(String(keyB64 || ''), 'base64');
    const iv = Buffer.from(String(ivB64 || ''), 'base64');
    const data = Buffer.from(String(input || ''), 'base64');

    if (algorithm.toLowerCase() === 'aes-256-gcm') {
      // Support optional final authTag appended with ":" (ciphertext:tag), else require 'authTag' in input JSON upstream.
      let ciphertext = data;
      let tag = null;

      // Try to parse "ciphertext:authtag" style if provided as plain string
      // (server.js passes fields separately, but this helps manual testing)
      if (!key.length || !iv.length) throw new Error('Key/IV required for AES-GCM');
      const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);

      // If user tucked tag in the input, try to split by 16-byte tail (common pattern).
      // Here we require server to pass authTag via body if needed. If missing, try best-effort:
      if (ciphertext.length > 16) {
        tag = ciphertext.slice(-16);
        ciphertext = ciphertext.slice(0, -16);
        try { decipher.setAuthTag(tag); } catch (_) {}
      }

      let plain;
      try {
        plain = Buffer.concat([decipher.update(ciphertext), decipher.final()]);
      } catch (e) {
        throw new Error('GCM auth failed or bad inputs');
      }
      return { algorithm, plaintext: plain.toString('utf8'), extension: extension || null };
    }

    if (algorithm.toLowerCase() === 'aes-128-cbc') {
      if (!key.length || !iv.length) throw new Error('Key/IV required for AES-CBC');
      const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
      const dec = Buffer.concat([decipher.update(data), decipher.final()]);
      return { algorithm, plaintext: dec.toString('utf8'), extension: extension || null };
    }

    throw new Error(`Unsupported decryption algorithm in stub: ${algorithm}`);
  }

  async dnsLookup(hostname) {
    const [addr] = await dns.lookup(hostname, { all: true });
    return { hostname, address: addr?.address || null, family: addr?.family || null };
  }

  async ping(host, _privileged = false) {
    // Cross-platform best-effort ping (no root required)
    // Linux/mac: ping -c 1; Windows: ping -n 1
    const countFlag = process.platform === 'win32' ? '-n' : '-c';
    const cmd = `ping ${countFlag} 1 ${host}`;
    return new Promise((resolve) => {
      exec(cmd, { timeout: 4000 }, (err, stdout, stderr) => {
        resolve({
          host,
          ok: !err,
          rtt: /time[=<]\s*\d+(\.\d+)?\s*ms/i.test(stdout) ? stdout.match(/time[=<]\s*([\d.]+)\s*ms/i)[1] + ' ms' : null,
          raw: (stdout || stderr || '').slice(0, 1000)
        });
      });
    });
  }

  // System Information Methods
  async getOsInfo() {
    return {
      platform: os.platform(),
      type: os.type(),
      release: os.release(),
      arch: os.arch(),
      hostname: os.hostname(),
      userInfo: os.userInfo()
    };
  }

  async getCpuInfo() {
    const cpus = os.cpus();
    return {
      count: cpus.length,
      model: cpus[0]?.model,
      speed: cpus[0]?.speed,
      loadAverage: os.loadavg(),
      cpus: cpus.map(cpu => ({
        model: cpu.model,
        speed: cpu.speed,
        times: cpu.times
      }))
    };
  }

  async getMemoryInfo() {
    return {
      total: os.totalmem(),
      free: os.freemem(),
      used: os.totalmem() - os.freemem(),
      usage: ((os.totalmem() - os.freemem()) / os.totalmem() * 100).toFixed(2) + '%'
    };
  }

  async getNetworkInfo() {
    const interfaces = os.networkInterfaces();
    const result = {};
    
    for (const [name, nets] of Object.entries(interfaces)) {
      result[name] = nets.map(net => ({
        address: net.address,
        family: net.family,
        internal: net.internal,
        mac: net.mac
      }));
    }
    
    return result;
  }

  async getUptime() {
    const uptime = os.uptime();
    const days = Math.floor(uptime / 86400);
    const hours = Math.floor((uptime % 86400) / 3600);
    const minutes = Math.floor((uptime % 3600) / 60);
    const seconds = Math.floor(uptime % 60);
    
    return {
      seconds: uptime,
      formatted: `${days}d ${hours}h ${minutes}m ${seconds}s`,
      humanReadable: `${days} days, ${hours} hours, ${minutes} minutes, ${seconds} seconds`
    };
  }

  // Database Methods (Read-Only for Security)
  async executeQuery(query, limit = 100) {
    // Security: Only allow SELECT statements
    const trimmedQuery = query.trim().toLowerCase();
    if (!trimmedQuery.startsWith('select')) {
      throw new Error('Only SELECT queries are allowed for security reasons');
    }
    
    // Additional security checks
    const dangerousKeywords = ['drop', 'delete', 'insert', 'update', 'alter', 'create', 'truncate', 'exec', 'execute'];
    for (const keyword of dangerousKeywords) {
      if (trimmedQuery.includes(keyword)) {
        throw new Error(`Dangerous keyword '${keyword}' detected. Only SELECT queries are allowed.`);
      }
    }
    
    // Mock implementation - in real use, connect to actual database
    return {
      query: query.trim(),
      rows: [
        { id: 1, name: 'Sample Data', created_at: new Date().toISOString() },
        { id: 2, name: 'Another Record', created_at: new Date().toISOString() }
      ],
      count: 2,
      limit: Math.min(limit, 1000),
      message: 'This is a mock implementation. Connect to a real database for actual queries.'
    };
  }

  async getTables() {
    // Mock implementation
    return {
      tables: [
        { name: 'users', rows: 150, size: '2.5 MB' },
        { name: 'orders', rows: 1250, size: '15.2 MB' },
        { name: 'products', rows: 89, size: '1.1 MB' }
      ],
      message: 'This is a mock implementation. Connect to a real database for actual table information.'
    };
  }

  async getTableSchema(tableName) {
    // Mock implementation
    return {
      table: tableName,
      columns: [
        { name: 'id', type: 'INTEGER', nullable: false, primary: true },
        { name: 'name', type: 'VARCHAR(255)', nullable: false, primary: false },
        { name: 'email', type: 'VARCHAR(255)', nullable: true, primary: false },
        { name: 'created_at', type: 'TIMESTAMP', nullable: false, primary: false }
      ],
      message: 'This is a mock implementation. Connect to a real database for actual schema information.'
    };
  }

  async getDatabaseStats() {
    // Mock implementation
    return {
      totalTables: 3,
      totalRows: 1489,
      totalSize: '18.8 MB',
      lastBackup: new Date(Date.now() - 86400000).toISOString(),
      uptime: '7 days, 3 hours',
      message: 'This is a mock implementation. Connect to a real database for actual statistics.'
    };
  }

  // Workflow Methods
  async executeWorkflow(name, steps, parallel = false) {
    try {
      const parsedSteps = JSON.parse(steps);
      if (!Array.isArray(parsedSteps)) {
        throw new Error('Steps must be a JSON array');
      }

      const results = [];
      const startTime = Date.now();

      if (parallel) {
        // Execute steps in parallel
        const promises = parsedSteps.map(async (step, index) => {
          try {
            const result = await this.executeWorkflowStep(step, index);
            return { step: index, success: true, result };
          } catch (error) {
            return { step: index, success: false, error: error.message };
          }
        });
        
        const stepResults = await Promise.all(promises);
        results.push(...stepResults);
      } else {
        // Execute steps sequentially
        for (let i = 0; i < parsedSteps.length; i++) {
          try {
            const result = await this.executeWorkflowStep(parsedSteps[i], i);
            results.push({ step: i, success: true, result });
          } catch (error) {
            results.push({ step: i, success: false, error: error.message });
            // Stop on first error in sequential mode
            break;
          }
        }
      }

      const endTime = Date.now();
      return {
        name,
        success: results.every(r => r.success),
        duration: endTime - startTime,
        steps: results,
        totalSteps: parsedSteps.length,
        completedSteps: results.length
      };
    } catch (error) {
      throw new Error(`Workflow execution failed: ${error.message}`);
    }
  }

  async executeWorkflowStep(step, index) {
    const { action, params = {} } = step;
    
    // Map workflow actions to actual methods
    const actionMap = {
      'hash': () => this.hash(params.input, params.algorithm),
      'encrypt': () => this.encrypt(params.algorithm, params.input),
      'decrypt': () => this.decrypt(params.algorithm, params.input, params.key, params.iv),
      'dns': () => this.dnsLookup(params.hostname),
      'ping': () => this.ping(params.host),
      'os_info': () => this.getOsInfo(),
      'cpu_info': () => this.getCpuInfo(),
      'memory_info': () => this.getMemoryInfo(),
      'delay': () => new Promise(resolve => setTimeout(resolve, params.ms || 1000))
    };

    if (!actionMap[action]) {
      throw new Error(`Unknown workflow action: ${action}`);
    }

    return await actionMap[action]();
  }

  async getWorkflowTemplates() {
    return {
      templates: [
        {
          name: 'System Health Check',
          description: 'Check system status and performance',
          steps: [
            { action: 'os_info', params: {} },
            { action: 'cpu_info', params: {} },
            { action: 'memory_info', params: {} },
            { action: 'ping', params: { host: '8.8.8.8' } }
          ]
        },
        {
          name: 'Data Encryption Pipeline',
          description: 'Encrypt and hash sensitive data',
          steps: [
            { action: 'hash', params: { input: '{{input}}', algorithm: 'sha256' } },
            { action: 'encrypt', params: { input: '{{input}}', algorithm: 'aes-256-gcm' } }
          ]
        },
        {
          name: 'Network Diagnostics',
          description: 'Comprehensive network testing',
          steps: [
            { action: 'dns', params: { hostname: '{{hostname}}' } },
            { action: 'ping', params: { host: '{{hostname}}' } },
            { action: 'delay', params: { ms: 2000 } },
            { action: 'ping', params: { host: '8.8.8.8' } }
          ]
        }
      ]
    };
  }

  async validateWorkflow(steps) {
    try {
      const parsedSteps = JSON.parse(steps);
      if (!Array.isArray(parsedSteps)) {
        return { valid: false, error: 'Steps must be a JSON array' };
      }

      const validActions = ['hash', 'encrypt', 'decrypt', 'dns', 'ping', 'os_info', 'cpu_info', 'memory_info', 'delay'];
      const errors = [];

      for (let i = 0; i < parsedSteps.length; i++) {
        const step = parsedSteps[i];
        if (!step.action) {
          errors.push(`Step ${i}: Missing 'action' property`);
        } else if (!validActions.includes(step.action)) {
          errors.push(`Step ${i}: Invalid action '${step.action}'`);
        }
        if (!step.params) {
          errors.push(`Step ${i}: Missing 'params' property`);
        }
      }

      return {
        valid: errors.length === 0,
        errors,
        stepCount: parsedSteps.length
      };
    } catch (error) {
      return { valid: false, error: `Invalid JSON: ${error.message}` };
    }
  }

  async getWorkflowHistory(limit = 50) {
    // Mock implementation
    return {
      executions: [
        {
          id: 'wf_001',
          name: 'System Health Check',
          status: 'completed',
          startTime: new Date(Date.now() - 300000).toISOString(),
          duration: 1250,
          steps: 4
        },
        {
          id: 'wf_002',
          name: 'Data Encryption Pipeline',
          status: 'failed',
          startTime: new Date(Date.now() - 600000).toISOString(),
          duration: 500,
          steps: 2,
          error: 'Invalid input parameter'
        }
      ],
      total: 2,
      message: 'This is a mock implementation. Implement persistent storage for actual workflow history.'
    };
  }

  // Security Methods
  async getTokenInfo() {
    return {
      tokenType: 'Bearer',
      algorithm: 'HMAC-SHA256',
      expiresIn: '24h',
      issuedAt: new Date().toISOString(),
      permissions: ['read', 'write', 'execute'],
      message: 'This is a mock implementation. Implement proper JWT or session-based authentication.'
    };
  }

  async validateToken(token) {
    // Mock validation - in real implementation, verify JWT signature
    const isValid = token && token.length > 10;
    return {
      valid: isValid,
      token: token ? token.substring(0, 8) + '...' : null,
      expiresAt: isValid ? new Date(Date.now() + 86400000).toISOString() : null,
      permissions: isValid ? ['read', 'write', 'execute'] : [],
      message: isValid ? 'Token is valid' : 'Invalid or expired token'
    };
  }

  async getUserPermissions() {
    return {
      user: 'admin',
      role: 'administrator',
      permissions: [
        { resource: 'crypto', actions: ['hash', 'encrypt', 'decrypt'] },
        { resource: 'network', actions: ['dns', 'ping'] },
        { resource: 'files', actions: ['upload', 'download', 'list'] },
        { resource: 'system', actions: ['os_info', 'cpu_info', 'memory_info'] },
        { resource: 'database', actions: ['query', 'tables', 'schema'] },
        { resource: 'workflow', actions: ['execute', 'validate', 'templates'] }
      ],
      restrictions: [],
      message: 'This is a mock implementation. Implement proper RBAC system.'
    };
  }

  async getAuditLog(limit = 100, level = 'all') {
    const mockLogs = [
      {
        timestamp: new Date(Date.now() - 300000).toISOString(),
        level: 'info',
        action: 'token_validation',
        user: 'admin',
        ip: '127.0.0.1',
        details: 'Token validated successfully'
      },
      {
        timestamp: new Date(Date.now() - 600000).toISOString(),
        level: 'warn',
        action: 'failed_login',
        user: 'unknown',
        ip: '192.168.1.100',
        details: 'Invalid token provided'
      },
      {
        timestamp: new Date(Date.now() - 900000).toISOString(),
        level: 'error',
        action: 'sql_injection_attempt',
        user: 'admin',
        ip: '127.0.0.1',
        details: 'Dangerous SQL keyword detected in query'
      }
    ];

    const filteredLogs = level === 'all' 
      ? mockLogs 
      : mockLogs.filter(log => log.level === level);

    return {
      logs: filteredLogs.slice(0, limit),
      total: filteredLogs.length,
      level,
      message: 'This is a mock implementation. Implement proper audit logging system.'
    };
  }
}

module.exports = RawrZStandalone;
