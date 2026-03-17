# RawrZ Payload Builder - Deployment & Operations Guide 🚀

## Deployment Architecture

### System Requirements
```
Component          | Minimum    | Recommended | Enterprise
-------------------|------------|-------------|-------------
CPU                | 4 cores    | 8 cores     | 16+ cores
RAM                | 8 GB       | 16 GB       | 32+ GB
Storage            | 100 GB SSD | 500 GB SSD  | 1+ TB NVMe
Network            | 100 Mbps   | 1 Gbps      | 10+ Gbps
OS                 | Win10/Linux| Win11/Linux | Server OS
```

### Infrastructure Setup

#### Standalone Deployment
```powershell
# Windows Deployment
git clone https://github.com/rawrz/payload-builder.git
cd "RawrZ Payload Builder"
npm install --production
npm run build
npm run start

# Create Windows Service
sc create "RawrZ Service" binPath="C:\RawrZ\rawrz-service.exe"
sc config "RawrZ Service" start=auto
sc start "RawrZ Service"
```

#### Docker Deployment
```dockerfile
# Dockerfile
FROM node:18-alpine

WORKDIR /app
COPY package*.json ./
RUN npm ci --only=production

COPY . .
RUN npm run build

EXPOSE 3000
USER node

CMD ["npm", "start"]
```

```yaml
# docker-compose.yml
version: '3.8'
services:
  rawrz-builder:
    build: .
    ports:
      - "3000:3000"
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
    environment:
      - NODE_ENV=production
      - RAWRZ_LOG_LEVEL=info
    restart: unless-stopped
    
  rawrz-database:
    image: postgres:15-alpine
    environment:
      - POSTGRES_DB=rawrz
      - POSTGRES_USER=rawrz
      - POSTGRES_PASSWORD=${DB_PASSWORD}
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped

volumes:
  postgres_data:
```

#### Kubernetes Deployment
```yaml
# k8s-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: rawrz-builder
  namespace: security-tools
spec:
  replicas: 3
  selector:
    matchLabels:
      app: rawrz-builder
  template:
    metadata:
      labels:
        app: rawrz-builder
    spec:
      containers:
      - name: rawrz-builder
        image: rawrz/payload-builder:latest
        ports:
        - containerPort: 3000
        env:
        - name: NODE_ENV
          value: "production"
        - name: RAWRZ_DATABASE_URL
          valueFrom:
            secretKeyRef:
              name: rawrz-secrets
              key: database-url
        resources:
          requests:
            memory: "2Gi"
            cpu: "1000m"
          limits:
            memory: "4Gi"
            cpu: "2000m"
        volumeMounts:
        - name: data-volume
          mountPath: /app/data
        - name: config-volume
          mountPath: /app/config
      volumes:
      - name: data-volume
        persistentVolumeClaim:
          claimName: rawrz-data-pvc
      - name: config-volume
        configMap:
          name: rawrz-config
---
apiVersion: v1
kind: Service
metadata:
  name: rawrz-builder-service
  namespace: security-tools
spec:
  selector:
    app: rawrz-builder
  ports:
  - port: 80
    targetPort: 3000
  type: LoadBalancer
```

## Configuration Management

### Environment Configuration
```javascript
// config/production.js
module.exports = {
  // Server Configuration
  server: {
    port: process.env.PORT || 3000,
    host: process.env.HOST || '0.0.0.0',
    ssl: {
      enabled: process.env.SSL_ENABLED === 'true',
      cert: process.env.SSL_CERT_PATH,
      key: process.env.SSL_KEY_PATH
    }
  },
  
  // Database Configuration
  database: {
    url: process.env.DATABASE_URL,
    pool: {
      min: 5,
      max: 20,
      idle: 10000
    },
    ssl: process.env.NODE_ENV === 'production'
  },
  
  // Security Configuration
  security: {
    encryption: {
      algorithm: 'aes-256-gcm',
      keyDerivation: 'argon2',
      iterations: 100000
    },
    authentication: {
      jwtSecret: process.env.JWT_SECRET,
      tokenExpiry: '24h',
      refreshTokenExpiry: '7d'
    },
    rateLimit: {
      windowMs: 15 * 60 * 1000, // 15 minutes
      max: 100 // requests per window
    }
  },
  
  // Logging Configuration
  logging: {
    level: process.env.LOG_LEVEL || 'info',
    file: {
      enabled: true,
      path: './logs/rawrz.log',
      maxSize: '100MB',
      maxFiles: 10
    },
    console: {
      enabled: process.env.NODE_ENV !== 'production'
    }
  },
  
  // Engine Configuration
  engines: {
    crypto: {
      enabled: true,
      algorithms: ['aes-256-gcm', 'chacha20-poly1305', 'rsa-4096']
    },
    polymorphic: {
      enabled: true,
      mutationRate: 0.3,
      maxIterations: 10
    },
    evasion: {
      enabled: true,
      antiVM: true,
      antiDebug: true,
      antiSandbox: true
    }
  }
};
```

### Secrets Management
```javascript
// config/secrets.js
const AWS = require('aws-sdk');
const { SecretManagerServiceClient } = require('@google-cloud/secret-manager');

class SecretsManager {
  constructor() {
    this.provider = process.env.SECRETS_PROVIDER || 'env';
    this.cache = new Map();
    this.cacheTimeout = 5 * 60 * 1000; // 5 minutes
  }
  
  async getSecret(key) {
    // Check cache first
    const cached = this.cache.get(key);
    if (cached && Date.now() - cached.timestamp < this.cacheTimeout) {
      return cached.value;
    }
    
    let value;
    
    switch (this.provider) {
      case 'aws':
        value = await this.getAWSSecret(key);
        break;
      case 'gcp':
        value = await this.getGCPSecret(key);
        break;
      case 'azure':
        value = await this.getAzureSecret(key);
        break;
      case 'vault':
        value = await this.getVaultSecret(key);
        break;
      default:
        value = process.env[key];
    }
    
    // Cache the secret
    this.cache.set(key, {
      value,
      timestamp: Date.now()
    });
    
    return value;
  }
  
  async getAWSSecret(key) {
    const secretsManager = new AWS.SecretsManager({
      region: process.env.AWS_REGION || 'us-east-1'
    });
    
    try {
      const result = await secretsManager.getSecretValue({
        SecretId: key
      }).promise();
      
      return JSON.parse(result.SecretString);
    } catch (error) {
      console.error(`Failed to retrieve AWS secret ${key}:`, error);
      throw error;
    }
  }
  
  async getGCPSecret(key) {
    const client = new SecretManagerServiceClient();
    const name = `projects/${process.env.GCP_PROJECT}/secrets/${key}/versions/latest`;
    
    try {
      const [version] = await client.accessSecretVersion({ name });
      return version.payload.data.toString();
    } catch (error) {
      console.error(`Failed to retrieve GCP secret ${key}:`, error);
      throw error;
    }
  }
}

module.exports = new SecretsManager();
```

## Monitoring & Observability

### Application Monitoring
```javascript
// monitoring/metrics.js
const prometheus = require('prom-client');

// Create metrics registry
const register = new prometheus.Registry();

// Default metrics
prometheus.collectDefaultMetrics({ register });

// Custom metrics
const httpRequestDuration = new prometheus.Histogram({
  name: 'http_request_duration_seconds',
  help: 'Duration of HTTP requests in seconds',
  labelNames: ['method', 'route', 'status_code'],
  buckets: [0.1, 0.5, 1, 2, 5]
});

const encryptionOperations = new prometheus.Counter({
  name: 'encryption_operations_total',
  help: 'Total number of encryption operations',
  labelNames: ['algorithm', 'operation', 'status']
});

const payloadGenerations = new prometheus.Counter({
  name: 'payload_generations_total',
  help: 'Total number of payload generations',
  labelNames: ['type', 'status']
});

const activeConnections = new prometheus.Gauge({
  name: 'active_connections',
  help: 'Number of active connections'
});

// Register metrics
register.registerMetric(httpRequestDuration);
register.registerMetric(encryptionOperations);
register.registerMetric(payloadGenerations);
register.registerMetric(activeConnections);

// Middleware for HTTP metrics
const metricsMiddleware = (req, res, next) => {
  const start = Date.now();
  
  res.on('finish', () => {
    const duration = (Date.now() - start) / 1000;
    httpRequestDuration
      .labels(req.method, req.route?.path || req.path, res.statusCode)
      .observe(duration);
  });
  
  next();
};

module.exports = {
  register,
  metricsMiddleware,
  encryptionOperations,
  payloadGenerations,
  activeConnections
};
```

### Health Checks
```javascript
// monitoring/health.js
class HealthChecker {
  constructor() {
    this.checks = new Map();
    this.status = 'healthy';
  }
  
  // Register health check
  registerCheck(name, checkFunction, timeout = 5000) {
    this.checks.set(name, {
      check: checkFunction,
      timeout,
      lastResult: null,
      lastCheck: null
    });
  }
  
  // Run all health checks
  async runChecks() {
    const results = {};
    let overallStatus = 'healthy';
    
    for (const [name, checkInfo] of this.checks) {
      try {
        const result = await Promise.race([
          checkInfo.check(),
          new Promise((_, reject) => 
            setTimeout(() => reject(new Error('Timeout')), checkInfo.timeout)
          )
        ]);
        
        results[name] = {
          status: 'healthy',
          result,
          timestamp: new Date().toISOString()
        };
        
        checkInfo.lastResult = result;
        checkInfo.lastCheck = Date.now();
        
      } catch (error) {
        results[name] = {
          status: 'unhealthy',
          error: error.message,
          timestamp: new Date().toISOString()
        };
        
        overallStatus = 'unhealthy';
      }
    }
    
    this.status = overallStatus;
    
    return {
      status: overallStatus,
      checks: results,
      timestamp: new Date().toISOString()
    };
  }
  
  // Database connectivity check
  async checkDatabase() {
    const db = require('../database');
    const result = await db.query('SELECT 1 as health');
    return result.rows[0].health === 1;
  }
  
  // Memory usage check
  async checkMemory() {
    const usage = process.memoryUsage();
    const maxMemory = 1024 * 1024 * 1024; // 1GB
    
    if (usage.heapUsed > maxMemory) {
      throw new Error(`Memory usage too high: ${usage.heapUsed}`);
    }
    
    return {
      heapUsed: usage.heapUsed,
      heapTotal: usage.heapTotal,
      external: usage.external
    };
  }
  
  // Disk space check
  async checkDiskSpace() {
    const fs = require('fs');
    const stats = fs.statSync('./');
    
    // Check available space (simplified)
    const freeSpace = stats.free || 0;
    const minFreeSpace = 1024 * 1024 * 1024; // 1GB
    
    if (freeSpace < minFreeSpace) {
      throw new Error(`Low disk space: ${freeSpace} bytes`);
    }
    
    return { freeSpace };
  }
  
  // Engine status check
  async checkEngines() {
    const engines = require('../src/engines');
    const results = {};
    
    for (const [name, engine] of Object.entries(engines)) {
      try {
        if (engine.healthCheck) {
          results[name] = await engine.healthCheck();
        } else {
          results[name] = 'no health check available';
        }
      } catch (error) {
        throw new Error(`Engine ${name} unhealthy: ${error.message}`);
      }
    }
    
    return results;
  }
}

// Initialize health checker
const healthChecker = new HealthChecker();

// Register checks
healthChecker.registerCheck('database', () => healthChecker.checkDatabase());
healthChecker.registerCheck('memory', () => healthChecker.checkMemory());
healthChecker.registerCheck('disk', () => healthChecker.checkDiskSpace());
healthChecker.registerCheck('engines', () => healthChecker.checkEngines());

module.exports = healthChecker;
```

### Logging Configuration
```javascript
// logging/logger.js
const winston = require('winston');
const DailyRotateFile = require('winston-daily-rotate-file');

// Custom log format
const logFormat = winston.format.combine(
  winston.format.timestamp(),
  winston.format.errors({ stack: true }),
  winston.format.json(),
  winston.format.printf(({ timestamp, level, message, ...meta }) => {
    return JSON.stringify({
      timestamp,
      level,
      message,
      ...meta
    });
  })
);

// Create logger
const logger = winston.createLogger({
  level: process.env.LOG_LEVEL || 'info',
  format: logFormat,
  defaultMeta: {
    service: 'rawrz-builder',
    version: process.env.npm_package_version,
    hostname: require('os').hostname(),
    pid: process.pid
  },
  transports: [
    // Console transport
    new winston.transports.Console({
      format: winston.format.combine(
        winston.format.colorize(),
        winston.format.simple()
      )
    }),
    
    // File transport with rotation
    new DailyRotateFile({
      filename: 'logs/rawrz-%DATE%.log',
      datePattern: 'YYYY-MM-DD',
      maxSize: '100m',
      maxFiles: '30d',
      zippedArchive: true
    }),
    
    // Error file transport
    new winston.transports.File({
      filename: 'logs/error.log',
      level: 'error',
      maxsize: 50 * 1024 * 1024, // 50MB
      maxFiles: 5
    })
  ],
  
  // Handle exceptions and rejections
  exceptionHandlers: [
    new winston.transports.File({ filename: 'logs/exceptions.log' })
  ],
  rejectionHandlers: [
    new winston.transports.File({ filename: 'logs/rejections.log' })
  ]
});

// Security audit logger
const auditLogger = winston.createLogger({
  level: 'info',
  format: logFormat,
  transports: [
    new DailyRotateFile({
      filename: 'logs/audit-%DATE%.log',
      datePattern: 'YYYY-MM-DD',
      maxSize: '100m',
      maxFiles: '365d', // Keep audit logs for 1 year
      zippedArchive: true
    })
  ]
});

// Performance logger
const performanceLogger = winston.createLogger({
  level: 'info',
  format: logFormat,
  transports: [
    new DailyRotateFile({
      filename: 'logs/performance-%DATE%.log',
      datePattern: 'YYYY-MM-DD-HH',
      maxSize: '50m',
      maxFiles: '7d',
      zippedArchive: true
    })
  ]
});

module.exports = {
  logger,
  auditLogger,
  performanceLogger
};
```

## Security Hardening

### Application Security
```javascript
// security/hardening.js
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const slowDown = require('express-slow-down');

// Security middleware configuration
const securityMiddleware = [
  // Helmet for security headers
  helmet({
    contentSecurityPolicy: {
      directives: {
        defaultSrc: ["'self'"],
        styleSrc: ["'self'", "'unsafe-inline'"],
        scriptSrc: ["'self'"],
        imgSrc: ["'self'", "data:", "https:"],
        connectSrc: ["'self'"],
        fontSrc: ["'self'"],
        objectSrc: ["'none'"],
        mediaSrc: ["'self'"],
        frameSrc: ["'none'"]
      }
    },
    hsts: {
      maxAge: 31536000,
      includeSubDomains: true,
      preload: true
    }
  }),
  
  // Rate limiting
  rateLimit({
    windowMs: 15 * 60 * 1000, // 15 minutes
    max: 100, // limit each IP to 100 requests per windowMs
    message: 'Too many requests from this IP',
    standardHeaders: true,
    legacyHeaders: false
  }),
  
  // Slow down repeated requests
  slowDown({
    windowMs: 15 * 60 * 1000, // 15 minutes
    delayAfter: 50, // allow 50 requests per windowMs without delay
    delayMs: 500 // add 500ms delay per request after delayAfter
  })
];

// Input validation
const validateInput = (schema) => {
  return (req, res, next) => {
    const { error } = schema.validate(req.body);
    if (error) {
      return res.status(400).json({
        error: 'Invalid input',
        details: error.details.map(d => d.message)
      });
    }
    next();
  };
};

// Authentication middleware
const authenticate = async (req, res, next) => {
  try {
    const token = req.headers.authorization?.split(' ')[1];
    
    if (!token) {
      return res.status(401).json({ error: 'No token provided' });
    }
    
    const decoded = jwt.verify(token, process.env.JWT_SECRET);
    req.user = decoded;
    next();
  } catch (error) {
    return res.status(401).json({ error: 'Invalid token' });
  }
};

// Authorization middleware
const authorize = (permissions) => {
  return (req, res, next) => {
    if (!req.user) {
      return res.status(401).json({ error: 'Not authenticated' });
    }
    
    const hasPermission = permissions.some(permission => 
      req.user.permissions.includes(permission)
    );
    
    if (!hasPermission) {
      return res.status(403).json({ error: 'Insufficient permissions' });
    }
    
    next();
  };
};

module.exports = {
  securityMiddleware,
  validateInput,
  authenticate,
  authorize
};
```

### Network Security
```javascript
// security/network.js
const tls = require('tls');
const crypto = require('crypto');

// TLS Configuration
const tlsOptions = {
  // Minimum TLS version
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.3',
  
  // Cipher suites (secure only)
  ciphers: [
    'ECDHE-RSA-AES256-GCM-SHA384',
    'ECDHE-RSA-AES128-GCM-SHA256',
    'ECDHE-RSA-AES256-SHA384',
    'ECDHE-RSA-AES128-SHA256',
    'ECDHE-RSA-AES256-SHA',
    'ECDHE-RSA-AES128-SHA'
  ].join(':'),
  
  // ECDH curves
  ecdhCurve: 'secp384r1',
  
  // Disable weak protocols
  secureProtocol: 'TLSv1_2_method',
  
  // Certificate verification
  rejectUnauthorized: true,
  
  // Session resumption
  sessionIdContext: crypto.randomBytes(32).toString('hex')
};

// Certificate management
class CertificateManager {
  constructor() {
    this.certificates = new Map();
    this.caStore = [];
  }
  
  // Load certificate
  loadCertificate(name, certPath, keyPath, passphrase) {
    const cert = fs.readFileSync(certPath);
    const key = fs.readFileSync(keyPath);
    
    this.certificates.set(name, {
      cert,
      key,
      passphrase,
      loaded: Date.now()
    });
  }
  
  // Generate self-signed certificate
  generateSelfSigned(commonName, validityDays = 365) {
    const keys = crypto.generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: { type: 'spki', format: 'pem' },
      privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
    });
    
    // Create certificate (simplified)
    const cert = this.createCertificate(keys.publicKey, keys.privateKey, {
      commonName,
      validityDays
    });
    
    return { cert, key: keys.privateKey };
  }
  
  // Validate certificate chain
  validateCertificateChain(cert, chain) {
    try {
      const certificate = new crypto.X509Certificate(cert);
      
      // Check expiration
      if (certificate.validTo < new Date()) {
        throw new Error('Certificate expired');
      }
      
      // Check issuer
      if (chain && chain.length > 0) {
        const issuer = new crypto.X509Certificate(chain[0]);
        if (!certificate.checkIssued(issuer)) {
          throw new Error('Certificate not issued by provided CA');
        }
      }
      
      return true;
    } catch (error) {
      console.error('Certificate validation failed:', error);
      return false;
    }
  }
}

// Network firewall rules
const firewallRules = {
  // Allowed IP ranges
  allowedIPs: [
    '10.0.0.0/8',
    '172.16.0.0/12',
    '192.168.0.0/16'
  ],
  
  // Blocked IP ranges
  blockedIPs: [
    '0.0.0.0/8',
    '127.0.0.0/8',
    '169.254.0.0/16',
    '224.0.0.0/4'
  ],
  
  // Rate limiting by IP
  rateLimits: {
    global: { requests: 1000, window: 3600 },
    perIP: { requests: 100, window: 3600 }
  }
};

module.exports = {
  tlsOptions,
  CertificateManager,
  firewallRules
};
```

## Backup & Recovery

### Backup Strategy
```javascript
// backup/backup.js
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { spawn } = require('child_process');

class BackupManager {
  constructor(config) {
    this.config = config;
    this.backupPath = config.backupPath || './backups';
    this.encryptionKey = config.encryptionKey;
    this.retentionDays = config.retentionDays || 30;
  }
  
  // Create full backup
  async createFullBackup() {
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const backupName = `rawrz-full-${timestamp}`;
    const backupDir = path.join(this.backupPath, backupName);
    
    // Create backup directory
    fs.mkdirSync(backupDir, { recursive: true });
    
    // Backup components
    await Promise.all([
      this.backupDatabase(backupDir),
      this.backupConfiguration(backupDir),
      this.backupLogs(backupDir),
      this.backupUserData(backupDir),
      this.backupCertificates(backupDir)
    ]);
    
    // Create manifest
    const manifest = {
      type: 'full',
      timestamp: new Date().toISOString(),
      components: ['database', 'config', 'logs', 'userdata', 'certificates'],
      version: process.env.npm_package_version
    };
    
    fs.writeFileSync(
      path.join(backupDir, 'manifest.json'),
      JSON.stringify(manifest, null, 2)
    );
    
    // Compress and encrypt
    const archivePath = await this.compressBackup(backupDir);
    const encryptedPath = await this.encryptBackup(archivePath);
    
    // Cleanup unencrypted files
    fs.rmSync(backupDir, { recursive: true });
    fs.unlinkSync(archivePath);
    
    return encryptedPath;
  }
  
  // Backup database
  async backupDatabase(backupDir) {
    const dbConfig = require('../config').database;
    const dumpPath = path.join(backupDir, 'database.sql');
    
    return new Promise((resolve, reject) => {
      const pg_dump = spawn('pg_dump', [
        dbConfig.url,
        '--file', dumpPath,
        '--verbose',
        '--no-password'
      ]);
      
      pg_dump.on('close', (code) => {
        if (code === 0) {
          resolve(dumpPath);
        } else {
          reject(new Error(`pg_dump failed with code ${code}`));
        }
      });
    });
  }
  
  // Backup configuration
  async backupConfiguration(backupDir) {
    const configDir = path.join(backupDir, 'config');
    fs.mkdirSync(configDir, { recursive: true });
    
    // Copy configuration files
    const configFiles = [
      'config/production.js',
      'config/engines.json',
      '.env'
    ];
    
    for (const file of configFiles) {
      if (fs.existsSync(file)) {
        const dest = path.join(configDir, path.basename(file));
        fs.copyFileSync(file, dest);
      }
    }
  }
  
  // Compress backup
  async compressBackup(backupDir) {
    const archivePath = `${backupDir}.tar.gz`;
    
    return new Promise((resolve, reject) => {
      const tar = spawn('tar', [
        '-czf', archivePath,
        '-C', path.dirname(backupDir),
        path.basename(backupDir)
      ]);
      
      tar.on('close', (code) => {
        if (code === 0) {
          resolve(archivePath);
        } else {
          reject(new Error(`tar failed with code ${code}`));
        }
      });
    });
  }
  
  // Encrypt backup
  async encryptBackup(archivePath) {
    const encryptedPath = `${archivePath}.enc`;
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipher('aes-256-gcm', this.encryptionKey);
    
    const input = fs.createReadStream(archivePath);
    const output = fs.createWriteStream(encryptedPath);
    
    // Write IV first
    output.write(iv);
    
    return new Promise((resolve, reject) => {
      input.pipe(cipher).pipe(output);
      
      output.on('finish', () => {
        // Append auth tag
        const tag = cipher.getAuthTag();
        fs.appendFileSync(encryptedPath, tag);
        resolve(encryptedPath);
      });
      
      output.on('error', reject);
    });
  }
  
  // Restore from backup
  async restoreFromBackup(backupPath) {
    // Decrypt backup
    const decryptedPath = await this.decryptBackup(backupPath);
    
    // Extract archive
    const extractDir = await this.extractBackup(decryptedPath);
    
    // Read manifest
    const manifest = JSON.parse(
      fs.readFileSync(path.join(extractDir, 'manifest.json'), 'utf8')
    );
    
    // Restore components
    if (manifest.components.includes('database')) {
      await this.restoreDatabase(path.join(extractDir, 'database.sql'));
    }
    
    if (manifest.components.includes('config')) {
      await this.restoreConfiguration(path.join(extractDir, 'config'));
    }
    
    // Cleanup
    fs.rmSync(extractDir, { recursive: true });
    fs.unlinkSync(decryptedPath);
    
    return manifest;
  }
  
  // Cleanup old backups
  async cleanupOldBackups() {
    const backupFiles = fs.readdirSync(this.backupPath)
      .filter(file => file.endsWith('.enc'))
      .map(file => ({
        name: file,
        path: path.join(this.backupPath, file),
        mtime: fs.statSync(path.join(this.backupPath, file)).mtime
      }))
      .sort((a, b) => b.mtime - a.mtime);
    
    const cutoffDate = new Date();
    cutoffDate.setDate(cutoffDate.getDate() - this.retentionDays);
    
    const filesToDelete = backupFiles.filter(file => file.mtime < cutoffDate);
    
    for (const file of filesToDelete) {
      fs.unlinkSync(file.path);
      console.log(`Deleted old backup: ${file.name}`);
    }
    
    return filesToDelete.length;
  }
}

module.exports = BackupManager;
```

## Performance Optimization

### Caching Strategy
```javascript
// performance/cache.js
const Redis = require('redis');
const NodeCache = require('node-cache');

class CacheManager {
  constructor() {
    // Redis for distributed caching
    this.redis = Redis.createClient({
      host: process.env.REDIS_HOST || 'localhost',
      port: process.env.REDIS_PORT || 6379,
      password: process.env.REDIS_PASSWORD,
      db: process.env.REDIS_DB || 0
    });
    
    // Node cache for local caching
    this.localCache = new NodeCache({
      stdTTL: 600, // 10 minutes default TTL
      checkperiod: 120, // Check for expired keys every 2 minutes
      useClones: false
    });
    
    this.redis.on('error', (err) => {
      console.error('Redis error:', err);
    });
  }
  
  // Get from cache (try local first, then Redis)
  async get(key) {
    // Try local cache first
    const localValue = this.localCache.get(key);
    if (localValue !== undefined) {
      return localValue;
    }
    
    // Try Redis
    try {
      const redisValue = await this.redis.get(key);
      if (redisValue) {
        const parsed = JSON.parse(redisValue);
        // Store in local cache for faster access
        this.localCache.set(key, parsed, 300); // 5 minutes local TTL
        return parsed;
      }
    } catch (error) {
      console.error('Redis get error:', error);
    }
    
    return null;
  }
  
  // Set in cache (both local and Redis)
  async set(key, value, ttl = 600) {
    // Set in local cache
    this.localCache.set(key, value, ttl);
    
    // Set in Redis
    try {
      await this.redis.setex(key, ttl, JSON.stringify(value));
    } catch (error) {
      console.error('Redis set error:', error);
    }
  }
  
  // Delete from cache
  async del(key) {
    this.localCache.del(key);
    
    try {
      await this.redis.del(key);
    } catch (error) {
      console.error('Redis del error:', error);
    }
  }
  
  // Cache with automatic refresh
  async getOrSet(key, fetchFunction, ttl = 600) {
    let value = await this.get(key);
    
    if (value === null) {
      value = await fetchFunction();
      await this.set(key, value, ttl);
    }
    
    return value;
  }
  
  // Invalidate pattern
  async invalidatePattern(pattern) {
    try {
      const keys = await this.redis.keys(pattern);
      if (keys.length > 0) {
        await this.redis.del(...keys);
      }
    } catch (error) {
      console.error('Redis pattern invalidation error:', error);
    }
    
    // Clear local cache (simple approach - clear all)
    this.localCache.flushAll();
  }
}

module.exports = new CacheManager();
```

### Database Optimization
```javascript
// performance/database.js
const { Pool } = require('pg');

class DatabaseOptimizer {
  constructor(config) {
    this.pool = new Pool({
      ...config,
      // Connection pool optimization
      min: 5,
      max: 20,
      idleTimeoutMillis: 30000,
      connectionTimeoutMillis: 2000,
      
      // Performance settings
      statement_timeout: 30000,
      query_timeout: 30000,
      application_name: 'rawrz-builder'
    });
    
    // Connection monitoring
    this.pool.on('connect', (client) => {
      console.log('Database connection established');
      
      // Set session parameters for performance
      client.query(`
        SET work_mem = '256MB';
        SET maintenance_work_mem = '1GB';
        SET effective_cache_size = '4GB';
        SET random_page_cost = 1.1;
        SET seq_page_cost = 1.0;
      `);
    });
    
    this.pool.on('error', (err) => {
      console.error('Database pool error:', err);
    });
  }
  
  // Query with automatic retry
  async query(text, params, retries = 3) {
    for (let attempt = 1; attempt <= retries; attempt++) {
      try {
        const start = Date.now();
        const result = await this.pool.query(text, params);
        const duration = Date.now() - start;
        
        // Log slow queries
        if (duration > 1000) {
          console.warn(`Slow query (${duration}ms):`, text);
        }
        
        return result;
      } catch (error) {
        if (attempt === retries) {
          throw error;
        }
        
        // Wait before retry
        await new Promise(resolve => setTimeout(resolve, attempt * 1000));
      }
    }
  }
  
  // Bulk insert optimization
  async bulkInsert(table, columns, data) {
    const client = await this.pool.connect();
    
    try {
      await client.query('BEGIN');
      
      // Use COPY for large datasets
      if (data.length > 1000) {
        const copyText = `COPY ${table} (${columns.join(', ')}) FROM STDIN WITH CSV`;
        const stream = client.query(copyFrom(copyText));
        
        for (const row of data) {
          stream.write(row.join(',') + '\n');
        }
        
        stream.end();
        await stream.promise();
      } else {
        // Use batch insert for smaller datasets
        const placeholders = data.map((_, i) => 
          `(${columns.map((_, j) => `$${i * columns.length + j + 1}`).join(', ')})`
        ).join(', ');
        
        const values = data.flat();
        const query = `INSERT INTO ${table} (${columns.join(', ')}) VALUES ${placeholders}`;
        
        await client.query(query, values);
      }
      
      await client.query('COMMIT');
    } catch (error) {
      await client.query('ROLLBACK');
      throw error;
    } finally {
      client.release();
    }
  }
  
  // Database maintenance
  async performMaintenance() {
    const maintenanceTasks = [
      'VACUUM ANALYZE',
      'REINDEX DATABASE rawrz',
      'UPDATE pg_stat_statements SET calls = 0, total_time = 0'
    ];
    
    for (const task of maintenanceTasks) {
      try {
        await this.query(task);
        console.log(`Maintenance task completed: ${task}`);
      } catch (error) {
        console.error(`Maintenance task failed: ${task}`, error);
      }
    }
  }
}

module.exports = DatabaseOptimizer;
```

This deployment guide provides comprehensive coverage of deployment strategies, configuration management, monitoring, security hardening, backup procedures, and performance optimization for the RawrZ Payload Builder in production environments.