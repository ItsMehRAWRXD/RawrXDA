'use strict';

const fs = require('fs');
const fsp = require('fs/promises');
const path = require('path');
const Ajv = require('ajv');

const ENGINES_DIR = path.resolve(__dirname);
const SCHEMA_PATH = path.join(__dirname, 'schemas', 'engine.schema.json');

// cache
let _cache = { engines: [], mtime: 0 };
let _watching = false;

const ajv = new Ajv({ 
  allErrors: true, 
  allowUnionTypes: true,
  strict: false,
  validateFormats: false
});

// Add meta-schemas to fix validation issues
try {
  // Load the correct meta-schemas for AJV
  const draft07 = require('ajv/dist/refs/json-schema-draft-07.json');
  const draft2019 = require('ajv/dist/refs/json-schema-2019-09/schema.json');
  const draft2020 = require('ajv/dist/refs/json-schema-2020-12/schema.json');
  
  // Add meta-schemas with proper error handling
  try {
    ajv.addMetaSchema(draft07);
  } catch (e) {
    // Meta-schema already exists, this is normal
  }
  
  try {
    ajv.addMetaSchema(draft2019);
  } catch (e) {
    // Meta-schema already exists, this is normal
  }
  
  try {
    ajv.addMetaSchema(draft2020);
  } catch (e) {
    // Meta-schema already exists, this is normal
  }
  
  console.log('[ENGINES] JSON Schema validation initialized with meta-schemas');
} catch (e) {
  console.warn('[ENGINES] Could not load AJV meta-schemas, using fallback validation:', e.message);
}

async function loadSchema() {
  try {
    const raw = await fsp.readFile(SCHEMA_PATH, 'utf8');
    const schema = JSON.parse(raw);
    
    // Validate the schema itself
    if (!ajv.validateSchema(schema)) {
      console.warn('[ENGINES] Schema validation failed:', ajv.errors);
      return null;
    }
    
    const compiled = ajv.compile(schema);
    console.log('[ENGINES] Engine schema loaded and validated successfully');
    return compiled;
  } catch (e) {
    console.warn('[ENGINES] Schema loading failed, skipping validation:', e.message);
    return null;
  }
}

function normalizeAction(a = {}) {
  // safe defaults
  const method = (a.method || 'POST').toUpperCase();
  const pathStr = a.path || '/';
  const inputs = Array.isArray(a.inputs) ? a.inputs : [];
  return {
    method,
    path: pathStr,
    summary: a.summary || `${method} ${pathStr}`,
    inputs: inputs.map(f => ({
      name: String(f.name || '').trim(),
      type: f.type || 'text',
      required: Boolean(f.required),
      help: f.help || '',
      placeholder: f.placeholder || '',
      maxLength: typeof f.maxLength === 'number' ? f.maxLength : undefined,
      options: Array.isArray(f.options) ? f.options : undefined,
      multiline: Boolean(f.multiline)
    }))
  };
}

function normalizeEngine(e = {}) {
  return {
    id: String(e.id || '').trim() || 'engine',
    name: e.name || e.id || 'Engine',
    version: e.version || '0.0.0',
    icon: e.icon || '🔧',
    group: e.group || 'General',
    actions: Array.isArray(e.actions) ? e.actions.map(normalizeAction) : []
  };
}

async function readEngines(validate) {
  const entries = await fsp.readdir(ENGINES_DIR, { withFileTypes: true });

  const files = entries
    .filter(d => d.isFile() && /\.js$/i.test(d.name) && !d.name.includes('index.js'))
    .map(d => path.join(ENGINES_DIR, d.name));

  const engines = [];
  for (const file of files) {
    try {
      delete require.cache[require.resolve(file)]; // hot reload
      const mod = require(file);
      const raw = mod?.default || mod;
      
      // Check if it's a class-based engine (has constructor and methods)
      if (raw && typeof raw === 'function' && raw.prototype) {
        // Convert class-based engine to manifest
        const engineName = path.basename(file, '.js');
        const manifest = {
          id: engineName.toLowerCase().replace(/-/g, '_'),
          name: engineName.replace(/-/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
          version: '1.0.0',
          icon: '🔧',
          group: 'Core',
          actions: [
            {
              method: 'POST',
              path: `/${engineName.toLowerCase()}`,
              summary: `Execute ${engineName} operations`,
              inputs: []
            }
          ]
        };
        
        if (validate && !validate(manifest)) {
          console.warn(`[ENGINES] Invalid generated manifest for ${path.basename(file)}:`, validate.errors);
          continue;
        }
        engines.push(normalizeEngine(manifest));
      } else if (raw && typeof raw === 'object' && raw.id) {
        // It's already a manifest
        if (validate && !validate(raw)) {
          console.warn(`[ENGINES] Invalid engine manifest: ${path.basename(file)}\n`, validate.errors);
          continue;
        }
        engines.push(normalizeEngine(raw));
      } else {
        console.warn(`[ENGINES] Unknown engine format: ${path.basename(file)}`);
      }
    } catch (e) {
      if (e.message.includes('spawn') || e.message.includes('cmd.exe') || e.message.includes('reg query')) {
        console.warn(`[ENGINES] Engine ${path.basename(file)} blocked by security software - using fallback`);
      } else {
        console.warn(`[ENGINES] Failed to load ${path.basename(file)}:`, e.message);
      }
    }
  }
  return engines;
}

async function statTreeMtime(root) {
  try {
    const files = await fsp.readdir(root, { withFileTypes: true });
    let m = 0;
    for (const d of files) {
      const p = path.join(root, d.name);
      const s = await fsp.stat(p);
      m = Math.max(m, s.mtimeMs);
    }
    return m;
  } catch (e) {
    return 0;
  }
}

async function _reload() {
  const validate = await loadSchema();
  const engines = await readEngines(validate);
  const mtime = await statTreeMtime(ENGINES_DIR);
  _cache = { engines, mtime };
  return _cache;
}

async function loadEngines(opts = {}) {
  const nowM = await statTreeMtime(ENGINES_DIR).catch(() => 0);
  if (!_cache.engines.length || nowM > _cache.mtime || opts.force) {
    await _reload();
  }
  return _cache.engines;
}

function watchEngines(onChange = () => {}) {
  if (_watching) return;
  _watching = true;
  
  let reloadTimeout = null;
  let lastReloadTime = 0;
  const debounceDelay = 2000; // 2 second debounce
  const minReloadInterval = 5000; // Minimum 5 seconds between reloads
  
  const debouncedReload = async () => {
    const now = Date.now();
    
    // Prevent too frequent reloads
    if (now - lastReloadTime < minReloadInterval) {
      console.log('[ENGINES] Skipping reload - too soon since last reload');
      return;
    }
    
    if (reloadTimeout) {
      clearTimeout(reloadTimeout);
    }
    
    reloadTimeout = setTimeout(async () => {
      try {
        await _reload();
        onChange(_cache.engines);
        lastReloadTime = Date.now();
        console.log('[ENGINES] Engines reloaded successfully');
      } catch (e) {
        console.warn('[ENGINES] Reload failed:', e.message);
      }
    }, debounceDelay);
  };
  
  // File patterns to ignore
  const ignorePatterns = [
    /\.tmp$/,
    /~$/,
    /\.swp$/,
    /\.swx$/,
    /\.DS_Store$/,
    /Thumbs\.db$/,
    /\.git/,
    /node_modules/,
    /\.log$/,
    /\.cache$/,
    /\.bak$/,
    /\.orig$/,
    /\.rej$/
  ];
  
  const shouldIgnore = (filename) => {
    return ignorePatterns.some(pattern => pattern.test(filename));
  };
  
  try {
    const watcher = fs.watch(ENGINES_DIR, { 
      recursive: false,
      persistent: true
    }, (eventType, filename) => {
      // Ignore if no filename or should be ignored
      if (!filename || shouldIgnore(filename)) {
        return;
      }
      
      // Only reload on actual file changes to .js files (but not index.js)
      if (eventType === 'change' && filename.endsWith('.js') && !filename.includes('index.js')) {
        console.log(`[ENGINES] Engine file changed: ${filename}`);
        debouncedReload();
      }
    });
    
    // Handle watcher errors
    watcher.on('error', (error) => {
      console.error('[ENGINES] File watcher error:', error.message);
      _watching = false;
    });
    
    console.log('[ENGINES] File watcher initialized for engine hot-reloading');
    
  } catch (error) {
    console.warn('[ENGINES] Could not initialize file watcher:', error.message);
    _watching = false;
  }
}

module.exports = {
  loadEngines,
  watchEngines,
  _reload, // for tests
};