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
const { loadEngines, watchEngines } = require('./src/engines'); // Dynamic GUI support
const { eventBus, COMMON_EVENTS } = require('./src/event_bus');
const JWTAuth = require('./src/auth/jwt');
const toggleManager = require('./src/config/toggles');
const delayManager = require('./src/config/delays'); // Toggle system

// --- App & Config ---
const app = express();
app.disable('x-powered-by');
app.set('trust proxy', 1);

const port = parseInt(process.env.PORT || '8080', 10);
const uploadsDir = path.resolve(__dirname, 'uploads');
const uploadsRoot = uploadsDir + path.sep;
const rawrz = new RawrZStandalone();

// Initialize JWT Authentication
const jwtAuth = new JWTAuth({
  secret: process.env.JWT_SECRET,
  refreshSecret: process.env.JWT_REFRESH_SECRET,
  accessTokenExpiry: process.env.JWT_ACCESS_EXPIRY || '15m',
  refreshTokenExpiry: process.env.JWT_REFRESH_EXPIRY || '7d'
});

// --- Auth Token ---
if (process.env.NODE_ENV === 'production' && !process.env.AUTH_TOKEN) {
  console.error('[FATAL] AUTH_TOKEN not set. Exiting.');
  process.exit(1);
}
if (!process.env.AUTH_TOKEN) {
  console.warn('[WARN] AUTH_TOKEN not set. Using ephemeral token for non-production session.');
  process.env.AUTH_TOKEN = crypto.randomBytes(32).toString('hex');
}

// --- Bootstrap ---
(async () => {
  // Add global error handler for security software interference
  process.on('uncaughtException', (error) => {
    if (error.message.includes('spawn') || error.message.includes('cmd.exe') || error.message.includes('reg query')) {
      console.warn('[SECURITY] Command execution blocked by security software - continuing with fallback methods');
      return; // Don't crash, continue with available features
    }
    console.error('[FATAL] Uncaught Exception:', error);
    process.exit(1);
  });

  process.on('unhandledRejection', (reason, promise) => {
    if (reason && reason.message && (reason.message.includes('spawn') || reason.message.includes('cmd.exe'))) {
      console.warn('[SECURITY] Command execution promise rejected by security software - continuing');
      return; // Don't crash, continue with available features
    }
    console.error('[FATAL] Unhandled Rejection at:', promise, 'reason:', reason);
  });

  // Emit system startup event
  eventBus.emit(COMMON_EVENTS.SYSTEM_STARTUP, {
    timestamp: new Date().toISOString(),
    version: process.env.npm_package_version || '1.0.0',
    nodeVersion: process.version,
    environment: process.env.NODE_ENV || 'development'
  });

  try {
    await fs.mkdir(uploadsDir, { recursive: true });
    console.log('[OK] Ensured uploads directory exists.');
  } catch (e) {
    console.error('[FATAL] Could not create uploads directory:', e.message);
    eventBus.emit(COMMON_EVENTS.SYSTEM_ERROR, {
      component: 'uploads',
      error: e.message,
      fatal: true
    });
    process.exit(1);
  }
  try {
    await rawrzEngine.initializeModules();
    console.log('[OK] RawrZ core engine initialized.');
    eventBus.emit(COMMON_EVENTS.ENGINE_LOADED, {
      engine: 'rawrz-core',
      status: 'initialized'
    });
  } catch (e) {
    console.error('[WARN] Core engine init failed:', e.message);
    eventBus.emit(COMMON_EVENTS.ENGINE_ERROR, {
      engine: 'rawrz-core',
      error: e.message
    });
  }

  // Load dynamic engines with security software resilience
  try {
    const engines = await loadEngines();
    console.log(`[OK] Loaded ${engines.length} dynamic engine(s): ${engines.map(e => e.id).join(', ')}`);
    
    // Emit events for each loaded engine
    engines.forEach(engine => {
      eventBus.emit(COMMON_EVENTS.ENGINE_LOADED, {
        engine: engine.id,
        name: engine.name,
        version: engine.version,
        actions: engine.actions?.length || 0
      });
    });
  } catch (e) {
    console.error('[WARN] Dynamic engine loading failed:', e.message);
    if (e.message.includes('spawn') || e.message.includes('cmd.exe') || e.message.includes('reg query')) {
      console.log('[INFO] Security software is blocking some engine features - continuing with available functionality');
    }
    eventBus.emit(COMMON_EVENTS.ENGINE_ERROR, {
      engine: 'dynamic-loading',
      error: e.message
    });
  }

  // Watch for engine changes in development
  if (process.env.NODE_ENV !== 'production') {
    watchEngines((engines) => {
      console.log(`[ENGINES] Hot-reloaded ${engines.length} engine(s): ${engines.map(e => e.id).join(', ')}`);
      eventBus.emit(COMMON_EVENTS.ENGINE_LOADED, {
        engine: 'hot-reload',
        count: engines.length,
        engines: engines.map(e => e.id)
      });
    });
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
  skip: (req) => {
    // Skip rate limiting if disabled via toggle
    return !toggleManager.isEnabled('security.rate_limiting');
  }
});
const uploadLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: 'Too many uploads from this IP, please try again after 15 minutes.',
  skip: (req) => {
    // Skip rate limiting if disabled via toggle
    return !toggleManager.isEnabled('security.rate_limiting');
  }
});

// compression (skip download path)
app.use(compression({
  filter: (req, res) => {
    // Skip compression if disabled via toggle
    if (!toggleManager.isEnabled('performance.compression')) {
      return false;
    }
    // Skip download path
    return (req.path === '/api/download') ? false : compression.filter(req, res);
  }
}));

app.use(express.json({ limit: '1mb' }));
app.use((err, req, res, next) => {
  if (err?.type === 'entity.parse.failed') return res.status(400).json({ error: 'Invalid JSON body' });
  next(err);
});

app.use('/api', apiLimiter);

// --- Auth Middleware with Toggle Support ---
const requireAuth = async (req, res, next) => {
  // Check if authentication is disabled via toggle
  if (!toggleManager.isEnabled('security.auth_required')) {
    console.log('[AUTH] Bypassed via toggle: security.auth_required');
    return next();
  }

  const header = req.headers.authorization || '';
  const token = header.startsWith('Bearer ') ? header.slice(7).trim() : null;
  if (!token || token !== process.env.AUTH_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  next();
};

// JWT Authentication routes
app.post('/api/auth/login', async (req, res) => {
  try {
    const { username, password } = req.body;
    
    // Simple authentication - in production, use proper user database
    if (username === 'admin' && password === 'admin123') {
      const user = {
        id: 'admin',
        username: 'admin',
        email: 'admin@rawrz.local',
        roles: ['admin']
      };
      
      const tokens = jwtAuth.generateTokenPair(user);
      res.json({
        success: true,
        user: {
          id: user.id,
          username: user.username,
          email: user.email,
          roles: user.roles
        },
        ...tokens
      });
    } else {
      res.status(401).json({ error: 'Invalid credentials' });
    }
  } catch (error) {
    console.error('[Auth Error]', error);
    res.status(500).json({ error: 'Authentication failed' });
  }
});

app.post('/api/auth/refresh', async (req, res) => {
  try {
    const { refreshToken } = req.body;
    const tokens = jwtAuth.refreshAccessToken(refreshToken);
    res.json(tokens);
  } catch (error) {
    res.status(401).json({ error: 'Invalid refresh token' });
  }
});

app.post('/api/auth/logout', async (req, res) => {
  try {
    const { refreshToken } = req.body;
    jwtAuth.logout(refreshToken);
    res.json({ success: true });
  } catch (error) {
    res.status(400).json({ error: 'Logout failed' });
  }
});

// Event bus status endpoint
app.get('/api/events/status', requireAuth, (_req, res) => {
  try {
    res.json(eventBus.health());
  } catch (error) {
    res.status(500).json({ error: 'Failed to get event bus status' });
  }
});

// Event history endpoint
app.get('/api/events/history', requireAuth, (req, res) => {
  try {
    const { event, limit = 100 } = req.query;
    const history = eventBus.getHistory(event, parseInt(limit));
    res.json({ events: history, count: history.length });
  } catch (error) {
    res.status(500).json({ error: 'Failed to get event history' });
  }
});

// --- Helpers with Toggle Support ---
const allowedAlgorithms = ['sha256', 'sha512', 'aes-256-gcm', 'aes-128-cbc'];
const checkAlgorithm = async (algorithm, res) => {
  // Check if algorithm validation is disabled via toggle
  if (!toggleManager.isEnabled('security.algorithm_validation')) {
    console.log('[ALGORITHM] Validation bypassed via toggle: security.algorithm_validation');
    return false;
  }

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
app.get('/api/manifest', async (_req, res) => {
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

// Toggle management panel
app.get('/toggles', requireAuth, async (_req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'toggle-panel.html'));
});

// --- Toggle Management API ---
app.get('/api/toggles', requireAuth, async (_req, res) => {
  try {
    res.json({
      toggles: toggleManager.getAll(),
      status: toggleManager.getStatus()
    });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to get toggles' });
  }
});

app.get('/api/toggles/category/:category', requireAuth, async (req, res) => {
  try {
    const { category } = req.params;
    const toggles = toggleManager.getByCategory(category);
    res.json({ category, toggles });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to get toggles by category' });
  }
});

app.post('/api/toggles/:toggleKey/enable', requireAuth, async (req, res) => {
  try {
    const { toggleKey } = req.params;
    const success = toggleManager.enable(toggleKey);
    if (success) {
      res.json({ success: true, toggle: toggleKey, action: 'enabled' });
    } else {
      res.status(404).json({ error: 'Toggle not found' });
    }
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to enable toggle' });
  }
});

app.post('/api/toggles/:toggleKey/disable', requireAuth, async (req, res) => {
  try {
    const { toggleKey } = req.params;
    const success = toggleManager.disable(toggleKey);
    if (success) {
      res.json({ success: true, toggle: toggleKey, action: 'disabled' });
    } else {
      res.status(404).json({ error: 'Toggle not found' });
    }
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to disable toggle' });
  }
});

app.post('/api/toggles/:toggleKey/toggle', requireAuth, async (req, res) => {
  try {
    const { toggleKey } = req.params;
    const newState = toggleManager.toggle(toggleKey);
    res.json({ success: true, toggle: toggleKey, enabled: newState });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to toggle' });
  }
});

app.post('/api/toggles/bulk', requireAuth, async (req, res) => {
  try {
    const { toggles } = req.body;
    if (!toggles || typeof toggles !== 'object') {
      return res.status(400).json({ error: 'Invalid toggles object' });
    }
    
    const changed = toggleManager.setBulk(toggles);
    res.json({ success: true, changed, message: `Updated ${changed} toggles` });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to update toggles' });
  }
});

app.post('/api/toggles/enable-full-functionality', requireAuth, async (_req, res) => {
  try {
    const enabled = toggleManager.enableFullFunctionality();
    res.json({ 
      success: true, 
      enabled, 
      message: `Enabled ${enabled} toggles for full functionality` 
    });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to enable full functionality' });
  }
});

app.post('/api/toggles/disable-security', requireAuth, async (_req, res) => {
  try {
    const disabled = toggleManager.disableAllSecurity();
    res.json({ 
      success: true, 
      disabled, 
      message: `Disabled ${disabled} security toggles - DANGEROUS MODE ENABLED`,
      warning: 'This disables all security restrictions. Use with extreme caution!'
    });
  } catch (e) {
    console.error('[Toggle Error]', e);
    res.status(500).json({ error: 'Failed to disable security' });
  }
});

// --- Delay Management API ---
app.get('/api/delays', requireAuth, async (_req, res) => {
  try {
    const delays = delayManager.getAllDelays();
    const enabled = delayManager.enabled;
    const randomVariation = delayManager.randomVariation;
    res.json({ 
      success: true, 
      delays, 
      enabled, 
      randomVariation,
      status: 'Delays retrieved successfully'
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.get('/api/delays/category/:category', requireAuth, async (req, res) => {
  try {
    const { category } = req.params;
    const delays = delayManager.getCategoryDelays(category);
    res.json({ success: true, category, delays });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/delays/preset', requireAuth, async (req, res) => {
  try {
    const { preset } = req.body;
    if (!preset || !['fast', 'normal', 'slow', 'realistic', 'stealth'].includes(preset)) {
      return res.status(400).json({ 
        success: false, 
        error: 'Invalid preset. Use: fast, normal, slow, realistic, stealth' 
      });
    }
    
    delayManager.applyPreset(preset);
    res.json({ 
      success: true, 
      preset, 
      message: `Applied ${preset} delay preset` 
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/delays/configure', requireAuth, async (req, res) => {
  try {
    const { category, operation, delay, enabled, randomVariation } = req.body;
    
    if (category && operation && delay !== undefined) {
      delayManager.setDelay(category, operation, delay);
    }
    
    if (enabled !== undefined) {
      delayManager.setEnabled(enabled);
    }
    
    if (randomVariation !== undefined) {
      delayManager.setRandomVariation(randomVariation);
    }
    
    res.json({ 
      success: true, 
      message: 'Delay configuration updated',
      configured: { category, operation, delay, enabled, randomVariation }
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/delays/reset', requireAuth, async (_req, res) => {
  try {
    delayManager.resetDelays();
    res.json({ 
      success: true, 
      message: 'Delays reset to default values' 
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/delays/enable', requireAuth, async (_req, res) => {
  try {
    delayManager.setEnabled(true);
    res.json({ 
      success: true, 
      enabled: true,
      message: 'Delays enabled' 
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/delays/disable', requireAuth, async (_req, res) => {
  try {
    delayManager.setEnabled(false);
    res.json({ 
      success: true, 
      enabled: false,
      message: 'Delays disabled' 
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// --- Engine Reload Endpoint (for development) ---
if (process.env.NODE_ENV !== 'production') {
  app.post('/api/engines/reload', requireAuth, async (_req, res) => {
    try {
      await loadEngines({ force: true });
      res.json({ ok: true, message: 'Engines reloaded successfully' });
    } catch (e) {
      console.error('[Engine Reload Error]', e);
      res.status(500).json({ error: 'Failed to reload engines' });
    }
  });
}

// --- Crypto Router ---
const cryptoRouter = express.Router();
cryptoRouter.use(express.json({ limit: '64kb' }));
cryptoRouter.use(requireAuth);

cryptoRouter.post('/hash',
  body('input').isString().notEmpty().isLength({ max: 60 * 1024 }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    eventBus.emit(COMMON_EVENTS.API_REQUEST, {
      method: 'POST',
      path: '/api/hash',
      userAgent: req.get('User-Agent'),
      ip: req.ip
    });
    
    try {
      const { input, algorithm = 'sha256', save = false, extension } = req.body;
      if (checkAlgorithm(algorithm, res)) return;
      const result = await rawrz.hash(input, algorithm, !!save, extension);
      
      eventBus.emit(COMMON_EVENTS.ENGINE_EXECUTED, {
        engine: 'crypto',
        action: 'hash',
        algorithm,
        success: true
      });
      
      res.json(result);
    } catch (err) {
      console.error('[Crypto Error]', err.stack || err.message);
      eventBus.emit(COMMON_EVENTS.API_ERROR, {
        method: 'POST',
        path: '/api/hash',
        error: err.message
      });
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
      if (checkAlgorithm(algorithm, res)) return;
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
      if (checkAlgorithm(algorithm, res)) return;
      res.json(await rawrz.decrypt(algorithm, input, key, iv, extension));
    } catch (err) {
      console.error('[Crypto Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.use('/api', cryptoRouter);

// --- Multer (memory) ---
const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: 100 * 1024 * 1024, files: 10 },
});

// --- Routes ---
app.get('/health', (_req, res) => res.json({ ok: true, status: 'healthy' }));
app.get('/panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'panel.html')));
app.get('/', (_req, res) => res.redirect('/panel'));

app.get('/api/dns',
  query('hostname').isString().notEmpty(),
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

// System Information Routes
app.get('/api/sysinfo/os', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getOsInfo()); }
  catch (err) {
    console.error('[OS Info Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/sysinfo/cpu', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getCpuInfo()); }
  catch (err) {
    console.error('[CPU Info Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/sysinfo/memory', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getMemoryInfo()); }
  catch (err) {
    console.error('[Memory Info Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/sysinfo/network', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getNetworkInfo()); }
  catch (err) {
    console.error('[Network Info Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/sysinfo/uptime', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getUptime()); }
  catch (err) {
    console.error('[Uptime Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// WMIC Routes - Universal System Management Interface
const universalWMIC = require('./src/engines/universal-wmic-engine');

// Get supported WMIC classes
app.get('/api/wmic/classes', requireAuth, async (_req, res) => {
  try {
    // Check if WMIC features are enabled
    if (!toggleManager.isEnabled('features.wmic_full_access')) {
      return res.status(403).json({ 
        error: 'WMIC features disabled via toggles',
        toggle: 'features.wmic_full_access'
      });
    }
    
    const classes = universalWMIC.getSupportedClasses();
    res.json({
      classes,
      platform: universalWMIC.platform,
      totalClasses: classes.length
    });
  } catch (err) {
    console.error('[WMIC Classes Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Check if WMIC class is supported
app.get('/api/wmic/classes/:className/supported', requireAuth, async (req, res) => {
  try {
    const { className } = req.params;
    const isSupported = universalWMIC.isClassSupported(className);
    res.json({
      className,
      supported: isSupported,
      platform: universalWMIC.platform
    });
  } catch (err) {
    console.error('[WMIC Class Check Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Execute WMIC query
app.post('/api/wmic/query', requireAuth, 
  body('className').isString().notEmpty(),
  body('properties').optional().isArray(),
  body('whereClause').optional().isString(),
  body('format').optional().isIn(['csv', 'table', 'list']),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { className, properties = [], whereClause = '', format = 'csv' } = req.body;
      
      if (!universalWMIC.isClassSupported(className)) {
        return res.status(400).json({ 
          error: `WMIC class '${className}' not supported`,
          supportedClasses: universalWMIC.getSupportedClasses()
        });
      }
      
      const result = await universalWMIC.wmic(className, properties, whereClause, format);
      res.json({
        className,
        properties,
        whereClause,
        format,
        result,
        platform: universalWMIC.platform,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error('[WMIC Query Error]', err.stack || err.message);
      res.status(500).json({ error: err.message || 'Internal Server Error' });
    }
  }
);

// Get system summary
app.get('/api/wmic/system-summary', requireAuth, async (_req, res) => {
  try {
    const summary = await universalWMIC.getSystemSummary();
    res.json({
      summary,
      platform: universalWMIC.platform,
      timestamp: new Date().toISOString()
    });
  } catch (err) {
    console.error('[WMIC System Summary Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Process management
app.get('/api/wmic/processes', requireAuth, async (_req, res) => {
  try {
    const processes = await universalWMIC.wmic('process');
    res.json({
      processes,
      count: processes.length,
      platform: universalWMIC.platform,
      timestamp: new Date().toISOString()
    });
  } catch (err) {
    console.error('[WMIC Processes Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/wmic/processes/:pid/kill', requireAuth,
  body('force').optional().isBoolean(),
  async (req, res) => {
    try {
      // Check if process management features are enabled
      if (!toggleManager.isEnabled('features.process_killing')) {
        return res.status(403).json({ 
          error: 'Process management features disabled via toggles',
          toggle: 'features.process_killing'
        });
      }
      
      const { pid } = req.params;
      const { force = false } = req.body;
      
      if (!pid || isNaN(parseInt(pid))) {
        return res.status(400).json({ error: 'Invalid process ID' });
      }
      
      const result = await universalWMIC.killProcess(parseInt(pid));
      res.json({
        pid: parseInt(pid),
        result,
        force,
        platform: universalWMIC.platform,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error('[WMIC Kill Process Error]', err.stack || err.message);
      res.status(500).json({ error: err.message || 'Internal Server Error' });
    }
  }
);

// Service management
app.get('/api/wmic/services', requireAuth, async (_req, res) => {
  try {
    const services = await universalWMIC.wmic('service');
    res.json({
      services,
      count: services.length,
      platform: universalWMIC.platform,
      timestamp: new Date().toISOString()
    });
  } catch (err) {
    console.error('[WMIC Services Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/wmic/services/:serviceName/:action', requireAuth,
  body('serviceName').isString().notEmpty(),
  body('action').isIn(['start', 'stop', 'restart']),
  async (req, res) => {
    try {
      // Check if service control features are enabled
      if (!toggleManager.isEnabled('features.service_control')) {
        return res.status(403).json({ 
          error: 'Service control features disabled via toggles',
          toggle: 'features.service_control'
        });
      }
      
      const { serviceName, action } = req.params;
      
      let result;
      if (action === 'start') {
        result = await universalWMIC.startService(serviceName);
      } else if (action === 'stop') {
        result = await universalWMIC.stopService(serviceName);
      } else if (action === 'restart') {
        await universalWMIC.stopService(serviceName);
        await new Promise(resolve => setTimeout(resolve, 1000)); // Wait 1 second
        result = await universalWMIC.startService(serviceName);
      }
      
      res.json({
        serviceName,
        action,
        result,
        platform: universalWMIC.platform,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error('[WMIC Service Control Error]', err.stack || err.message);
      res.status(500).json({ error: err.message || 'Internal Server Error' });
    }
  }
);

// Specific WMIC class endpoints for common queries
const commonWmicClasses = [
  'computersystem', 'cpu', 'memorychip', 'logicaldisk', 
  'networkadapter', 'product', 'useraccount', 'bios', 'startup', 'environment'
];

commonWmicClasses.forEach(className => {
  app.get(`/api/wmic/${className}`, requireAuth, async (req, res) => {
    try {
      const { properties = [], where = '', format = 'csv' } = req.query;
      const propertiesArray = Array.isArray(properties) ? properties : 
                              properties ? properties.split(',') : [];
      
      const result = await universalWMIC.wmic(className, propertiesArray, where, format);
      res.json({
        className,
        result,
        count: Array.isArray(result) ? result.length : 1,
        platform: universalWMIC.platform,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error(`[WMIC ${className} Error]`, err.stack || err.message);
      res.status(500).json({ error: err.message || 'Internal Server Error' });
    }
  });
});

// Clear WMIC cache
app.post('/api/wmic/cache/clear', requireAuth, async (_req, res) => {
  try {
    universalWMIC.clearCache();
    res.json({
      message: 'WMIC cache cleared successfully',
      timestamp: new Date().toISOString()
    });
  } catch (err) {
    console.error('[WMIC Cache Clear Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Stealth Engine Routes
const stealthEngine = require('./src/engines/stealth-engine');
app.get('/api/stealth/status', requireAuth, async (_req, res) => {
  try {
    // Check if stealth features are enabled
    if (!toggleManager.isEnabled('stealth.anti_vm_detection') && 
        !toggleManager.isEnabled('stealth.anti_debug') && 
        !toggleManager.isEnabled('stealth.anti_sandbox')) {
      return res.json({ 
        status: 'disabled', 
        message: 'Stealth features disabled via toggles',
        timestamp: new Date().toISOString() 
      });
    }
    
    const status = stealthEngine.getStealthStatus();
    res.json({ status, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Stealth Status Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/stealth/scan', requireAuth, 
  body('mode').optional().isIn(['basic', 'standard', 'full', 'maximum']),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      // Check if stealth features are enabled
      if (!toggleManager.isEnabled('stealth.anti_vm_detection') && 
          !toggleManager.isEnabled('stealth.anti_debug') && 
          !toggleManager.isEnabled('stealth.anti_sandbox')) {
        return res.status(403).json({ 
          error: 'Stealth features disabled via toggles',
          toggles: {
            'stealth.anti_vm_detection': toggleManager.isEnabled('stealth.anti_vm_detection'),
            'stealth.anti_debug': toggleManager.isEnabled('stealth.anti_debug'),
            'stealth.anti_sandbox': toggleManager.isEnabled('stealth.anti_sandbox')
          }
        });
      }
      
      const { mode = 'standard' } = req.body;
      const result = await stealthEngine.performStealthScan(mode);
      res.json({ result, mode, timestamp: new Date().toISOString() });
    } catch (err) {
      console.error('[Stealth Scan Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  }
);

// Health Monitor Routes
const healthMonitor = require('./src/engines/health-monitor');
app.get('/api/health/status', requireAuth, async (_req, res) => {
  try {
    const status = healthMonitor.getSystemHealth();
    res.json({ status, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Health Status Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/health/metrics', requireAuth, async (_req, res) => {
  try {
    const metrics = healthMonitor.getMetrics();
    res.json({ metrics, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Health Metrics Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Red Killer Routes
const redKiller = require('./src/engines/red-killer');
app.get('/api/redkiller/status', requireAuth, async (_req, res) => {
  try {
    // Check if process management features are enabled
    if (!toggleManager.isEnabled('features.process_killing')) {
      return res.json({ 
        status: 'disabled', 
        message: 'Process management features disabled via toggles',
        timestamp: new Date().toISOString() 
      });
    }
    
    const status = redKiller.getStatus();
    res.json({ status, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Red Killer Status Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/redkiller/scan', requireAuth, async (_req, res) => {
  try {
    // Check if process management features are enabled
    if (!toggleManager.isEnabled('features.process_killing')) {
      return res.status(403).json({ 
        error: 'Process management features disabled via toggles',
        toggle: 'features.process_killing'
      });
    }
    
    const result = await redKiller.performScan();
    res.json({ result, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Red Killer Scan Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// OpenSSL Management Routes
const opensslManagement = require('./src/engines/openssl-management');
app.get('/api/openssl/status', requireAuth, async (_req, res) => {
  try {
    // Check if advanced crypto features are enabled
    if (!toggleManager.isEnabled('features.advanced_crypto')) {
      return res.json({ 
        status: 'disabled', 
        message: 'Advanced crypto features disabled via toggles',
        timestamp: new Date().toISOString() 
      });
    }
    
    const status = opensslManagement.getStatus();
    res.json({ status, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[OpenSSL Status Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/openssl/algorithms', requireAuth, async (_req, res) => {
  try {
    // Check if advanced crypto features are enabled
    if (!toggleManager.isEnabled('features.advanced_crypto')) {
      return res.status(403).json({ 
        error: 'Advanced crypto features disabled via toggles',
        toggle: 'features.advanced_crypto'
      });
    }
    
    const algorithms = opensslManagement.getSupportedAlgorithms();
    res.json({ algorithms, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[OpenSSL Algorithms Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Advanced Anti-Analysis Routes
const advancedAntiAnalysis = require('./src/engines/advanced-anti-analysis');
app.get('/api/anti-analysis/status', requireAuth, async (_req, res) => {
  try {
    // Check if anti-analysis features are enabled
    if (!toggleManager.isEnabled('stealth.anti_vm_detection') && 
        !toggleManager.isEnabled('stealth.anti_debug') && 
        !toggleManager.isEnabled('stealth.anti_sandbox')) {
      return res.json({ 
        status: 'disabled', 
        message: 'Anti-analysis features disabled via toggles',
        timestamp: new Date().toISOString() 
      });
    }
    
    const status = advancedAntiAnalysis.getStatus();
    res.json({ status, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Anti-Analysis Status Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/anti-analysis/scan', requireAuth, async (_req, res) => {
  try {
    // Check if anti-analysis features are enabled
    if (!toggleManager.isEnabled('stealth.anti_vm_detection') && 
        !toggleManager.isEnabled('stealth.anti_debug') && 
        !toggleManager.isEnabled('stealth.anti_sandbox')) {
      return res.status(403).json({ 
        error: 'Anti-analysis features disabled via toggles',
        toggles: {
          'stealth.anti_vm_detection': toggleManager.isEnabled('stealth.anti_vm_detection'),
          'stealth.anti_debug': toggleManager.isEnabled('stealth.anti_debug'),
          'stealth.anti_sandbox': toggleManager.isEnabled('stealth.anti_sandbox')
        }
      });
    }
    
    const result = await advancedAntiAnalysis.performAnalysis();
    res.json({ result, timestamp: new Date().toISOString() });
  } catch (err) {
    console.error('[Anti-Analysis Scan Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Generic Engine Execution Route
app.post('/api/engines/:engineId/execute', requireAuth,
  body('action').optional().isString(),
  body('params').optional().isObject(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    
    try {
      const { engineId } = req.params;
      const { action = 'execute', params = {} } = req.body;
      
      // Dynamic engine loading and execution
      const engines = await loadEngines();
      const engine = engines.find(e => e.id === engineId);
      
      if (!engine) {
        return res.status(404).json({ error: `Engine '${engineId}' not found` });
      }
      
      // Try to load the actual engine module
      const enginePath = path.join(__dirname, 'src', 'engines', `${engineId}.js`);
      let engineModule;
      
      try {
        delete require.cache[require.resolve(enginePath)];
        engineModule = require(enginePath);
      } catch (e) {
        return res.status(500).json({ error: `Failed to load engine module: ${e.message}` });
      }
      
      // Execute the engine action
      let result;
      if (typeof engineModule === 'function' && engineModule.prototype) {
        // Class-based engine
        const instance = new engineModule();
        if (typeof instance[action] === 'function') {
          result = await instance[action](params);
        } else {
          result = { message: `Action '${action}' not found in engine`, availableMethods: Object.getOwnPropertyNames(Object.getPrototypeOf(instance)) };
        }
      } else if (typeof engineModule[action] === 'function') {
        // Function-based engine
        result = await engineModule[action](params);
      } else {
        result = { message: `Action '${action}' not found`, availableMethods: Object.keys(engineModule) };
      }
      
      res.json({
        engineId,
        action,
        result,
        timestamp: new Date().toISOString()
      });
    } catch (err) {
      console.error('[Engine Execute Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  }
);

// Database Routes
app.post('/api/db/query',
  requireAuth, body('query').isString().notEmpty().isLength({ max: 10000 }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { query, limit = 100 } = req.body;
      res.json(await rawrz.executeQuery(query, parseInt(limit)));
    } catch (err) {
      console.error('[Database Query Error]', err.stack || err.message);
      res.status(500).json({ error: err.message || 'Database query failed' });
    }
  });

app.get('/api/db/tables', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getTables()); }
  catch (err) {
    console.error('[Database Tables Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.get('/api/db/schema',
  requireAuth, query('table').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      res.json(await rawrz.getTableSchema(String(req.query.table)));
    } catch (err) {
      console.error('[Database Schema Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.get('/api/db/stats', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getDatabaseStats()); }
  catch (err) {
    console.error('[Database Stats Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// Workflow Routes
app.post('/api/workflow/execute',
  requireAuth, body('name').isString().notEmpty(),
  body('steps').isString().notEmpty().isLength({ max: 50000 }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { name, steps, parallel = false } = req.body;
      res.json(await rawrz.executeWorkflow(name, steps, Boolean(parallel)));
    } catch (err) {
      console.error('[Workflow Execute Error]', err.stack || err.message);
      res.status(500).json({ error: err.message || 'Workflow execution failed' });
    }
  });

app.get('/api/workflow/templates', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getWorkflowTemplates()); }
  catch (err) {
    console.error('[Workflow Templates Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/workflow/validate',
  requireAuth, body('steps').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { steps } = req.body;
      res.json(await rawrz.validateWorkflow(steps));
    } catch (err) {
      console.error('[Workflow Validate Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.get('/api/workflow/history',
  requireAuth, query('limit').optional().isInt({ min: 1, max: 1000 }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const limit = parseInt(req.query.limit) || 50;
      res.json(await rawrz.getWorkflowHistory(limit));
    } catch (err) {
      console.error('[Workflow History Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

// Security Routes
app.get('/api/security/token-info', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getTokenInfo()); }
  catch (err) {
    console.error('[Token Info Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/security/validate-token',
  requireAuth, body('token').isString().notEmpty(),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { token } = req.body;
      res.json(await rawrz.validateToken(token));
    } catch (err) {
      console.error('[Token Validation Error]', err.stack || err.message);
      res.status(500).json({ error: 'Internal Server Error' });
    }
  });

app.get('/api/security/permissions', requireAuth, async (_req, res) => {
  try { res.json(await rawrz.getUserPermissions()); }
  catch (err) {
    console.error('[Permissions Error]', err.stack || err.message);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

app.post('/api/security/audit-log',
  requireAuth, body('limit').optional().isInt({ min: 1, max: 1000 }),
  body('level').optional().isIn(['all', 'info', 'warn', 'error', 'critical']),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return res.status(400).json({ errors: errors.array() });
    try {
      const { limit = 100, level = 'all' } = req.body;
      res.json(await rawrz.getAuditLog(parseInt(limit), level));
    } catch (err) {
      console.error('[Audit Log Error]', err.stack || err.message);
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
      // Check if file type validation is enabled
      if (toggleManager.isEnabled('security.file_type_validation')) {
        const type = await fileTypeFromBuffer(req.file.buffer.subarray(0, 4100)); // 4KB sniff
        const allowed = [
          'application/x-executable','application/x-msdownload','application/x-sharedlib',
          'application/octet-stream','application/x-dll','application/x-exe',
          'application/x-elf','application/x-mach-o'
        ];
        if (!type || !allowed.includes(type.mime)) {
          return res.status(400).json({ error: `File type not supported: ${type ? type.mime : 'unknown'}` });
        }
      } else {
        console.log('[UPLOAD] File type validation bypassed via toggle: security.file_type_validation');
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
app.use((err, req, res, next) => {
  if (err instanceof multer.MulterError) {
    if (err.code === 'LIMIT_FILE_SIZE') return res.status(413).json({ error: 'File size too large. Max 100MB allowed.' });
    if (err.code === 'LIMIT_FILE_COUNT') return res.status(413).json({ error: 'Too many files uploaded. Max 10 files allowed.' });
    return res.status(400).json({ error: `Upload error: ${err.message}` });
  }
  next(err);
});

// --- Global Error Handler ---
app.use((err, req, res, next) => {
  console.error('[Unhandled Error]', err.stack || err.message);
  res.status(500).json({ error: 'Internal Server Error' });
});

// --- 404 ---
app.use((req, res) => res.status(404).json({ error: 'Not Found' }));

// --- Server & Shutdown ---
const server = app.listen(port, () => {
  console.log(`
╔══════════════════════════════════════════════════════════════════════════════╗
║                        RawrZ HTTP Control Center                          ║
║                           Web Interface Launched                          ║
╚══════════════════════════════════════════════════════════════════════════════╝
`);
  console.log(`🌐 Control Center: http://localhost:${port}/panel`);
  console.log(`🔧 Toggle Panel: http://localhost:${port}/toggles`);
  console.log(`📊 Health Check: http://localhost:${port}/health`);
  console.log(`🔑 Auth Required: ${toggleManager.isEnabled('security.auth_required') ? 'YES' : 'NO'}`);
  console.log(`⚡ Server running on port ${port}`);
  
  // Display toggle status
  const toggleStatus = toggleManager.getStatus();
  console.log(`\n📈 Feature Status: ${toggleStatus.enabled}/${toggleStatus.total} toggles enabled`);
  
  if (process.env.NODE_ENV !== 'production') {
    console.log('\n🔧 Development Mode:');
    console.log('   • Hot-reload enabled');
    console.log('   • Schema validation active');
    console.log('   • Run `npm audit` regularly for security checks');
    
    // Show security bypass status
    if (!toggleManager.isEnabled('security.auth_required')) {
      console.log('\n⚠️  WARNING: Authentication is DISABLED - this is insecure!');
    }
    if (!toggleManager.isEnabled('security.rate_limiting')) {
      console.log('\n⚠️  WARNING: Rate limiting is DISABLED - this may allow abuse!');
    }
  }
  
  console.log('\n💡 Use the web interface for full control center functionality');
  console.log('💡 CLI mode available via: npm run cli');
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
const shutdown = (signal) => () => {
  console.log(`[${signal}] Shutting down...`);
  
  // Emit system shutdown event
  eventBus.emit(COMMON_EVENTS.SYSTEM_SHUTDOWN, {
    signal,
    timestamp: new Date().toISOString(),
    uptime: process.uptime()
  });
  
  server.close(err => process.exit(err ? 1 : 0));
};
process.on('SIGTERM', shutdown('SIGTERM'));
process.on('SIGINT', shutdown('SIGINT'));
process.on('unhandledRejection', e => console.error('[unhandledRejection]', e));
process.on('uncaughtException', e => { console.error('[uncaughtException]', e); process.exit(1); });
