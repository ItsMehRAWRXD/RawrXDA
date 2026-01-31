/*  RawrZ HTTP Platform – production-ready edition with dynamic GUI */
'use strict';

// --- Dependencies ---
const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const path = require('path');
const multer = require('multer');
const crypto = require('crypto');
const rateLimit = require('express-rate-limit');
const { body, query, validationResult } = require('express-validator');
const morgan = require('morgan');
const cron = require('node-cron');
const fs = require('fs').promises;
const { constants } = require('fs');
const { fileTypeFromBuffer } = require('file-type');
const { exec } = require('child_process');
const compression = require('compression');
require('dotenv').config();

// --- Internal Modules ---
const RawrZStandalone = require('./rawrz-standalone');
const rawrzEngine = require('./src/engines/rawrz-engine');
// Note: These modules are loaded from the rawrz-http-encryptor directory
const universalWMIC = require('./rawrz-http-encryptor/src/engines/universal-wmic-engine');
const { loadEngines } = require('./rawrz-http-encryptor/src/engines'); // Dynamic GUI support

// --- App & Config ---
const app = express();
app.disable('x-powered-by');
app.set('trust proxy', 1);

// Port will be auto-detected in bootstrap function
let port = parseInt(process.env.PORT || '8080', 10);
const uploadsDir = path.resolve(__dirname, 'uploads');
const uploadsRoot = uploadsDir + path.sep;
const rawrz = new RawrZStandalone();

// --- Auth Token ---
if (process.env.NODE_ENV === 'production' && !process.env.AUTH_TOKEN) {
  console.error('[FATAL] AUTH_TOKEN not set. Exiting.');
  process.exit(1);
}
if (!process.env.AUTH_TOKEN) {
  console.warn('[WARN] AUTH_TOKEN not set. Using ephemeral token for non-production session.');
  process.env.AUTH_TOKEN = crypto.randomBytes(32).toString('hex');
}
console.log(`[AUTH] Using token: ${process.env.AUTH_TOKEN.substring(0, 8)}...`);

// --- Bootstrap ---
(async () => {
  // Auto-detect available port starting from 8080
  const findAvailablePort = async (startPort = 8080) => {
    const net = require('net');
    
    const isPortAvailable = (port) => {
      return new Promise((resolve) => {
        const server = net.createServer();
        
        server.listen(port, () => {
          server.once('close', () => resolve(true));
          server.close();
        });
        
        server.on('error', () => resolve(false));
      });
    };
    
    let port = startPort;
    while (port < startPort + 100) { // Try up to 100 ports
      if (await isPortAvailable(port)) {
        return port;
      }
      port++;
    }
    throw new Error(`No available ports found in range ${startPort}-${startPort + 100}`);
  };

  // Auto-detect port if 8080 is busy
  try {
    if (port === 8080) {
      port = await findAvailablePort();
      console.log(`[PORT] Auto-selected available port: ${port}`);
    }
  } catch (e) {
    console.error('[FATAL] Could not find available port:', e.message);
    process.exit(1);
  }

  try {
    await fs.mkdir(uploadsDir, { recursive: true });
    console.log('[OK] Ensured uploads directory exists.');
  } catch (e) {
    console.error('[FATAL] Could not create uploads directory:', e.message);
    process.exit(1);
  }
  try {
    await rawrzEngine.initializeModules();
    console.log('[OK] RawrZ core engine initialized.');
  } catch (e) {
    console.error('[WARN] Core engine init failed:', e.message);
  }

  if (process.env.NODE_ENV === 'production') {
    exec('npm audit --json', (error, stdout) => {
      try {
        const report = JSON.parse(stdout || '{}');
        const total = report?.metadata?.vulnerabilities?.total ?? 0;
        if (error) console.warn('[SECURITY] npm audit returned an error; check dependencies manually.');
        if (total > 0) console.error(`[SECURITY] Found ${total} dependency vulnerabilities on startup!`);
        else console.log('[SECURITY] npm audit scan completed with no vulnerabilities found.');
      } catch (parseErr) {
        console.error('[SECURITY] Failed to parse npm audit output:', parseErr.message);
      }
    });
  }
})();

// --- Security Headers ---
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'"],
      objectSrc: ["'none'"],
      imgSrc: ["'self'", "data:"],
      connectSrc: ["'self'"],
      styleSrc: ["'self'"],
      frameAncestors: ["'self'"]
    },
  },
  noSniff: true,
  referrerPolicy: { policy: 'no-referrer' },
  crossOriginResourcePolicy: { policy: 'same-origin' },
}));
if (process.env.NODE_ENV === 'production') {
  app.use(helmet.hsts({ maxAge: 15552000, includeSubDomains: true, preload: false }));
}
app.use((req, res, next) => {
  res.setHeader('Permissions-Policy', 'geolocation=(), microphone=(), camera=()');
  next();
});

// --- CORS ---
const allowedOrigins = process.env.ALLOWED_ORIGIN
  ? process.env.ALLOWED_ORIGIN.split(',').map(s => s.trim())
  : ['*'];

const corsOptions = {
  origin: (origin, callback) => {
    if (!origin) return callback(null, true);
    if (allowedOrigins.includes('*') || allowedOrigins.includes(origin)) return callback(null, true);
    return callback(new Error('Not allowed by CORS'));
  },
  credentials: allowedOrigins.length > 1 || !allowedOrigins.includes('*'),
};
app.use(cors(corsOptions));

// --- Logging ---
app.use(morgan('combined'));

// --- Rate Limiters & Body Parser ---
const apiLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 100,
  standardHeaders: true,
  legacyHeaders: false,
  message: 'Too many requests from this IP, please try again after 15 minutes.',
});
const uploadLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: 'Too many uploads from this IP, please try again after 15 minutes.',
});

// compression (skip download path)
app.use(compression({
  filter: (req, res) => (req.path === '/api/download') ? false : compression.filter(req, res)
}));

app.use(express.json({ limit: '1mb' }));
app.use((err, req, res, next) => {
  if (err?.type === 'entity.parse.failed') return res.status(400).json({ error: 'Invalid JSON body' });
  next(err);
});

app.use('/api', apiLimiter);

// --- Auth Middleware ---
const requireAuth = async (req, res, next) => {
  const header = req.headers.authorization || '';
  const token = header.startsWith('Bearer ') ? header.slice(7).trim() : null;
  if (!token || token !== process.env.AUTH_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  next();
};

// --- Helpers ---
const allowedAlgorithms = ['sha256', 'sha512', 'aes-256-gcm', 'aes-128-cbc'];
const checkAlgorithm = async (algorithm, res) => {
  if (!allowedAlgorithms.includes(String(algorithm).toLowerCase())) {
    res.status(400).json({ error: 'Unsupported algorithm' });
    return true;
  }
  return false;
};
const allowedFileMimes = [
  'application/x-executable', 'application/x-msdownload', 'application/x-sharedlib',
  'application/octet-stream', 'application/x-dll', 'application/x-exe',
  'application/x-elf', 'application/x-mach-o'
];

// --- Static (long cache) ---
app.use('/static', express.static(path.join(__dirname, 'public'), {
  immutable: true,
  maxAge: '1y'
}));

// --- Dynamic Manifest Endpoint (for GUI) ---
app.get('/api/manifest', requireAuth, async (_req, res) => {
  try {
    const engines = await loadEngines();
    res.json({
      name: 'RawrZ HTTP Platform',
      version: '1.0.0',
      baseUrl: '',
      engines
    });
  } catch (e) {
    console.error('[Manifest Error]', e);
    res.status(500).json({ error: 'Failed to build manifest' });
  }
});

// --- Crypto Router ---
const cryptoRouter = express.Router();
cryptoRouter.use(express.json({ limit: '64kb' }));
cryptoRouter.use(requireAuth);

cryptoRouter.post('/hash',
  body('input').isString().notEmpty().isLength({ max: 60 * 1024 }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { input, algorithm = 'sha256', save = false, extension } = req.body;
      if (await checkAlgorithm(algorithm, res)) return;
      res.json(await rawrz.hash(input, algorithm, !!save, extension));
    } catch (err) {
      console.error('[Crypto Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

cryptoRouter.post('/encrypt',
  body('input').isString().notEmpty().isLength({ max: 60 * 1024 }),
  body('algorithm').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { algorithm, input, extension } = req.body;
      if (await checkAlgorithm(algorithm, res)) return;
      res.json(await rawrz.encrypt(algorithm, input, extension));
    } catch (err) {
      console.error('[Crypto Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

cryptoRouter.post('/decrypt',
  body('input').isString().notEmpty().isLength({ max: 60 * 1024 }),
  body('algorithm').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { algorithm, input, key, iv, extension } = req.body;
      if (await checkAlgorithm(algorithm, res)) return;
      res.json(await rawrz.decrypt(algorithm, input, key, iv, extension));
    } catch (err) {
      console.error('[Crypto Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.use('/api', cryptoRouter);

// --- WMIC Router ---
const wmicRouter = express.Router();
wmicRouter.use(express.json({ limit: '64kb' }));
wmicRouter.use(requireAuth);

// Get all supported WMIC classes
wmicRouter.get('/classes', async (_req, res) => {
  try {
    const classes = universalWMIC.getSupportedClasses();
    res.json({
      supported: classes,
      count: classes.length,
      platform: universalWMIC.platform
    });
  } catch (err) {
    console.error('[WMIC Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Get system summary
wmicRouter.get('/summary', async (_req, res) => {
  try {
    const summary = await universalWMIC.getSystemSummary();
    res.json(summary);
  } catch (err) {
    console.error('[WMIC Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Execute WMIC command
wmicRouter.post('/execute',
  body('className').isString().notEmpty(),
  body('properties').optional().isArray(),
  body('whereClause').optional().isString(),
  body('format').optional().isString(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { className, properties = [], whereClause = '', format = 'csv' } = req.body;
      
      if (!universalWMIC.isClassSupported(className)) {
        return res.status(400).json({ 
          error: `WMIC class '${className}' not supported`,
          supported: universalWMIC.getSupportedClasses()
        });
      }
      
      const result = await universalWMIC.wmic(className, properties, whereClause, format);
      res.json({
        className,
        properties,
        whereClause,
        format,
        result,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error('[WMIC Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

// Individual WMIC class endpoints
const wmicClasses = [
  'computersystem', 'cpu', 'memorychip', 'logicaldisk', 'networkadapter',
  'process', 'service', 'product', 'useraccount', 'bios', 'startup', 'environment'
];

wmicClasses.forEach(className => {
  wmicRouter.get(`/${className}`, async (req, res) => {
    try {
      const { properties = [], whereClause = '', format = 'csv' } = req.query;
      const props = Array.isArray(properties) ? properties : properties.split(',');
      
      const result = await universalWMIC.wmic(className, props, whereClause, format);
      res.json({
        className,
        result,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error(`[WMIC Error] ${className}:`, err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });
});

// Process management endpoints
wmicRouter.post('/process/kill',
  body('pid').isInt().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { pid } = req.body;
      const result = await universalWMIC.killProcess(pid);
      res.json({ success: true, pid, result });
    } catch (err) {
      console.error('[WMIC Process Error]', err.stack || err.message);
      res.status(500).json({ error: 'Failed to kill process' });
    }
  });

// Service management endpoints
wmicRouter.post('/service/start',
  body('serviceName').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { serviceName } = req.body;
      const result = await universalWMIC.startService(serviceName);
      res.json({ success: true, serviceName, result });
    } catch (err) {
      console.error('[WMIC Service Error]', err.stack || err.message);
      res.status(500).json({ error: 'Failed to start service' });
    }
  });

wmicRouter.post('/service/stop',
  body('serviceName').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { serviceName } = req.body;
      const result = await universalWMIC.stopService(serviceName);
      res.json({ success: true, serviceName, result });
    } catch (err) {
      console.error('[WMIC Service Error]', err.stack || err.message);
      res.status(500).json({ error: 'Failed to stop service' });
    }
  });

// Cache management
wmicRouter.post('/cache/clear', async (_req, res) => {
  try {
    universalWMIC.clearCache();
    res.json({ success: true, message: 'Cache cleared' });
  } catch (err) {
    console.error('[WMIC Cache Error]', err.stack || err.message);
    res.status(500).json({ error: 'Failed to clear cache' });
  }
});

app.use('/api/wmic', wmicRouter);

// --- Multer (memory) ---
const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: 100 * 1024 * 1024, files: 10 },
});

// --- Routes ---
app.get('/health', async (_req, res) => res.json({ ok: true, status: 'healthy' }));
app.get('/panel', requireAuth, async (_req, res) => res.sendFile(path.join(__dirname, 'public', 'panel.html')));
app.get('/', async (_req, res) => res.redirect('/panel'));

app.get('/api/dns',
  requireAuth, query('hostname').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try { res.json(await rawrz.dnsLookup(String(req.query.hostname))); }
    catch (err) {
      console.error('[DNS Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.get('/api/ping',
  requireAuth, query('host').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try { res.json(await rawrz.ping(String(req.query.host), false)); }
    catch (err) {
      console.error('[Ping Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.get('/api/files', requireAuth, async (_req, res) => {
  try {
    const list = await fs.readdir(uploadsDir);
    const files = await Promise.all(list.filter(f => f !== '.gitkeep').map(async file => {
      const filePath = path.join(uploadsDir, file);
      const stats = await fs.stat(filePath);
      return { name: file, size: stats.size, modified: stats.mtime };
    }));
    res.json(files);
  } catch (err) {
    console.error('[Files Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/upload',
  requireAuth, uploadLimiter, upload.single('file'),
  async (req, res) => {
    if (!req.file) return res.status(400).json({ error: 'No file uploaded' });
    try {
      const type = await fileTypeFromBuffer(req.file.buffer.subarray(0, 4100)); // 4KB sniff
      const allowed = [
        'application/x-executable','application/x-msdownload','application/x-sharedlib',
        'application/octet-stream','application/x-dll','application/x-exe',
        'application/x-elf','application/x-mach-o'
      ];
      if (!type || !allowed.includes(type.mime)) {
        return res.status(400).json({ error: `File type not supported: ${type ? type.mime : 'unknown'}` });
      }

      const base = path.basename(req.file.originalname || 'file').replace(/[\r\n]/g, '');
      const filename = `${Date.now()}-${crypto.randomBytes(8).toString('hex')}-${base}`;
      const filePath = path.join(uploadsDir, filename);
      await fs.writeFile(filePath, req.file.buffer, { mode: 0o600 });
      res.json({ success: true, filename, size: req.file.size });
    } catch (err) {
      console.error('[Upload Error]', err.stack || err.message);
      res.status(500).json({ error: 'Upload failed' });
    }
  });

app.get('/api/download',
  requireAuth, query('filename').isString().notEmpty().customSanitizer(v => path.basename(String(v))),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });

    const fileName = path.basename(String(req.query.filename));
    const safePath = path.join(uploadsDir, fileName);

    if (!path.resolve(safePath).startsWith(path.resolve(uploadsRoot))) {
      return res.status(403).json({ error: 'Forbidden' });
    }
    res.set('Cache-Control', 'no-store');

    try {
      await fs.access(safePath, constants.R_OK);
      res.download(safePath, fileName, (err) => {
        if (err) {
          console.error('[Download Error]', err.stack || err.message);
          res.status(500).json({ error: 'Internal Server Error' });
        }
      });
    } catch (err) {
      if (err.code === 'ENOENT') return res.status(404).json({ error: 'File not found' });
      console.error('[Download Access Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

// --- File Cleanup Cron Job ---
cron.schedule('0 0 * * *', async () => {
  console.log('Running scheduled file cleanup...');
  const now = Date.now();
  const oneDayInMs = 24 * 60 * 60 * 1000;
  try {
    const files = await fs.readdir(uploadsDir);
    for (const file of files) {
      const filePath = path.join(uploadsDir, file);
      const stats = await fs.stat(filePath);
      if (stats.isFile() && (now - stats.mtime.getTime()) > oneDayInMs) {
        await fs.unlink(filePath);
        console.log(`Deleted old file: ${file}`);
      }
    }
    console.log('File cleanup completed.');
  } catch (e) {
    console.error('File cleanup failed:', e.message);
  }
});

// --- Multer Error Handler ---
app.use(async (err, req, res, next) => {
  if (err instanceof multer.MulterError) {
    if (err.code === 'LIMIT_FILE_SIZE') return res.status(413).json({ error: 'File size too large. Max 100MB allowed.' });
    if (err.code === 'LIMIT_FILE_COUNT') return res.status(413).json({ error: 'Too many files uploaded. Max 10 files allowed.' });
    return res.status(400).json({ error: `Upload error: ${err.message}` });
  }
  next(err);
});

// --- Global Error Handler ---
app.use(async (err, req, res, next) => {
  console.error('[Unhandled Error]', err.stack || err.message);
  res.status(500).json({ error: 'Internal Server Error' });
});

// --- 404 ---
app.use(async (req, res) => res.status(404).json({ error: 'Not Found' }));

// --- Server & Shutdown ---
const server = app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
  console.log(`Dynamic GUI available at: http://localhost:${port}/panel`);
  if (process.env.NODE_ENV !== 'production') {
    console.log('[INFO] Remember to run `npm audit` regularly to check for vulnerable dependencies.');
  }
});

// Handle server listen errors
server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`[ERROR] Port ${port} is already in use. Please kill the process using this port or use a different port.`);
    console.error(`[ERROR] You can find the process using: netstat -ano | findstr :${port}`);
    process.exit(1);
  } else {
    console.error('[ERROR] Server failed to start:', err.message);
    process.exit(1);
  }
});
const shutdown = (signal) => async () => {
  console.log(`[${signal}] Shutting down...`);
  server.close(err => process.exit(err ? 1 : 0));
};
process.on('SIGTERM', shutdown('SIGTERM'));
process.on('SIGINT', shutdown('SIGINT'));
process.on('unhandledRejection', async e => console.error('[unhandledRejection]', e));
process.on('uncaughtException', async e => { console.error('[uncaughtException]', e); process.exit(1); });
