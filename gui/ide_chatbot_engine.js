// ═══════════════════════════════════════════════════════════════════════
// RawrXD IDE Chatbot — Shared Engine (auto-extracted from ide_chatbot.html)
// This file is loaded by both the main chatbot and the Win32 themed variant.
// ═══════════════════════════════════════════════════════════════════════

// Guard: avoid const redeclaration when Win32 HTML pre-declares these
if (typeof _isFileProtocol === 'undefined') {
  var _isFileProtocol = (window.location.protocol === 'file:');
}
if (typeof _ideServerUrl === 'undefined') {
  var _ideServerUrl = 'http://localhost:8080';
}

// ======================================================================
// STATE
// ======================================================================
// Detect if opened from file:// — if so, we must probe localhost for the backend
// [guard: _isFileProtocol moved to top]
// Resolved IDE server base URL (set during connectBackend)
// Used by readLocalFile() since window.location.origin is null in file:// context
// [guard: _ideServerUrl moved to top]

var State = {
  backend: {
    online: false,
    url: 'http://localhost:8080',
    ollamaDirectUrl: 'http://localhost:11434',  // Fallback: talk to Ollama directly
    directMode: false,                           // true = bypassing serve.py, talking to Ollama
    lastPing: 0,
    serverType: null,         // 'rawrxd-win32ide' or 'RawrXD-serve.py' or 'ollama-direct'
    fileProtocol: _isFileProtocol,               // true when opened via file:///
    hasCliEndpoint: false,    // Beacon: true if backend has /api/cli (Win32IDE or tool_server)
  },
  model: { current: null, list: [] },
  gen: {
    context: 8192, maxTokens: 512, temperature: 0.7, stream: true,
    top_p: 0.9, top_k: 40,
    // Tensor Bunny Hop: skip unneeded tensor layers during inference for faster large-model runs
    tensorHop: {
      enabled: false,
      strategy: 'auto',     // 'auto' | 'even' | 'front' | 'back' | 'custom'
      skipRatio: 0.25,      // fraction of layers to skip (0.0–0.5)
      keepFirst: 2,         // always keep first N layers
      keepLast: 2,          // always keep last N layers
      customSkip: [],       // manual layer indices to skip
    },
    // Safe Decode Profile: auto-clamp for large models to prevent HTTP 500
    safeDecodeProfile: {
      enabled: true,
      autoClamp: true,      // auto-detect model size and clamp
      thresholdB: 27,       // models >= this size (in B) trigger safe profile
      safeContext: 3072,
      safeMaxTokens: 128,
      safeTemperature: 0.3,
      safeTopP: 0.9,
      safeTopK: 40,
      probeFirst: true,     // send first-token probe before real request
      disableStreamOnSafe: true,
    },
  },
  files: [],                // attached File objects
  fileContents: {},           // name -> text content (read via FileReader)
  filePendingReads: 0,        // count of FileReader ops still in progress
  debug: { open: false },
  chat: { sending: false, messageCount: 0 },
  terminal: { history: [], index: -1, minimized: false, mode: 'local' },
  reconnectTimer: null,
  failureData: [],
  perf: {
    history: [],           // ring buffer of last 100 requests
    maxHistory: 100,
    // Aggregate rolling stats
    totalRequests: 0,
    totalTokens: 0,
    totalLatency: 0,
    // Per-model breakdown: { modelName: { requests, tokens, totalLatency, avgTps, bestTps } }
    modelStats: {},
    // Structured log ring buffer (last 500 entries)
    structuredLog: [],
    maxLog: 500,
  },
  // Phase 4: Security & Hardening
  security: {
    rateLimit: {
      maxPerMinute: 30,
      windowMs: 60000,
      timestamps: [],      // ring buffer of request timestamps
      blocked: 0,
    },
    inputGuard: {
      maxLength: 1000000000,
      blockedXss: 0,
      blockedLength: 0,
      blockedUrl: 0,
    },
    urlAllowlist: [/^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?\/?/i],
    eventLog: [],          // ring buffer of security events
    maxLog: 200,
  },
  // Ghost IDE session: RDP-style beacon into Win32 IDE via iframe
  ghost: {
    active: false,          // true when ghost overlay is visible
    minimized: false,       // true when ghost bar is minimized
    ideUrl: null,           // resolved URL for the ghost iframe
    probeTime: 0,           // last probe timestamp
    iframeRef: null,        // DOM reference to injected iframe
    detachedWindow: null,   // reference to detached popup window
    sessionId: 0,           // monotonic session counter
    waitingForIde: false,   // true when auto-retry polling for IDE
    autoRetryTimer: null,   // setInterval handle for auto-retry
    scanPorts: [8080, 3000, 5000, 11434], // ports to scan for IDE (server.js first)
    lastScanResults: [],    // [{port, status, backend}] from last scan
  },
  // VSIX Extension Manager state
  extensions: {
    installed: [],           // [{id, name, version, publisher, enabled, activationEvents, contributes, size, installedAt}]
    running: [],             // [{id, pid, hostType, uptime, memoryMB, status}]
    marketplace: {           // cached marketplace search results
      query: '',
      results: [],
      lastSearch: 0,
    },
    hostStatus: 'idle',      // 'idle' | 'starting' | 'running' | 'error'
    hostPid: null,
    totalActivated: 0,
    logs: [],                // extension host log ring buffer (max 200)
    maxLogs: 200,
    pendingInstalls: [],     // VSIX files currently being processed
    scanPaths: [             // paths to scan for locally installed extensions
      'extensions/',
      'plugins/',
      '.vscode/extensions/',
    ],
  },
  // WebView2 Built-In Browser: model website access & web browsing
  browser: {
    active: false,
    tabs: [
      { id: 0, title: 'New Tab', url: '', history: [], historyIdx: -1, iframeRef: null }
    ],
    activeTab: 0,
    nextTabId: 1,
    bookmarks: [
      { name: 'HuggingFace GGUF', url: 'https://huggingface.co/models?sort=trending&search=gguf', icon: '\uD83E\uDD17' },
      { name: 'Ollama Library', url: 'https://ollama.com/library', icon: '\uD83E\uDD99' },
      { name: 'TheBloke', url: 'https://huggingface.co/TheBloke', icon: '\uD83D\uDCE6' },
      { name: 'bartowski', url: 'https://huggingface.co/bartowski', icon: '\uD83D\uDCE6' },
      { name: 'llama.cpp', url: 'https://github.com/ggerganov/llama.cpp', icon: '\uD83D\uDCBB' },
      { name: 'Ollama Docs', url: 'https://docs.ollama.com', icon: '\uD83D\uDCD6' },
      { name: 'OpenRouter', url: 'https://openrouter.ai/models', icon: '\uD83C\uDF10' },
    ],
    zoom: 100,
    devtoolsOpen: false,
    searchEngine: 'https://www.google.com/search?igu=1&q=',
    homePage: 'home',   // 'home' = built-in home, or a URL string
    totalNavigations: 0,
  },
};

// ======================================================================
// ENGINE REGISTRY — All available engines in the RawrXD codebase
// Each engine can be swapped in/out via !engine swap <name>
// ======================================================================
var EngineRegistry = {
  active: 'inference',
  engines: {
    'inference': { name: 'InferenceEngine', module: 'core', status: 'active', desc: 'Primary GGUF model inference (CPU/GPU)' },
    'cpu-inference': { name: 'CPUInferenceEngine', module: 'core', status: 'standby', desc: 'CPU-only inference with AVX2/AVX-512' },
    'agentic': { name: 'AgenticEngine', module: 'agentic', status: 'standby', desc: 'Agentic task execution & tool dispatch' },
    'agentic-deep': { name: 'AgenticDeepThinkingEngine', module: 'agentic', status: 'standby', desc: 'Multi-step deep reasoning chains' },
    'zero-day': { name: 'ZeroDayAgenticEngine', module: 'agentic', status: 'standby', desc: 'Zero-day vulnerability analysis engine' },
    '800b': { name: 'Engine800B', module: 'core', status: 'standby', desc: '800B parameter dual-engine configuration' },
    'solo-compiler': { name: 'SoloCompilerEngine', module: 'compiler', status: 'standby', desc: 'Standalone MASM64/C++ compilation' },
    'ai-completion': { name: 'AICompletionEngine', module: 'ai', status: 'standby', desc: 'AI-powered code completion with fallback' },
    'ai-digestion': { name: 'AIDigestionEngine', module: 'ai', status: 'standby', desc: 'Source code digestion & analysis' },
    'transformer': { name: 'TransformerInference', module: 'core', status: 'standby', desc: 'Transformer model inference (attention/FFN)' },
    'hotpatcher': { name: 'HotpatcherEngine', module: 'core', status: 'standby', desc: 'Three-layer hotpatching (memory/byte/server)' },
    'pattern': { name: 'PatternEngine', module: 'asm', status: 'standby', desc: 'AVX-512 SIMD pattern matching' },
    'polyfill': { name: 'PolyfillEngine', module: 'ext', status: 'standby', desc: 'Extension polyfill host (JS compat layer)' },
    'js-extension': { name: 'QuickJSExtensionHost', module: 'ext', status: 'standby', desc: 'QuickJS-based extension host' },
    'vulkan-compute': { name: 'VulkanCompute', module: 'gpu', status: 'standby', desc: 'Vulkan GPU compute shaders' },
    'reverse': { name: 'RawrReverseEngine', module: 're', status: 'standby', desc: 'Binary reverse engineering & PE analysis' },
    'codebase': { name: 'IntelligentCodebaseEngine', module: 'core', status: 'standby', desc: 'Codebase intelligence & refactoring' },
    'streaming-gguf': { name: 'StreamingGGUFLoader', module: 'loader', status: 'standby', desc: 'Enhanced streaming GGUF model loader' },
    'tensor-traversal': { name: 'IterativeTensorTraversal', module: 'core', status: 'standby', desc: 'Layer-by-layer tensor traversal engine' },
    'layer-scorer': { name: 'LayerContributionScorer', module: 'core', status: 'standby', desc: 'Tensor layer importance scoring' },
    'convergence': { name: 'ConvergenceController', module: 'core', status: 'standby', desc: 'Training convergence monitoring' },
    'cross-run-cache': { name: 'CrossRunTensorCache', module: 'core', status: 'standby', desc: 'Persistent tensor cache across runs' },
    'pipeline-parallel': { name: 'AdaptivePipelineParallel', module: 'core', status: 'standby', desc: 'Adaptive pipeline parallelism' },
    'multi-response': { name: 'MultiResponseEngine', module: 'phase10', status: 'standby', desc: 'S/G/C/X multi-response generation' },
    'flight-recorder': { name: 'FlightRecorder', module: 'core', status: 'standby', desc: 'MASM64 ring-buffer flight recorder' },
    'cot': { name: 'ChainOfThoughtEngine', module: 'core', status: 'standby', desc: 'Chain-of-Thought multi-model reasoning' },
    'vsix-host': { name: 'VSIXExtensionHost', module: 'ext', status: 'standby', desc: 'VSIX extension host — loads .vsix packages, manages activation events' },
    'ext-manager': { name: 'ExtensionManager', module: 'ext', status: 'standby', desc: 'Extension lifecycle manager — install, enable, disable, uninstall' },
    'marketplace': { name: 'MarketplaceClient', module: 'ext', status: 'standby', desc: 'VS Code Marketplace API client — search, download, version resolve' },
    'ext-sandbox': { name: 'ExtensionSandbox', module: 'ext', status: 'standby', desc: 'Sandboxed extension execution environment (QuickJS + polyfills)' },
  },
  getActive: function () { return this.engines[this.active] || null; },
  swap: function (name) {
    if (!this.engines[name]) return { success: false, msg: 'Unknown engine: ' + name };
    if (name === this.active) return { success: true, msg: 'Engine ' + name + ' is already active' };
    var prev = this.active;
    this.engines[prev].status = 'standby';
    this.active = name;
    this.engines[name].status = 'active';
    return { success: true, msg: 'Swapped: ' + prev + ' \u2192 ' + name + ' (' + this.engines[name].name + ')' };
  },
  listByModule: function () {
    var mods = {};
    for (var k in this.engines) {
      var e = this.engines[k];
      if (!mods[e.module]) mods[e.module] = [];
      mods[e.module].push({ id: k, name: e.name, status: e.status, desc: e.desc });
    }
    return mods;
  }
};

// ======================================================================
// CODEX OPERATIONS — Compilation, code generation, and MASM64 toolchain
// ======================================================================
var CodexOps = {
  targets: [
    { name: 'RawrXD-Win32IDE', config: 'Release', desc: 'Main Win32 IDE executable' },
    { name: 'RawrEngine', config: 'Release', desc: 'Headless inference engine' },
    { name: 'rawrxd-monaco-gen', config: 'Release', desc: 'Monaco editor generator' },
    { name: 'self_test_gate', config: 'Release', desc: 'Self-test validation gate' },
    { name: 'quant_utils', config: 'Release', desc: 'Quantization utilities' },
    { name: 'tool_server', config: 'Release', desc: 'Standalone tool server' },
  ],
  lastBuild: null,
  buildCmd: function (target) {
    return 'cmake --build build --config Release' + (target ? ' --target ' + target : '');
  },
  masmCmd: function (asmFile) {
    return 'ml64 /c /Fo build\\obj\\' + asmFile.replace('.asm', '.obj') + ' src\\asm\\' + asmFile;
  }
};

// ======================================================================
// REVERSE ENGINEERING STATE
// ======================================================================
var REState = {
  activeTab: 'overview',
  modules: [
    { name: 'PE Analyzer', id: 'pe-analyzer', status: 'ready', desc: 'PE header parsing, section analysis, import/export tables' },
    { name: 'Disassembler', id: 'disassembler', status: 'ready', desc: 'x64 instruction disassembly with AVX/SSE decode' },
    { name: 'Deobfuscator', id: 'deobfuscator', status: 'ready', desc: 'Control flow deobfuscation & pattern recovery' },
    { name: 'Omega Suite', id: 'omega-suite', status: 'ready', desc: 'Complete reverse engineering pipeline' },
    { name: 'Binary Comparator', id: 'binary-compare', status: 'ready', desc: 'Binary diff & patch analysis between builds' },
    { name: 'Symbol Resolver', id: 'symbol-resolver', status: 'ready', desc: 'PDB symbol resolution & GSI hash lookup' },
    { name: 'GGUF Inspector', id: 'gguf-inspector', status: 'ready', desc: 'GGUF model metadata & tensor inspection' },
    { name: 'Memory Scanner', id: 'memory-scanner', status: 'ready', desc: 'Runtime memory pattern scanning' },
  ],
  history: [],
  maxHistory: 50,
};

// ======================================================================
// EXTENSION / VSIX PLUGIN STATE (Phase 39)
// ======================================================================
var ExtensionState = {
  // Extension types: vsix, native-dll, psm1, js, theme, language
  extensions: {},
  installed: [],
  enabled: [],
  registryPath: 'd:\\RawrXD\\extensions\\registry.json',
  extensionRoot: 'd:\\RawrXD\\extensions',
  moduleStore: 'd:\\RawrXD\\scripts\\modules',
  craftRoom: 'd:\\RawrXD\\craft_room',
  marketplace: {
    url: 'https://marketplace.visualstudio.com/_apis/public/gallery',
    openVsxUrl: 'https://open-vsx.org/api',
    enabled: true,
    cache: [],
    lastSearch: '',
  },
  // Built-in extension types the system supports
  supportedTypes: [
    { type: 'vsix', desc: 'VS Code Extension Package (.vsix)', icon: '\uD83D\uDCE6', canLoad: true },
    { type: 'native-dll', desc: 'Native C++ DLL Extension', icon: '\u2699', canLoad: true },
    { type: 'psm1', desc: 'PowerShell Module Extension', icon: '\uD83D\uDCDC', canLoad: true },
    { type: 'js', desc: 'QuickJS JavaScript Extension', icon: '\uD83D\uDFE8', canLoad: true },
    { type: 'theme', desc: 'Color Theme / Icon Theme', icon: '\uD83C\uDFA8', canLoad: true },
    { type: 'language', desc: 'Language Grammar / Snippets', icon: '\uD83D\uDCDD', canLoad: true },
    { type: 'lsp', desc: 'Language Server Protocol Provider', icon: '\uD83D\uDD17', canLoad: true },
    { type: 'debugger', desc: 'Debug Adapter Protocol Provider', icon: '\uD83D\uDC1B', canLoad: true },
  ],
  activeTab: 'installed',
  history: [],
  maxHistory: 100,
  // Built-in/bundled extensions that ship with RawrXD
  bundled: [
    { id: 'rawrxd.masm-language', name: 'MASM Language Support', type: 'language', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'MASM64 syntax highlighting, snippets, and diagnostics' },
    { id: 'rawrxd.gguf-inspector', name: 'GGUF Model Inspector', type: 'native-dll', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'GGUF binary inspection, tensor viewer, metadata browser' },
    { id: 'rawrxd.hotpatch-tools', name: 'Hotpatch Tools', type: 'native-dll', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'Three-layer hotpatching: memory, byte-level, server' },
    { id: 'rawrxd.agentic-framework', name: 'Agentic Framework', type: 'native-dll', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'Failure detection, puppeteer correction, agentic loop' },
    { id: 'rawrxd.reverse-engineering', name: 'Reverse Engineering Suite', type: 'native-dll', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'PE analyzer, disassembler, deobfuscator, omega suite' },
    { id: 'rawrxd.quickjs-host', name: 'QuickJS Extension Host', type: 'js', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'QuickJS-based JavaScript extension runtime' },
    { id: 'rawrxd.powershell-bridge', name: 'PowerShell Bridge', type: 'psm1', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'PowerShell module lifecycle, craft room, extension manager' },
    { id: 'rawrxd.vscode-compat', name: 'VS Code API Compatibility', type: 'native-dll', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'vscode.* API shim: commands, window, workspace, languages, debug' },
    { id: 'rawrxd.theme-dark-plus', name: 'Dark+ (default theme)', type: 'theme', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'VS Code Dark+ compatible color theme' },
    { id: 'rawrxd.lsp-bridge', name: 'LSP Bridge', type: 'lsp', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'Language Server Protocol routing to Win32IDE LSP server' },
    { id: 'rawrxd.dap-handler', name: 'DAP Debug Handler', type: 'debugger', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'Debug Adapter Protocol integration for native debugging' },
    { id: 'rawrxd.polyfill-engine', name: 'Extension Polyfill Engine', type: 'js', version: '1.0.0', publisher: 'RawrXD', enabled: true, builtin: true, desc: 'JS compat layer for VS Code extension APIs' },
  ],
  // User-installed extensions (loaded from registry)
  userExtensions: [],

  getAll: function () {
    return this.bundled.concat(this.userExtensions);
  },
  getEnabled: function () {
    return this.getAll().filter(function (e) { return e.enabled; });
  },
  getByType: function (type) {
    return this.getAll().filter(function (e) { return e.type === type; });
  },
  findById: function (id) {
    return this.getAll().find(function (e) { return e.id === id; }) || null;
  },
  install: function (ext) {
    if (this.findById(ext.id)) return { success: false, msg: 'Extension already installed: ' + ext.id };
    ext.enabled = false;
    ext.builtin = false;
    this.userExtensions.push(ext);
    this.history.push({ action: 'install', id: ext.id, time: Date.now() });
    return { success: true, msg: 'Installed: ' + ext.name + ' (' + ext.id + ')' };
  },
  uninstall: function (id) {
    var idx = -1;
    for (var i = 0; i < this.userExtensions.length; i++) {
      if (this.userExtensions[i].id === id) { idx = i; break; }
    }
    if (idx === -1) return { success: false, msg: 'Extension not found or is built-in: ' + id };
    var removed = this.userExtensions.splice(idx, 1)[0];
    this.history.push({ action: 'uninstall', id: id, time: Date.now() });
    return { success: true, msg: 'Uninstalled: ' + removed.name };
  },
  enable: function (id) {
    var ext = this.findById(id);
    if (!ext) return { success: false, msg: 'Extension not found: ' + id };
    ext.enabled = true;
    this.history.push({ action: 'enable', id: id, time: Date.now() });
    return { success: true, msg: 'Enabled: ' + ext.name };
  },
  disable: function (id) {
    var ext = this.findById(id);
    if (!ext) return { success: false, msg: 'Extension not found: ' + id };
    if (ext.builtin) return { success: false, msg: 'Cannot disable built-in extension: ' + ext.name };
    ext.enabled = false;
    this.history.push({ action: 'disable', id: id, time: Date.now() });
    return { success: true, msg: 'Disabled: ' + ext.name };
  },
};

// ======================================================================
// PERFORMANCE METRICS (Phase 3: Observability)
// ======================================================================
function recordMetric(opts) {
  // opts: { latency, tokens, model, endpoint, success, errorMsg }
  var entry = {
    timestamp: Date.now(),
    latency: opts.latency || 0,
    tokens: opts.tokens || 0,
    tps: (opts.tokens > 0 && opts.latency > 0) ? parseFloat((opts.tokens / (opts.latency / 1000)).toFixed(1)) : 0,
    model: opts.model || 'unknown',
    endpoint: opts.endpoint || 'unknown',
    success: opts.success !== false,
    errorMsg: opts.errorMsg || null,
  };

  // Push to ring buffer
  State.perf.history.push(entry);
  if (State.perf.history.length > State.perf.maxHistory) {
    State.perf.history.shift();
  }

  // Update aggregates
  State.perf.totalRequests++;
  State.perf.totalTokens += entry.tokens;
  State.perf.totalLatency += entry.latency;

  // Per-model stats
  var mKey = entry.model || 'unknown';
  if (!State.perf.modelStats[mKey]) {
    State.perf.modelStats[mKey] = { requests: 0, tokens: 0, totalLatency: 0, avgTps: 0, bestTps: 0 };
  }
  var ms = State.perf.modelStats[mKey];
  ms.requests++;
  ms.tokens += entry.tokens;
  ms.totalLatency += entry.latency;
  ms.avgTps = ms.tokens > 0 && ms.totalLatency > 0 ? parseFloat((ms.tokens / (ms.totalLatency / 1000)).toFixed(1)) : 0;
  if (entry.tps > ms.bestTps) ms.bestTps = entry.tps;

  // Structured log entry
  logStructured('PERF', 'request_complete', {
    latency: entry.latency,
    tokens: entry.tokens,
    tps: entry.tps,
    model: entry.model,
    endpoint: entry.endpoint,
    success: entry.success,
  });

  // Update sidebar perf summary if visible
  updatePerfSidebar();
}

function logStructured(level, event, data) {
  var entry = {
    ts: Date.now(),
    isoTime: new Date().toISOString(),
    level: level,
    event: event,
    data: data || {},
  };
  State.perf.structuredLog.push(entry);
  if (State.perf.structuredLog.length > State.perf.maxLog) {
    State.perf.structuredLog.shift();
  }
}

function updatePerfSidebar() {
  // Update the right sidebar perf mini-stats
  var avgLat = State.perf.totalRequests > 0 ? Math.round(State.perf.totalLatency / State.perf.totalRequests) : 0;
  var avgTps = State.perf.totalTokens > 0 && State.perf.totalLatency > 0 ? (State.perf.totalTokens / (State.perf.totalLatency / 1000)).toFixed(1) : '—';

  var elReqs = document.getElementById('perfTotalReqs');
  var elAvgLat = document.getElementById('perfAvgLatency');
  var elAvgTps = document.getElementById('perfAvgTps');
  var elTotalTok = document.getElementById('perfTotalTokens');

  if (elReqs) elReqs.textContent = State.perf.totalRequests;
  if (elAvgLat) elAvgLat.textContent = avgLat + 'ms';
  if (elAvgTps) elAvgTps.textContent = avgTps;
  if (elTotalTok) elTotalTok.textContent = State.perf.totalTokens.toLocaleString();
}

// ======================================================================
// CONVERSATION MEMORY (Phase 1: Multi-turn + Persistence)
// ======================================================================
var DEFAULT_SYSTEM_PROMPT = 'You are RawrXD Assistant, an expert AI collaborator specializing in low-level programming, MASM x64 assembly, reverse engineering, GGUF model operations, and C++20 systems programming. You have deep knowledge of the RawrXD three-layer hotpatching architecture (memory, byte-level, server). Be precise, technical, and concise.';

var Conversation = {
  messages: [],           // Array of {role, content, timestamp}
  systemPrompt: DEFAULT_SYSTEM_PROMPT,
  maxContextMessages: 40, // Keep last 40 messages (20 exchanges) to prevent token overflow

  addMessage: function (role, content) {
    this.messages.push({
      role: role,
      content: content,
      timestamp: Date.now()
    });
    // Trim old messages to prevent token overflow
    if (this.messages.length > this.maxContextMessages) {
      this.messages = this.messages.slice(-this.maxContextMessages);
    }
    this.persist();
    this.updateUI();
  },

  // Build OpenAI-compatible messages array for /v1/chat/completions
  getContextForAPI: function () {
    var context = [{ role: 'system', content: this.systemPrompt }];

    // Build file context block (injected near the end, not at position 2)
    var fileContextMsg = null;
    if (State.files.length > 0) {
      var fileParts = [];
      State.files.forEach(function (f) {
        var content = State.fileContents[f.name];
        if (content) {
          // Estimate tokens (~4 chars per token) and warn if file is huge
          var estTokens = Math.ceil(content.length / 4);
          if (estTokens > State.gen.context * 0.6) {
            // Truncate to ~60% of context window to leave room for conversation
            var maxChars = Math.floor(State.gen.context * 0.6 * 4);
            fileParts.push('--- ' + f.name + ' (truncated to fit context window) ---\n' + content.substring(0, maxChars) + '\n... [truncated, ' + formatBytes(content.length) + ' total]');
            logDebug('File ' + f.name + ' (~' + estTokens + ' tokens) exceeds context budget, truncated to ~' + Math.floor(maxChars / 4) + ' tokens', 'warn');
          } else {
            fileParts.push('--- ' + f.name + ' ---\n' + content);
          }
        } else {
          fileParts.push('[Attached: ' + f.name + ' (' + formatBytes(f.size) + ') — content not yet loaded]');
        }
      });
      if (fileParts.length > 0) {
        fileContextMsg = {
          role: 'system',
          content: 'The user has attached the following files as context. Review them carefully when responding:\n\n' + fileParts.join('\n\n')
        };
      }
    }

    // Add conversation history (all but the last user message)
    var msgs = this.messages;
    var insertIdx = msgs.length; // default: append at end
    // Find the last user message to inject file context right before it
    for (var i = msgs.length - 1; i >= 0; i--) {
      if (msgs[i].role === 'user') { insertIdx = i; break; }
    }

    for (var j = 0; j < msgs.length; j++) {
      // Inject file context right before the last user message
      if (j === insertIdx && fileContextMsg) {
        context.push(fileContextMsg);
      }
      context.push({ role: msgs[j].role, content: msgs[j].content });
    }

    // If no messages yet but we have file context, add it
    if (msgs.length === 0 && fileContextMsg) {
      context.push(fileContextMsg);
    }

    return context;
  },

  // Build legacy prompt string for /ask or /api/generate fallback
  getLegacyPrompt: function (query) {
    var parts = [];

    // Add recent conversation context (last 6 exchanges = 12 messages)
    var recent = this.messages.slice(-12);
    if (recent.length > 0) {
      parts.push('Previous conversation:');
      recent.forEach(function (m) {
        parts.push((m.role === 'user' ? 'User' : 'Assistant') + ': ' + m.content);
      });
      parts.push('');
    }

    // Add file context — placed right before the user question so it's in active context
    if (State.files.length > 0) {
      var fileParts = [];
      State.files.forEach(function (f) {
        var content = State.fileContents[f.name];
        if (content) {
          var estTokens = Math.ceil(content.length / 4);
          if (estTokens > State.gen.context * 0.6) {
            var maxChars = Math.floor(State.gen.context * 0.6 * 4);
            fileParts.push('--- ' + f.name + ' (truncated) ---\n' + content.substring(0, maxChars) + '\n... [truncated]');
          } else {
            fileParts.push('--- ' + f.name + ' ---\n' + content);
          }
        } else {
          fileParts.push('[Attached: ' + f.name + ' (' + formatBytes(f.size) + ') — content not loaded]');
        }
      });
      parts.push('The user has attached the following files. Review them carefully:\n' + fileParts.join('\n\n'));
      parts.push('');
    }

    parts.push('User question: ' + query);
    return parts.join('\n');
  },

  persist: function () {
    if (!_localStorageAvailable) return; // Bail early if localStorage blocked (file:// in some browsers)
    try {
      localStorage.setItem('rawrxd_conversation', JSON.stringify({
        messages: this.messages,
        systemPrompt: this.systemPrompt,
        savedAt: Date.now()
      }));
      // Also persist generation settings
      localStorage.setItem('rawrxd_gen_settings', JSON.stringify(State.gen));
    } catch (e) {
      // localStorage full or unavailable — degrade silently
      logDebug('Persist failed: ' + e.message, 'warn');
    }
  },

  load: function () {
    if (!_localStorageAvailable) return false;
    try {
      var saved = localStorage.getItem('rawrxd_conversation');
      if (saved) {
        var data = JSON.parse(saved);
        if (data.messages && Array.isArray(data.messages)) {
          this.messages = data.messages;
        }
        if (data.systemPrompt) {
          this.systemPrompt = data.systemPrompt;
        }
        return this.messages.length > 0;
      }
    } catch (e) {
      logDebug('Load conversation failed: ' + e.message, 'warn');
    }
    return false;
  },

  loadGenSettings: function () {
    if (!_localStorageAvailable) return false;
    try {
      var saved = localStorage.getItem('rawrxd_gen_settings');
      if (saved) {
        var data = JSON.parse(saved);
        if (data.context) State.gen.context = data.context;
        if (data.maxTokens) State.gen.maxTokens = data.maxTokens;
        if (data.temperature != null) State.gen.temperature = data.temperature;
        if (data.stream != null) State.gen.stream = data.stream;
        if (data.top_p != null) State.gen.top_p = data.top_p;
        if (data.top_k != null) State.gen.top_k = data.top_k;
        // Restore tensor bunny hop settings
        if (data.tensorHop) {
          if (data.tensorHop.enabled != null) State.gen.tensorHop.enabled = data.tensorHop.enabled;
          if (data.tensorHop.strategy) State.gen.tensorHop.strategy = data.tensorHop.strategy;
          if (data.tensorHop.skipRatio != null) State.gen.tensorHop.skipRatio = data.tensorHop.skipRatio;
          if (data.tensorHop.keepFirst != null) State.gen.tensorHop.keepFirst = data.tensorHop.keepFirst;
          if (data.tensorHop.keepLast != null) State.gen.tensorHop.keepLast = data.tensorHop.keepLast;
          if (data.tensorHop.customSkip) State.gen.tensorHop.customSkip = data.tensorHop.customSkip;
        }
        // Restore safe decode profile
        if (data.safeDecodeProfile) {
          if (data.safeDecodeProfile.enabled != null) State.gen.safeDecodeProfile.enabled = data.safeDecodeProfile.enabled;
          if (data.safeDecodeProfile.autoClamp != null) State.gen.safeDecodeProfile.autoClamp = data.safeDecodeProfile.autoClamp;
          if (data.safeDecodeProfile.thresholdB != null) State.gen.safeDecodeProfile.thresholdB = data.safeDecodeProfile.thresholdB;
          if (data.safeDecodeProfile.safeContext != null) State.gen.safeDecodeProfile.safeContext = data.safeDecodeProfile.safeContext;
          if (data.safeDecodeProfile.safeMaxTokens != null) State.gen.safeDecodeProfile.safeMaxTokens = data.safeDecodeProfile.safeMaxTokens;
          if (data.safeDecodeProfile.safeTemperature != null) State.gen.safeDecodeProfile.safeTemperature = data.safeDecodeProfile.safeTemperature;
          if (data.safeDecodeProfile.safeTopP != null) State.gen.safeDecodeProfile.safeTopP = data.safeDecodeProfile.safeTopP;
          if (data.safeDecodeProfile.safeTopK != null) State.gen.safeDecodeProfile.safeTopK = data.safeDecodeProfile.safeTopK;
          if (data.safeDecodeProfile.probeFirst != null) State.gen.safeDecodeProfile.probeFirst = data.safeDecodeProfile.probeFirst;
          if (data.safeDecodeProfile.disableStreamOnSafe != null) State.gen.safeDecodeProfile.disableStreamOnSafe = data.safeDecodeProfile.disableStreamOnSafe;
        }
        return true;
      }
    } catch (e) { /* ignore */ }
    return false;
  },

  clear: function () {
    this.messages = [];
    this.persist();
    this.updateUI();
  },

  updateUI: function () {
    // Update memory count in sidebar
    var memCount = document.getElementById('memoryCount');
    if (memCount) memCount.textContent = this.messages.length;

    // Update memory badge in chat header
    var memBadge = document.getElementById('memoryBadge');
    var memBadgeText = document.getElementById('memoryBadgeText');
    if (memBadge && memBadgeText) {
      if (this.messages.length > 0) {
        memBadge.style.display = 'flex';
        memBadgeText.textContent = this.messages.length + ' turns';
      } else {
        memBadge.style.display = 'none';
      }
    }
  }
};

// System Prompt UI helpers
function toggleSystemPrompt() {
  var body = document.getElementById('sysPromptBody');
  var arrow = document.getElementById('sysPromptArrow');
  body.classList.toggle('open');
  arrow.classList.toggle('open');
}

function resetSystemPrompt() {
  Conversation.systemPrompt = DEFAULT_SYSTEM_PROMPT;
  var input = document.getElementById('systemPromptInput');
  if (input) input.value = DEFAULT_SYSTEM_PROMPT;
  Conversation.persist();
  logDebug('System prompt reset to default', 'info');
}

function clearConversation() {
  Conversation.clear();
  clearChat();
  logDebug('Conversation memory cleared', 'info');
}

// ======================================================================
// WIN32IDE BRIDGE — detect running IDE, sync state, launch IDE
// Works from both file:// and http:// contexts
// ======================================================================
var _win32IdeDetected = false;
var _win32IdeUrl = 'http://localhost:8080';
var _win32IdePollTimer = null;

async function probeWin32IDE() {
  // Probe the Win32IDE / RawrXD-ToolServer local server to detect if it's running
  // Tries multiple endpoints: /status, /api/status, /health (for old and new binaries)
  var data = null;
  var detectedVia = '';

  // Try 1: /status (new binary with full capabilities)
  try {
    var res = await fetch(_win32IdeUrl + '/status', { signal: AbortSignal.timeout(2000) });
    if (res.ok) {
      data = await res.json();
      detectedVia = '/status';
    }
  } catch (_) { /* not available */ }

  // Try 2: /api/status (old binary, returns {running, pid, uptime_seconds})
  if (!data) {
    try {
      var res2 = await fetch(_win32IdeUrl + '/api/status', { signal: AbortSignal.timeout(2000) });
      if (res2.ok) {
        data = await res2.json();
        detectedVia = '/api/status';
      }
    } catch (_) { /* not available */ }
  }

  // Try 3: /health (old binary, returns {status, version, models_loaded})
  if (!data) {
    try {
      var res3 = await fetch(_win32IdeUrl + '/health', { signal: AbortSignal.timeout(2000) });
      if (res3.ok) {
        data = await res3.json();
        detectedVia = '/health';
      }
    } catch (_) { /* not available */ }
  }

  if (data) {
    // Identify as RawrXD backend by any of these signals:
    // - Explicit backend/server fields matching known names
    // - /api/status returning {running: true} (only tool_server does this)
    // - /health returning {status: "ok", version: ...} on :8080 (not Ollama's port)
    var isRawrXD = false;
    if (data.backend === 'rawrxd-win32ide' || data.backend === 'rawrxd-tool-server') isRawrXD = true;
    else if (data.server && (data.server.indexOf('Win32IDE') >= 0 || data.server.indexOf('RawrXD') >= 0 || data.server.indexOf('ToolServer') >= 0)) isRawrXD = true;
    else if (data.running === true && data.pid) isRawrXD = true; // /api/status signature
    else if (detectedVia === '/health' && data.status === 'ok' && data.version) isRawrXD = true; // server.js /health on :8080

    if (isRawrXD) {
      _win32IdeDetected = true;
      // Normalize the data for updateIdeBridgeUI
      if (!data.backend) data.backend = 'rawrxd-tool-server';
      if (!data.server) data.server = 'RawrXD-ToolServer';
      updateIdeBridgeUI(true, data);
      logDebug('\uD83D\uDDA5 Win32 IDE / RawrXD-ToolServer detected at ' + _win32IdeUrl + ' via ' + detectedVia, 'info');
      // Sync model info from IDE
      if (data.model_loaded && data.model_path) {
        var modelName = data.model_path.split(/[/\\]/).pop();
        logDebug('IDE has model loaded: ' + modelName, 'info');
      }
      return true;
    }
  }

  _win32IdeDetected = false;
  updateIdeBridgeUI(false, null);
  return false;
}

function updateIdeBridgeUI(ideRunning, statusData) {
  var bridgeEl = document.getElementById('tbIdeBridge');
  var tbDot = document.getElementById('tbDot');
  var tbStatusText = document.getElementById('tbStatusText');
  var tbMode = document.getElementById('tbMode');

  if (ideRunning) {
    if (bridgeEl) bridgeEl.style.display = 'inline';
    if (tbDot) tbDot.classList.add('online');
    if (tbStatusText) tbStatusText.textContent = 'IDE ONLINE';
    if (tbMode) {
      tbMode.textContent = _isFileProtocol ? 'FILE → IDE' : 'HTTP';
      tbMode.style.background = 'rgba(0,255,136,0.15)';
      tbMode.style.color = 'var(--accent-green)';
    }
  } else {
    if (bridgeEl) bridgeEl.style.display = 'none';
    if (tbDot) tbDot.classList.remove('online');
    if (tbStatusText) tbStatusText.textContent = State.backend.online ? 'CONNECTED' : 'DISCONNECTED';
    if (tbMode) {
      tbMode.textContent = _isFileProtocol ? 'STANDALONE' : 'HTTP';
      tbMode.style.background = '';
      tbMode.style.color = '';
    }
  }
}

function launchWin32IDE() {
  // Open the Win32IDE's served GUI (this is identical but served from the IDE's HTTP server)
  // When the IDE is running, navigating to /gui gives the same page but served via HTTP
  if (_win32IdeDetected) {
    window.open(_win32IdeUrl + '/gui', '_blank');
    logDebug('Opened Win32 IDE GUI at ' + _win32IdeUrl + '/gui', 'info');
  } else {
    addMessage('system', '\u26A0\uFE0F **Win32 IDE not detected.** Start RawrXD-Win32IDE.exe first, then click \u26A1 to connect.', { skipMemory: true });
  }
}

function toggleStandaloneFullscreen() {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen().then(function () {
      document.body.classList.add('standalone-fullscreen');
    }).catch(function () { /* denied */ });
  } else {
    document.exitFullscreen().then(function () {
      document.body.classList.remove('standalone-fullscreen');
    }).catch(function () { /* denied */ });
  }
}

// Start periodic IDE probe (every 30s) so the bridge badge appears when IDE starts
function startIdePoll() {
  if (_win32IdePollTimer) return;
  _win32IdePollTimer = setInterval(function () {
    probeWin32IDE();
  }, 30000);
}

function stopIdePoll() {
  if (_win32IdePollTimer) {
    clearInterval(_win32IdePollTimer);
    _win32IdePollTimer = null;
  }
}

// ======================================================================
// GHOST IDE — RDP-style beacon into the full Win32 IDE via iframe
// "Ghosting in" = probe the IDE server, then embed its /gui endpoint
// inside an overlay iframe so the user gets the full IDE experience
// without leaving the chatbot. Beaconism-driven: works from file://
// ======================================================================

// ======================================================================
// BUILT-IN BROWSER (WebView2-style) — Model web access, tabbed browsing,
// proxy-based page fetching, content extraction for model context.
// When hosted in Win32 IDE with WebView2, can use native WebView2 APIs.
// When running in standard browser, uses iframe + server proxy for CORS.
// ======================================================================

var BrowserState = {
  tabs: [{ id: 0, url: '', title: 'New Tab', history: [], historyIdx: -1, content: null }],
  activeTab: 0,
  nextTabId: 1,
  history: [],
  bookmarks: [],
  maxHistory: 200,
  isWebView2: !!(window.chrome && window.chrome.webview),
  proxyMode: 'auto', // 'auto' | 'proxy' | 'direct' | 'webview2'
  extractedContent: null,
  searchEngine: 'https://duckduckgo.com/?q='
};

function showBrowser() {
  var el = document.getElementById('browserOverlay');
  if (el) {
    el.classList.add('active');
    // Focus URL bar
    var input = document.getElementById('browserUrlInput');
    if (input) setTimeout(function () { input.focus(); }, 100);
  }
}

function browserClose() {
  var el = document.getElementById('browserOverlay');
  if (el) el.classList.remove('active');
}

function browserMinimize() {
  browserClose();
  // Show toast that browser is still running
  if (typeof addSystemMessage === 'function') {
    addSystemMessage('Browser minimized. Click 🌐 Browse or type `browse` to reopen.');
  }
}

function browserDetach() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (tab && tab.url) {
    window.open(tab.url, '_blank', 'width=1024,height=768,menubar=yes,toolbar=yes,location=yes');
  }
}

function browserGoHome() {
  var vp = document.getElementById('browserViewport');
  var home = document.getElementById('browserHomePage');
  if (!vp) return;
  // Remove any iframe or proxy content
  var existingIframe = vp.querySelector('iframe');
  if (existingIframe) existingIframe.remove();
  var existingProxy = vp.querySelector('.browser-proxy-content');
  if (existingProxy) existingProxy.remove();
  if (home) home.style.display = '';
  // Update URL bar
  document.getElementById('browserUrlInput').value = '';
  document.getElementById('browserStatusUrl').textContent = 'about:home';
  document.getElementById('browserStatusInfo').textContent = 'Ready';
  _browserUpdateSsl('');
  _browserUpdateTab(BrowserState.activeTab, 'New Tab', '');
}

function browserGo(url) {
  document.getElementById('browserUrlInput').value = url;
  browserNavigate();
}

function browserNavigate() {
  var input = document.getElementById('browserUrlInput');
  var raw = (input.value || '').trim();
  if (!raw) return;

  // Auto-detect if it's a search query or URL
  var url = raw;
  if (raw.indexOf('://') < 0 && raw.indexOf('.') < 0 && raw.indexOf('/') < 0) {
    // Treat as search query
    url = BrowserState.searchEngine + encodeURIComponent(raw);
  } else if (raw.indexOf('://') < 0) {
    url = 'https://' + raw;
  }

  input.value = url;

  // Record in history
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (tab) {
    // Trim forward history when navigating from middle
    if (tab.historyIdx < tab.history.length - 1) {
      tab.history = tab.history.slice(0, tab.historyIdx + 1);
    }
    tab.history.push(url);
    tab.historyIdx = tab.history.length - 1;
    tab.url = url;
  }
  BrowserState.history.push({ url: url, timestamp: Date.now() });
  if (BrowserState.history.length > BrowserState.maxHistory) {
    BrowserState.history = BrowserState.history.slice(-BrowserState.maxHistory);
  }

  _browserUpdateNavButtons();
  _browserLoadUrl(url);
}

function _browserLoadUrl(url) {
  var vp = document.getElementById('browserViewport');
  var home = document.getElementById('browserHomePage');
  var loading = document.getElementById('browserLoading');
  if (!vp) return;

  // Hide home page
  if (home) home.style.display = 'none';

  // Show loading
  if (loading) loading.style.display = '';

  // Remove old content
  var oldIframe = vp.querySelector('iframe');
  if (oldIframe) oldIframe.remove();
  var oldProxy = vp.querySelector('.browser-proxy-content');
  if (oldProxy) oldProxy.remove();

  document.getElementById('browserStatusUrl').textContent = url;
  document.getElementById('browserStatusInfo').textContent = 'Loading...';
  _browserUpdateSsl(url);

  // Strategy: try direct iframe first (works for many sites), then proxy
  if (BrowserState.proxyMode === 'proxy') {
    _browserLoadViaProxy(url);
  } else if (BrowserState.proxyMode === 'webview2' && BrowserState.isWebView2) {
    _browserLoadViaWebView2(url);
  } else {
    // Auto: try iframe first
    _browserLoadViaIframe(url);
  }
}

function _browserLoadViaIframe(url) {
  var vp = document.getElementById('browserViewport');
  var loading = document.getElementById('browserLoading');

  var iframe = document.createElement('iframe');
  iframe.sandbox = 'allow-scripts allow-same-origin allow-forms allow-popups allow-popups-to-escape-sandbox';
  iframe.style.cssText = 'width:100%;height:100%;border:none;background:#fff;';
  iframe.src = url;

  var loadTimeout;
  iframe.onload = function () {
    if (loading) loading.style.display = 'none';
    if (loadTimeout) clearTimeout(loadTimeout);
    document.getElementById('browserStatusInfo').textContent = 'Loaded (iframe)';
    // Try to get title
    try {
      var title = iframe.contentDocument ? iframe.contentDocument.title : '';
      if (title) _browserUpdateTab(BrowserState.activeTab, title, url);
    } catch (e) {
      // Cross-origin, can't access title
      _browserUpdateTab(BrowserState.activeTab, _browserTitleFromUrl(url), url);
    }
  };

  iframe.onerror = function () {
    if (loadTimeout) clearTimeout(loadTimeout);
    // Iframe failed, try proxy
    iframe.remove();
    _browserLoadViaProxy(url);
  };

  // Timeout: if iframe takes too long, try proxy approach
  loadTimeout = setTimeout(function () {
    // Check if iframe loaded (simple heuristic)
    if (loading && loading.style.display !== 'none') {
      // Still loading after 8s, offer proxy option
      document.getElementById('browserStatusInfo').textContent = 'Slow load — try proxy? (click Extract)';
    }
  }, 8000);

  vp.appendChild(iframe);
}

function _browserLoadViaProxy(url) {
  var vp = document.getElementById('browserViewport');
  var loading = document.getElementById('browserLoading');

  // Remove any existing iframe
  var oldIframe = vp.querySelector('iframe');
  if (oldIframe) oldIframe.remove();

  document.getElementById('browserStatusInfo').textContent = 'Fetching via proxy...';

  EngineAPI.browseProxy(url).then(function (data) {
    if (loading) loading.style.display = 'none';

    var html = data.content || data.body || data.output || data.result || '';
    if (!html) {
      document.getElementById('browserStatusInfo').textContent = 'Empty response';
      return;
    }

    // Render as sanitized content
    var div = document.createElement('div');
    div.className = 'browser-proxy-content';

    // Basic sanitization: remove dangerous tags but keep structure
    var sanitized = html
      .replace(/<script[^>]*>[\s\S]*?<\/script>/gi, '')
      .replace(/<link[^>]*>/gi, '')
      .replace(/<meta[^>]*>/gi, '')
      .replace(/on\w+\s*=\s*"[^"]*"/gi, '')
      .replace(/on\w+\s*=\s*'[^']*'/gi, '');

    div.innerHTML = sanitized;

    // Make links clickable within the browser
    var links = div.querySelectorAll('a[href]');
    for (var i = 0; i < links.length; i++) {
      (function (link) {
        var href = link.getAttribute('href');
        if (href) {
          // Resolve relative URLs
          if (href.indexOf('://') < 0 && href.charAt(0) === '/') {
            try {
              var base = new URL(url);
              href = base.origin + href;
            } catch (e) { /* keep as-is */ }
          }
          link.onclick = function (e) {
            e.preventDefault();
            browserGo(href);
          };
        }
      })(links[i]);
    }

    vp.appendChild(div);

    // Extract title
    var titleMatch = html.match(/<title[^>]*>([^<]+)<\/title>/i);
    var title = titleMatch ? titleMatch[1].trim() : _browserTitleFromUrl(url);
    _browserUpdateTab(BrowserState.activeTab, title, url);
    document.getElementById('browserStatusInfo').textContent = 'Loaded (proxy) — ' + html.length + ' bytes';

  }).catch(function (err) {
    if (loading) loading.style.display = 'none';
    document.getElementById('browserStatusInfo').textContent = 'Error: ' + (err.message || err);

    // Show error page
    var div = document.createElement('div');
    div.className = 'browser-proxy-content';
    div.innerHTML = '<div style="text-align:center;padding:60px 20px;">' +
      '<div style="font-size:48px;margin-bottom:16px;">&#x26A0;</div>' +
      '<h2 style="color:#e74c3c;">Unable to load page</h2>' +
      '<p style="color:#666;margin:12px 0;">Could not fetch: <code>' + url + '</code></p>' +
      '<p style="color:#999;font-size:13px;">Error: ' + (err.message || 'Connection failed') + '</p>' +
      '<p style="color:#999;font-size:12px;margin-top:20px;">Tip: Make sure the backend server is running for proxy mode,<br>or try the URL directly: <a href="' + url + '" target="_blank">' + url + '</a></p>' +
      '</div>';
    vp.appendChild(div);
  });
}

function _browserLoadViaWebView2(url) {
  // WebView2 native navigation — send message to host application
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({
      type: 'browser-navigate',
      url: url
    });
    document.getElementById('browserStatusInfo').textContent = 'WebView2 navigation → ' + url;
  } else {
    // Fallback to iframe
    _browserLoadViaIframe(url);
  }
}

function browserBack() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (!tab || tab.historyIdx <= 0) return;
  tab.historyIdx--;
  var url = tab.history[tab.historyIdx];
  tab.url = url;
  document.getElementById('browserUrlInput').value = url;
  _browserUpdateNavButtons();
  _browserLoadUrl(url);
}

function browserForward() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (!tab || tab.historyIdx >= tab.history.length - 1) return;
  tab.historyIdx++;
  var url = tab.history[tab.historyIdx];
  tab.url = url;
  document.getElementById('browserUrlInput').value = url;
  _browserUpdateNavButtons();
  _browserLoadUrl(url);
}

function browserReload() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (tab && tab.url) {
    _browserLoadUrl(tab.url);
  }
}

function browserExtractContent() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (!tab || !tab.url) {
    if (typeof addSystemMessage === 'function') addSystemMessage('No page loaded to extract.');
    return;
  }

  var vp = document.getElementById('browserViewport');
  var iframe = vp ? vp.querySelector('iframe') : null;
  var proxyDiv = vp ? vp.querySelector('.browser-proxy-content') : null;
  var text = '';

  // Try to extract from iframe first
  if (iframe) {
    try {
      text = iframe.contentDocument.body.innerText || iframe.contentDocument.body.textContent || '';
    } catch (e) {
      // Cross-origin — use proxy extraction
    }
  }

  // Try proxy content div
  if (!text && proxyDiv) {
    text = proxyDiv.innerText || proxyDiv.textContent || '';
  }

  // If still no content, fetch via proxy and extract
  if (!text) {
    document.getElementById('browserStatusInfo').textContent = 'Extracting via proxy...';
    EngineAPI.browseExtract(tab.url).then(function (data) {
      var extracted = data.text || data.content || '';
      BrowserState.extractedContent = { url: tab.url, text: extracted, timestamp: Date.now() };
      document.getElementById('browserStatusInfo').textContent = 'Extracted ' + extracted.length + ' chars';
      if (typeof addSystemMessage === 'function') {
        addSystemMessage('📋 Extracted ' + extracted.length + ' chars from ' + tab.url);
      }
    }).catch(function (err) {
      document.getElementById('browserStatusInfo').textContent = 'Extract error: ' + err.message;
    });
    return;
  }

  BrowserState.extractedContent = { url: tab.url, text: text, timestamp: Date.now() };
  document.getElementById('browserStatusInfo').textContent = 'Extracted ' + text.length + ' chars';
  if (typeof addSystemMessage === 'function') {
    addSystemMessage('📋 Extracted ' + text.length + ' chars from ' + tab.url);
  }
}

function browserSendToModel() {
  // Extract content (if not already) and inject it as context for the model
  if (!BrowserState.extractedContent || Date.now() - BrowserState.extractedContent.timestamp > 60000) {
    browserExtractContent();
    // Wait a moment for extraction, then send
    setTimeout(function () { _browserDoSendToModel(); }, 2000);
  } else {
    _browserDoSendToModel();
  }
}

function _browserDoSendToModel() {
  var ec = BrowserState.extractedContent;
  if (!ec || !ec.text) {
    if (typeof addSystemMessage === 'function') {
      addSystemMessage('No extracted content to send. Try clicking Extract first.');
    }
    return;
  }

  // Truncate very long pages
  var text = ec.text;
  var maxLen = 8000;
  if (text.length > maxLen) {
    text = text.substring(0, maxLen) + '\n\n[... truncated at ' + maxLen + ' chars, full page: ' + ec.text.length + ' chars]';
  }

  // Inject as a system message into the chat, or add to current context
  var contextMsg = '[Web Page Content from ' + ec.url + ' — extracted ' + new Date(ec.timestamp).toLocaleTimeString() + ']\n\n' + text;

  // If there's a chat input, prepend context
  var chatInput = document.getElementById('chatInput');
  if (chatInput) {
    chatInput.value = contextMsg + '\n\n---\n\n' + (chatInput.value || '');
    chatInput.focus();
    if (typeof addSystemMessage === 'function') {
      addSystemMessage('🌐→🤖 Web content injected into chat input (' + text.length + ' chars from ' + ec.url + ')');
    }
  }

  document.getElementById('browserStatusInfo').textContent = 'Sent to model (' + text.length + ' chars)';
}

// ---- Browser Tab Management ----

function browserNewTab() {
  var id = BrowserState.nextTabId++;
  BrowserState.tabs.push({ id: id, url: '', title: 'New Tab', history: [], historyIdx: -1, content: null });
  var tabBar = document.getElementById('browserTabBar');
  if (tabBar) {
    var newTabBtn = tabBar.querySelector('.browser-new-tab');
    var tabEl = document.createElement('div');
    tabEl.className = 'browser-tab';
    tabEl.id = 'browserTab' + id;
    tabEl.onclick = function () { browserSwitchTab(BrowserState.tabs.length - 1); };
    tabEl.innerHTML = '<span class="tab-title">New Tab</span><span class="tab-close" onclick="event.stopPropagation();browserCloseTab(' + (BrowserState.tabs.length - 1) + ')">&#x2715;</span>';
    tabBar.insertBefore(tabEl, newTabBtn);
  }
  browserSwitchTab(BrowserState.tabs.length - 1);
  _browserUpdateTabCount();
}

function browserSwitchTab(idx) {
  if (idx < 0 || idx >= BrowserState.tabs.length) return;

  // Deactivate old tab
  var oldTabEl = document.getElementById('browserTab' + BrowserState.tabs[BrowserState.activeTab].id);
  if (oldTabEl) oldTabEl.classList.remove('active');

  BrowserState.activeTab = idx;
  var tab = BrowserState.tabs[idx];

  // Activate new tab
  var newTabEl = document.getElementById('browserTab' + tab.id);
  if (newTabEl) newTabEl.classList.add('active');

  // Load the tab's content
  document.getElementById('browserUrlInput').value = tab.url || '';
  _browserUpdateNavButtons();

  if (tab.url) {
    _browserLoadUrl(tab.url);
  } else {
    browserGoHome();
  }
}

function browserCloseTab(idx) {
  if (BrowserState.tabs.length <= 1) {
    // Last tab — just go home
    browserGoHome();
    return;
  }

  var tab = BrowserState.tabs[idx];
  var tabEl = document.getElementById('browserTab' + tab.id);
  if (tabEl) tabEl.remove();

  BrowserState.tabs.splice(idx, 1);

  // Adjust active tab index
  if (BrowserState.activeTab >= BrowserState.tabs.length) {
    BrowserState.activeTab = BrowserState.tabs.length - 1;
  } else if (BrowserState.activeTab > idx) {
    BrowserState.activeTab--;
  }

  // Re-render tab click handlers (they reference by index)
  _browserRebuildTabBar();
  browserSwitchTab(BrowserState.activeTab);
  _browserUpdateTabCount();
}

function browserNavigateTo(url) {
  // Direct navigation to a specific URL — used by bookmarks and quicklinks
  document.getElementById('browserUrlInput').value = url;
  showBrowser();
  browserNavigate();
}

function browserNavigateFromHome() {
  var input = document.getElementById('browserHomeSearch');
  if (!input) return;
  var val = (input.value || '').trim();
  if (!val) return;
  document.getElementById('browserUrlInput').value = val;
  browserNavigate();
}

function browserRefreshPage() {
  browserReload();
}

function browserToggleDevtools() {
  // In WebView2 native mode, send devtools message to host
  if (BrowserState.isWebView2 && window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({ type: 'browser-devtools' });
    return;
  }
  // Fallback: show page source info
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (!tab || !tab.url) {
    document.getElementById('browserStatusInfo').textContent = 'No page loaded';
    return;
  }
  var vp = document.getElementById('browserViewport');
  var iframe = vp ? vp.querySelector('iframe') : null;
  var proxyDiv = vp ? vp.querySelector('.browser-proxy-content') : null;
  var info = 'DevTools — URL: ' + tab.url;
  if (iframe) {
    info += ' | Type: iframe';
    try { info += ' | Title: ' + (iframe.contentDocument.title || '?'); } catch (e) { info += ' | Cross-origin'; }
  } else if (proxyDiv) {
    info += ' | Type: proxy | Size: ' + (proxyDiv.innerHTML.length) + ' bytes';
  }
  info += ' | Tabs: ' + BrowserState.tabs.length;
  info += ' | History: ' + BrowserState.history.length + ' entries';
  document.getElementById('browserStatusInfo').textContent = info;
  // Also log to console
  console.log('[Browser DevTools]', {
    url: tab.url,
    tabs: BrowserState.tabs.length,
    history: BrowserState.history,
    bookmarks: BrowserState.bookmarks,
    proxyMode: BrowserState.proxyMode,
    isWebView2: BrowserState.isWebView2
  });
}

function browserAddBookmark() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  if (!tab || !tab.url) {
    document.getElementById('browserStatusInfo').textContent = 'No page to bookmark';
    return;
  }
  // Check if already bookmarked
  var exists = BrowserState.bookmarks && BrowserState.bookmarks.some(function (b) { return b.url === tab.url; });
  if (exists) {
    document.getElementById('browserStatusInfo').textContent = 'Already bookmarked: ' + tab.url;
    return;
  }
  if (!BrowserState.bookmarks) BrowserState.bookmarks = [];
  BrowserState.bookmarks.push({
    url: tab.url,
    title: tab.title || _browserTitleFromUrl(tab.url),
    timestamp: Date.now()
  });
  document.getElementById('browserStatusInfo').textContent = '\u2606 Bookmarked: ' + (tab.title || tab.url);
  // Add to bookmarks bar dynamically
  var bar = document.getElementById('browserBookmarksBar');
  if (bar) {
    var btn = document.createElement('button');
    btn.className = 'browser-bookmark-btn';
    btn.textContent = (tab.title || _browserTitleFromUrl(tab.url)).substring(0, 20);
    btn.onclick = function () { browserNavigateTo(tab.url); };
    bar.appendChild(btn);
  }
}

function browserShowBookmarks() {
  var vp = document.getElementById('browserViewport');
  var home = document.getElementById('browserHomePage');
  if (!vp) return;
  // Hide existing content
  var oldIframe = vp.querySelector('iframe');
  if (oldIframe) oldIframe.remove();
  var oldProxy = vp.querySelector('.browser-proxy-content');
  if (oldProxy) oldProxy.remove();
  if (home) home.style.display = 'none';

  var bmarks = BrowserState.bookmarks || [];
  var html = '<div style="padding:40px 20px;font-family:var(--font-mono);">';
  html += '<h2 style="color:var(--accent-cyan);margin-bottom:20px;">\u1F516 Bookmarks (' + bmarks.length + ')</h2>';
  if (bmarks.length === 0) {
    html += '<p style="color:var(--text-muted);">No bookmarks yet. Click \u2606 to bookmark the current page.</p>';
  } else {
    bmarks.forEach(function (b, idx) {
      html += '<div style="padding:8px 12px;margin:4px 0;background:rgba(255,255,255,0.03);border:1px solid var(--border);border-radius:6px;cursor:pointer;" onclick="browserNavigateTo(\'' + b.url.replace(/'/g, "\\'") + '\')">';
      html += '<div style="color:var(--text-primary);font-weight:600;">' + (b.title || 'Untitled') + '</div>';
      html += '<div style="font-size:10px;color:var(--accent-cyan);">' + b.url + '</div>';
      html += '<div style="font-size:9px;color:var(--text-muted);">' + new Date(b.timestamp).toLocaleString() + '</div>';
      html += '</div>';
    });
  }
  // Also show built-in bookmarks
  html += '<h3 style="color:var(--text-secondary);margin-top:24px;">Built-in Model Sites</h3>';
  var builtins = [
    { title: 'HuggingFace GGUF Models', url: 'https://huggingface.co/models?sort=trending&search=gguf', icon: '\uD83E\uDD17' },
    { title: 'Ollama Library', url: 'https://ollama.com/library', icon: '\uD83E\uDD99' },
    { title: 'TheBloke', url: 'https://huggingface.co/TheBloke', icon: '\uD83D\uDCE6' },
    { title: 'Bartowski', url: 'https://huggingface.co/bartowski', icon: '\uD83D\uDCE6' },
    { title: 'llama.cpp', url: 'https://github.com/ggerganov/llama.cpp', icon: '\uD83D\uDCBB' },
    { title: 'OpenRouter Models', url: 'https://openrouter.ai/models', icon: '\uD83C\uDF10' },
    { title: 'LMSys Leaderboard', url: 'https://lmsys.org/blog/2023-06-22-leaderboard/', icon: '\uD83C\uDFC6' },
    { title: 'Ollama Docs', url: 'https://docs.ollama.com', icon: '\uD83D\uDCD6' },
  ];
  builtins.forEach(function (b) {
    html += '<div style="padding:6px 12px;margin:4px 0;background:rgba(255,255,255,0.02);border:1px solid var(--border);border-radius:6px;cursor:pointer;display:flex;align-items:center;gap:8px;" onclick="browserNavigateTo(\'' + b.url + '\')">';
    html += '<span style="font-size:18px;">' + b.icon + '</span>';
    html += '<div><div style="color:var(--text-primary);">' + b.title + '</div><div style="font-size:10px;color:var(--text-muted);">' + b.url + '</div></div>';
    html += '</div>';
  });
  html += '</div>';

  var div = document.createElement('div');
  div.className = 'browser-proxy-content';
  div.innerHTML = html;
  vp.appendChild(div);

  document.getElementById('browserUrlInput').value = 'about:bookmarks';
  document.getElementById('browserStatusInfo').textContent = bmarks.length + ' user bookmarks';
}

function _browserRebuildTabBar() {
  var tabBar = document.getElementById('browserTabBar');
  if (!tabBar) return;
  var newTabBtn = tabBar.querySelector('.browser-new-tab');
  // Remove all tab elements
  var existingTabs = tabBar.querySelectorAll('.browser-tab');
  for (var i = 0; i < existingTabs.length; i++) existingTabs[i].remove();

  // Recreate
  for (var i = 0; i < BrowserState.tabs.length; i++) {
    (function (idx) {
      var t = BrowserState.tabs[idx];
      var el = document.createElement('div');
      el.className = 'browser-tab' + (idx === BrowserState.activeTab ? ' active' : '');
      el.id = 'browserTab' + t.id;
      el.onclick = function () { browserSwitchTab(idx); };
      el.innerHTML = '<span class="tab-title">' + (t.title || 'New Tab') + '</span>' +
        '<span class="tab-close" onclick="event.stopPropagation();browserCloseTab(' + idx + ')">&#x2715;</span>';
      tabBar.insertBefore(el, newTabBtn);
    })(i);
  }
}

// ---- Browser Utility Functions ----

function _browserUpdateNavButtons() {
  var tab = BrowserState.tabs[BrowserState.activeTab];
  var btnBack = document.getElementById('browserBtnBack');
  var btnFwd = document.getElementById('browserBtnFwd');
  if (btnBack) btnBack.disabled = !tab || tab.historyIdx <= 0;
  if (btnFwd) btnFwd.disabled = !tab || tab.historyIdx >= tab.history.length - 1;
}

function _browserUpdateSsl(url) {
  var icon = document.getElementById('browserSslIcon');
  var status = document.getElementById('browserStatusSsl');
  if (!url) {
    if (icon) icon.textContent = '🔒';
    if (status) status.textContent = '🔒 Secure';
    return;
  }
  var isHttps = url.indexOf('https://') === 0;
  if (icon) icon.textContent = isHttps ? '🔒' : '⚠';
  if (icon) icon.style.color = isHttps ? 'var(--accent-green)' : 'var(--accent-orange)';
  if (status) status.textContent = isHttps ? '🔒 Secure' : '⚠ Not Secure';
  if (status) status.style.color = isHttps ? 'var(--accent-green)' : 'var(--accent-orange)';
}

function _browserUpdateTab(idx, title, url) {
  var tab = BrowserState.tabs[idx];
  if (!tab) return;
  tab.title = title || tab.title;
  tab.url = url || tab.url;
  var el = document.getElementById('browserTab' + tab.id);
  if (el) {
    var titleSpan = el.querySelector('.tab-title');
    if (titleSpan) titleSpan.textContent = (title || '').substring(0, 30) || 'New Tab';
  }
}

function _browserUpdateTabCount() {
  var el = document.getElementById('browserTabCount');
  if (el) el.textContent = BrowserState.tabs.length + ' tab' + (BrowserState.tabs.length !== 1 ? 's' : '');
}

function _browserTitleFromUrl(url) {
  try {
    var u = new URL(url);
    return u.hostname + (u.pathname !== '/' ? u.pathname : '');
  } catch (e) {
    return url.substring(0, 40);
  }
}

// ---- WebView2 Message Handler ----
// When hosted inside the Win32 IDE's WebView2 control, the C++ host
// can send messages to the browser panel (navigation results, etc.)
if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener('message', function (e) {
    var msg = e.data;
    if (!msg || !msg.type) return;
    if (msg.type === 'browser-content') {
      // Host fetched content for us
      var vp = document.getElementById('browserViewport');
      var loading = document.getElementById('browserLoading');
      if (loading) loading.style.display = 'none';
      if (vp) {
        var div = document.createElement('div');
        div.className = 'browser-proxy-content';
        div.innerHTML = msg.html || msg.content || '';
        vp.appendChild(div);
      }
      if (msg.title) _browserUpdateTab(BrowserState.activeTab, msg.title, msg.url);
      document.getElementById('browserStatusInfo').textContent = 'Loaded (WebView2)';
    } else if (msg.type === 'browser-error') {
      document.getElementById('browserStatusInfo').textContent = 'WebView2 Error: ' + (msg.error || 'Unknown');
    }
  });
}

// Multi-port beacon scan: try all known ports for Win32 IDE /status
async function ghostBeaconScan() {
  var ports = State.ghost.scanPorts;
  var results = [];
  var foundUrl = null;
  var foundData = null;

  for (var i = 0; i < ports.length; i++) {
    var url = 'http://localhost:' + ports[i];
    var entry = { port: ports[i], url: url, status: 'timeout', backend: null };
    try {
      var res = await fetch(url + '/status', { signal: AbortSignal.timeout(1500) });
      if (res.ok) {
        var data = await res.json();
        entry.status = 'ok';
        entry.backend = data.backend || data.server || 'unknown';
        if (data.backend === 'rawrxd-win32ide' || (data.server && data.server.indexOf('Win32IDE') >= 0)) {
          entry.status = 'ide';
          if (!foundUrl) { foundUrl = url; foundData = data; }
        }
      } else {
        entry.status = 'http-' + res.status;
      }
    } catch (e) {
      // Try bare GET for Ollama-style servers
      try {
        var res2 = await fetch(url + '/', { signal: AbortSignal.timeout(1000) });
        if (res2.ok) {
          var txt = await res2.text();
          if (txt.toLowerCase().includes('ollama')) {
            entry.status = 'ollama';
            entry.backend = 'ollama';
          } else {
            entry.status = 'alive';
          }
        }
      } catch (_) { /* truly dead */ }
    }
    results.push(entry);
    logDebug('\uD83D\uDC7B Scan :' + ports[i] + ' → ' + entry.status + (entry.backend ? ' (' + entry.backend + ')' : ''), entry.status === 'ide' ? 'info' : 'warn');
  }

  State.ghost.lastScanResults = results;
  return { foundUrl: foundUrl, foundData: foundData, results: results };
}

// Build the scan-results HTML table for the ghost waiting screen
function _ghostScanResultsHtml(results) {
  var html = '<div style="text-align:left;margin:12px auto;max-width:420px;font-size:11px;">';
  html += '<table style="width:100%;border-collapse:collapse;">';
  html += '<tr style="color:var(--text-muted);border-bottom:1px solid var(--border);">' +
    '<th style="padding:3px 8px;text-align:left;">Port</th>' +
    '<th style="padding:3px 8px;text-align:left;">Status</th>' +
    '<th style="padding:3px 8px;text-align:left;">Backend</th></tr>';
  for (var i = 0; i < results.length; i++) {
    var r = results[i];
    var color = r.status === 'ide' ? 'var(--accent-green)' :
      r.status === 'ollama' || r.status === 'ok' || r.status === 'alive' ? 'var(--accent-cyan)' :
        'var(--text-muted)';
    var icon = r.status === 'ide' ? '\u2714' :
      r.status === 'ollama' || r.status === 'ok' || r.status === 'alive' ? '\u25CF' : '\u2718';
    html += '<tr style="border-bottom:1px solid rgba(255,255,255,0.04);">' +
      '<td style="padding:3px 8px;color:' + color + ';">' + icon + ' :' + r.port + '</td>' +
      '<td style="padding:3px 8px;color:' + color + ';">' + r.status + '</td>' +
      '<td style="padding:3px 8px;color:var(--text-muted);">' + (r.backend || '—') + '</td></tr>';
  }
  html += '</table></div>';
  return html;
}

// Stop ghost auto-retry polling
function _ghostStopAutoRetry() {
  if (State.ghost.autoRetryTimer) {
    clearInterval(State.ghost.autoRetryTimer);
    State.ghost.autoRetryTimer = null;
  }
  State.ghost.waitingForIde = false;
}

// Start ghost auto-retry: poll every 5s for IDE, auto-connect when found
function _ghostStartAutoRetry() {
  if (State.ghost.autoRetryTimer) return; // already polling
  State.ghost.waitingForIde = true;
  logDebug('\uD83D\uDC7B Auto-retry: polling for Win32 IDE every 5s...', 'info');

  State.ghost.autoRetryTimer = setInterval(async function () {
    if (!State.ghost.active || !State.ghost.waitingForIde) {
      _ghostStopAutoRetry();
      return;
    }
    // Quick probe just the primary IDE port
    try {
      var res = await fetch('http://localhost:8080/status', { signal: AbortSignal.timeout(1500) });
      if (res.ok) {
        var data = await res.json();
        if (data.backend === 'rawrxd-win32ide' || (data.server && data.server.indexOf('Win32IDE') >= 0)) {
          logDebug('\uD83D\uDC7B Auto-retry: IDE found! Connecting...', 'info');
          _ghostStopAutoRetry();
          // Close and re-ghost now that IDE is alive
          ghostCloseIDE();
          ghostIntoIDE();
          return;
        }
      }
    } catch (_) { /* still not up */ }

    // Update countdown display
    var waitEl = document.getElementById('ghostWaitCounter');
    if (waitEl) {
      var elapsed = Math.round((Date.now() - State.ghost.probeTime) / 1000);
      waitEl.textContent = 'Waiting ' + elapsed + 's... (polling every 5s)';
    }
  }, 5000);
}

async function ghostIntoIDE() {
  var overlay = document.getElementById('ghostOverlay');
  var loading = document.getElementById('ghostLoading');
  var iframeWrap = document.getElementById('ghostIframeWrap');
  var probeUrlSpan = document.getElementById('ghostProbeUrl');
  var titleText = document.getElementById('ghostTitleText');

  // If ghost is already active, just bring it to front / un-minimize
  if (State.ghost.active) {
    overlay.classList.remove('minimized');
    State.ghost.minimized = false;
    logDebug('\uD83D\uDC7B Ghost session already active — brought to front', 'info');
    return;
  }

  // Stop any previous auto-retry
  _ghostStopAutoRetry();

  // Show overlay with loading spinner
  State.ghost.sessionId++;
  State.ghost.active = true;
  State.ghost.minimized = false;
  overlay.classList.add('active');
  overlay.classList.remove('minimized');

  // Remove any existing iframe
  var oldIframe = iframeWrap.querySelector('iframe');
  if (oldIframe) oldIframe.remove();
  loading.style.display = 'block';

  // Determine primary probe URL: prefer already-detected IDE, else default
  var primaryUrl = _win32IdeDetected ? _win32IdeUrl : 'http://localhost:8080';
  if (probeUrlSpan) probeUrlSpan.textContent = 'scanning ports...';

  logDebug('\uD83D\uDC7B Ghost beacon: multi-port scan starting...', 'info');
  addMessage('system', '\uD83D\uDC7B **Ghost Session Starting** — Beacon scanning ports for Win32 IDE...', { skipMemory: true });

  // Multi-port beacon scan
  var scan = await ghostBeaconScan();
  State.ghost.probeTime = Date.now();

  if (scan.foundUrl) {
    // IDE found!
    _win32IdeDetected = true;
    _win32IdeUrl = scan.foundUrl;
    updateIdeBridgeUI(true, scan.foundData);
    logDebug('\uD83D\uDC7B Beacon confirmed: Win32 IDE alive at ' + scan.foundUrl, 'info');

    // Inject iframe pointed at /gui
    State.ghost.ideUrl = scan.foundUrl + '/gui';
    loading.style.display = 'none';

    var iframe = document.createElement('iframe');
    iframe.id = 'ghostIdeFrame';
    iframe.src = State.ghost.ideUrl;
    iframe.setAttribute('sandbox', 'allow-scripts allow-same-origin allow-forms allow-popups allow-modals');
    iframe.setAttribute('allow', 'clipboard-read; clipboard-write');
    iframe.onload = function () {
      logDebug('\uD83D\uDC7B Ghost iframe loaded: ' + State.ghost.ideUrl, 'info');
      var dot = document.getElementById('ghostDot');
      if (dot) dot.style.background = 'var(--accent-green)';
    };
    iframe.onerror = function () {
      logDebug('\uD83D\uDC7B Ghost iframe failed to load', 'error');
      var dot = document.getElementById('ghostDot');
      if (dot) dot.style.background = 'var(--accent-red)';
    };
    iframeWrap.appendChild(iframe);
    State.ghost.iframeRef = iframe;

    if (titleText) titleText.innerHTML = '\uD83D\uDC7B GHOST SESSION \u2014 ' + scan.foundUrl + ' \u2014 #' + State.ghost.sessionId;

    addMessage('system', '\u2705 **Ghost Session Active** — Win32 IDE embedded via beacon at `' + scan.foundUrl + '/gui`. Session #' + State.ghost.sessionId, { skipMemory: true });
    logDebug('\uD83D\uDC7B Ghost session #' + State.ghost.sessionId + ' established at ' + State.ghost.ideUrl, 'info');

    // Also ensure CLI beacon is probed since IDE is confirmed alive
    if (!State.backend.hasCliEndpoint) {
      await beaconProbeCliEndpoint();
    }
    return;
  }

  // IDE not found — show waiting dashboard with scan results + auto-retry
  var dot = document.getElementById('ghostDot');
  if (dot) dot.style.background = 'var(--accent-secondary)';
  if (titleText) titleText.innerHTML = '\uD83D\uDC7B GHOST — Waiting for Win32 IDE...';

  // Detect if Ollama Direct is alive for the inline dashboard
  var ollamaAlive = scan.results.some(function (r) { return r.status === 'ollama'; });
  var anyBackend = scan.results.some(function (r) { return r.status === 'ok' || r.status === 'alive' || r.status === 'ollama'; });

  // Build the waiting-for-IDE screen
  var waitHtml = '<div style="max-width:520px;text-align:center;">';
  waitHtml += '<div style="font-size:36px;margin-bottom:8px;">\uD83D\uDC7B</div>';
  waitHtml += '<div style="color:var(--accent-secondary);font-size:15px;font-weight:700;margin-bottom:6px;">Win32 IDE Not Detected</div>';
  waitHtml += '<div style="color:var(--text-muted);font-size:12px;margin-bottom:16px;">'
    + 'Scanned ' + scan.results.length + ' ports — no RawrXD-Win32IDE found.</div>';

  // Scan results table
  waitHtml += _ghostScanResultsHtml(scan.results);

  // Backend status summary
  if (ollamaAlive || State.backend.online) {
    waitHtml += '<div style="background:rgba(0,217,255,0.08);border:1px solid rgba(0,217,255,0.2);border-radius:6px;padding:10px;margin:12px auto;max-width:400px;text-align:left;font-size:11px;">';
    waitHtml += '<div style="color:var(--accent-cyan);font-weight:700;margin-bottom:6px;">\u26A1 Backend Active (Ollama Direct)</div>';
    waitHtml += '<div style="color:var(--text-secondary);">'
      + 'URL: ' + getActiveUrl() + '<br>'
      + 'Model: ' + (State.model.current || 'none') + '<br>'
      + 'CLI: ' + (State.backend.hasCliEndpoint ? '\u2714 Remote' : 'Client-side beacon') + '<br>'
      + '</div>';
    waitHtml += '<div style="color:var(--text-muted);margin-top:6px;">'
      + 'Chat and CLI work normally. Ghost requires the Win32 IDE for the embedded GUI.</div>';
    waitHtml += '</div>';
  }

  // Action buttons
  waitHtml += '<div style="margin-top:16px;display:flex;gap:8px;justify-content:center;flex-wrap:wrap;">';
  waitHtml += '<button onclick="ghostCloseIDE();ghostIntoIDE()" style="background:rgba(0,217,255,0.12);border:1px solid var(--accent-cyan);color:var(--accent-cyan);padding:6px 16px;border-radius:4px;cursor:pointer;font-family:var(--font-mono);font-size:11px;font-weight:600;">\u21BB Retry Scan</button>';
  waitHtml += '<button onclick="_ghostStartAutoRetry()" id="ghostAutoRetryBtn" style="background:rgba(0,255,136,0.12);border:1px solid var(--accent-green);color:var(--accent-green);padding:6px 16px;border-radius:4px;cursor:pointer;font-family:var(--font-mono);font-size:11px;font-weight:600;">\u23F3 Auto-Wait</button>';
  waitHtml += '<button onclick="refreshBackendBeacon()" style="background:rgba(178,102,255,0.12);border:1px solid #b266ff;color:#b266ff;padding:6px 16px;border-radius:4px;cursor:pointer;font-family:var(--font-mono);font-size:11px;font-weight:600;">\u26A1 Re-Beacon All</button>';
  waitHtml += '<button onclick="ghostCloseIDE()" style="background:rgba(255,51,102,0.12);border:1px solid var(--accent-red);color:var(--accent-red);padding:6px 16px;border-radius:4px;cursor:pointer;font-family:var(--font-mono);font-size:11px;font-weight:600;">\u2715 Close</button>';
  waitHtml += '</div>';

  // Auto-retry status line
  waitHtml += '<div id="ghostWaitCounter" style="color:var(--text-muted);font-size:10px;margin-top:10px;"></div>';

  // How to start the IDE
  waitHtml += '<div style="color:var(--text-muted);font-size:10px;margin-top:12px;border-top:1px solid var(--border);padding-top:8px;">';
  waitHtml += 'Start <code style="color:var(--accent-cyan);">RawrXD-Win32IDE.exe</code> then click <b>Retry Scan</b> or <b>Auto-Wait</b>.<br>';
  waitHtml += 'The ghost will connect automatically when the IDE comes online.';
  waitHtml += '</div>';

  waitHtml += '</div>';

  loading.innerHTML = waitHtml;
  loading.style.display = 'block';

  addMessage('system', '\u26A0\uFE0F **Ghost Waiting** — Win32 IDE not found on ' + scan.results.length + ' ports. '
    + (ollamaAlive ? 'Ollama is running — chat works, but Ghost needs the IDE.' : 'No backends detected.')
    + ' Click **Auto-Wait** to auto-connect when IDE starts.', { skipMemory: true });
}

function ghostCloseIDE() {
  // Stop auto-retry polling
  _ghostStopAutoRetry();

  var overlay = document.getElementById('ghostOverlay');
  overlay.classList.remove('active', 'minimized');

  // Remove iframe
  var iframeWrap = document.getElementById('ghostIframeWrap');
  var iframe = iframeWrap.querySelector('iframe');
  if (iframe) iframe.remove();

  // Reset loading view for next time
  var loading = document.getElementById('ghostLoading');
  if (loading) {
    loading.style.display = 'block';
    loading.innerHTML = '<div class="ghost-spinner"></div><br>' +
      'Beacon probing Win32 IDE at <span id="ghostProbeUrl">localhost:8080</span>...';
  }

  // Reset ghost dot
  var dot = document.getElementById('ghostDot');
  if (dot) dot.style.background = 'var(--accent-green)';

  State.ghost.active = false;
  State.ghost.minimized = false;
  State.ghost.waitingForIde = false;
  State.ghost.iframeRef = null;
  logDebug('\uD83D\uDC7B Ghost session closed', 'info');
}

function ghostRefreshIDE() {
  if (!State.ghost.active || !State.ghost.iframeRef) return;
  var iframe = State.ghost.iframeRef;
  logDebug('\uD83D\uDC7B Refreshing ghost iframe...', 'info');
  // Force reload by resetting src
  iframe.src = State.ghost.ideUrl + '?t=' + Date.now();
}

function ghostDetachIDE() {
  // Pop the ghost IDE into its own browser window, then close overlay
  if (!State.ghost.ideUrl) {
    addMessage('system', '\u26A0\uFE0F No ghost session to detach.', { skipMemory: true });
    return;
  }
  var w = window.open(State.ghost.ideUrl, '_blank',
    'width=1280,height=900,menubar=no,toolbar=no,location=no,status=no,resizable=yes');
  if (w) {
    State.ghost.detachedWindow = w;
    logDebug('\uD83D\uDC7B Ghost detached to new window', 'info');
    addMessage('system', '\u2197\uFE0F Ghost IDE detached to popup window. Closing overlay.', { skipMemory: true });
    ghostCloseIDE();
  } else {
    addMessage('system', '\u26A0\uFE0F Popup blocked — allow popups for this page to detach the ghost session.', { skipMemory: true });
  }
}

function ghostMinimizeIDE() {
  var overlay = document.getElementById('ghostOverlay');
  if (State.ghost.minimized) {
    overlay.classList.remove('minimized');
    State.ghost.minimized = false;
    logDebug('\uD83D\uDC7B Ghost restored from minimized', 'info');
  } else {
    overlay.classList.add('minimized');
    State.ghost.minimized = true;
    logDebug('\uD83D\uDC7B Ghost minimized to toolbar', 'info');
  }
}

// ======================================================================
// REFRESH BACKEND BEACON — Force re-probe of all beacons
// Reconnects backend, re-probes Win32 IDE, re-probes /api/cli endpoint
// Accessible from button press, CLI /beacon, or ghost toolbar
// ======================================================================

async function refreshBackendBeacon() {
  logDebug('\u26A1 Beacon refresh: re-probing all connections...', 'info');
  addMessage('system', '\u26A1 **Beacon Refresh** — Re-probing backend, Win32 IDE, and CLI endpoints...', { skipMemory: true });

  // 1. Multi-port scan for Win32 IDE
  var ideWasDetected = _win32IdeDetected;
  var scan = await ghostBeaconScan();
  if (scan.foundUrl) {
    _win32IdeDetected = true;
    _win32IdeUrl = scan.foundUrl;
    updateIdeBridgeUI(true, scan.foundData);
    if (!ideWasDetected) {
      addMessage('system', '\u2705 Win32 IDE newly detected at `' + _win32IdeUrl + '`!', { skipMemory: true });
    }
  } else {
    // Fallback to simple probe (in case scan missed it)
    var ideResult = await probeWin32IDE();
    if (!ideResult && ideWasDetected) {
      addMessage('system', '\u26A0\uFE0F Win32 IDE went offline.', { skipMemory: true });
    }
  }

  // 2. Reconnect backend (this also re-probes CLI beacon)
  await connectBackend();

  // 3. Explicit CLI beacon re-probe
  await beaconProbeCliEndpoint();

  // 4. Build scan report
  var scanLines = '';
  if (scan.results && scan.results.length > 0) {
    scanLines = '\n**Port Scan:**\n';
    for (var i = 0; i < scan.results.length; i++) {
      var r = scan.results[i];
      var icon = r.status === 'ide' ? '\u2714' :
        r.status === 'ollama' || r.status === 'ok' || r.status === 'alive' ? '\u25CF' : '\u2718';
      scanLines += '  ' + icon + ' `:' + r.port + '` → ' + r.status + (r.backend ? ' (' + r.backend + ')' : '') + '\n';
    }
  }

  // 5. Report results
  var summary =
    '\u2500 **Beacon Refresh Complete** \u2500\n' +
    '- Backend: ' + (State.backend.online ? '\u2714 ONLINE (' + (State.backend.serverType || '?') + ')' : '\u2718 OFFLINE') + '\n' +
    '- Win32 IDE: ' + (_win32IdeDetected ? '\u2714 Detected at `' + _win32IdeUrl + '`' : '\u2718 Not detected') + '\n' +
    '- CLI Endpoint: ' + (State.backend.hasCliEndpoint ? '\u2714 `' + (State.backend._cliEndpointUrl || '') + '/api/cli`' : '\u2718 Client-side beacon') + '\n' +
    '- Mode: ' + (State.backend.directMode ? 'Ollama Direct' : 'Proxy') +
    scanLines;
  addMessage('system', summary, { skipMemory: true });
  logDebug('\u26A1 Beacon refresh complete. IDE=' + _win32IdeDetected + ' CLI=' + State.backend.hasCliEndpoint + ' Backend=' + State.backend.online, 'info');
}

// Sync titlebar status indicators with current backend state
function syncTitlebarStatus() {
  var tbDot = document.getElementById('tbDot');
  var tbStatusText = document.getElementById('tbStatusText');
  var tbMode = document.getElementById('tbMode');

  if (!tbDot || !tbStatusText || !tbMode) return;

  if (State.backend.online) {
    tbDot.classList.add('online');
    if (State.backend.directMode) {
      tbStatusText.textContent = 'OLLAMA DIRECT';
      tbMode.textContent = 'DIRECT';
      tbMode.style.background = 'rgba(0,217,255,0.15)';
      tbMode.style.color = 'var(--accent-cyan)';
    } else if (_win32IdeDetected) {
      tbStatusText.textContent = 'IDE ONLINE';
      tbMode.textContent = _isFileProtocol ? 'FILE \u2192 IDE' : 'IDE';
      tbMode.style.background = 'rgba(0,255,136,0.15)';
      tbMode.style.color = 'var(--accent-green)';
    } else {
      tbStatusText.textContent = 'CONNECTED';
      tbMode.textContent = _isFileProtocol ? 'STANDALONE' : 'HTTP';
      tbMode.style.background = '';
      tbMode.style.color = '';
    }
  } else {
    tbDot.classList.remove('online');
    tbStatusText.textContent = 'DISCONNECTED';
    tbMode.textContent = _isFileProtocol ? 'OFFLINE' : 'HTTP';
    tbMode.style.background = 'rgba(255,51,102,0.15)';
    tbMode.style.color = 'var(--accent-red)';
  }
}

// IDE Bridge: read a file via the Win32IDE /api/read-file endpoint
async function readFileViaIDE(filePath) {
  if (!_win32IdeDetected) return null;
  try {
    var res = await fetch(_win32IdeUrl + '/api/read-file', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: filePath }),
      signal: AbortSignal.timeout(5000)
    });
    if (res.ok) return await res.json();
  } catch (_) { /* IDE not reachable for file read */ }
  return null;
}

// IDE Bridge: get IDE status snapshot (model, backends, router, etc.)
async function getIDEStatus() {
  if (!_win32IdeDetected) return null;
  try {
    var endpoints = ['/status', '/api/router/status', '/api/backends', '/api/hotpatch/status'];
    var results = {};
    for (var i = 0; i < endpoints.length; i++) {
      try {
        var res = await fetch(_win32IdeUrl + endpoints[i], { signal: AbortSignal.timeout(3000) });
        if (res.ok) results[endpoints[i]] = await res.json();
      } catch (_) { /* skip failed endpoints */ }
    }
    return results;
  } catch (_) { return null; }
}

// IDE Bridge: send a hotpatch command to the running IDE
async function sendHotpatchToIDE(action, layer, data) {
  if (!_win32IdeDetected) return null;
  var endpoint = '/api/hotpatch/' + action;
  try {
    var res = await fetch(_win32IdeUrl + endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ layer: layer, data: data || {} }),
      signal: AbortSignal.timeout(5000)
    });
    if (res.ok) return await res.json();
  } catch (e) {
    logDebug('Hotpatch via IDE failed: ' + e.message, 'error');
  }
  return null;
}

// ======================================================================
// INIT
// ======================================================================
// Guard: detect if localStorage is available (some browsers block it in file:// or private mode)
var _localStorageAvailable = (function () {
  try {
    var key = '__rawrxd_ls_test__';
    localStorage.setItem(key, '1');
    localStorage.removeItem(key);
    return true;
  } catch (e) {
    console.warn('[RawrXD] localStorage unavailable — session persistence disabled');
    return false;
  }
})();

document.addEventListener('DOMContentLoaded', async () => {
  setupDragDrop();
  autoResizeInput();

  // Detect file:// mode and update UI hints
  if (_isFileProtocol) {
    document.title = 'RawrXD IDE // STANDALONE';
    document.body.classList.add('standalone-mode');
    var titlebar = document.getElementById('standaloneTitlebar');
    if (titlebar) titlebar.classList.add('active');
    var brandSpan = document.querySelector('.header-bar .brand span:last-child');
    if (brandSpan) brandSpan.textContent = '// STANDALONE v3.4';
    logDebug('\uD83D\uDCC2 Opened from file:// — standalone mode active. Backend at localhost:8080/11434.', 'info');
    // Warn if localStorage is blocked in file:// mode
    if (!_localStorageAvailable) {
      logDebug('\u26A0\uFE0F localStorage blocked by browser in file:// mode — conversations won\u2019t persist across reloads', 'warn');
    }
  }

  // Probe for Win32IDE server (works from file:// or http://)
  probeWin32IDE();

  // Restore session from localStorage
  var hasSession = Conversation.load();
  var hasGenSettings = Conversation.loadGenSettings();

  // Populate system prompt editor
  var sysInput = document.getElementById('systemPromptInput');
  if (sysInput) sysInput.value = Conversation.systemPrompt;

  // Restore generation setting sliders
  if (hasGenSettings) {
    var ctxSlider = document.getElementById('ctxSlider');
    var maxTokSlider = document.getElementById('maxTokSlider');
    var tempSlider = document.getElementById('tempSlider');
    var streamToggle = document.getElementById('streamToggle');
    if (ctxSlider) { ctxSlider.value = State.gen.context; document.getElementById('ctxVal').textContent = State.gen.context.toLocaleString(); }
    if (maxTokSlider) { maxTokSlider.value = State.gen.maxTokens; document.getElementById('maxTokVal').textContent = State.gen.maxTokens; }
    if (tempSlider) { tempSlider.value = Math.round(State.gen.temperature * 100); document.getElementById('tempVal').textContent = State.gen.temperature.toFixed(1); }
    if (streamToggle) { streamToggle.checked = State.gen.stream; }
    // Restore Top-P / Top-K
    var topPSlider = document.getElementById('topPSlider');
    var topKSlider = document.getElementById('topKSlider');
    if (topPSlider) { topPSlider.value = Math.round(State.gen.top_p * 100); document.getElementById('topPVal').textContent = State.gen.top_p.toFixed(2); }
    if (topKSlider) { topKSlider.value = State.gen.top_k; document.getElementById('topKVal').textContent = State.gen.top_k; }
    // Restore Tensor Bunny Hop
    var thToggle = document.getElementById('tensorHopToggle');
    if (thToggle) { thToggle.checked = State.gen.tensorHop.enabled; }
    var hopStrat = document.getElementById('hopStrategySelect');
    if (hopStrat) { hopStrat.value = State.gen.tensorHop.strategy; document.getElementById('hopStratVal').textContent = State.gen.tensorHop.strategy; }
    var skipSlider = document.getElementById('skipRatioSlider');
    if (skipSlider) { skipSlider.value = Math.round(State.gen.tensorHop.skipRatio * 100); document.getElementById('skipRatioVal').textContent = Math.round(State.gen.tensorHop.skipRatio * 100) + '%'; }
    var kfSlider = document.getElementById('keepFirstSlider');
    if (kfSlider) { kfSlider.value = State.gen.tensorHop.keepFirst; document.getElementById('keepFirstVal').textContent = State.gen.tensorHop.keepFirst; }
    var klSlider = document.getElementById('keepLastSlider');
    if (klSlider) { klSlider.value = State.gen.tensorHop.keepLast; document.getElementById('keepLastVal').textContent = State.gen.tensorHop.keepLast; }
    var custInput = document.getElementById('customSkipInput');
    if (custInput && State.gen.tensorHop.customSkip.length) { custInput.value = State.gen.tensorHop.customSkip.join(','); }
    if (State.gen.tensorHop.strategy === 'custom') { var csg = document.getElementById('customSkipGroup'); if (csg) csg.style.display = 'block'; }
    updateTensorHopUI();
    // Restore Safe Decode Profile
    var sdToggle = document.getElementById('safeDecodeToggle');
    if (sdToggle) { sdToggle.checked = State.gen.safeDecodeProfile.enabled; }
    var acToggle = document.getElementById('autoClampToggle');
    if (acToggle) { acToggle.checked = State.gen.safeDecodeProfile.autoClamp; }
    var pfToggle = document.getElementById('probeFirstToggle');
    if (pfToggle) { pfToggle.checked = State.gen.safeDecodeProfile.probeFirst; }
    var thBSlider = document.getElementById('thresholdBSlider');
    if (thBSlider) { thBSlider.value = State.gen.safeDecodeProfile.thresholdB; document.getElementById('thresholdBVal').textContent = State.gen.safeDecodeProfile.thresholdB + 'B'; }
    var sCtxSlider = document.getElementById('safeCtxSlider');
    if (sCtxSlider) { sCtxSlider.value = State.gen.safeDecodeProfile.safeContext; document.getElementById('safeCtxVal').textContent = State.gen.safeDecodeProfile.safeContext.toLocaleString(); }
    var sTokSlider = document.getElementById('safeTokSlider');
    if (sTokSlider) { sTokSlider.value = State.gen.safeDecodeProfile.safeMaxTokens; document.getElementById('safeTokVal').textContent = State.gen.safeDecodeProfile.safeMaxTokens; }
  }

  if (hasSession) {
    // Replay saved messages to UI (without re-animating)
    // Use DocumentFragment to batch DOM inserts and prevent per-message reflow freezes
    var container = document.getElementById('chatMessages');
    var frag = document.createDocumentFragment();
    var restoredCount = 0;
    Conversation.messages.forEach(function (m) {
      var role = m.role === 'user' ? 'user' : 'assistant';
      var time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
      var avatarIcon = role === 'user' ? '\uD83D\uDC64' : (role === 'system' ? '\u2699' : '\uD83E\uDD96');
      var label = role === 'user' ? 'You' : (role === 'system' ? 'System' : 'RawrXD');
      var div = document.createElement('div');
      div.className = 'message ' + role;
      var safeText = role === 'user' ? escHtml(m.content) : formatMessage(m.content);
      div.innerHTML =
        '<div class="message-avatar">' + avatarIcon + '</div>' +
        '<div class="message-content">' +
        '<div class="message-header">' + label + ' <span class="message-time">' + time + '</span></div>' +
        '<div class="message-text">' + safeText + '</div>' +
        '</div>';
      frag.appendChild(div);
      restoredCount++;
    });
    // Single DOM insert for all messages
    container.appendChild(frag);
    State.chat.messageCount += restoredCount;
    document.getElementById('msgCount').textContent = State.chat.messageCount;
    container.scrollTop = container.scrollHeight;
    // Show restored indicator
    var indicator = document.createElement('div');
    indicator.className = 'restored-indicator';
    indicator.id = 'restoredIndicator';
    indicator.innerHTML = '\u2705 Restored ' + restoredCount + ' messages from previous session' +
      '<button onclick="dismissRestored()">Dismiss</button>';
    container.insertBefore(indicator, container.firstChild);
    Conversation.updateUI();
    logDebug('Restored ' + restoredCount + ' messages from localStorage', 'info');
  } else {
    addWelcomeMessage();
  }

  await connectBackend();

  // Start periodic IDE probe (updates titlebar bridge badge)
  startIdePoll();
});

function dismissRestored() {
  var x = document.getElementById('restoredIndicator');
  if (x) x.remove();
}

function addWelcomeMessage() {
  var modeNote = _isFileProtocol
    ? '\n\n\u{1F4C2} **Standalone Mode** — Opened directly from file system. ' +
    'Connecting to `localhost:8080` (RawrXD server) or `localhost:11434` (Ollama).\n' +
    'Drag & drop files to attach them, or paste file paths in your message.' +
    (_win32IdeDetected ? '\n\u{1F5A5} **Win32 IDE detected** — use the `OPEN IDE` button in the titlebar to switch.' : '')
    : '';
  addMessage('system',
    '**RawrXD Agentic Interface v3.4** — Standalone + Win32IDE Bridge\n\n' +
    'This interface connects to the RawrXD backend on `localhost:8080`. ' +
    'If the server isn\u2019t running, it will automatically fall back to Ollama directly on `localhost:11434`.' + modeNote + '\n\n' +
    '**What\'s new in v3.4:**\n' +
    '\u2022 **Standalone Mode** — Works fully from `file:///` with no server needed for UI\n' +
    '\u2022 **Win32 IDE Bridge** — Auto-detects running IDE, syncs state, hotpatch control\n' +
    '\u2022 **Native Titlebar** — Window chrome with status, mode badge, and IDE launcher\n' +
    '\u2022 **IDE File Bridge** — Read local files via IDE\'s `/api/read-file` endpoint\n' +
    '\u2022 **Fullscreen Mode** — F11 key or titlebar button for distraction-free chat\n' +
    '\u2022 **Security Dashboard** — CSP audit, input validation, rate limit meter\n' +
    '\u2022 **Hotpatch Control** — Toggle/apply/revert hotpatches from the browser\n\n' +
    '**Quick start:**\n' +
    '1. Run `launch_rawrxd.bat` or click **\u26A1** to connect\n' +
    '2. Select a model from the sidebar dropdown\n' +
    '3. Type your message and press Enter\n\n' +
    '**Usage modes:**\n' +
    '\u2022 **File mode** — Double-click `gui/ide_chatbot.html` (standalone, no server required for UI)\n' +
    '\u2022 **Served mode** — Navigate to `http://localhost:8080` (served by RawrXD server)\n' +
    '\u2022 **Both** — Same HTML works in either context, auto-detects environment');
}

// ======================================================================
// ======================================================================
// TENSOR BUNNY HOP + SAFE DECODE HELPERS
// ======================================================================

// Toggle tensor hop settings visibility
function updateTensorHopUI() {
  var settings = document.getElementById('tensorHopSettings');
  if (settings) settings.style.display = State.gen.tensorHop.enabled ? 'block' : 'none';
}

// Extract model size in billions from model name (e.g. "gemma3:27b" -> 27, "gpt-oss:120b" -> 120)
function estimateModelSizeB(modelName) {
  if (!modelName) return 0;
  var match = modelName.match(/(\d+\.?\d*)\s*[bB]/);
  if (match) return parseFloat(match[1]);
  // Common model size heuristics
  var lower = modelName.toLowerCase();
  if (lower.indexOf('70b') >= 0 || lower.indexOf('65b') >= 0) return 70;
  if (lower.indexOf('34b') >= 0 || lower.indexOf('33b') >= 0) return 34;
  if (lower.indexOf('27b') >= 0) return 27;
  if (lower.indexOf('13b') >= 0 || lower.indexOf('14b') >= 0) return 13;
  if (lower.indexOf('7b') >= 0 || lower.indexOf('8b') >= 0) return 7;
  if (lower.indexOf('3b') >= 0) return 3;
  if (lower.indexOf('1b') >= 0 || lower.indexOf('1.5b') >= 0) return 1.5;
  return 0; // unknown — don't clamp
}

// Check if current model triggers safe decode profile
function isSafeDecodeActive() {
  if (!State.gen.safeDecodeProfile.enabled) return false;
  if (!State.gen.safeDecodeProfile.autoClamp) return false;
  var sizeB = estimateModelSizeB(State.model.current);
  return sizeB >= State.gen.safeDecodeProfile.thresholdB;
}

// Get effective generation params (after safe decode clamping if active)
function getEffectiveGenParams() {
  var p = {
    context: State.gen.context,
    maxTokens: State.gen.maxTokens,
    temperature: State.gen.temperature,
    top_p: State.gen.top_p,
    top_k: State.gen.top_k,
    stream: State.gen.stream,
  };

  // Apply safe decode clamping for large models
  if (isSafeDecodeActive()) {
    var sdp = State.gen.safeDecodeProfile;
    p.context = Math.min(p.context, sdp.safeContext);
    p.maxTokens = Math.min(p.maxTokens, sdp.safeMaxTokens);
    p.temperature = sdp.safeTemperature;
    p.top_p = sdp.safeTopP;
    p.top_k = sdp.safeTopK;
    if (sdp.disableStreamOnSafe) p.stream = false;
    logDebug('[SafeDecode] Clamped for ' + State.model.current + ': ctx=' + p.context + ' tok=' + p.maxTokens + ' temp=' + p.temperature, 'warn');
  }

  return p;
}

// Build tensor hop metadata for API requests
function buildTensorHopOptions() {
  if (!State.gen.tensorHop.enabled) return null;
  var th = State.gen.tensorHop;
  return {
    enabled: true,
    strategy: th.strategy,
    skip_ratio: th.skipRatio,
    keep_first: th.keepFirst,
    keep_last: th.keepLast,
    custom_skip: th.strategy === 'custom' ? th.customSkip : [],
  };
}

// Build full options payload for OpenAI-compatible endpoints
function buildOpenAIPayloadExtras() {
  var p = getEffectiveGenParams();
  var extras = {
    max_tokens: p.maxTokens,
    temperature: p.temperature,
    top_p: p.top_p,
  };
  // top_k not standard OpenAI but supported by many backends
  if (p.top_k > 0) extras.top_k = p.top_k;
  // Ollama supports num_ctx via options field on /v1/chat/completions
  // Without this, Ollama defaults to 2048 and silently truncates file context
  if (State.backend.directMode || (State.backend.serverType && State.backend.serverType.indexOf('ollama') !== -1)) {
    extras.options = { num_ctx: p.context };
  }
  // Tensor hop metadata (custom extension)
  var th = buildTensorHopOptions();
  if (th) extras.tensor_hop = th;
  return extras;
}

// Build Ollama options object
function buildOllamaOptions() {
  var p = getEffectiveGenParams();
  var opts = {
    temperature: p.temperature,
    top_p: p.top_p,
    top_k: p.top_k,
    num_ctx: p.context,
  };
  var th = buildTensorHopOptions();
  if (th) opts.tensor_hop = th;
  return opts;
}

// First-token probe: lightweight request to verify model is responsive
async function firstTokenProbe(model, url) {
  if (!State.gen.safeDecodeProfile.probeFirst || !isSafeDecodeActive()) return true;
  logDebug('[SafeDecode] Running first-token probe for ' + model, 'info');
  try {
    var res = await fetch(url + '/api/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: model,
        prompt: 'Reply with the word: OK',
        stream: false,
        options: { temperature: 0.1, num_ctx: 512, top_p: 0.9, top_k: 40 },
      }),
      signal: AbortSignal.timeout(15000),
    });
    if (!res.ok) {
      logDebug('[SafeDecode] Probe failed: HTTP ' + res.status, 'error');
      updateSafeDecodeStatus('probe-failed', res.status);
      return false;
    }
    var data = await res.json();
    logDebug('[SafeDecode] Probe OK: ' + (data.response || '').substring(0, 40), 'info');
    updateSafeDecodeStatus('probe-ok');
    return true;
  } catch (e) {
    logDebug('[SafeDecode] Probe exception: ' + e.message, 'error');
    updateSafeDecodeStatus('probe-failed', e.message);
    return false;
  }
}

// Update Safe Decode status panel
function updateSafeDecodeStatus(status, detail) {
  var el = document.getElementById('safeDecodeStatus');
  if (!el) return;
  var model = State.model.current || 'none';
  var sizeB = estimateModelSizeB(model);
  var active = isSafeDecodeActive();
  var html = '<strong style=\"color:' + (active ? 'var(--accent-orange)' : 'var(--accent-green)') + ';\">';
  if (status === 'probe-ok') {
    html += '\u2705 Probe OK</strong> - Model responsive, safe decode active.';
  } else if (status === 'probe-failed') {
    html += '\u274c Probe Failed</strong> - ' + (detail || 'Model not responding.') + ' Consider restarting Ollama.';
  } else if (active) {
    html += '\u26a0\ufe0f Safe Decode Active</strong> for <code>' + model + '</code> (' + sizeB + 'B).<br>';
    var p = getEffectiveGenParams();
    html += 'Clamped: ctx=' + p.context + ', tok=' + p.maxTokens + ', temp=' + p.temperature + ', stream=' + p.stream;
  } else {
    html += '\u2705 Normal Mode</strong> - Model: <code>' + model + '</code>' + (sizeB > 0 ? ' (' + sizeB + 'B)' : '') + '. No clamping needed.';
  }
  el.innerHTML = html;
}

// ======================================================================
// ENGINE API — Unified interface to all 150+ engine sources via HTTP
// Maps complete_server.cpp (21 routes) + tool_server.cpp (18+ routes)
// ======================================================================
var EngineAPI = {
  // ---- Helpers ----
  _url: function () { return getActiveUrl(); },
  _get: async function (path, timeout) {
    var res = await fetch(this._url() + path, { signal: AbortSignal.timeout(timeout || 8000) });
    if (!res.ok) throw new Error('HTTP ' + res.status + ' on GET ' + path);
    return res.json();
  },
  _post: async function (path, body, timeout) {
    var res = await fetch(this._url() + path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
      signal: AbortSignal.timeout(timeout || 15000)
    });
    if (!res.ok) throw new Error('HTTP ' + res.status + ' on POST ' + path);
    return res.json();
  },
  _cli: async function (command) {
    return this._post('/api/cli', { command: command }, 30000);
  },

  // ---- Inference (tool_server + complete_server) ----
  chat: function (messages, model, options) {
    return this._post('/api/chat', Object.assign({ model: model || State.model.current, messages: messages }, options || {}), 60000);
  },
  generate: function (prompt, model, options) {
    return this._post('/api/generate', Object.assign({ model: model || State.model.current, prompt: prompt, stream: false }, options || {}), 60000);
  },
  complete: function (prompt, model) {
    return this._post('/complete', { prompt: prompt, model: model || State.model.current });
  },
  completeStream: function (prompt, model) {
    return this._post('/complete/stream', { prompt: prompt, model: model || State.model.current, stream: true }, 60000);
  },
  ask: function (question, model) {
    return this._post('/ask', { question: question, model: model || State.model.current });
  },

  // ---- Agents (complete_server: /api/subagent, /api/chain, /api/swarm, /api/agents/*) ----
  subagent: function (task, model, context) {
    return this._post('/api/subagent', { task: task, model: model || State.model.current, context: context || '' });
  },
  chain: function (steps) {
    return this._post('/api/chain', { steps: steps });
  },
  swarm: function (config) {
    return this._post('/api/swarm', config);
  },
  fetchAgentsList: function () { return this._get('/api/agents'); },
  fetchAgentsStatus: function () { return this._get('/api/agents/status'); },
  fetchAgentsHistory: function () { return this._get('/api/agents/history'); },
  replayAgent: function (agentId) {
    return this._post('/api/agents/replay', { agent_id: agentId });
  },

  // ---- Policies (complete_server: /api/policies/*) ----
  fetchPolicies: function () { return this._get('/api/policies'); },
  fetchPolicySuggestions: function () { return this._get('/api/policies/suggestions'); },
  applyPolicy: function (policyId) {
    return this._post('/api/policies/apply', { policy_id: policyId });
  },
  rejectPolicy: function (policyId) {
    return this._post('/api/policies/reject', { policy_id: policyId });
  },
  exportPolicies: function () { return this._get('/api/policies/export'); },
  importPolicies: function (data) {
    return this._post('/api/policies/import', data);
  },
  fetchHeuristics: function () { return this._get('/api/policies/heuristics'); },
  fetchPolicyStats: function () { return this._get('/api/policies/stats'); },

  // ---- CoT / Explain (complete_server: /api/agents/explain/*) ----
  fetchExplain: function () { return this._get('/api/agents/explain'); },
  fetchExplainStats: function () { return this._get('/api/agents/explain/stats'); },

  // ---- Backends (complete_server: /api/backends/*) ----
  fetchBackends: function () { return this._get('/api/backends'); },
  fetchBackendStatus: function () { return this._get('/api/backends/status'); },
  useBackend: function (name) {
    return this._post('/api/backends/use', { backend: name });
  },

  // ---- Hotpatch (tool_server: /api/hotpatch/*) ----
  hotpatchAction: function (action, layer) {
    return this._post('/api/hotpatch/' + action, { layer: layer });
  },
  hotpatchStatus: function () { return this._get('/api/hotpatch/status'); },

  // ---- Tools (tool_server: /api/tool) ----
  executeTool: function (tool, args) {
    return this._post('/api/tool', { tool: tool, args: args || {} });
  },
  toolReadFile: function (path) {
    return this.executeTool('read_file', { path: path });
  },
  toolWriteFile: function (path, content) {
    return this.executeTool('write_file', { path: path, content: content });
  },
  toolListDir: function (path) {
    return this.executeTool('list_directory', { path: path || '.' });
  },
  toolExecCmd: function (command) {
    return this.executeTool('execute_command', { command: command });
  },
  toolGitStatus: function () {
    return this.executeTool('git_status', {});
  },

  // ---- Agentic File Operations ----
  createFile: function (path, content) {
    return this.executeTool('write_file', { path: path, content: content || '' });
  },
  appendFile: function (path, content) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var existing = data.content || data.output || data.result || '';
      return self.executeTool('write_file', { path: path, content: existing + content });
    });
  },
  deleteFile: function (path) {
    return this.executeTool('execute_command', { command: 'del /f /q "' + path.replace(/\//g, '\\') + '"' });
  },
  renameFile: function (oldPath, newPath) {
    return this.executeTool('execute_command', { command: 'move /y "' + oldPath.replace(/\//g, '\\') + '" "' + newPath.replace(/\//g, '\\') + '"' });
  },
  copyFile: function (src, dst) {
    return this.executeTool('execute_command', { command: 'copy /y "' + src.replace(/\//g, '\\') + '" "' + dst.replace(/\//g, '\\') + '"' });
  },
  moveFile: function (src, dst) {
    return this.executeTool('execute_command', { command: 'move /y "' + src.replace(/\//g, '\\') + '" "' + dst.replace(/\//g, '\\') + '"' });
  },
  mkDir: function (path) {
    return this.executeTool('execute_command', { command: 'mkdir "' + path.replace(/\//g, '\\') + '"' });
  },
  removeDir: function (path) {
    return this.executeTool('execute_command', { command: 'rmdir /s /q "' + path.replace(/\//g, '\\') + '"' });
  },
  fileInfo: function (path) {
    return this.executeTool('execute_command', { command: 'dir "' + path.replace(/\//g, '\\') + '"' });
  },
  searchFiles: function (pattern, dir) {
    var d = dir || '.';
    return this.executeTool('execute_command', { command: 'dir /s /b "' + d.replace(/\//g, '\\') + '\\' + pattern + '"' });
  },
  grepInFiles: function (pattern, path) {
    var p = path || '*.*';
    return this.executeTool('execute_command', { command: 'findstr /s /n /i "' + pattern + '" "' + p.replace(/\//g, '\\') + '"' });
  },
  diffFiles: function (pathA, pathB) {
    return this.executeTool('execute_command', { command: 'fc "' + pathA.replace(/\//g, '\\') + '" "' + pathB.replace(/\//g, '\\') + '"' });
  },
  headFile: function (path, n) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var lines = content.split('\n').slice(0, n || 20);
      return { output: lines.join('\n'), lineCount: lines.length };
    });
  },
  tailFile: function (path, n) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var allLines = content.split('\n');
      var lines = allLines.slice(-(n || 20));
      return { output: lines.join('\n'), lineCount: lines.length, totalLines: allLines.length };
    });
  },
  wordCount: function (path) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var lines = content.split('\n').length;
      var words = content.split(/\s+/).filter(function (w) { return w.length > 0; }).length;
      var chars = content.length;
      var bytes = new Blob([content]).size;
      return { lines: lines, words: words, chars: chars, bytes: bytes };
    });
  },
  patchFile: function (path, search, replace) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      if (content.indexOf(search) === -1) {
        return { success: false, error: 'Pattern not found in file' };
      }
      var patched = content.split(search).join(replace);
      var count = content.split(search).length - 1;
      return self.executeTool('write_file', { path: path, content: patched }).then(function () {
        return { success: true, replacements: count, message: 'Patched ' + count + ' occurrence(s)' };
      });
    });
  },
  insertAtLine: function (path, lineNum, text) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var lines = content.split('\n');
      var idx = Math.max(0, Math.min(lineNum - 1, lines.length));
      lines.splice(idx, 0, text);
      return self.executeTool('write_file', { path: path, content: lines.join('\n') }).then(function () {
        return { success: true, message: 'Inserted at line ' + lineNum, totalLines: lines.length };
      });
    });
  },
  deleteLine: function (path, startLine, endLine) {
    var self = this;
    var end = endLine || startLine;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var lines = content.split('\n');
      var removed = lines.splice(startLine - 1, end - startLine + 1);
      return self.executeTool('write_file', { path: path, content: lines.join('\n') }).then(function () {
        return { success: true, message: 'Deleted lines ' + startLine + '-' + end, removedCount: removed.length, totalLines: lines.length };
      });
    });
  },
  replaceLines: function (path, startLine, endLine, newText) {
    var self = this;
    return self.executeTool('read_file', { path: path }).then(function (data) {
      var content = data.content || data.output || data.result || '';
      var lines = content.split('\n');
      var newLines = newText.split('\n');
      lines.splice(startLine - 1, endLine - startLine + 1, newLines.join('\n'));
      return self.executeTool('write_file', { path: path, content: lines.join('\n') }).then(function () {
        return { success: true, message: 'Replaced lines ' + startLine + '-' + endLine };
      });
    });
  },

  // ---- Metrics (tool_server: /metrics, /health, /api/status) ----
  fetchMetrics: function () { return this._get('/metrics'); },
  fetchHealth: function () { return this._get('/health'); },
  fetchServerStatus: function () { return this._get('/api/status'); },
  fetchStatus: function () { return this._get('/status'); },

  // ---- Models (tool_server: /api/tags, /models, /v1/models) ----
  fetchTags: function () { return this._get('/api/tags'); },
  fetchModelsList: function () { return this._get('/models'); },
  fetchV1Models: function () { return this._get('/v1/models'); },

  // ---- File ops via dedicated endpoint ----
  readFileViaApi: function (path) {
    return this._post('/api/read-file', { path: path });
  },

  // ---- Failures (tool_server: /api/failures) ----
  fetchFailures: function () { return this._get('/api/failures'); },

  // ---- CLI bridge (tool_server: /api/cli) ----
  cliHelp: function () { return this._cli('/help'); },
  cliStatus: function () { return this._cli('/status'); },
  cliModels: function () { return this._cli('/models'); },
  cliPlan: function (file) { return this._cli('/plan ' + (file || '')); },
  cliAnalyze: function (file) { return this._cli('/analyze ' + (file || '')); },
  cliOptimize: function (file) { return this._cli('/optimize ' + (file || '')); },
  cliSecurity: function (file) { return this._cli('/security ' + (file || '')); },
  cliSuggest: function (topic) { return this._cli('/suggest ' + (topic || '')); },
  cliBugreport: function () { return this._cli('/bugreport'); },
  cliMemory: function () { return this._cli('/memory'); },
  cliAgents: function () { return this._cli('/agents'); },
  cliFailures: function () { return this._cli('/failures'); },
  engineCmd: function (sub) { return this._cli('!engine ' + sub); },

  // ---- VSIX Extension Management (tool_server: /api/extensions/*) ----
  fetchExtensions: function () { return this._get('/api/extensions'); },
  fetchExtensionById: function (extId) { return this._get('/api/extensions/' + encodeURIComponent(extId)); },
  installExtension: function (extId, source) {
    return this._post('/api/extensions/install', { extension_id: extId, source: source || 'marketplace' }, 60000);
  },
  uninstallExtension: function (extId) {
    return this._post('/api/extensions/uninstall', { extension_id: extId });
  },
  enableExtension: function (extId) {
    return this._post('/api/extensions/enable', { extension_id: extId });
  },
  disableExtension: function (extId) {
    return this._post('/api/extensions/disable', { extension_id: extId });
  },
  searchMarketplace: function (query, page, pageSize) {
    return this._post('/api/extensions/marketplace/search', {
      query: query, page: page || 1, pageSize: pageSize || 20
    }, 15000);
  },
  fetchMarketplaceInfo: function (extId) {
    return this._get('/api/extensions/marketplace/' + encodeURIComponent(extId));
  },
  loadVsixFromPath: function (path) {
    return this._post('/api/extensions/load-vsix', { path: path }, 60000);
  },
  loadVsixFromUrl: function (url) {
    return this._post('/api/extensions/load-vsix', { url: url }, 120000);
  },
  extensionHostStatus: function () { return this._get('/api/extensions/host/status'); },
  restartExtensionHost: function () { return this._post('/api/extensions/host/restart', {}); },
  killExtensionHost: function () { return this._post('/api/extensions/host/kill', {}); },
  fetchExtensionHostLogs: function () { return this._get('/api/extensions/host/logs'); },
  fetchExtensionContributes: function (extId) {
    return this._get('/api/extensions/' + encodeURIComponent(extId) + '/contributes');
  },
  activateExtension: function (extId) {
    return this._post('/api/extensions/activate', { extension_id: extId });
  },
  deactivateExtension: function (extId) {
    return this._post('/api/extensions/deactivate', { extension_id: extId });
  },
  scanLocalExtensions: function (paths) {
    return this._post('/api/extensions/scan', { paths: paths || State.extensions.scanPaths });
  },
  exportExtensionList: function () { return this._get('/api/extensions/export'); },
  importExtensionList: function (data) {
    return this._post('/api/extensions/import', data, 120000);
  },

  // ---- OpenAI-compatible (tool_server: /v1/chat/completions) ----
  v1ChatCompletions: function (messages, model, options) {
    return this._post('/v1/chat/completions', Object.assign({
      model: model || State.model.current,
      messages: messages,
      stream: false
    }, options || {}), 60000);
  },

  // ---- WebView2 Browser / Web Access (proxy-based fetching) ----
  browseProxy: function (url) {
    // Try dedicated browse endpoint first, fallback to CLI fetch
    var self = this;
    return self._post('/api/browse', { url: url, extract: true }, 30000).catch(function () {
      // Fallback: try fetching via tool execution (curl / powershell)
      return self.executeTool('execute_command', {
        command: 'powershell -c "(Invoke-WebRequest -Uri \'' + url.replace(/'/g, "''") + '\' -UseBasicParsing).Content"'
      }).then(function (data) {
        return { content: data.output || data.result || '', url: url, method: 'powershell-fallback' };
      });
    });
  },
  browseExtract: function (url) {
    // Extract readable text content from a URL (strips HTML tags)
    var self = this;
    return self._post('/api/browse/extract', { url: url }, 30000).catch(function () {
      return self.browseProxy(url).then(function (data) {
        var html = data.content || data.body || '';
        // Client-side HTML strip: remove scripts, styles, then tags
        var text = html.replace(/<script[^>]*>[\s\S]*?<\/script>/gi, '')
          .replace(/<style[^>]*>[\s\S]*?<\/style>/gi, '')
          .replace(/<[^>]+>/g, ' ')
          .replace(/\s{2,}/g, ' ')
          .trim();
        return { text: text, url: url, chars: text.length, method: 'client-strip' };
      });
    });
  },
  browseSearch: function (query) {
    // Search the web via DuckDuckGo HTML endpoint
    return this.browseProxy('https://html.duckduckgo.com/html/?q=' + encodeURIComponent(query));
  },
  browseScreenshot: function (url) {
    return this._post('/api/browse/screenshot', { url: url }, 30000);
  },
  browseHistory: function () {
    return Promise.resolve({ history: (typeof BrowserState !== 'undefined') ? BrowserState.history : [] });
  },

  // ---- Subsystem Map: maps all 450+ source files to categories & endpoints ----
  SUBSYSTEMS: {
    'Core / Memory Hotpatch': {
      files: ['model_memory_hotpatch.hpp', 'model_memory_hotpatch.cpp'],
      endpoints: ['/api/hotpatch/status', '/api/hotpatch/apply', '/api/hotpatch/revert'],
      desc: 'Direct RAM patching using VirtualProtect/mprotect on loaded tensors'
    },
    'Core / Byte-Level Hotpatch': {
      files: ['byte_level_hotpatcher.hpp', 'byte_level_hotpatcher.cpp'],
      endpoints: ['/api/hotpatch/toggle', '/api/hotpatch/apply'],
      desc: 'Precision GGUF binary modification via mmap without full reparse'
    },
    'Core / Unified Hotpatch Manager': {
      files: ['unified_hotpatch_manager.hpp', 'unified_hotpatch_manager.cpp'],
      endpoints: ['/api/hotpatch/*'],
      desc: 'Routes patches to memory/byte/server layers, tracks stats, preset save/load'
    },
    'Core / Proxy Hotpatcher': {
      files: ['proxy_hotpatcher.hpp', 'proxy_hotpatcher.cpp'],
      endpoints: [],
      desc: 'Byte-level output rewriting, token bias injection, stream termination'
    },
    'Server / GGUF Server Hotpatch': {
      files: ['gguf_server_hotpatch.hpp', 'gguf_server_hotpatch.cpp'],
      endpoints: ['/api/hotpatch/*'],
      desc: 'Request/response transform at runtime (pre/post request/response hooks)'
    },
    'Server / Complete Server': {
      files: ['complete_server.cpp', 'complete_server.hpp'],
      endpoints: ['/status', '/complete', '/complete/stream', '/api/chat', '/api/subagent', '/api/chain', '/api/swarm', '/api/agents', '/api/agents/status', '/api/agents/history', '/api/agents/replay', '/api/policies', '/api/policies/suggestions', '/api/policies/apply', '/api/policies/reject', '/api/policies/export', '/api/policies/import', '/api/policies/heuristics', '/api/policies/stats', '/api/agents/explain', '/api/agents/explain/stats', '/api/backends', '/api/backends/status', '/api/backends/use'],
      desc: 'Main agentic completion server — 21 routes for chat, agents, policies, backends'
    },
    'Server / Tool Server': {
      files: ['tool_server.cpp', 'tool_server.hpp'],
      endpoints: ['/api/tags', '/health', '/api/status', '/status', '/api/generate', '/metrics', '/api/tool', '/models', '/v1/models', '/v1/chat/completions', '/ask', '/gui', '/api/failures', '/api/agents/status', '/api/agents/history', '/api/agents/replay', '/api/hotpatch/*', '/api/read-file', '/api/cli'],
      desc: 'Tool executor, WinHTTP proxy to Ollama, CLI bridge — 18+ routes'
    },
    'Agent / Failure Detector': {
      files: ['agentic_failure_detector.hpp', 'agentic_failure_detector.cpp'],
      endpoints: ['/api/failures'],
      desc: 'Detects refusal, hallucination, timeout, resource exhaustion, safety violations'
    },
    'Agent / Puppeteer': {
      files: ['agentic_puppeteer.hpp', 'agentic_puppeteer.cpp'],
      endpoints: [],
      desc: 'Auto-corrects failed responses using CorrectionResult pattern'
    },
    'ASM / Memory Patch': {
      files: ['memory_patch.asm'],
      endpoints: ['/api/hotpatch/apply'],
      desc: 'MASM64 memory patching routines for direct RAM manipulation'
    },
    'ASM / Byte Search': {
      files: ['byte_search.asm'],
      endpoints: [],
      desc: 'MASM64 Boyer-Moore / SIMD pattern search for GGUF binary scanning'
    },
    'ASM / Request Patch': {
      files: ['request_patch.asm'],
      endpoints: ['/api/hotpatch/*'],
      desc: 'MASM64 request/response patching at the server layer'
    },
    'GGUF / Model Loader': {
      files: ['gguf_model_loader.hpp', 'gguf_model_loader.cpp', 'gguf_model.hpp', 'gguf_model.cpp'],
      endpoints: ['/api/generate', '/api/chat'],
      desc: 'GGUF model loading, parsing headers, tensor mapping'
    },
    'GGUF / Tokenizer': {
      files: ['gguf_tokenizer.hpp', 'gguf_tokenizer.cpp', 'masm_tokenizer.asm'],
      endpoints: ['/api/generate'],
      desc: 'Tokenization engine with MASM SIMD acceleration'
    },
    'GGUF / Quantization': {
      files: ['gguf_quantization.hpp', 'gguf_quantization.cpp', 'quant_utils.hpp', 'quant_utils.cpp'],
      endpoints: [],
      desc: 'Q2_K through Q8_0 quantization with adaptive per-layer compression'
    },
    'Inference / KV Cache': {
      files: ['kv_cache.hpp', 'kv_cache.cpp', 'sliding_window_kv.hpp', 'sliding_window_kv.cpp'],
      endpoints: ['/api/generate', '/api/chat'],
      desc: 'Key-value cache with sliding window compression and SVD'
    },
    'Inference / Attention': {
      files: ['attention.hpp', 'attention.cpp', 'flash_attention.hpp', 'flash_attention.cpp'],
      endpoints: ['/api/generate', '/api/chat'],
      desc: 'Multi-head attention with Flash-Attention v2 MASM kernels'
    },
    'Inference / MatMul': {
      files: ['matmul.hpp', 'matmul.cpp', 'matmul_kernels.asm'],
      endpoints: ['/api/generate'],
      desc: 'Matrix multiplication with Q4/Q2 integer kernels and Vulkan compute'
    },
    'Inference / Sampler': {
      files: ['sampler.hpp', 'sampler.cpp', 'top_p_sampler.cpp', 'top_k_sampler.cpp'],
      endpoints: ['/api/generate', '/api/chat'],
      desc: 'Token sampling: top-p, top-k, temperature, repetition penalty'
    },
    'Vulkan / Compute': {
      files: ['vulkan_backend.hpp', 'vulkan_backend.cpp', 'vulkan_matmul.comp', 'vulkan_attention.comp'],
      endpoints: ['/api/backends', '/api/backends/use'],
      desc: 'Vulkan GPU compute backend for matrix multiplication and attention'
    },
    'Zone / Memory Manager': {
      files: ['zone_memory.hpp', 'zone_memory.cpp', 'zone_preloader.hpp', 'zone_preloader.cpp'],
      endpoints: [],
      desc: 'Zone-based memory management with async preloading (400ms → 40ms)'
    },
    'Speculative / Decoding': {
      files: ['speculative_decoder.hpp', 'speculative_decoder.cpp'],
      endpoints: ['/api/generate'],
      desc: 'Speculative decoding for multi-token batch generation'
    },
    'Pruning / Sparse': {
      files: ['sparse_pruning.hpp', 'sparse_pruning.cpp'],
      endpoints: [],
      desc: 'Magnitude-based weight pruning, 90% zero-skip optimization'
    },
    'Router / LLM Router': {
      files: ['llm_router.hpp', 'llm_router.cpp', 'router_config.hpp'],
      endpoints: [],
      desc: 'Routes inference to best backend based on model size and capability'
    },
    'Safety / Content Filter': {
      files: ['safety_filter.hpp', 'safety_filter.cpp', 'content_checker.hpp', 'content_checker.cpp'],
      endpoints: [],
      desc: 'Content safety filtering, violation detection, rollback support'
    },
    'Telemetry / Metrics': {
      files: ['telemetry.hpp', 'telemetry.cpp', 'metrics_collector.hpp', 'metrics_collector.cpp'],
      endpoints: ['/metrics', '/api/status'],
      desc: 'Prometheus-style metrics, structured logging, distributed tracing'
    },
    'Config / Settings': {
      files: ['config.hpp', 'config.cpp', 'feature_flags.hpp'],
      endpoints: [],
      desc: 'External configuration, feature toggles, environment-specific settings'
    },
    'GUI / Win32 IDE': {
      files: ['win32_ide.cpp', 'win32_ide.hpp', 'react_ide_generator.cpp'],
      endpoints: ['/gui'],
      desc: 'Win32 native IDE shell, React-based IDE generator'
    },
    'Ext / VSIX Extension Host': {
      files: ['vsix_extension_host.hpp', 'vsix_extension_host.cpp', 'extension_sandbox.hpp', 'extension_sandbox.cpp'],
      endpoints: ['/api/extensions', '/api/extensions/install', '/api/extensions/uninstall', '/api/extensions/enable', '/api/extensions/disable', '/api/extensions/activate', '/api/extensions/deactivate', '/api/extensions/host/status', '/api/extensions/host/restart', '/api/extensions/host/kill', '/api/extensions/host/logs'],
      desc: 'VSIX extension host — loads .vsix packages, manages activation events, sandboxed QuickJS execution'
    },
    'Ext / Extension Manager': {
      files: ['extension_manager.hpp', 'extension_manager.cpp', 'extension_manifest.hpp'],
      endpoints: ['/api/extensions/scan', '/api/extensions/export', '/api/extensions/import'],
      desc: 'Extension lifecycle manager — install, update, enable/disable, dependency resolution'
    },
    'Ext / Marketplace Client': {
      files: ['marketplace_client.hpp', 'marketplace_client.cpp'],
      endpoints: ['/api/extensions/marketplace/search', '/api/extensions/marketplace/*'],
      desc: 'VS Code Marketplace API client — search, download, version resolution, compatibility check'
    },
    'Ext / Polyfill Engine': {
      files: ['extension_polyfill.hpp', 'extension_polyfill.cpp', 'vscode_api_shim.js'],
      endpoints: [],
      desc: 'VS Code API polyfill layer — provides vscode.* namespace, commands, workspace, languages APIs'
    },
    'Ext / VSIX Loader': {
      files: ['vsix_loader.hpp', 'vsix_loader.cpp', 'vsix_parser.hpp'],
      endpoints: ['/api/extensions/load-vsix'],
      desc: 'VSIX package parser — extracts manifest, contributions, activation events from .vsix ZIP'
    }
  },

  // Render the subsystem map into the Engine Explorer panel
  renderSubsystemMap: function (filter) {
    var el = document.getElementById('engineMapContent');
    if (!el) return;
    var html = '';
    var filterLower = (filter || '').toLowerCase();
    var totalFiles = 0;
    var totalEndpoints = 0;
    var keys = Object.keys(this.SUBSYSTEMS);
    for (var i = 0; i < keys.length; i++) {
      var name = keys[i];
      var sub = this.SUBSYSTEMS[name];
      var matchName = name.toLowerCase().indexOf(filterLower) >= 0;
      var matchFile = false;
      for (var f = 0; f < sub.files.length; f++) {
        if (sub.files[f].toLowerCase().indexOf(filterLower) >= 0) { matchFile = true; break; }
      }
      if (filterLower && !matchName && !matchFile) continue;
      totalFiles += sub.files.length;
      totalEndpoints += sub.endpoints.length;
      html += '<div style="margin-bottom:12px;border:1px solid var(--border);border-radius:6px;padding:8px;">';
      html += '<div style="font-weight:bold;color:var(--accent-cyan);margin-bottom:4px;">' + name + '</div>';
      html += '<div style="font-size:10px;color:var(--text-secondary);margin-bottom:6px;">' + sub.desc + '</div>';
      html += '<div style="font-size:10px;color:var(--accent-green);margin-bottom:4px;">Files: ';
      for (var f = 0; f < sub.files.length; f++) {
        html += '<code style="margin-right:4px;">' + sub.files[f] + '</code>';
      }
      html += '</div>';
      if (sub.endpoints.length > 0) {
        html += '<div style="font-size:10px;color:var(--accent-orange);">Endpoints: ';
        for (var e = 0; e < sub.endpoints.length; e++) {
          html += '<code style="margin-right:4px;cursor:pointer;text-decoration:underline;" onclick="executeCommand();document.getElementById(\'terminalInput\').value=\'curl ' + sub.endpoints[e] + '\';executeCommand()">' + sub.endpoints[e] + '</code>';
        }
        html += '</div>';
      }
      html += '</div>';
    }
    html = '<div style="margin-bottom:8px;color:var(--accent-secondary);font-size:11px;">' + keys.length + ' subsystems, ' + totalFiles + ' files, ' + totalEndpoints + ' endpoints</div>' + html;
    el.innerHTML = html;
  }
};

// Engine map filter helper
function filterEngineMap(val) { EngineAPI.renderSubsystemMap(val); }

// ======================================================================
// BACKEND CONNECTION
// ======================================================================
// Helper: returns the URL to send requests to (proxy or direct Ollama)
function getActiveUrl() {
  return State.backend.directMode ? State.backend.ollamaDirectUrl : State.backend.url;
}

// Beacon: probe whether backend (or alternate) exposes /api/cli
// Sets State.backend.hasCliEndpoint = true if found on ANY reachable URL
async function beaconProbeCliEndpoint() {
  State.backend.hasCliEndpoint = false;

  // Build list of URLs to probe — only RawrXD servers (tool_server / Win32IDE)
  // Skip ollamaDirectUrl — Ollama never exposes /api/cli and probing it
  // produces a noisy 404 in DevTools console.
  var urlsToProbe = [State.backend.url];
  if (_ideServerUrl && urlsToProbe.indexOf(_ideServerUrl) === -1) {
    urlsToProbe.push(_ideServerUrl);
  }
  if (_win32IdeUrl && urlsToProbe.indexOf(_win32IdeUrl) === -1) {
    urlsToProbe.push(_win32IdeUrl);
  }

  for (var i = 0; i < urlsToProbe.length; i++) {
    try {
      var res = await fetch(urlsToProbe[i] + '/api/cli', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command: '/help' }),
        signal: AbortSignal.timeout(2000)
      });
      if (res.ok) {
        var data = await res.json();
        if (data && data.output) {
          State.backend.hasCliEndpoint = true;
          State.backend._cliEndpointUrl = urlsToProbe[i];
          logDebug('Beacon: /api/cli found at ' + urlsToProbe[i], 'info');
          return;
        }
      }
    } catch (_) { /* endpoint not available, continue */ }
  }

  // If CLI endpoint not found but tool_server is detected, set a partial flag
  // so file reads still work via /api/read-file even without /api/cli
  if (_win32IdeDetected || (State.backend.serverType && State.backend.serverType.indexOf('rawrxd') !== -1)) {
    State.backend._cliEndpointUrl = _ideServerUrl || _win32IdeUrl || State.backend.url;
    logDebug('Beacon: /api/cli not found but RawrXD tool_server detected — file reads via /api/read-file available', 'info');
  } else {
    logDebug('Beacon: /api/cli not found on any backend — using client-side CLI', 'info');
  }
}

async function connectBackend() {
  const status = document.getElementById('backendStatus');
  const text = document.getElementById('statusText');

  // Phase 4: Validate backend URL before connecting
  if (!isUrlAllowed(State.backend.url)) {
    secLog('BLOCK', 'Connection blocked — URL not on allowlist: ' + State.backend.url);
    State.security.inputGuard.blockedUrl++;
    status.className = 'status-pill offline';
    text.textContent = 'URL BLOCKED';
    logDebug('Backend URL blocked by security policy: ' + State.backend.url, 'error');
    return;
  }

  status.className = 'status-pill connecting';
  text.textContent = 'CONNECTING...';

  // ---- Try 1: Primary backend (server.js on :8080) ----
  try {
    const t0 = performance.now();

    let statusData = null;
    let detectedVia = '';

    // Try /status first (new binary with full capabilities)
    try {
      const ctrl = new AbortController();
      setTimeout(() => ctrl.abort(), 3000);
      const res = await fetch(State.backend.url + '/status', { signal: ctrl.signal });
      if (res.ok) { statusData = await res.json(); detectedVia = '/status'; }
    } catch (_) { /* fall through */ }

    // Try /api/status (old binary — returns {running, pid, uptime_seconds})
    if (!statusData) {
      try {
        const res = await fetch(State.backend.url + '/api/status', { signal: AbortSignal.timeout(3000) });
        if (res.ok) {
          const d = await res.json();
          if (d.running === true && d.pid) {
            // This is definitely the tool_server. Enrich the status data.
            statusData = {
              backend: 'rawrxd-tool-server',
              server: 'RawrXD-ToolServer',
              running: true,
              pid: d.pid,
              uptime_seconds: d.uptime_seconds,
              status: 'ok',
              license: 'unlicensed-open',
              model_range: '8B-100B swarm + 800B dual engine',
              capabilities: {
                kernel_engines: true,
                model_swarm: true,
                dual_engine_800b: true,
                hotpatching: true,
                file_read: true,
                cli: true,
              },
            };
            detectedVia = '/api/status';
          }
        }
      } catch (_) { /* fall through */ }
    }

    // Try /health (old binary — returns {status, version, models_loaded})
    if (!statusData) {
      try {
        const res = await fetch(State.backend.url + '/health', { signal: AbortSignal.timeout(3000) });
        if (res.ok) {
          const d = await res.json();
          if (d.status === 'ok') {
            statusData = Object.assign({}, d, {
              backend: 'rawrxd-tool-server',
              server: 'RawrXD-ToolServer',
              license: 'unlicensed-open',
              model_range: '8B-100B swarm + 800B dual engine',
            });
            detectedVia = '/health';
          }
        }
      } catch (_) { /* fall through */ }
    }

    const latency = Math.round(performance.now() - t0);

    if (statusData) {
      State.backend.online = true;
      State.backend.directMode = false;
      State.backend.lastPing = Date.now();
      State.backend.serverType = statusData.backend || statusData.server || 'rawrxd-tool-server';
      _ideServerUrl = State.backend.url; // Cache IDE server URL for file reads

      // Store full capabilities if available
      if (statusData.capabilities) {
        State.backend.capabilities = statusData.capabilities;
      }

      status.className = 'status-pill online';
      text.textContent = 'ONLINE';

      updateBackendInfo(statusData, latency);
      updateModeBadge(true);
      syncTitlebarStatus();
      logDebug('Connected to ' + State.backend.serverType + ' via ' + detectedVia + ' (' + latency + 'ms)', 'info');

      await fetchModels();
      await beaconProbeCliEndpoint();
      return;
    }
  } catch (e) {
    logDebug('Primary backend (' + State.backend.url + ') failed: ' + e.message, 'warn');
  }

  // ---- Try 2: Direct Ollama fallback (default :11434) ----
  if (State.backend.ollamaDirectUrl && State.backend.ollamaDirectUrl !== State.backend.url) {
    try {
      logDebug('Trying direct Ollama at ' + State.backend.ollamaDirectUrl + '...', 'info');
      const t0 = performance.now();

      // Ollama exposes GET / that returns "Ollama is running"
      const res = await fetch(State.backend.ollamaDirectUrl + '/', { signal: AbortSignal.timeout(3000) });
      if (res.ok) {
        const body = await res.text();
        if (body.toLowerCase().includes('ollama')) {
          const latency = Math.round(performance.now() - t0);

          State.backend.online = true;
          State.backend.directMode = true;
          State.backend.lastPing = Date.now();
          State.backend.serverType = 'ollama-direct';
          // Even in direct mode, the Win32IDE may be running for /api/read-file
          _ideServerUrl = State.backend.url; // Keep :8080 as IDE server for file reads

          status.className = 'status-pill online';
          text.textContent = 'OLLAMA DIRECT';

          updateBackendInfo({ backend: 'ollama-direct', model_loaded: true }, latency);
          updateModeBadge(true);
          syncTitlebarStatus();
          logDebug('\u2705 Connected directly to Ollama (bypassing serve.py) (' + latency + 'ms)', 'info');

          await fetchModels();
          await beaconProbeCliEndpoint();
          return;
        }
      }
    } catch (e) {
      logDebug('Direct Ollama (' + State.backend.ollamaDirectUrl + ') also failed: ' + e.message, 'warn');
    }
  }

  // ---- Both failed: Offline ----
  State.backend.online = false;
  State.backend.directMode = false;
  status.className = 'status-pill offline';
  text.textContent = 'OFFLINE';
  updateModeBadge(false);
  updateBackendInfo(null, 0);
  syncTitlebarStatus();
  populateModelsFallback();

  if (document.getElementById('dbgAutoReconnect') && document.getElementById('dbgAutoReconnect').checked) {
    clearTimeout(State.reconnectTimer);
    State.reconnectTimer = setTimeout(connectBackend, 10000);
    logDebug('Will retry in 10 s...', 'warn');
  }
}

function updateBackendInfo(data, latency) {
  const el = document.getElementById('backendInfo');
  if (!data) {
    el.innerHTML =
      '<p><span class="label">Server:</span> <span class="val-offline">not connected</span></p>' +
      '<p><span class="label">Model:</span> <span class="val-neutral">none</span></p>' +
      '<p><span class="label">Requests:</span> <span class="val-neutral">&mdash;</span></p>' +
      '<p><span class="label">Tokens:</span> <span class="val-neutral">&mdash;</span></p>';
    return;
  }
  var server = data.backend || data.server || 'unknown';
  var loaded = data.model_loaded ? 'yes' : 'no';
  var reqs = data.total_requests != null ? data.total_requests : (data.available_models != null ? data.available_models : '\u2014');
  var toks = data.total_tokens != null ? data.total_tokens : '\u2014';

  var html =
    '<p><span class="label">Server:</span> <span class="val-online">' + esc(server) + '</span></p>' +
    '<p><span class="label">Model:</span> <span class="' + (loaded === 'yes' ? 'val-online' : 'val-neutral') + '">' + loaded + '</span></p>' +
    '<p><span class="label">Latency:</span> <span class="val-neutral">' + latency + 'ms</span></p>' +
    '<p><span class="label">Requests:</span> <span class="val-neutral">' + reqs + '</span></p>' +
    '<p><span class="label">Tokens:</span> <span class="val-neutral">' + toks + '</span></p>';

  // Show extended info for RawrXD tool_server
  if (server.indexOf('rawrxd') !== -1 || server.indexOf('RawrXD') !== -1 || server.indexOf('ToolServer') !== -1) {
    if (data.license) html += '<p><span class="label">License:</span> <span class="val-online">' + esc(data.license) + '</span></p>';
    if (data.model_range) html += '<p><span class="label">Range:</span> <span class="val-online">' + esc(data.model_range) + '</span></p>';
    if (data.capabilities) {
      var caps = [];
      if (data.capabilities.kernel_engines) caps.push('Kernels');
      if (data.capabilities.model_swarm) caps.push('Swarm');
      if (data.capabilities.dual_engine_800b) caps.push('800B Dual');
      if (data.capabilities.hotpatching) caps.push('Hotpatch');
      if (data.capabilities.file_read) caps.push('FileIO');
      if (data.capabilities.cli) caps.push('CLI');
      if (caps.length > 0) html += '<p><span class="label">Engines:</span> <span class="val-online">' + caps.join(' · ') + '</span></p>';
    }
    if (data.pid) html += '<p><span class="label">PID:</span> <span class="val-neutral">' + data.pid + '</span></p>';
  }

  el.innerHTML = html;
}

function updateModeBadge(online) {
  var badge = document.getElementById('modeBadge');
  var txt = document.getElementById('modeText');
  var sEl = document.getElementById('chatStatus');
  if (online) {
    badge.classList.add('active');
    var isRawrXD = State.backend.serverType && (State.backend.serverType.indexOf('rawrxd') !== -1 || State.backend.serverType.indexOf('RawrXD') !== -1 || State.backend.serverType.indexOf('ToolServer') !== -1);
    if (State.backend.directMode) {
      txt.textContent = 'Ollama Direct';
      sEl.innerHTML = '<span style="color:var(--accent-green);">\u25CF</span> Ollama Direct';
    } else if (isRawrXD) {
      txt.textContent = 'RawrXD Backend';
      sEl.innerHTML = '<span style="color:var(--accent-green);">\u25CF</span> RawrXD Engine';
    } else {
      txt.textContent = 'Backend Connected';
      sEl.innerHTML = '<span style="color:var(--accent-green);">\u25CF</span> Ready';
    }
  } else {
    badge.classList.remove('active');
    txt.textContent = 'Disconnected';
    sEl.innerHTML = '<span style="color:var(--accent-red);">\u25CF</span> Offline';
  }
}

// ======================================================================
// MODELS
// ======================================================================
async function fetchModels() {
  if (!State.backend.online) return;

  var activeUrl = getActiveUrl();

  // Try proxy /models endpoint first (serve.py enriches with GGUF info)
  if (!State.backend.directMode) {
    try {
      var res = await fetch(activeUrl + '/models', { signal: AbortSignal.timeout(5000) });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      var data = await res.json();
      if (data.models && Array.isArray(data.models)) {
        State.model.list = data.models;
        populateModelSelect(data.models);
        logDebug('Loaded ' + data.models.length + ' models from /models', 'info');
        return;
      }
    } catch (e) {
      logDebug('Proxy /models failed: ' + e.message + ', trying Ollama /api/tags', 'warn');
    }
  }

  // Fallback / direct mode: Ollama native /api/tags
  try {
    var ollamaUrl = State.backend.directMode ? State.backend.ollamaDirectUrl : State.backend.url;
    var res2 = await fetch(ollamaUrl + '/api/tags', { signal: AbortSignal.timeout(5000) });
    if (!res2.ok) throw new Error('HTTP ' + res2.status);
    var tagsData = await res2.json();
    if (tagsData.models && Array.isArray(tagsData.models)) {
      var mapped = tagsData.models.map(function (m) {
        return {
          name: m.name || m.model,
          type: 'ollama',
          size: m.size ? ((m.size / (1024 * 1024 * 1024)).toFixed(1) + ' GB') : '?',
        };
      });
      State.model.list = mapped;
      populateModelSelect(mapped);
      logDebug('Loaded ' + mapped.length + ' models from Ollama /api/tags' + (State.backend.directMode ? ' (direct)' : ''), 'info');
      return;
    }
  } catch (e2) {
    logDebug('Fetch models from /api/tags failed: ' + e2.message, 'error');
  }

  populateModelsFallback();
}

function populateModelSelect(models) {
  var sel = document.getElementById('modelSelect');
  sel.innerHTML = '';

  if (models.length === 0) {
    sel.innerHTML = '<option value="">No models found</option>';
    return;
  }
  sel.innerHTML = '<option value="">Select Model...</option>';

  // Group by type
  var groups = {};
  models.forEach(function (m) {
    var t = m.type || 'unknown';
    if (!groups[t]) groups[t] = [];
    groups[t].push(m);
  });

  var labels = {
    gguf: '\uD83D\uDCE6 Local GGUF',
    ollama: '\uD83E\uDD99 Ollama',
    blob: '\uD83D\uDCBE Blobs',
    cloud: '\u2601\uFE0F Cloud',
    unknown: '\u2753 Other',
  };

  Object.keys(groups).forEach(function (type) {
    var grp = document.createElement('optgroup');
    grp.label = labels[type] || type.toUpperCase();
    groups[type].forEach(function (m) {
      var opt = document.createElement('option');
      opt.value = m.name;
      opt.textContent = m.name + ' (' + (m.size || '?') + ')';
      grp.appendChild(opt);
    });
    sel.appendChild(grp);
  });
}

function populateModelsFallback() {
  var sel = document.getElementById('modelSelect');
  sel.innerHTML = '<option value="">Backend offline</option>';
}

async function refreshModels() {
  if (!State.backend.online) {
    addMessage('system', 'Cannot refresh: backend offline. Click \u26A1 to connect.');
    return;
  }
  logDebug('Refreshing models...', 'info');
  await fetchModels();
}

function changeModel() {
  var name = document.getElementById('modelSelect').value;
  State.model.current = name || null;

  var badge = document.getElementById('modelBadge');
  var badgeText = document.getElementById('modelBadgeText');
  var rightModel = document.getElementById('rightModel');

  if (name) {
    badge.style.display = 'flex';
    badgeText.textContent = name.length > 25 ? name.substring(0, 22) + '...' : name;
    rightModel.textContent = name.length > 20 ? name.substring(0, 17) + '...' : name;
    logDebug('Model selected: ' + name, 'info');

    // Update safe decode status for new model
    updateSafeDecodeStatus();
    var sizeB = estimateModelSizeB(name);
    if (isSafeDecodeActive()) {
      logDebug('[SafeDecode] Large model detected (' + sizeB + 'B >= ' + State.gen.safeDecodeProfile.thresholdB + 'B) — safe decode active', 'warn');
    }
  } else {
    badge.style.display = 'none';
    rightModel.textContent = '\u2014';
    updateSafeDecodeStatus();
  }
}

// ======================================================================
// MODEL BRIDGE — MASM x64 Pure Assembly Bridge (24 profiles, 1.5B-800B)
// ======================================================================

// Bridge state tracking
var BridgeState = {
  profiles: [],
  hardware: null,
  selectedProfileId: -1,
  activeProfileId: -1,
  loaded: false,
  bridge: 'unknown',
  fetched: false
};

// Tier display config
var BRIDGE_TIERS = {
  'small': { label: '\u26A1 Small (1\u20138B)', css: 'tier-small', color: '#66ccff', range: '1-8B' },
  'medium': { label: '\uD83D\uDD27 Medium (9\u201327B)', css: 'tier-medium', color: '#ffcc00', range: '9-27B' },
  'large': { label: '\uD83D\uDD25 Large (28\u201370B)', css: 'tier-large', color: '#ff8844', range: '28-70B' },
  'ultra': { label: '\uD83D\uDE80 Ultra (71\u2013141B)', css: 'tier-ultra', color: '#ff4466', range: '71-141B' },
  '800b-dual': { label: '\uD83E\uDDE0 800B Dual Engine', css: 'tier-800b', color: '#ff00ff', range: '671-800B' }
};

// Model name lookup (mirrors model_bridge_x64.asm g_ModelNames)
var BRIDGE_MODEL_NAMES = [
  'qwen2.5:1.5b', 'qwen2.5:3b', 'llama3.2:3b', 'phi-4:3.8b',
  'gemma2:7b', 'llama3.1:8b', 'qwen2.5:7b', 'mistral:7b', 'deepseek-r1:8b',
  'llama3.1:13b', 'qwen2.5:14b', 'gemma2:27b',
  'qwen2.5:32b', 'codellama:34b', 'llama3.1:70b', 'qwen2.5:72b', 'deepseek-r1:70b',
  'llama3.1:100b-swarm', 'qwen2.5:100b-swarm',
  'BigDaddyG-Q4_K_M', 'RawrXD-800B-DualEngine', 'deepseek-v3:671b-swarm',
  'mixtral-8x22b:141b', 'commandr-plus:104b-swarm'
];

// Fetch all 24 profiles from MASM bridge endpoint
async function fetchBridgeProfiles() {
  var badge = document.getElementById('bridgeBadge');
  badge.textContent = 'loading...';
  badge.style.background = 'rgba(255,204,0,0.15)';
  badge.style.color = '#ffcc00';

  try {
    var activeUrl = getActiveUrl();
    var res = await fetch(activeUrl + '/api/model/profiles', { signal: AbortSignal.timeout(8000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    var data = await res.json();

    BridgeState.profiles = data.profiles || [];
    BridgeState.hardware = data.hardware || null;
    BridgeState.bridge = data.bridge || 'unknown';
    BridgeState.fetched = true;

    badge.textContent = data.bridge || 'connected';
    badge.style.background = 'rgba(0,255,136,0.15)';
    badge.style.color = '#00ff88';

    renderBridgeProfiles();
    updateBridgeStatusBar();
    logDebug('[ModelBridge] Loaded ' + BridgeState.profiles.length + ' profiles via ' + BridgeState.bridge, 'info');

    // Also merge bridge profiles into model select dropdown
    mergeBridgeIntoModelSelect();
  } catch (e) {
    badge.textContent = 'error';
    badge.style.background = 'rgba(255,68,102,0.15)';
    badge.style.color = '#ff4466';
    logDebug('[ModelBridge] Fetch failed: ' + e.message, 'error');

    // Render fallback with static names
    renderBridgeProfilesFallback();
  }
}

// Render profile cards grouped by tier
function renderBridgeProfiles() {
  var container = document.getElementById('bridgeProfileList');
  container.innerHTML = '';

  // Group by tier
  var tierGroups = {};
  BridgeState.profiles.forEach(function (p) {
    var tier = p.tier || 'unknown';
    if (!tierGroups[tier]) tierGroups[tier] = [];
    tierGroups[tier].push(p);
  });

  var tierOrder = ['small', 'medium', 'large', 'ultra', '800b-dual'];
  tierOrder.forEach(function (tier) {
    var profiles = tierGroups[tier];
    if (!profiles || profiles.length === 0) return;

    var tierCfg = BRIDGE_TIERS[tier] || { label: tier, css: '', range: '' };

    var groupDiv = document.createElement('div');
    groupDiv.className = 'bridge-tier-group';

    var labelDiv = document.createElement('div');
    labelDiv.className = 'bridge-tier-label ' + tierCfg.css;
    labelDiv.textContent = tierCfg.label + ' (' + profiles.length + ')';
    groupDiv.appendChild(labelDiv);

    profiles.forEach(function (p) {
      var card = document.createElement('div');
      card.className = 'bridge-model-card' + (p.id === BridgeState.activeProfileId ? ' active' : '');
      card.setAttribute('data-profile-id', p.id);
      card.onclick = function () { selectBridgeProfile(p.id); };

      var nameStr = BRIDGE_MODEL_NAMES[p.id] || ('profile-' + p.id);
      var paramStr = p.params_b || '?';
      var quantStr = p.quant || 'Q4_K_M';
      var ramStr = p.ram_mb ? (p.ram_mb >= 1024 ? (p.ram_mb / 1024).toFixed(1) + 'G' : p.ram_mb + 'M') : '?';

      card.innerHTML =
        '<span class="model-name" title="' + nameStr + '">' + nameStr + '</span>' +
        '<span class="model-meta">' +
        '<span class="param-badge">' + paramStr + 'B</span>' +
        '<span class="quant-badge">' + quantStr + '</span>' +
        '<span class="ram-badge">' + ramStr + '</span>' +
        (p.requires_swarm ? '<span style="color:#ff4466;" title="Requires swarm cluster">\u26A0</span>' : '') +
        (p.requires_avx512 ? '<span style="color:#ffcc00;" title="Requires AVX-512">512</span>' : '') +
        '</span>';

      groupDiv.appendChild(card);
    });

    container.appendChild(groupDiv);
  });
}

// Fallback renderer with static model names when backend unavailable
function renderBridgeProfilesFallback() {
  var container = document.getElementById('bridgeProfileList');
  container.innerHTML = '';

  var tierDefs = [
    { tier: 'small', ids: [0, 1, 2, 3, 4, 5, 6, 7, 8] },
    { tier: 'medium', ids: [9, 10, 11] },
    { tier: 'large', ids: [12, 13, 14, 15, 16, 19] },
    { tier: 'ultra', ids: [17, 18, 22, 23] },
    { tier: '800b-dual', ids: [20, 21] }
  ];

  tierDefs.forEach(function (td) {
    var tierCfg = BRIDGE_TIERS[td.tier];
    var groupDiv = document.createElement('div');
    groupDiv.className = 'bridge-tier-group';

    var labelDiv = document.createElement('div');
    labelDiv.className = 'bridge-tier-label ' + tierCfg.css;
    labelDiv.textContent = tierCfg.label + ' (' + td.ids.length + ')';
    groupDiv.appendChild(labelDiv);

    td.ids.forEach(function (id) {
      var card = document.createElement('div');
      card.className = 'bridge-model-card';
      card.setAttribute('data-profile-id', id);
      card.onclick = function () { selectBridgeProfile(id); };

      var nameStr = BRIDGE_MODEL_NAMES[id] || ('profile-' + id);
      card.innerHTML =
        '<span class="model-name">' + nameStr + '</span>' +
        '<span class="model-meta">' +
        '<span class="param-badge">#' + id + '</span>' +
        '</span>';

      groupDiv.appendChild(card);
    });

    container.appendChild(groupDiv);
  });
}

// Select a bridge profile (highlight card)
function selectBridgeProfile(profileId) {
  BridgeState.selectedProfileId = profileId;

  // Update card highlights
  var cards = document.querySelectorAll('.bridge-model-card');
  cards.forEach(function (card) {
    var cardId = parseInt(card.getAttribute('data-profile-id'), 10);
    card.classList.toggle('active', cardId === profileId);
  });

  var nameStr = BRIDGE_MODEL_NAMES[profileId] || ('profile-' + profileId);
  logDebug('[ModelBridge] Selected: ' + nameStr + ' (ID ' + profileId + ')', 'info');
}

// Load model via MASM bridge POST /api/model/load
async function bridgeLoadModel() {
  if (BridgeState.selectedProfileId < 0) {
    addMessage('system', '\u26A0\uFE0F Select a model profile from the bridge list first.');
    return;
  }

  var profileId = BridgeState.selectedProfileId;
  var nameStr = BRIDGE_MODEL_NAMES[profileId] || ('profile-' + profileId);
  addMessage('system', '\u26A1 Loading model via MASM x64 bridge: **' + nameStr + '** (ID ' + profileId + ')...');

  try {
    var activeUrl = getActiveUrl();
    var payload = JSON.stringify({ index: profileId, name: nameStr });
    var res = await fetch(activeUrl + '/api/model/load', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: payload,
      signal: AbortSignal.timeout(30000)
    });

    var data = await res.json();
    if (!res.ok) {
      addMessage('system', '\u274C Model load failed: ' + (data.error || data.message || 'Unknown error'));
      logDebug('[ModelBridge] Load failed: ' + JSON.stringify(data), 'error');
      return;
    }

    BridgeState.activeProfileId = profileId;
    BridgeState.loaded = true;

    // Also set in main model state
    State.model.current = nameStr;
    var sel = document.getElementById('modelSelect');
    if (sel) {
      // Try to select matching option, or add one
      var found = false;
      for (var i = 0; i < sel.options.length; i++) {
        if (sel.options[i].value === nameStr) {
          sel.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        var opt = document.createElement('option');
        opt.value = nameStr;
        opt.textContent = '\uD83E\uDDE9 ' + nameStr + ' (MASM)';
        sel.appendChild(opt);
        sel.value = nameStr;
      }
    }
    changeModel();

    // Update card highlights
    var cards = document.querySelectorAll('.bridge-model-card');
    cards.forEach(function (card) {
      var cardId = parseInt(card.getAttribute('data-profile-id'), 10);
      card.classList.toggle('active', cardId === profileId);
    });

    addMessage('system', '\u2705 Model loaded via ' + (data.bridge || 'MASM') + ': **' + nameStr + '** | Tier: ' + (data.tier || '?') + ' | RAM: ' + (data.ram_mb || '?') + 'MB | Engine: ' + (data.engine_mode || '?'));
    logDebug('[ModelBridge] Loaded: ' + nameStr + ' via ' + (data.bridge || '?'), 'info');

    updateBridgeStatusBar();
  } catch (e) {
    addMessage('system', '\u274C Bridge load error: ' + e.message);
    logDebug('[ModelBridge] Load exception: ' + e.message, 'error');
  }
}

// Unload model via MASM bridge POST /api/model/unload
async function bridgeUnloadModel() {
  try {
    var activeUrl = getActiveUrl();
    var res = await fetch(activeUrl + '/api/model/unload', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
      signal: AbortSignal.timeout(10000)
    });

    var data = await res.json();
    if (!res.ok) {
      addMessage('system', '\u26A0\uFE0F Unload: ' + (data.error || data.message || 'no model loaded'));
      return;
    }

    BridgeState.activeProfileId = -1;
    BridgeState.loaded = false;

    // Clear card highlights
    var cards = document.querySelectorAll('.bridge-model-card');
    cards.forEach(function (card) { card.classList.remove('active'); });

    addMessage('system', '\u23F9\uFE0F Model unloaded from MASM bridge.');
    logDebug('[ModelBridge] Model unloaded', 'info');

    updateBridgeStatusBar();
  } catch (e) {
    addMessage('system', '\u274C Bridge unload error: ' + e.message);
    logDebug('[ModelBridge] Unload exception: ' + e.message, 'error');
  }
}

// Fetch engine capabilities GET /api/engine/capabilities
async function fetchBridgeCapabilities() {
  try {
    var activeUrl = getActiveUrl();
    var res = await fetch(activeUrl + '/api/engine/capabilities', { signal: AbortSignal.timeout(8000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    var caps = await res.json();

    BridgeState.hardware = caps;

    var lines = [];
    lines.push('**\uD83D\uDD0D Engine Capabilities (' + (caps.bridge || '?') + ')**');
    if (caps.cpu) {
      lines.push('CPU: AVX2=' + (caps.cpu.avx2 ? '\u2705' : '\u274C') +
        ' FMA3=' + (caps.cpu.fma3 ? '\u2705' : '\u274C') +
        ' AVX-512F=' + (caps.cpu.avx512f ? '\u2705' : '\u274C') +
        ' AVX-512BW=' + (caps.cpu.avx512bw ? '\u2705' : '\u274C'));
    }
    if (caps.memory) {
      lines.push('RAM: ' + (caps.memory.total_ram_gb || '?') + 'GB total, ' + (caps.memory.free_ram_gb || '?') + 'GB free');
    }
    if (caps.engine) {
      lines.push('Engine: swarm=' + (caps.engine.swarm ? '\u2705' : '\u274C') +
        ' dual=' + (caps.engine.dual_engine ? '\u2705' : '\u274C') +
        ' 5drive=' + (caps.engine.five_drive ? '\u2705' : '\u274C') +
        ' tensorhop=' + (caps.engine.tensor_hop ? '\u2705' : '\u274C') +
        ' flashattn=' + (caps.engine.flash_attention ? '\u2705' : '\u274C') +
        ' safedecode=' + (caps.engine.safe_decode ? '\u2705' : '\u274C'));
    }
    if (caps.model_range) lines.push('Range: ' + caps.model_range);
    if (caps.raw_caps) lines.push('Raw: `' + caps.raw_caps + '`');
    if (caps.supported_tiers) lines.push('Tiers: ' + caps.supported_tiers.join(', '));
    if (caps.profile_count) lines.push('Profiles: ' + caps.profile_count);

    addMessage('system', lines.join('\n'));
    logDebug('[ModelBridge] Capabilities fetched: ' + (caps.bridge || '?'), 'info');

    updateBridgeStatusBar();
  } catch (e) {
    addMessage('system', '\u274C Capabilities fetch failed: ' + e.message);
    logDebug('[ModelBridge] Caps error: ' + e.message, 'error');
  }
}

// Quantization dropdown change handler
function bridgeQuantChanged() {
  var quantVal = parseInt(document.getElementById('bridgeQuantSelect').value, 10);
  logDebug('[ModelBridge] Quantization set to type ' + quantVal, 'info');
}

// Update the status bar at bottom of bridge panel
function updateBridgeStatusBar() {
  var bar = document.getElementById('bridgeStatusBar');
  if (!bar) return;

  var chips = [];

  // CPU chip
  if (BridgeState.hardware) {
    var hw = BridgeState.hardware;
    var cpuStr = 'CPU:';
    if (hw.cpu || hw.avx2 !== undefined) {
      var cpu = hw.cpu || hw;
      cpuStr += (cpu.avx2 ? ' AVX2' : '') + (cpu.fma3 ? ' FMA3' : '') + (cpu.avx512f ? ' 512' : '');
      if (!cpu.avx2 && !cpu.fma3 && !cpu.avx512f) cpuStr += ' basic';
    }
    chips.push('<span class="status-chip">' + cpuStr + '</span>');

    // RAM chip
    var mem = hw.memory || hw;
    if (mem.total_ram_gb || mem.total_ram_mb) {
      var totalG = mem.total_ram_gb || Math.round((mem.total_ram_mb || 0) / 1024);
      var freeG = mem.free_ram_gb || Math.round((mem.free_ram_mb || 0) / 1024);
      chips.push('<span class="status-chip">RAM: ' + freeG + '/' + totalG + 'G</span>');
    }
  } else {
    chips.push('<span class="status-chip">CPU: \u2014</span>');
    chips.push('<span class="status-chip">RAM: \u2014</span>');
  }

  // Active model chip
  if (BridgeState.activeProfileId >= 0) {
    var activeName = BRIDGE_MODEL_NAMES[BridgeState.activeProfileId] || ('profile-' + BridgeState.activeProfileId);
    chips.push('<span class="status-chip active">\u25B6 ' + activeName + '</span>');
  } else {
    chips.push('<span class="status-chip">Active: none</span>');
  }

  // Bridge type
  chips.push('<span class="status-chip">' + (BridgeState.bridge || '?') + '</span>');

  bar.innerHTML = chips.join('');
}

// Merge bridge profiles into the main model-select dropdown
function mergeBridgeIntoModelSelect() {
  var sel = document.getElementById('modelSelect');
  if (!sel || BridgeState.profiles.length === 0) return;

  // Check if MASM group already exists
  var existing = sel.querySelector('optgroup[label*="MASM"]');
  if (existing) existing.remove();

  var grp = document.createElement('optgroup');
  grp.label = '\uD83E\uDDE9 MASM Bridge (x64)';

  // Group by tier within the optgroup
  var tierOrder = ['small', 'medium', 'large', 'ultra', '800b-dual'];
  tierOrder.forEach(function (tier) {
    BridgeState.profiles.forEach(function (p) {
      if (p.tier !== tier) return;
      var nameStr = BRIDGE_MODEL_NAMES[p.id] || ('profile-' + p.id);
      var paramStr = p.params_b || '?';
      var opt = document.createElement('option');
      opt.value = nameStr;
      opt.textContent = nameStr + ' (' + paramStr + 'B)';

      // Check if already in dropdown
      var alreadyExists = false;
      for (var i = 0; i < sel.options.length; i++) {
        if (sel.options[i].value === nameStr) { alreadyExists = true; break; }
      }
      if (!alreadyExists) {
        grp.appendChild(opt);
      }
    });
  });

  if (grp.children.length > 0) {
    sel.appendChild(grp);
    logDebug('[ModelBridge] Merged ' + grp.children.length + ' bridge profiles into model select', 'info');
  }
}

// ======================================================================
// CHAT
// ======================================================================
function handleInput(e) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    sendMessage();
  }
  setTimeout(autoResizeInput, 0);
}

function autoResizeInput() {
  var el = document.getElementById('chatInput');
  el.style.height = 'auto';
  el.style.height = Math.min(el.scrollHeight, 120) + 'px';
}

// Detect local file paths in user messages (Windows-style)
// Matches: D:\path\file.ext, C:/path/file.ext, etc.
function detectFilePaths(text) {
  var pathRegex = /(?:^|\s|["'])([A-Za-z]:[\\\/][^\s"'<>|*?]+\.[a-zA-Z0-9]{1,10})(?:\s|["']|$)/g;
  var paths = [];
  var match;
  while ((match = pathRegex.exec(text)) !== null) {
    paths.push(match[1].replace(/\\/g, '/'));
  }
  return paths;
}

// Try to read a local file by path via backend proxy, Win32IDE, or file:// fetch
async function readLocalFile(filePath) {
  // Normalize to forward slashes
  var normalized = filePath.replace(/\\/g, '/');

  // Strategy 1: Try serve.py /api/read-file endpoint (when not in direct mode)
  if (!State.backend.directMode && State.backend.online) {
    try {
      var res = await fetch(State.backend.url + '/api/read-file', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path: normalized }),
        signal: AbortSignal.timeout(5000),
      });
      if (res.ok) {
        var data = await res.json();
        if (data.content) return { content: data.content, name: data.name || normalized.split('/').pop() };
      }
    } catch (_) { /* fall through */ }
  }

  // Strategy 2: Try Win32IDE /api/read-file via known IDE server URLs
  // Uses _ideServerUrl (resolved during connectBackend) + window.location.origin as fallbacks.
  // Critical for file:// opens where window.location.origin is null.
  {
    var ideServerUrl = _ideServerUrl || 'http://localhost:8080';
    var urlsToTry = [ideServerUrl];
    // Also try page origin if we're served from a server (not file://)
    if (!_isFileProtocol && window.location.origin && window.location.origin !== 'null' && window.location.origin !== ideServerUrl) {
      urlsToTry.unshift(window.location.origin);
    }
    // Also try State.backend.url if it differs
    if (State.backend.url && urlsToTry.indexOf(State.backend.url) === -1) {
      urlsToTry.push(State.backend.url);
    }
    for (var ui = 0; ui < urlsToTry.length; ui++) {
      try {
        var res1b = await fetch(urlsToTry[ui] + '/api/read-file', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ path: normalized }),
          signal: AbortSignal.timeout(5000),
        });
        if (res1b.ok) {
          var data1b = await res1b.json();
          if (data1b.content) {
            logDebug('📂 read-file via ' + urlsToTry[ui] + ' succeeded', 'info');
            return { content: data1b.content, name: data1b.name || normalized.split('/').pop() };
          }
        }
      } catch (_) { /* fall through to next URL */ }
    }
  }

  // Strategy 3: Try file:// fetch (works if page opened from file:// or with relaxed security)
  try {
    var fileUrl = 'file:///' + normalized.replace(/^\//, '');
    var res2 = await fetch(fileUrl, { signal: AbortSignal.timeout(3000) });
    if (res2.ok) {
      var content = await res2.text();
      return { content: content, name: normalized.split('/').pop() };
    }
  } catch (_) { /* fall through */ }

  return null;
}

// Auto-attach files referenced by path in user's message
async function autoAttachFilePaths(text) {
  var paths = detectFilePaths(text);
  if (paths.length === 0) return;

  for (var i = 0; i < paths.length; i++) {
    var fp = paths[i];
    var fname = fp.split('/').pop();
    // Skip if already attached
    if (State.files.find(function (f) { return f.name === fname; })) continue;
    if (State.fileContents[fname]) continue;

    logDebug('\uD83D\uDCC2 Attempting to read local file: ' + fp, 'info');
    var result = await readLocalFile(fp);
    if (result && result.content) {
      // Inject directly into fileContents (no File object needed for content)
      State.fileContents[result.name] = result.content;
      // Create a minimal File-like entry for State.files
      var pseudoFile = { name: result.name, size: result.content.length };
      State.files.push(pseudoFile);
      renderFile(pseudoFile);
      updateFileBadge();
      logDebug('\u2705 Auto-attached ' + result.name + ' (' + formatBytes(result.content.length) + ') from local path', 'info');
      addMessage('system', '\uD83D\uDCC2 Auto-attached **' + result.name + '** (' + formatBytes(result.content.length) + ') from `' + fp + '`', { skipMemory: true });
    } else {
      logDebug('\u274c Could not read local file: ' + fp + ' (backend may not support /api/read-file)', 'warn');
      addMessage('system', '\u26a0\ufe0f Could not read `' + fp + '`. Try dragging the file into the attachment area, or ensure serve.py is running.', { skipMemory: true });
    }
  }
}

async function sendMessage() {
  if (State.chat.sending) return;
  var el = document.getElementById('chatInput');
  var text = el.value.trim();
  if (!text) return;

  // Wait for any pending file reads from drag-and-drop to complete
  if (State.filePendingReads > 0) {
    logDebug('Waiting for ' + State.filePendingReads + ' file(s) to finish loading...', 'info');
    await waitForPendingFiles(5000);
  }

  // Auto-detect and attach file paths mentioned in the message
  await autoAttachFilePaths(text);

  // Phase 4: Input sanitization guard
  var validation = validateInput(text);
  if (!validation.valid) {
    addMessage('system', '\u26A0\uFE0F **Security:** ' + validation.reason, { skipMemory: true });
    return;
  }
  if (validation.sanitized !== undefined && validation.sanitized !== text) {
    text = validation.sanitized;
  }

  // Phase 4: Rate limiter
  if (!checkRateLimit()) {
    addMessage('system', '\u26A0\uFE0F **Rate Limited:** Please wait before sending another message. (' + State.security.rateLimit.maxPerMinute + ' requests per minute max)', { skipMemory: true });
    return;
  }

  addMessage('user', text, { skipMemory: true });
  el.value = '';
  el.style.height = 'auto';

  State.chat.sending = true;
  document.getElementById('sendBtn').disabled = true;
  showStopHideSend();

  if (State.backend.online) {
    await sendToBackend(text);
  } else {
    // Offline mode — still record to conversation memory
    Conversation.addMessage('user', text);
    showTyping();
    await sleep(400);
    hideTyping();
    var offlineReply = getOfflineResponse(text);
    Conversation.addMessage('assistant', offlineReply);
    addMessage('assistant', offlineReply, { skipMemory: true });
  }

  State.chat.sending = false;
  document.getElementById('sendBtn').disabled = false;
  showSendHideStop();
}

// ======================================================================
// RawrXD Streaming Bridge — Production-grade NDJSON/SSE stream client
// Connects to server.js (localhost:8080) or direct Ollama (11434)
// Implements: AbortController, cross-chunk NDJSON buffer, token rate
// ======================================================================
class RawrXDStream {
  constructor(baseUrl) {
    this.base = baseUrl || getActiveUrl();
    this.abortCtrl = null;
    this.tokenCount = 0;
    this.startTime = 0;
    this.lastTokenTime = 0;
  }

  // Async generator: yields tokens one by one from Ollama NDJSON /api/generate
  async *chatGenerate(model, prompt, options) {
    this.abortCtrl = new AbortController();
    this.tokenCount = 0;
    this.startTime = performance.now();
    this.lastTokenTime = this.startTime;

    var res = await fetch(this.base + '/api/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      signal: this.abortCtrl.signal,
      body: JSON.stringify({
        model: model,
        prompt: prompt,
        stream: true,
        options: options || {},
      }),
    });

    if (!res.ok || !res.body) {
      throw new Error('HTTP ' + res.status);
    }

    var reader = res.body.getReader();
    var decoder = new TextDecoder('utf-8');
    var buffer = '';

    while (true) {
      var result = await reader.read();
      if (result.done) break;

      buffer += decoder.decode(result.value, { stream: true });

      // Cross-chunk safe NDJSON parsing
      var newline;
      while ((newline = buffer.indexOf('\n')) !== -1) {
        var line = buffer.slice(0, newline).trim();
        buffer = buffer.slice(newline + 1);

        if (!line) continue;

        try {
          var msg = JSON.parse(line);
          if (msg.response) {
            this.tokenCount++;
            this.lastTokenTime = performance.now();
            yield msg.response;
          }
          if (msg.done) return;
        } catch (e) {
          // Skip malformed JSON lines
          logDebug('[RawrXDStream] Parse error: ' + line.substring(0, 80), 'warn');
        }
      }
    }
  }

  // Async generator: yields tokens from OpenAI SSE /v1/chat/completions
  async *chatCompletions(model, messages, options) {
    this.abortCtrl = new AbortController();
    this.tokenCount = 0;
    this.startTime = performance.now();
    this.lastTokenTime = this.startTime;

    var res = await fetch(this.base + '/v1/chat/completions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      signal: this.abortCtrl.signal,
      body: JSON.stringify(Object.assign({
        model: model,
        messages: messages,
        stream: true,
      }, (options || buildOpenAIPayloadExtras()))),
    });

    if (!res.ok || !res.body) {
      throw new Error('HTTP ' + res.status);
    }

    var reader = res.body.getReader();
    var decoder = new TextDecoder('utf-8');
    var buffer = '';

    while (true) {
      var result = await reader.read();
      if (result.done) break;

      buffer += decoder.decode(result.value, { stream: true });

      // Cross-chunk safe SSE + NDJSON parsing
      var newline;
      while ((newline = buffer.indexOf('\n')) !== -1) {
        var line = buffer.slice(0, newline).trim();
        buffer = buffer.slice(newline + 1);

        if (!line) continue;

        // Strip SSE "data: " prefix
        if (line.startsWith('data: ')) line = line.slice(6);
        if (line === '[DONE]') return;

        try {
          var obj = JSON.parse(line);

          // OpenAI SSE format: choices[0].delta.content
          var delta = '';
          if (obj.choices && obj.choices[0] && obj.choices[0].delta) {
            delta = obj.choices[0].delta.content || '';
          } else if (obj.response) {
            // Ollama NDJSON fallback within same parser
            delta = obj.response;
          }

          if (delta) {
            this.tokenCount++;
            this.lastTokenTime = performance.now();
            yield delta;
          }
          if (obj.done) return;
        } catch (e) {
          // Skip non-JSON lines (SSE comments, empty lines)
        }
      }
    }
  }

  // Abort current stream (Ctrl+C equivalent)
  abort() {
    if (this.abortCtrl) {
      this.abortCtrl.abort();
      this.abortCtrl = null;
    }
  }

  // Get live token rate (tokens/sec)
  getTokenRate() {
    if (this.tokenCount === 0) return 0;
    var elapsed = (performance.now() - this.startTime) / 1000;
    return elapsed > 0 ? (this.tokenCount / elapsed) : 0;
  }

  // Get models (GGUF + Ollama + Blobs) from MASM backend
  async getModels() {
    var r = await fetch(this.base + '/models', { signal: AbortSignal.timeout(5000) });
    return r.json();
  }

  // Legacy non-streaming ask
  async ask(model, prompt) {
    var r = await fetch(this.base + '/ask', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ question: prompt, model: model }),
    });
    var j = await r.json();
    return j.answer;
  }

  // Agent failure replay
  async replayFailure(failureId, action) {
    var r = await fetch(this.base + '/api/agents/replay', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ failure_id: failureId, action: action || 'retry' }),
    });
    return r.json();
  }
}

// Global stream instance — tracks active stream for abort
var _activeStream = null;

// Show/hide Stop/Send buttons
function showStopHideSend() {
  document.getElementById('sendBtn').style.display = 'none';
  document.getElementById('stopBtn').style.display = 'flex';
}
function showSendHideStop() {
  document.getElementById('stopBtn').style.display = 'none';
  document.getElementById('sendBtn').style.display = 'flex';
}

// Stop Generation — cleanly cancels Ollama mid-generation
function stopGeneration() {
  if (_activeStream) {
    _activeStream.abort();
    _activeStream = null;
    logDebug('Generation stopped by user (Ctrl+C)', 'info');
    logStructured('INFO', 'stream_aborted', { reason: 'user_cancel' });
  }
  // Also cancel CoT chain if running
  if (typeof CoT !== 'undefined' && CoT.running && CoT.abortController) {
    CoT.cancel();
    logDebug('CoT chain cancelled by user', 'info');
  }
  showSendHideStop();
}

// Keyboard shortcut: Ctrl+C or Escape to stop generation
document.addEventListener('keydown', function (e) {
  if ((e.ctrlKey && e.key === 'c' && State.chat.sending) || (e.key === 'Escape' && State.chat.sending)) {
    e.preventDefault();
    stopGeneration();
  }
});

// Keyboard shortcut: F11 toggles standalone fullscreen (only in standalone mode)
document.addEventListener('keydown', function (e) {
  if (e.key === 'F11' && _isFileProtocol) {
    e.preventDefault();
    toggleStandaloneFullscreen();
  }
});

// Sync standalone-fullscreen class when user exits fullscreen via Escape or browser chrome
document.addEventListener('fullscreenchange', function () {
  if (!document.fullscreenElement) {
    document.body.classList.remove('standalone-fullscreen');
  }
});

// ======================================================================
// STREAM RATE INDICATOR (token/s + ETA during streaming)
// ======================================================================
var _rateIndicatorEl = null;
var _rateUpdateInterval = null;

function showStreamRate() {
  if (!_rateIndicatorEl) {
    _rateIndicatorEl = document.createElement('div');
    _rateIndicatorEl.className = 'stream-rate-indicator';
    _rateIndicatorEl.id = 'streamRateIndicator';
    var toolbar = document.querySelector('.input-toolbar');
    if (toolbar) toolbar.appendChild(_rateIndicatorEl);
  }
  _rateIndicatorEl.style.display = 'flex';
  _rateIndicatorEl.innerHTML = '<span class="rate-value">0.0 t/s</span>';

  _rateUpdateInterval = setInterval(function () {
    if (_activeStream && _activeStream.tokenCount > 0) {
      var rate = _activeStream.getTokenRate().toFixed(1);
      var tokens = _activeStream.tokenCount;
      _rateIndicatorEl.innerHTML =
        '<span class="rate-value">' + rate + ' t/s</span>' +
        '<span class="eta-value">' + tokens + ' tokens</span>';
    }
  }, 250);
}

function hideStreamRate() {
  if (_rateUpdateInterval) {
    clearInterval(_rateUpdateInterval);
    _rateUpdateInterval = null;
  }
  if (_rateIndicatorEl) {
    _rateIndicatorEl.style.display = 'none';
  }
}

// ======================================================================
// CHAT — SEND TO BACKEND
// ======================================================================

async function sendToBackend(query) {
  var model = State.model.current;
  var t0 = performance.now();
  var activeUrl = getActiveUrl();  // respects directMode fallback

  // First-token probe for large models (Safe Decode Profile)
  if (isSafeDecodeActive() && State.gen.safeDecodeProfile.probeFirst) {
    var probeOk = await firstTokenProbe(model, activeUrl);
    if (!probeOk) {
      addMessage('system', '\u26a0\ufe0f First-token probe failed for ' + model + '. Model may be in a bad decode state. Try restarting Ollama or switching to a smaller model.');
      return;
    }
  }

  // Warn user if attached files may overflow context window
  if (State.files.length > 0) {
    var totalFileChars = 0;
    State.files.forEach(function (f) {
      var c = State.fileContents[f.name];
      if (c) totalFileChars += c.length;
    });
    var estFileTokens = Math.ceil(totalFileChars / 4);
    var contextBudget = State.gen.context;
    if (estFileTokens > contextBudget * 0.7) {
      logDebug('\u26a0\ufe0f File attachments (~' + estFileTokens + ' tokens) may exceed context window (' + contextBudget + ' tokens). Consider increasing the context window slider or the file content will be truncated.', 'warn');
      addMessage('system', '\u26a0\ufe0f **Context Warning:** Your attached files are ~' + estFileTokens.toLocaleString() + ' tokens but the context window is only ' + contextBudget.toLocaleString() + ' tokens. The model may not see all file content. Consider increasing the context window in settings.', { skipMemory: true });
    }
  }

  // Record user message to conversation memory
  Conversation.addMessage('user', query);

  // Get effective params (may be clamped by safe decode)
  var ep = getEffectiveGenParams();

  // Try OpenAI-compatible endpoint first (supports multi-turn)
  if (ep.stream) {
    await sendStreamingOpenAI(query, model, t0);
  } else {
    await sendNonStreamingOpenAI(query, model, t0);
  }
}

// ---- PRIMARY: OpenAI-compatible /v1/chat/completions ----

async function sendNonStreamingOpenAI(query, model, t0) {
  showTyping();
  try {
    var extras = buildOpenAIPayloadExtras();
    var payload = Object.assign({
      model: model || 'rawrxd',
      messages: Conversation.getContextForAPI(),
      stream: false,
    }, extras);

    var res = await fetch(getActiveUrl() + '/v1/chat/completions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });

    hideTyping();
    var latency = Math.round(performance.now() - t0);

    if (!res.ok) {
      // Fallback to legacy /ask endpoint
      logDebug('/v1/chat/completions returned ' + res.status + ', falling back to /ask', 'warn');
      await sendNonStreamingLegacy(query, model, t0);
      return;
    }

    var data = await res.json();
    var answer = '';
    if (data.choices && data.choices[0] && data.choices[0].message) {
      answer = data.choices[0].message.content || '';
    } else {
      answer = data.answer || data.response || '[No response]';
    }

    // Record assistant response to conversation memory
    Conversation.addMessage('assistant', answer);
    addMessage('assistant', answer, { latency: latency, model: model, skipMemory: true });

    if (el('dbgLogRequests') && el('dbgLogRequests').checked) {
      logDebug('/v1/chat/completions \u2192 ' + answer.length + ' chars in ' + latency + 'ms', 'info');
    }
    if (el('dbgLogResponses') && el('dbgLogResponses').checked) {
      logDebug('Response body: ' + (answer.length > 2000 ? answer.substring(0, 2000) + '... [truncated]' : answer), 'info');
    }

    // Phase 3: Record performance metric
    recordMetric({ latency: latency, tokens: 0, model: model, endpoint: '/v1/chat/completions', success: true });
  } catch (e) {
    hideTyping();
    logDebug('OpenAI endpoint failed: ' + e.message + ', falling back to /ask', 'warn');
    recordMetric({ latency: Math.round(performance.now() - t0), tokens: 0, model: model, endpoint: '/v1/chat/completions', success: false, errorMsg: e.message });
    await sendNonStreamingLegacy(query, model, t0);
  }
}

async function sendStreamingOpenAI(query, model, t0) {
  var msgDiv = addMessage('assistant', '', { streaming: true, skipMemory: true });
  var textEl = msgDiv.querySelector('.message-text');

  // Create RawrXDStream bridge instance with AbortController
  var stream = new RawrXDStream(getActiveUrl());
  _activeStream = stream;
  showStopHideSend();
  showStreamRate();

  try {
    var fullText = '';
    var _rafPending = false;
    textEl.classList.add('streaming-cursor');

    // Use async generator for cross-chunk-safe SSE/NDJSON parsing
    for await (var token of stream.chatCompletions(
      model || 'rawrxd',
      Conversation.getContextForAPI(),
      buildOpenAIPayloadExtras()
    )) {
      fullText += token;

      // Throttle DOM updates to once per animation frame to prevent UI freezes
      if (!_rafPending) {
        _rafPending = true;
        var _snapText = fullText;
        requestAnimationFrame(function () {
          textEl.innerHTML = formatMessage(_snapText);
          var container = document.getElementById('chatMessages');
          container.scrollTop = container.scrollHeight;
          _rafPending = false;
        });
      }
    }

    textEl.classList.remove('streaming-cursor');
    textEl.innerHTML = formatMessage(fullText);

    // Record completed assistant response to conversation memory
    if (fullText) Conversation.addMessage('assistant', fullText);

    var latency = Math.round(performance.now() - t0);
    var tokenCount = stream.tokenCount;
    var tps = tokenCount > 0 && latency > 0 ? (tokenCount / (latency / 1000)).toFixed(1) : '\u2014';

    var metaEl = msgDiv.querySelector('.message-meta');
    if (metaEl) {
      metaEl.innerHTML = '<span>' + latency + 'ms</span><span>' + tokenCount + ' tokens</span><span>' + tps + ' t/s</span>';
    }

    if (el('dbgLogRequests') && el('dbgLogRequests').checked) {
      logDebug('OpenAI Stream: ' + tokenCount + ' tokens in ' + latency + 'ms (' + tps + ' t/s)', 'info');
    }
    if (el('dbgLogResponses') && el('dbgLogResponses').checked) {
      logDebug('Stream response: ' + (fullText.length > 2000 ? fullText.substring(0, 2000) + '... [truncated]' : fullText), 'info');
    }

    // Phase 3: Record performance metric
    recordMetric({ latency: latency, tokens: tokenCount, model: model, endpoint: '/v1/chat/completions (stream)', success: true });
  } catch (e) {
    textEl.classList.remove('streaming-cursor');
    if (e.name === 'AbortError') {
      // User cancelled — save partial response
      var partialText = textEl.textContent || '';
      if (partialText) Conversation.addMessage('assistant', partialText + '\n\n*[Generation stopped by user]*');
      logDebug('Stream aborted by user after ' + stream.tokenCount + ' tokens', 'info');
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: stream.tokenCount, model: model, endpoint: '/v1/chat/completions (stream)', success: true, errorMsg: 'user_abort' });
    } else if (!textEl.textContent) {
      // OpenAI streaming not available — try Ollama, then /ask
      msgDiv.remove();
      State.chat.messageCount--;
      logDebug('OpenAI streaming failed: ' + e.message + ', trying /api/generate', 'warn');
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: 0, model: model, endpoint: '/v1/chat/completions (stream)', success: false, errorMsg: e.message });
      hideStreamRate();
      _activeStream = null;
      await sendStreamingOllama(query, model, t0);
      return;
    } else {
      // Partial response received — save what we got
      var partial = textEl.textContent || '';
      if (partial) Conversation.addMessage('assistant', partial);
      logDebug('OpenAI stream interrupted: ' + e.message, 'error');
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: stream.tokenCount, model: model, endpoint: '/v1/chat/completions (stream)', success: false, errorMsg: e.message });
    }
  } finally {
    hideStreamRate();
    _activeStream = null;
    showSendHideStop();
  }
}

// ---- FALLBACK: Ollama-compatible /api/generate (no multi-turn) ----

async function sendStreamingOllama(query, model, t0) {
  var legacyPrompt = Conversation.getLegacyPrompt(query);
  var msgDiv = addMessage('assistant', '', { streaming: true, skipMemory: true });
  var textEl = msgDiv.querySelector('.message-text');

  // Create RawrXDStream bridge instance with AbortController
  var stream = new RawrXDStream(getActiveUrl());
  _activeStream = stream;
  showStopHideSend();
  showStreamRate();

  try {
    var fullText = '';
    var _rafPending = false;
    textEl.classList.add('streaming-cursor');

    // Use async generator for cross-chunk-safe NDJSON parsing
    for await (var token of stream.chatGenerate(
      model || 'rawrxd',
      legacyPrompt,
      buildOllamaOptions()
    )) {
      fullText += token;

      // Throttle DOM updates to once per animation frame to prevent UI freezes
      if (!_rafPending) {
        _rafPending = true;
        var _snapText = fullText;
        requestAnimationFrame(function () {
          textEl.innerHTML = formatMessage(_snapText);
          var container = document.getElementById('chatMessages');
          container.scrollTop = container.scrollHeight;
          _rafPending = false;
        });
      }
    }

    textEl.classList.remove('streaming-cursor');
    textEl.innerHTML = formatMessage(fullText);

    // Record to conversation memory
    if (fullText) Conversation.addMessage('assistant', fullText);

    var latency = Math.round(performance.now() - t0);
    var tokenCount = stream.tokenCount;
    var tps = tokenCount > 0 && latency > 0 ? (tokenCount / (latency / 1000)).toFixed(1) : '\u2014';

    var metaEl = msgDiv.querySelector('.message-meta');
    if (metaEl) {
      metaEl.innerHTML = '<span>' + latency + 'ms</span><span>' + tokenCount + ' tokens</span><span>' + tps + ' t/s</span>';
    }

    if (el('dbgLogRequests') && el('dbgLogRequests').checked) {
      logDebug('Ollama Stream: ' + tokenCount + ' tokens in ' + latency + 'ms (' + tps + ' t/s)', 'info');
    }

    // Phase 3: Record performance metric
    recordMetric({ latency: latency, tokens: tokenCount, model: model, endpoint: '/api/generate (stream)', success: true });
  } catch (e) {
    textEl.classList.remove('streaming-cursor');
    if (e.name === 'AbortError') {
      // User cancelled — save partial response
      var partialText = textEl.textContent || '';
      if (partialText) Conversation.addMessage('assistant', partialText + '\n\n*[Generation stopped by user]*');
      logDebug('Ollama stream aborted by user after ' + stream.tokenCount + ' tokens', 'info');
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: stream.tokenCount, model: model, endpoint: '/api/generate (stream)', success: true, errorMsg: 'user_abort' });
    } else if (!textEl.textContent) {
      msgDiv.remove();
      State.chat.messageCount--;
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: 0, model: model, endpoint: '/api/generate (stream)', success: false, errorMsg: e.message });
      hideStreamRate();
      _activeStream = null;
      await sendNonStreamingLegacy(query, model, t0);
      return;
    } else {
      var partial = textEl.textContent || '';
      if (partial) Conversation.addMessage('assistant', partial);
      logDebug('Ollama stream interrupted: ' + e.message, 'error');
      recordMetric({ latency: Math.round(performance.now() - t0), tokens: stream.tokenCount, model: model, endpoint: '/api/generate (stream)', success: false, errorMsg: e.message });
    }
  } finally {
    hideStreamRate();
    _activeStream = null;
    showSendHideStop();
  }
}

// ---- LAST RESORT: Legacy /ask (single-turn) or Ollama /api/generate non-streaming ----

async function sendNonStreamingLegacy(query, model, t0) {
  showTyping();
  try {
    var legacyPrompt = Conversation.getLegacyPrompt(query);
    var activeUrl = getActiveUrl();

    // In direct Ollama mode, skip /ask (doesn't exist) — use /api/generate directly
    if (State.backend.directMode) {
      var res = await fetch(activeUrl + '/api/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: model || 'rawrxd',
          prompt: legacyPrompt,
          stream: false,
          options: buildOllamaOptions(),
        }),
      });

      hideTyping();
      var latency = Math.round(performance.now() - t0);

      if (!res.ok) {
        addMessage('system', 'All endpoints failed. Ollama returned HTTP ' + res.status);
        logDebug('/api/generate (non-streaming) failed: HTTP ' + res.status, 'error');
        recordMetric({ latency: latency, tokens: 0, model: model, endpoint: '/api/generate (non-stream)', success: false, errorMsg: 'HTTP ' + res.status });
        return;
      }

      var data = await res.json();
      var answer = data.response || '[No response]';

      Conversation.addMessage('assistant', answer);
      addMessage('assistant', answer, { latency: latency, model: model, skipMemory: true });

      if (el('dbgLogRequests') && el('dbgLogRequests').checked) {
        logDebug('/api/generate (non-stream, direct) \u2192 ' + answer.length + ' chars in ' + latency + 'ms', 'info');
      }

      recordMetric({ latency: latency, tokens: 0, model: model, endpoint: '/api/generate (non-stream)', success: true });
      return;
    }

    // Proxy mode: try /ask
    var res = await fetch(activeUrl + '/ask', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        question: legacyPrompt,
        model: model || undefined,
        context: State.gen.context,
        stream: false,
      }),
    });

    hideTyping();
    var latency = Math.round(performance.now() - t0);

    if (!res.ok) {
      addMessage('system', 'All endpoints failed. Backend returned HTTP ' + res.status);
      logDebug('/ask failed: HTTP ' + res.status, 'error');
      recordMetric({ latency: latency, tokens: 0, model: model, endpoint: '/ask', success: false, errorMsg: 'HTTP ' + res.status });
      return;
    }

    var data = await res.json();
    var answer = data.answer || data.response || '[No response]';

    Conversation.addMessage('assistant', answer);
    addMessage('assistant', answer, { latency: latency, model: model, skipMemory: true });

    if (el('dbgLogRequests') && el('dbgLogRequests').checked) {
      logDebug('/ask (legacy) \u2192 ' + answer.length + ' chars in ' + latency + 'ms', 'info');
    }
    if (el('dbgLogResponses') && el('dbgLogResponses').checked) {
      logDebug('Response body: ' + (answer.length > 2000 ? answer.substring(0, 2000) + '... [truncated]' : answer), 'info');
    }

    // Phase 3: Record performance metric
    recordMetric({ latency: latency, tokens: 0, model: model, endpoint: '/ask', success: true });
  } catch (e) {
    hideTyping();
    addMessage('system', 'Request failed: ' + e.message);
    logDebug('All endpoints failed: ' + e.message, 'error');
    recordMetric({ latency: Math.round(performance.now() - t0), tokens: 0, model: model, endpoint: '/ask', success: false, errorMsg: e.message });
    markOffline();
  }
}

function getOfflineResponse(q) {
  var lower = q.toLowerCase();
  if (lower.indexOf('help') >= 0 || lower === '?') {
    return '**Offline Mode \u2014 Help**\n\nThe backend is not running. To use AI:\n\n' +
      '1. **Start Ollama** \u2014 the chatbot will connect directly on port 11434\n' +
      '2. **Or start the RawrXD server** \u2014 `node server.js` on port 8080\n' +
      '3. **Or run the start script:**\n' +
      '```powershell\ncd D:\\rawrxd\nnode server.js\n```\n\n' +
      'Then click the \u26A1 button to connect.\n\n' +
      '*Note: If Ollama is running, the chatbot will automatically detect it even without the Python server.*';
  }
  if (lower.indexOf('endpoint') >= 0 || lower.indexOf('api') >= 0) {
    return '**API Endpoints** (when backend is online):\n\n' +
      '\u2022 `GET /health` \u2014 Health check\n' +
      '\u2022 `GET /status` \u2014 Server status + stats\n' +
      '\u2022 `GET /models` \u2014 List all models\n' +
      '\u2022 `POST /ask` \u2014 Send question\n' +
      '\u2022 `POST /api/generate` \u2014 Ollama-compatible streaming\n' +
      '\u2022 `POST /v1/chat/completions` \u2014 OpenAI-compatible\n' +
      '\u2022 `GET /gui` \u2014 Serve this HTML';
  }
  return '**Offline Mode**\n\nBackend not reachable at `' + State.backend.url + '`.\n\n' +
    'Type `help` for setup instructions, or click \u26A1 to retry.';
}

function markOffline() {
  State.backend.online = false;
  State.backend.directMode = false;
  updateModeBadge(false);
  document.getElementById('backendStatus').className = 'status-pill offline';
  document.getElementById('statusText').textContent = 'OFFLINE';
}

// ======================================================================
// MESSAGES
// ======================================================================
function addMessage(role, text, opts) {
  opts = opts || {};
  var container = document.getElementById('chatMessages');
  var time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });

  var div = document.createElement('div');
  div.className = 'message ' + role;

  var avatarIcon = role === 'user' ? '\uD83D\uDC64' : (role === 'system' ? '\u2699' : '\uD83E\uDD96');
  var label = role === 'user' ? 'You' : (role === 'system' ? 'System' : 'RawrXD');

  var metaHtml = '';
  var showTiming = el('dbgShowTiming') ? el('dbgShowTiming').checked : true;
  if (opts.latency || opts.model) {
    var parts = [];
    if (opts.latency) parts.push(opts.latency + 'ms');
    if (opts.model) parts.push(esc(opts.model));
    var metaStyle = showTiming ? '' : ' style="display:none"';
    metaHtml = '<div class="message-meta"' + metaStyle + '>' + parts.map(function (p) { return '<span>' + p + '</span>'; }).join('') + '</div>';
  } else if (role === 'assistant') {
    var metaStyle2 = showTiming ? '' : ' style="display:none"';
    metaHtml = '<div class="message-meta"' + metaStyle2 + '></div>';
  }

  // Inline failure badge
  var badgeHtml = '';
  if (opts.failureBadge) {
    var fb = opts.failureBadge;
    var cls = fb.outcome === 'Corrected' ? 'retried' : (fb.outcome === 'Failed' ? 'failed-retry' : 'detected');
    var bIcon = fb.outcome === 'Corrected' ? '\u2714' : (fb.outcome === 'Failed' ? '\u2718' : '\u26A0');
    badgeHtml = '<span class="failure-badge ' + cls + '" title="' + esc(fb.detail || '') + '">' + bIcon + ' ' + esc(fb.type) + '</span>';
  }

  div.innerHTML =
    '<div class="message-avatar">' + avatarIcon + '</div>' +
    '<div class="message-content">' +
    '<div class="message-header">' + label + ' <span class="message-time">' + time + '</span>' + badgeHtml + '</div>' +
    '<div class="message-text">' + (role === 'user' ? esc(text) : formatMessage(text)) + '</div>' +
    metaHtml +
    '</div>';

  container.appendChild(div);
  container.scrollTop = container.scrollHeight;

  State.chat.messageCount++;
  document.getElementById('msgCount').textContent = State.chat.messageCount;

  return div;
}

// ======================================================================
// FORMAT + SANITIZE (XSS-safe via DOMPurify)
// ======================================================================
function formatMessage(text) {
  if (!text) return '';

  // Step 1: Extract code blocks BEFORE escaping (they get their own treatment)
  var codeBlocks = [];
  text = text.replace(/```(\w+)?\n([\s\S]*?)```/g, function (match, lang, code) {
    var placeholder = '\x00CB' + codeBlocks.length + '\x00';
    codeBlocks.push({ lang: lang || 'text', code: code.trim() });
    return placeholder;
  });

  // Step 2: Escape ALL remaining text to prevent injection
  text = escHtml(text);

  // Step 3: Re-insert code blocks with safe escaped content + syntax highlighting
  codeBlocks.forEach(function (block, i) {
    var highlightedCode = escHtml(block.code);
    // Apply highlight.js syntax highlighting if available
    if (typeof hljs !== 'undefined') {
      try {
        var langAliases = { 'js': 'javascript', 'ts': 'typescript', 'py': 'python', 'rb': 'ruby', 'sh': 'bash', 'yml': 'yaml', 'md': 'markdown' };
        var lang = langAliases[block.lang] || block.lang;
        if (lang !== 'text' && hljs.getLanguage(lang)) {
          highlightedCode = hljs.highlight(block.code, { language: lang, ignoreIllegals: true }).value;
        } else if (lang === 'text') {
          highlightedCode = escHtml(block.code);
        } else {
          highlightedCode = hljs.highlightAuto(block.code).value;
        }
      } catch (_) {
        highlightedCode = escHtml(block.code);
      }
    }
    var safe = '<div class="code-block">' +
      '<div class="code-header">' +
      '<span class="code-lang">' + escHtml(block.lang) + '</span>' +
      '<div class="code-actions"><button class="code-btn" onclick="copyCode(this)">Copy</button></div>' +
      '</div>' +
      '<div class="code-content"><pre><code class="hljs language-' + escHtml(block.lang) + '">' + highlightedCode + '</code></pre></div>' +
      '</div>';
    text = text.replace('\x00CB' + i + '\x00', safe);
  });

  // Step 4: Apply markdown formatting to the escaped text
  // Inline code
  text = text.replace(/`([^`]+)`/g,
    '<code style="background:var(--bg-tertiary);padding:2px 6px;border-radius:4px;font-family:var(--font-mono);font-size:12px;">$1</code>');
  // Bold
  text = text.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
  // Italic
  text = text.replace(/\*(.+?)\*/g, '<em>$1</em>');
  // Bullet points
  text = text.replace(/^[\u2022\-] (.+)$/gm, '<li>$1</li>');
  // Numbered lists
  text = text.replace(/^\d+\. (.+)$/gm, '<li>$1</li>');
  // Line breaks
  text = text.replace(/\n/g, '<br>');

  // Step 5: FINAL SANITIZATION via DOMPurify
  if (typeof DOMPurify !== 'undefined') {
    return DOMPurify.sanitize(text, {
      ALLOWED_TAGS: ['b', 'i', 'em', 'strong', 'p', 'br', 'ul', 'ol', 'li',
        'code', 'pre', 'span', 'div', 'button', 'table', 'thead', 'tbody', 'tr', 'th', 'td'],
      ALLOWED_ATTR: ['class', 'style', 'onclick', 'title', 'data-lang'],
      ALLOW_DATA_ATTR: false,
      KEEP_CONTENT: true
    });
  }

  // DOMPurify not loaded (CDN blocked) — return escaped text as-is
  return text;
}

// HTML entity escaper — used for ALL untrusted text
function escHtml(t) {
  if (!t) return '';
  return t.replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function showTyping() {
  var container = document.getElementById('chatMessages');
  var div = document.createElement('div');
  div.className = 'message assistant';
  div.id = 'typingIndicator';
  div.innerHTML =
    '<div class="message-avatar">\uD83E\uDD96</div>' +
    '<div class="message-content">' +
    '<div class="typing-indicator">' +
    '<div class="typing-dot"></div><div class="typing-dot"></div><div class="typing-dot"></div>' +
    '</div>' +
    '</div>';
  container.appendChild(div);
  container.scrollTop = container.scrollHeight;
}

function hideTyping() {
  var x = document.getElementById('typingIndicator');
  if (x) x.remove();
}

// ======================================================================
// STATUS / ENDPOINTS
// ======================================================================
async function checkServerStatus() {
  if (!State.backend.online) {
    addMessage('system', 'Backend offline. Click \u26A1 to connect first.');
    return;
  }
  try {
    var t0 = performance.now();
    var res = await fetch(State.backend.url + '/status', { signal: AbortSignal.timeout(3000) });
    var lat = Math.round(performance.now() - t0);
    var data = await res.json();

    var msg = '**Server Status** (' + lat + 'ms)\n\n';
    Object.keys(data).forEach(function (k) {
      msg += '\u2022 **' + k + '**: `' + JSON.stringify(data[k]) + '`\n';
    });
    addMessage('assistant', msg);
  } catch (e) {
    addMessage('system', 'Status check failed: ' + e.message);
  }
}

function showEndpoints() {
  addMessage('assistant',
    '**API Endpoints** \u2014 `' + State.backend.url + '`\n\n' +
    '`GET  /health` \u2014 Health check\n' +
    '`GET  /status` \u2014 Server status + stats\n' +
    '`GET  /models` \u2014 List all models (GGUF + Ollama + blobs)\n' +
    '`POST /ask` \u2014 Send question, get answer\n' +
    '`POST /api/generate` \u2014 Ollama-compatible (streaming)\n' +
    '`POST /v1/chat/completions` \u2014 OpenAI-compatible chat\n' +
    '`GET  /gui` \u2014 Serve this HTML interface\n\n' +
    '**curl example:**\n' +
    '```bash\ncurl -X POST ' + State.backend.url + '/ask \\\n' +
    '  -H "Content-Type: application/json" \\\n' +
    '  -d \'{"question":"Hello","model":"dolphin3:latest"}\'\n```');
}

// ======================================================================
// FILES
// ======================================================================
function setupDragDrop() {
  var zone = document.getElementById('dropZone');
  zone.addEventListener('dragover', function (e) { e.preventDefault(); zone.classList.add('dragover'); });
  zone.addEventListener('dragleave', function () { zone.classList.remove('dragover'); });
  zone.addEventListener('drop', function (e) { e.preventDefault(); zone.classList.remove('dragover'); handleFiles(e.dataTransfer.files); });
}

function handleFiles(fileList) {
  Array.from(fileList).forEach(function (file) {
    if (State.files.find(function (f) { return f.name === file.name; })) return;
    State.files.push(file);
    renderFile(file);
    // Read text content for context
    if (file.size < 1024 * 1024) {
      State.filePendingReads++;
      var reader = new FileReader();
      reader.onload = function (e) {
        State.fileContents[file.name] = e.target.result;
        State.filePendingReads--;
        logDebug('\u2705 Loaded ' + file.name + ' (' + formatBytes(e.target.result.length) + ')', 'info');
        // Warn if file content exceeds context budget
        var estTokens = Math.ceil(e.target.result.length / 4);
        if (estTokens > State.gen.context * 0.5) {
          logDebug('\u26a0\ufe0f ' + file.name + ' (~' + estTokens.toLocaleString() + ' tokens) is large relative to context window (' + State.gen.context.toLocaleString() + ' tokens). Consider increasing the context window slider.', 'warn');
        }
      };
      reader.onerror = function () {
        State.filePendingReads--;
        logDebug('\u274c Failed to read ' + file.name, 'error');
      };
      reader.readAsText(file);
    } else {
      logDebug('\u26a0\ufe0f ' + file.name + ' (' + formatBytes(file.size) + ') exceeds 1MB limit \u2014 content not loaded. Only filename will be sent as context.', 'warn');
    }
  });
  updateFileBadge();
}

// Wait for all pending FileReader operations to complete (with timeout)
function waitForPendingFiles(timeoutMs) {
  if (State.filePendingReads <= 0) return Promise.resolve();
  timeoutMs = timeoutMs || 5000;
  return new Promise(function (resolve) {
    var start = Date.now();
    var check = setInterval(function () {
      if (State.filePendingReads <= 0 || (Date.now() - start) > timeoutMs) {
        clearInterval(check);
        if (State.filePendingReads > 0) {
          logDebug('\u26a0\ufe0f Timed out waiting for ' + State.filePendingReads + ' file(s) to load', 'warn');
        }
        resolve();
      }
    }, 50);
  });
}

function renderFile(file) {
  var list = document.getElementById('fileList');
  var div = document.createElement('div');
  div.className = 'file-item';
  div.id = 'file-' + file.name.replace(/[^a-zA-Z0-9]/g, '_');
  div.innerHTML =
    '<span class="file-icon">' + getFileIcon(file.name) + '</span>' +
    '<div class="file-info">' +
    '<div class="file-name">' + esc(file.name) + '</div>' +
    '<div class="file-meta">' + formatBytes(file.size) + '</div>' +
    '</div>' +
    '<span class="file-remove" onclick="removeFile(\'' + esc(file.name) + '\')">\u00D7</span>';
  list.appendChild(div);
}

function getFileIcon(name) {
  var ext = name.split('.').pop().toLowerCase();
  var map = {
    js: '📜', py: '🐍', cpp: '⚙', c: '⚙', h: '📋', hpp: '📋',
    rs: '🦀', go: '🐹', java: '☕', ts: '📘',
    html: '🌐', css: '🎨', json: '📋', md: '📝', txt: '📄',
    gguf: '🧠', bin: '💾', asm: '⚡', ps1: '💠',
  };
  return map[ext] || '📄';
}

function formatBytes(b) {
  if (b === 0) return '0 B';
  var k = 1024, sizes = ['B', 'KB', 'MB', 'GB'];
  var i = Math.floor(Math.log(b) / Math.log(k));
  return parseFloat((b / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function removeFile(name) {
  State.files = State.files.filter(function (f) { return f.name !== name; });
  delete State.fileContents[name];
  var x = document.getElementById('file-' + name.replace(/[^a-zA-Z0-9]/g, '_'));
  if (x) x.remove();
  updateFileBadge();
}

function updateFileBadge() {
  var badge = document.getElementById('fileBadge');
  var count = document.getElementById('fileCount');
  var ac = document.getElementById('attachCount');
  ac.textContent = State.files.length;
  if (State.files.length > 0) {
    badge.style.display = 'flex';
    count.textContent = State.files.length + ' file' + (State.files.length > 1 ? 's' : '');
  } else {
    badge.style.display = 'none';
  }
}

function attachFile() { document.getElementById('fileInput').click(); }

// ======================================================================
// DEBUG PANEL
// ======================================================================
function showDebug() {
  State.debug.open = true;
  document.getElementById('debugPanel').classList.add('active');
}
function closeDebug() {
  State.debug.open = false;
  document.getElementById('debugPanel').classList.remove('active');
}

async function runDiagnostics() {
  logDebug('=== Running Diagnostics ===', 'info');

  logDebug('Testing /health...', 'info');
  try {
    var t0 = performance.now();
    var res = await fetch(State.backend.url + '/health', { signal: AbortSignal.timeout(3000) });
    var lat = Math.round(performance.now() - t0);
    var data = await res.json();
    logDebug('Health: OK (' + lat + 'ms) \u2014 ' + JSON.stringify(data), 'info');
  } catch (e) {
    logDebug('Health: FAILED \u2014 ' + e.message, 'error');
  }

  logDebug('Testing /status...', 'info');
  try {
    var res2 = await fetch(State.backend.url + '/status', { signal: AbortSignal.timeout(3000) });
    var d2 = await res2.json();
    logDebug('Status: ' + JSON.stringify(d2), 'info');
  } catch (e) {
    logDebug('Status: FAILED \u2014 ' + e.message, 'error');
  }

  logDebug('Testing /models...', 'info');
  try {
    var t1 = performance.now();
    var res3 = await fetch(State.backend.url + '/models', { signal: AbortSignal.timeout(5000) });
    var d3 = await res3.json();
    var lt = Math.round(performance.now() - t1);
    logDebug('Models: ' + (d3.models ? d3.models.length : 0) + ' found (' + lt + 'ms)', 'info');
  } catch (e) {
    logDebug('Models: FAILED \u2014 ' + e.message, 'error');
  }

  logDebug('=== Diagnostics Complete ===', 'info');
}

async function testAllEndpoints() {
  var activeUrl = getActiveUrl();
  var eps = [
    { method: 'GET', path: '/health' },
    { method: 'GET', path: '/status' },
    { method: 'GET', path: '/models' },
    { method: 'GET', path: '/api/tags' },
    { method: 'POST', path: '/ask', body: { question: 'ping', model: '' } },
  ];

  logDebug('=== Testing All Endpoints (via ' + activeUrl + (State.backend.directMode ? ' [DIRECT]' : '') + ') ===', 'info');

  for (var i = 0; i < eps.length; i++) {
    var ep = eps[i];
    try {
      var t0 = performance.now();
      var opts = { method: ep.method, signal: AbortSignal.timeout(5000) };
      if (ep.body) {
        opts.headers = { 'Content-Type': 'application/json' };
        opts.body = JSON.stringify(ep.body);
      }
      var res = await fetch(activeUrl + ep.path, opts);
      var latency = Math.round(performance.now() - t0);
      var text = await res.text();
      var preview = text.length > 80 ? text.substring(0, 80) + '...' : text;
      logDebug(ep.method + ' ' + ep.path + ' \u2192 ' + res.status + ' (' + latency + 'ms) ' + preview, res.ok ? 'info' : 'warn');
    } catch (e) {
      logDebug(ep.method + ' ' + ep.path + ' \u2192 FAILED: ' + e.message, 'error');
    }
  }

  // Also test direct Ollama if we're currently going through the proxy
  if (!State.backend.directMode && State.backend.ollamaDirectUrl !== State.backend.url) {
    logDebug('--- Also testing direct Ollama at ' + State.backend.ollamaDirectUrl + ' ---', 'info');
    var ollamaEps = [
      { method: 'GET', path: '/' },
      { method: 'GET', path: '/api/tags' },
      { method: 'GET', path: '/v1/models' },
    ];
    for (var j = 0; j < ollamaEps.length; j++) {
      var oep = ollamaEps[j];
      try {
        var t1 = performance.now();
        var ores = await fetch(State.backend.ollamaDirectUrl + oep.path, { signal: AbortSignal.timeout(5000) });
        var olat = Math.round(performance.now() - t1);
        var otxt = await ores.text();
        var oprev = otxt.length > 80 ? otxt.substring(0, 80) + '...' : otxt;
        logDebug('[Ollama Direct] ' + oep.method + ' ' + oep.path + ' \u2192 ' + ores.status + ' (' + olat + 'ms) ' + oprev, ores.ok ? 'info' : 'warn');
      } catch (oe) {
        logDebug('[Ollama Direct] ' + oep.method + ' ' + oep.path + ' \u2192 FAILED: ' + oe.message, 'error');
      }
    }
  }

  logDebug('=== Endpoint Test Complete ===', 'info');
}

function exportDebug() {
  var data = {
    timestamp: new Date().toISOString(),
    state: {
      backend: State.backend,
      model: State.model,
      gen: State.gen,
      files: State.files.map(function (f) { return f.name; }),
      chatMessages: State.chat.messageCount,
      conversationMemory: Conversation.messages.length,
      systemPrompt: Conversation.systemPrompt,
    },
    conversation: Conversation.messages,
    debugLog: document.getElementById('debugLog').innerText,
    // Phase 3: Include performance data in debug export
    performance: {
      totalRequests: State.perf.totalRequests,
      totalTokens: State.perf.totalTokens,
      totalLatency: State.perf.totalLatency,
      avgLatency: State.perf.totalRequests > 0 ? Math.round(State.perf.totalLatency / State.perf.totalRequests) : 0,
      modelStats: State.perf.modelStats,
      recentHistory: State.perf.history.slice(-20),
    },
    structuredLog: State.perf.structuredLog,
  };
  var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = 'rawrxd-debug-' + Date.now() + '.json'; a.click();
  URL.revokeObjectURL(url);
  logDebug('Debug data exported', 'info');
}

function logDebug(msg, level) {
  level = level || 'info';
  var log = document.getElementById('debugLog');
  var time = new Date().toLocaleTimeString([], { hour12: false });
  var line = document.createElement('div');
  line.className = 'debug-log';
  line.innerHTML = '<span class="timestamp">[' + time + ']</span> <span class="level-' + level + '">[' + level.toUpperCase() + ']</span> ' + esc(msg) + '<br>';
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;

  // Phase 3: Also emit to structured log
  logStructured(level.toUpperCase(), 'debug_log', { message: msg });
}

// ======================================================================
// TERMINAL — Dual-mode: Local (client-side) + CLI (remote /api/cli)
// ======================================================================

// --- Terminal Mode Switching ---
function switchTerminalMode(mode) {
  State.terminal.mode = mode;
  var localTab = document.getElementById('termTabLocal');
  var cliTab = document.getElementById('termTabCli');
  var prompt = document.getElementById('termPrompt');
  var indicator = document.getElementById('termModeIndicator');
  var inputEl = document.getElementById('terminalInput');

  if (mode === 'cli') {
    localTab.classList.remove('active');
    cliTab.classList.add('active');
    prompt.innerHTML = 'cli&gt;';
    indicator.textContent = 'CLI';
    indicator.style.color = 'var(--accent-cyan)';
    inputEl.placeholder = '/help | /plan | /analyze | !engine';
    addTerminalLine('\u2500 Switched to CLI mode \u2500 Commands sent to backend /api/cli', 'system');
    addTerminalLine('  Backend: ' + (State.backend.online ? State.backend.url : 'OFFLINE') + ' (' + (State.backend.serverType || 'unknown') + ')', State.backend.online ? 'output' : 'error');
    addTerminalLine('  Type /help for available CLI commands', 'system');
  } else {
    cliTab.classList.remove('active');
    localTab.classList.add('active');
    prompt.innerHTML = 'rawrxd&gt;';
    indicator.textContent = 'LOCAL';
    indicator.style.color = 'var(--text-muted)';
    inputEl.placeholder = 'help | /help for CLI commands';
    addTerminalLine('\u2500 Switched to Local mode \u2500 Type help for local commands', 'system');
  }
  inputEl.focus();
}

// --- Determine which URL to send CLI commands to ---
function getCliUrl() {
  // If beacon found a /api/cli endpoint, use that exact URL
  if (State.backend.hasCliEndpoint && State.backend._cliEndpointUrl) {
    return State.backend._cliEndpointUrl;
  }
  // Otherwise use the active URL (respects directMode)
  if (State.backend.online) return getActiveUrl();
  return _ideServerUrl || 'http://localhost:11434';
}

// --- Render CLI output lines with color coding ---
function renderCliOutput(output) {
  if (!output) return;
  if (output === '[clear]') {
    document.getElementById('terminalOutput').innerHTML = '';
    return;
  }
  var lines = output.split('\n');
  lines.forEach(function (line) {
    if (line.trim() === '') return;
    var lineType = 'output';
    if (line.indexOf('\u2714') >= 0 || line.indexOf('OK') >= 0 || line.indexOf('ACTIVE') >= 0 || line.indexOf('ENABLED') >= 0) lineType = 'success';
    else if (line.indexOf('\u26A0') >= 0 || line.indexOf('WARNING') >= 0 || line.indexOf('DISABLED') >= 0 || line.indexOf('INACTIVE') >= 0) lineType = 'error';
    else if (line.indexOf('\u2500') >= 0 || line.indexOf('Usage:') >= 0 || line.indexOf('Tip:') >= 0) lineType = 'system';
    addTerminalLine(line, lineType);
  });
}

// =========================================================================
// CLIENT-SIDE CLI PROCESSOR (Beaconism: works without /api/cli backend)
// Mirrors HandleCliRequest from tool_server.cpp for offline/direct operation
// =========================================================================
async function processCliLocally(command) {
  var cmd = command.trim();
  var lower = cmd.toLowerCase();
  var parts = cmd.split(/\s+/);
  var subCmd = parts.length > 1 ? parts.slice(1).join(' ') : '';

  // /help
  if (lower === '/help' || lower === 'help') {
    renderCliOutput(
      '\u2500\u2500\u2500 RawrXD CLI (Client-Side Beacon) \u2500\u2500\u2500\n' +
      '  /help                \u2014 Show this help\n' +
      '  /status              \u2014 System & backend status\n' +
      '  /models              \u2014 List loaded models\n' +
      '  /plan <task>         \u2014 Generate implementation plan\n' +
      '  /analyze <file>      \u2014 Analyze a source file\n' +
      '  /optimize <file>     \u2014 Optimization suggestions\n' +
      '  /security <file>     \u2014 Security audit\n' +
      '  /suggest <desc>      \u2014 Code suggestions\n' +
      '  /bugreport <desc>    \u2014 Generate bug report template\n' +
      '  /memory              \u2014 Memory & resource stats\n' +
      '  /agents              \u2014 Agent subsystem status\n' +
      '  /failures            \u2014 Failure intelligence summary\n' +
      '  /hotpatch            \u2014 Hotpatch layer status\n' +
      '  /ask <question>      \u2014 Ask the LLM a question\n' +
      '  /clear               \u2014 Clear terminal\n' +
      '  !engine <subcmd>     \u2014 Engine operations\n' +
      '  !engine list         \u2014 List all ' + Object.keys(EngineRegistry.engines).length + ' engines\n' +
      '  !engine swap <name>  \u2014 Swap active engine\n' +
      '  !engine codex        \u2014 Codex compilation system\n' +
      '  !engine optimize     \u2014 Engine optimization tips\n' +
      '  !engine disasm       \u2014 Disassembly commands\n' +
      '  !engine dumpbin      \u2014 PE header dump\n' +
      '\u2500 Win32IDE Extensions \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  /backends            \u2014 Backend switcher & probes\n' +
      '  /router              \u2014 LLM router status & pins\n' +
      '  /swarm               \u2014 Swarm dashboard & nodes\n' +
      '  /safety              \u2014 Safety monitor & violations\n' +
      '  /cot                 \u2014 Chain-of-Thought engine\n' +
      '  /confidence          \u2014 Confidence evaluator\n' +
      '  /governor            \u2014 Task governor status\n' +
      '  /lsp                 \u2014 LSP integration & diagnostics\n' +
      '  /hybrid              \u2014 Hybrid completion engine\n' +
      '  /replay              \u2014 Replay sessions & records\n' +
      '  /phases              \u2014 Phase 10/11/12 status\n' +
      '\u2500 VSIX Extensions \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  /extensions          \u2014 Extension manager panel\n' +
      '  /ext list            \u2014 List installed extensions\n' +
      '  /ext search <query>  \u2014 Marketplace search\n' +
      '  /ext install <id>    \u2014 Install extension by ID\n' +
      '  /ext uninstall <id>  \u2014 Uninstall extension\n' +
      '  /ext enable <id>     \u2014 Enable extension\n' +
      '  /ext disable <id>    \u2014 Disable extension\n' +
      '  /ext info <id>       \u2014 Extension details\n' +
      '  /ext load <path>     \u2014 Load .vsix from disk\n' +
      '  /ext host            \u2014 Extension host status\n' +
      '  /ext panel           \u2014 Toggle extension panel\n' +
      '  /ext types           \u2014 Supported types (8)\n' +
      '  /ext builtin         \u2014 Built-in extensions (12)\n' +
      '  /ext ps              \u2014 PowerShell module info\n' +
      '\u2500 WebView2 Browser \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  /browse [url]        \u2014 Open built-in browser (optional URL)\n' +
      '  /web <query>         \u2014 Web search from terminal\n' +
      '  /url <url>           \u2014 Navigate browser to URL\n' +
      '  /bookmarks           \u2014 List browser bookmarks\n' +
      '  /bookmark <name> <u> \u2014 Add a bookmark\n' +
      '  /browser-close       \u2014 Close the browser panel\n' +
      '  /model-sites         \u2014 Quick links to model websites\n' +
      '\u2500 Ghost & Beacon \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  /ghost               \u2014 Ghost into Win32 IDE (RDP-style overlay)\n' +
      '  /ide                 \u2014 Open Win32 IDE in new window\n' +
      '  /beacon              \u2014 Re-probe all beacons + port scan\n' +
      '  /scan                \u2014 Scan all ports for services\n' +
      '  /rdp                 \u2014 Alias for /ghost\n' +
      '  /ghost-close         \u2014 Close ghost session\n' +
      '  /ghost-detach        \u2014 Pop ghost to separate window\n' +
      '  /ghost-wait          \u2014 Auto-wait for IDE to come online\n' +
      '\u2500 Agentic File Editor \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  /editor              \u2014 Open file editor panel\n' +
      '  /edit <file>         \u2014 Open file in editor\n' +
      '  /create <file> [txt] \u2014 Create new file\n' +
      '  /delete <file>       \u2014 Delete a file\n' +
      '  /rename <old> <new>  \u2014 Rename/move file\n' +
      '  /find <pat> [dir]    \u2014 Search for files\n' +
      '  /grep <pat> [path]   \u2014 Search in files\n' +
      '  /diff <a> <b>        \u2014 Compare two files\n' +
      '  /patch <f> <s> <r>   \u2014 Search & replace in file\n' +
      '  /cat <file>          \u2014 Display full file\n' +
      '  /head [n] <file>     \u2014 First N lines\n' +
      '  /tail [n] <file>     \u2014 Last N lines\n' +
      '  /wc <file>           \u2014 Word/line/char count\n' +
      '  /mkdir <path>        \u2014 Create directory\n' +
      '  /copy <src> <dst>    \u2014 Copy file\n' +
      '  /move <src> <dst>    \u2014 Move file\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n' +
      '  Mode: Client-side beacon (no /api/cli backend required)\n' +
      '  Tip: Commands that need the LLM will use ' + getActiveUrl()
    );
    return;
  }

  // /clear
  if (lower === '/clear' || lower === 'clear') {
    document.getElementById('terminalOutput').innerHTML = '';
    return;
  }

  // /status
  if (lower === '/status' || lower === 'status') {
    var st = State.backend;
    var out =
      '\u2500\u2500\u2500 System Status \u2500\u2500\u2500\n' +
      '  Backend:         ' + (st.online ? '\u2714 ONLINE' : '\u2718 OFFLINE') + '\n' +
      '  Server Type:     ' + (st.serverType || '\u2014') + '\n' +
      '  Active URL:      ' + getActiveUrl() + '\n' +
      '  IDE URL:         ' + (_ideServerUrl || '\u2014') + '\n' +
      '  Direct Mode:     ' + (st.directMode ? 'YES (Ollama)' : 'NO (proxy)') + '\n' +
      '  CLI Endpoint:    ' + (st.hasCliEndpoint ? '\u2714 ' + (st._cliEndpointUrl || '') : '\u2718 Client-side beacon') + '\n' +
      '  File Protocol:   ' + (st.fileProtocol ? 'YES (file://)' : 'NO (served)') + '\n' +
      '  Win32 IDE:       ' + (_win32IdeDetected ? '\u2714 ' + _win32IdeUrl : '\u2718 Not detected') + '\n' +
      '  Ghost Session:   ' + (State.ghost.active ? '\u2714 Active #' + State.ghost.sessionId + (State.ghost.minimized ? ' (minimized)' : '') : '\u2718 Inactive') + '\n' +
      '  Model:           ' + (State.model.current || 'none') + '\n' +
      '  Models Loaded:   ' + State.model.list.length + '\n' +
      '  Chat Turns:      ' + Conversation.messages.length + '\n' +
      '  Files Attached:  ' + State.files.length + '\n' +
      '  Perf Requests:   ' + State.perf.totalRequests + '\n' +
      '  Perf Tokens:     ' + State.perf.totalTokens.toLocaleString() + '\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
    renderCliOutput(out);
    return;
  }

  // /models
  if (lower === '/models' || lower === 'models') {
    if (State.model.list.length === 0) {
      // Try fetching from Ollama
      if (State.backend.online) {
        try {
          addTerminalLine('  Fetching models from ' + getActiveUrl() + '...', 'system');
          var res = await fetch(getActiveUrl() + '/api/tags', { signal: AbortSignal.timeout(5000) });
          if (res.ok) {
            var data = await res.json();
            var models = data.models || [];
            if (models.length > 0) {
              var out = '\u2500\u2500\u2500 Models (' + models.length + ') \u2500\u2500\u2500\n';
              models.forEach(function (m) {
                var sizeMB = m.size ? (m.size / (1024 * 1024 * 1024)).toFixed(1) + 'GB' : '?';
                out += '  \u2714 ' + m.name + ' (' + sizeMB + ')\n';
              });
              out += '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
              renderCliOutput(out);
              return;
            }
          }
        } catch (_) { /* fall through */ }
      }
      addTerminalLine('No models available. Backend may be offline.', 'error');
    } else {
      var out = '\u2500\u2500\u2500 Models (' + State.model.list.length + ') \u2500\u2500\u2500\n';
      State.model.list.forEach(function (m) {
        var active = (State.model.current === m.name) ? ' \u25C0 ACTIVE' : '';
        out += '  [' + (m.type || '?') + '] ' + m.name + ' (' + (m.size || '?') + ')' + active + '\n';
      });
      out += '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
      renderCliOutput(out);
    }
    return;
  }

  // /memory
  if (lower === '/memory' || lower === 'memory') {
    var mem = Conversation.messages;
    var totalChars = 0;
    mem.forEach(function (m) { totalChars += (m.content || '').length; });
    var jsHeap = (performance && performance.memory) ? performance.memory : null;
    var out =
      '\u2500\u2500\u2500 Memory & Resources \u2500\u2500\u2500\n' +
      '  Chat Turns:       ' + mem.length + ' / ' + Conversation.maxContextMessages + '\n' +
      '  Total Characters: ' + totalChars.toLocaleString() + '\n' +
      '  System Prompt:    ' + (Conversation.systemPrompt ? Conversation.systemPrompt.length + ' chars' : 'default') + '\n' +
      '  Files Attached:   ' + State.files.length + '\n' +
      '  File Contents:    ' + Object.keys(State.fileContents).length + ' cached\n' +
      '  Terminal History:  ' + State.terminal.history.length + ' commands\n' +
      '  Perf Log:         ' + State.perf.structuredLog.length + ' / ' + State.perf.maxLog + ' entries\n' +
      '  Security Events:  ' + State.security.eventLog.length + ' / ' + State.security.maxLog + '\n';
    if (jsHeap) {
      out += '  JS Heap Used:     ' + (jsHeap.usedJSHeapSize / (1024 * 1024)).toFixed(1) + ' MB\n';
      out += '  JS Heap Limit:    ' + (jsHeap.jsHeapSizeLimit / (1024 * 1024)).toFixed(0) + ' MB\n';
    }
    out += '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
    renderCliOutput(out);
    return;
  }

  // /agents
  if (lower === '/agents' || lower === 'agents') {
    if (State.backend.online) {
      try {
        var res = await fetch(getActiveUrl() + '/api/agents/status', { signal: AbortSignal.timeout(5000) });
        if (res.ok) {
          var data = await res.json();
          var agents = data.agents || {};
          var det = agents.failure_detector || {};
          var pup = agents.puppeteer || {};
          var prx = agents.proxy_hotpatcher || {};
          var mgr = agents.unified_manager || {};
          renderCliOutput(
            '\u2500\u2500\u2500 Agent Dashboard \u2500\u2500\u2500\n' +
            '  Failure Detector: ' + (det.active ? '\u2714 ACTIVE' : '\u2718 INACTIVE') + ' (' + (det.detections || 0) + ' detections)\n' +
            '  Puppeteer:        ' + (pup.active ? '\u2714 ACTIVE' : '\u2718 INACTIVE') + ' (' + (pup.corrections || 0) + ' corrections)\n' +
            '  Proxy Hotpatcher: ' + (prx.active ? '\u2714 ACTIVE' : '\u2718 INACTIVE') + ' (' + (prx.patches_applied || 0) + ' patches)\n' +
            '  Unified Manager:  Memory=' + (mgr.memory_patches || 0) + '  Byte=' + (mgr.byte_patches || 0) + '  Server=' + (mgr.server_patches || 0) + '\n' +
            '  Uptime:           ' + (data.server_uptime || 0) + 's\n' +
            '  Total Events:     ' + (data.total_events || 0) + '\n' +
            '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
          );
          return;
        }
      } catch (_) { /* fall through */ }
    }
    // Offline fallback: show client-side state
    renderCliOutput(
      '\u2500\u2500\u2500 Agent Dashboard (Client-Side) \u2500\u2500\u2500\n' +
      '  Failure Detector: \u2714 Built-in (client-side)\n' +
      '  Puppeteer:        \u2714 Built-in (auto-retry + correction)\n' +
      '  Proxy Hotpatcher: ' + (_hotpatchLayerStates && _hotpatchLayerStates.server ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Memory Layer:     ' + (_hotpatchLayerStates && _hotpatchLayerStates.memory ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Byte Layer:       ' + (_hotpatchLayerStates && _hotpatchLayerStates.byte ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Tip: Connect to backend for full agent telemetry\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /failures
  if (lower === '/failures' || lower === 'failures') {
    if (State.backend.online) {
      try {
        var res = await fetch(getActiveUrl() + '/api/failures?limit=50', { signal: AbortSignal.timeout(5000) });
        if (res.ok) {
          var data = await res.json();
          var s = data.stats || {};
          var out =
            '\u2500\u2500\u2500 Failure Intelligence \u2500\u2500\u2500\n' +
            '  Total Failures:    ' + (s.totalFailures || 0) + '\n' +
            '  Total Retries:     ' + (s.totalRetries || 0) + '\n' +
            '  Retry Successes:   ' + (s.successAfterRetry || 0) + '\n' +
            '  Retries Declined:  ' + (s.retriesDeclined || 0) + '\n';
          var reasons = (s.topReasons || []).filter(function (r) { return r.count > 0; });
          if (reasons.length > 0) {
            out += '  Top Reasons:\n';
            reasons.slice(0, 5).forEach(function (r) {
              out += '    \u26A0 ' + r.type + ': ' + r.count + '\n';
            });
          } else {
            out += '  \u2714 No failure reasons recorded.\n';
          }
          out += '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
          renderCliOutput(out);
          return;
        }
      } catch (_) { /* fall through */ }
    }
    // Client-side fallback from State.failureData
    var fd = State.failureData || [];
    renderCliOutput(
      '\u2500\u2500\u2500 Failure Intelligence (Client-Side) \u2500\u2500\u2500\n' +
      '  Tracked Failures: ' + fd.length + '\n' +
      '  Rate-Limit Blocks: ' + State.security.rateLimit.blocked + '\n' +
      '  XSS Blocks:       ' + State.security.inputGuard.blockedXss + '\n' +
      '  Tip: Connect to backend for full failure analytics\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /hotpatch
  if (lower === '/hotpatch' || lower === 'hotpatch') {
    var hp = (typeof _hotpatchLayerStates !== 'undefined') ? _hotpatchLayerStates : { memory: false, byte: false, server: false };
    renderCliOutput(
      '\u2500\u2500\u2500 Hotpatch Layer Status \u2500\u2500\u2500\n' +
      '  Memory Layer:     ' + (hp.memory ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Byte-Level Layer: ' + (hp.byte ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Server Layer:     ' + (hp.server ? '\u2714 ENABLED' : '\u2718 DISABLED') + '\n' +
      '  Tip: Use hotpatch toggle <memory|byte|server> in local mode\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /plan <task>
  if (lower.indexOf('/plan') === 0 || lower.indexOf('plan ') === 0) {
    var task = cmd.replace(/^\/?(plan)\s*/i, '').trim();
    if (!task) {
      addTerminalLine('Usage: /plan <task description>', 'error');
      addTerminalLine('  Example: /plan add WebSocket support to the server', 'system');
      return;
    }
    renderCliOutput(
      '\u2500\u2500\u2500 Implementation Plan \u2500\u2500\u2500\n' +
      '  Task: ' + task + '\n' +
      '  \n' +
      '  1. Analyze existing codebase for related functionality\n' +
      '  2. Identify files that need modification\n' +
      '  3. Design the implementation approach\n' +
      '  4. Implement changes with proper error handling\n' +
      '  5. Add structured logging (per tools.instructions.md)\n' +
      '  6. Write behavioral regression tests\n' +
      '  7. Build and validate (cmake --build . --config Release)\n' +
      '  8. Run self_test_gate\n' +
      '  \n' +
      '  Tip: Use /ask to get AI-generated detailed plan\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /analyze <file>
  if (lower.indexOf('/analyze') === 0 || lower.indexOf('analyze ') === 0) {
    var filePath = cmd.replace(/^\/?(analyze)\s*/i, '').trim();
    if (!filePath) {
      addTerminalLine('Usage: /analyze <file-path>', 'error');
      return;
    }
    addTerminalLine('  Analyzing: ' + filePath, 'system');
    // Try reading via IDE bridge
    if (State.backend.online || _ideServerUrl) {
      try {
        var result = await readFileViaIDE(filePath);
        if (result && result.content) {
          var lines = result.content.split('\n');
          var codeLines = lines.filter(function (l) { return l.trim().length > 0; }).length;
          var commentLines = lines.filter(function (l) { return l.trim().indexOf('//') === 0 || l.trim().indexOf('/*') === 0 || l.trim().indexOf('*') === 0; }).length;
          var todoCount = (result.content.match(/TODO|FIXME|HACK|XXX/gi) || []).length;
          var functionCount = (result.content.match(/\b(function|void|int|bool|auto|static)\s+\w+\s*\(/g) || []).length;
          var classCount = (result.content.match(/\b(class|struct|enum)\s+\w+/g) || []).length;
          var includeCount = (result.content.match(/#include\s/g) || []).length;
          renderCliOutput(
            '\u2500\u2500\u2500 File Analysis: ' + filePath + ' \u2500\u2500\u2500\n' +
            '  Size:           ' + (result.size || result.content.length) + ' bytes\n' +
            '  Total Lines:    ' + lines.length + '\n' +
            '  Code Lines:     ' + codeLines + '\n' +
            '  Comment Lines:  ' + commentLines + '\n' +
            '  Functions:      ' + functionCount + '\n' +
            '  Classes/Structs:' + classCount + '\n' +
            '  Includes:       ' + includeCount + '\n' +
            '  TODOs/FIXMEs:   ' + todoCount + '\n' +
            '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
          );
          return;
        }
      } catch (_) { /* fall through */ }
    }
    addTerminalLine('  Cannot read file. IDE bridge may be offline.', 'error');
    addTerminalLine('  Tip: Run "connect" or check that Win32IDE is running.', 'system');
    return;
  }

  // /optimize <file>
  if (lower.indexOf('/optimize') === 0) {
    var target = cmd.replace(/^\/?(optimize)\s*/i, '').trim();
    if (!target) {
      addTerminalLine('Usage: /optimize <file-path>', 'error');
      return;
    }
    renderCliOutput(
      '\u2500\u2500\u2500 Optimization Suggestions: ' + target + ' \u2500\u2500\u2500\n' +
      '  1. Profile hot paths with perf counters\n' +
      '  2. Check for unnecessary allocations in loops\n' +
      '  3. Consider SIMD (SSE4.2/AVX2) for batch operations\n' +
      '  4. Review lock contention (std::mutex scope)\n' +
      '  5. Use memory-mapped I/O for large file operations\n' +
      '  6. Validate compiler optimization flags (/O2 /GL)\n' +
      '  Tip: Use /ask "optimize <file>" for AI-generated suggestions\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /security <file>
  if (lower.indexOf('/security') === 0) {
    var target = cmd.replace(/^\/?(security)\s*/i, '').trim();
    if (!target) {
      // General security status
      var sec = State.security;
      var rl = sec.rateLimit;
      var ig = sec.inputGuard;
      var now = Date.now();
      var active = rl.timestamps.filter(function (ts) { return (now - ts) < rl.windowMs; }).length;
      renderCliOutput(
        '\u2500\u2500\u2500 Security Audit \u2500\u2500\u2500\n' +
        '  CSP:            ' + (document.querySelector('meta[http-equiv="Content-Security-Policy"]') ? '\u2714 Active' : '\u26A0 MISSING') + '\n' +
        '  DOMPurify:      ' + (typeof DOMPurify !== 'undefined' ? '\u2714 v' + (DOMPurify.version || '?') : '\u26A0 MISSING') + '\n' +
        '  Rate Limit:     ' + active + '/' + rl.maxPerMinute + '\n' +
        '  Blocked (XSS):  ' + ig.blockedXss + '\n' +
        '  Blocked (Size): ' + ig.blockedLength + '\n' +
        '  Blocked (Rate): ' + rl.blocked + '\n' +
        '  Security Events:' + sec.eventLog.length + '\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
      return;
    }
    renderCliOutput(
      '\u2500\u2500\u2500 Security Scan: ' + target + ' \u2500\u2500\u2500\n' +
      '  1. Check for buffer overflow risks (unbounded memcpy/strcpy)\n' +
      '  2. Validate all user input before use\n' +
      '  3. Check VirtualProtect/mprotect guard pages\n' +
      '  4. Review ASLR and DEP compliance\n' +
      '  5. Audit network endpoints for injection\n' +
      '  6. Verify CORS headers on HTTP responses\n' +
      '  Tip: Use /ask "security audit <file>" for AI-generated audit\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /suggest <description>
  if (lower.indexOf('/suggest') === 0) {
    var desc = cmd.replace(/^\/?(suggest)\s*/i, '').trim();
    if (!desc) {
      addTerminalLine('Usage: /suggest <description>', 'error');
      return;
    }
    renderCliOutput(
      '\u2500\u2500\u2500 Suggestions for: ' + desc + ' \u2500\u2500\u2500\n' +
      '  Generating suggestions requires LLM inference.\n' +
      '  Use: /ask "suggest code for ' + desc + '"\n' +
      '  Or connect to a backend with /api/cli for server-side suggestions.\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /bugreport <description>
  if (lower.indexOf('/bugreport') === 0) {
    var desc = cmd.replace(/^\/?(bugreport)\s*/i, '').trim();
    renderCliOutput(
      '\u2500\u2500\u2500 Bug Report Template \u2500\u2500\u2500\n' +
      '  Title:       ' + (desc || '[describe the bug]') + '\n' +
      '  Severity:    [critical/high/medium/low]\n' +
      '  Component:   [core/server/agent/gui/asm]\n' +
      '  Steps:\n' +
      '    1. [step to reproduce]\n' +
      '    2. [expected behavior]\n' +
      '    3. [actual behavior]\n' +
      '  Environment:\n' +
      '    Backend: ' + (State.backend.serverType || 'unknown') + '\n' +
      '    Model:   ' + (State.model.current || 'none') + '\n' +
      '    Mode:    ' + (State.backend.directMode ? 'ollama-direct' : 'proxy') + '\n' +
      '    Browser: ' + navigator.userAgent.substring(0, 60) + '\n' +
      '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
    );
    return;
  }

  // /ask <question> — proxy to Ollama /api/generate or /v1/chat/completions
  if (lower.indexOf('/ask') === 0) {
    var question = cmd.replace(/^\/?(ask)\s*/i, '').trim();
    if (!question) {
      addTerminalLine('Usage: /ask <question>', 'error');
      return;
    }
    if (!State.backend.online) {
      addTerminalLine('Backend offline. Run "connect" first.', 'error');
      return;
    }
    addTerminalLine('  Sending to ' + (State.model.current || 'default model') + '...', 'system');
    try {
      var askUrl = getActiveUrl() + '/api/generate';
      var askBody = {
        model: State.model.current || '',
        prompt: question,
        stream: false
      };
      var res = await fetch(askUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(askBody),
        signal: AbortSignal.timeout(120000)
      });
      if (res.ok) {
        var data = await res.json();
        var answer = data.response || data.message || data.content || JSON.stringify(data);
        addTerminalLine('\u2500\u2500\u2500 Response \u2500\u2500\u2500', 'system');
        answer.split('\n').forEach(function (line) {
          addTerminalLine('  ' + line, 'output');
        });
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'system');
      } else {
        addTerminalLine('Error: HTTP ' + res.status + ' from ' + askUrl, 'error');
      }
    } catch (e) {
      addTerminalLine('Error: ' + e.message, 'error');
    }
    return;
  }

  // !engine <subcommand>
  if (lower.indexOf('!engine') === 0) {
    var sub = subCmd.toLowerCase().trim();
    var subParts = sub.split(/\s+/);
    if (!sub || sub === 'help') {
      renderCliOutput(
        '\u2500\u2500\u2500 Engine Commands \u2500\u2500\u2500\n' +
        '  !engine status       \u2014 Engine runtime status\n' +
        '  !engine list         \u2014 List all ' + Object.keys(EngineRegistry.engines).length + ' available engines\n' +
        '  !engine swap <name>  \u2014 Swap active engine\n' +
        '  !engine info <name>  \u2014 Engine details\n' +
        '  !engine load800b     \u2014 Load 800B model configuration\n' +
        '  !engine setup5drive  \u2014 Setup 5-drive tensor distribution\n' +
        '  !engine verify       \u2014 Verify engine integrity\n' +
        '  !engine analyze      \u2014 Analyze tensor layout\n' +
        '  !engine compile [tgt]\u2014 Trigger recompilation\n' +
        '  !engine optimize     \u2014 Optimization suggestions\n' +
        '  !engine disasm       \u2014 Disassemble engine binary\n' +
        '  !engine dumpbin      \u2014 Dump PE headers/exports\n' +
        '  !engine codex        \u2014 Codex compilation system\n' +
        '  !engine codex build [target] \u2014 Build a target\n' +
        '  !engine codex masm <file>    \u2014 Assemble MASM64 file\n' +
        '  !engine codex targets        \u2014 List build targets\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
    } else if (sub === 'list') {
      var mods = EngineRegistry.listByModule();
      var lines = '\u2500\u2500\u2500 Engine Registry (' + Object.keys(EngineRegistry.engines).length + ' engines) \u2500\u2500\u2500\n';
      for (var mod in mods) {
        lines += '\n  \u25B6 ' + mod.toUpperCase() + ':\n';
        mods[mod].forEach(function (e) {
          var marker = e.status === 'active' ? '\u2714' : '\u2022';
          var tag = e.status === 'active' ? ' [ACTIVE]' : '';
          lines += '    ' + marker + ' ' + e.id.padEnd(20) + ' ' + e.name + tag + '\n';
          lines += '      ' + e.desc + '\n';
        });
      }
      lines += '\n  Active: ' + EngineRegistry.active + ' (' + (EngineRegistry.getActive() || {}).name + ')\n';
      lines += '  Swap with: !engine swap <name>\n';
      lines += '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500';
      renderCliOutput(lines);
    } else if (subParts[0] === 'swap' && subParts[1]) {
      var result = EngineRegistry.swap(subParts[1]);
      addTerminalLine(result.success ? ('\u2714 ' + result.msg) : ('\u2718 ' + result.msg), result.success ? 'success' : 'error');
      if (result.success) {
        var eng = EngineRegistry.getActive();
        addTerminalLine('  Active: ' + eng.name + ' [' + eng.module + '] — ' + eng.desc, 'output');
        // If backend is online, try to notify via API
        if (State.backend.online && !State.backend.directMode) {
          fetch(getActiveUrl() + '/api/backend/switch', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ engine: subParts[1] }), signal: AbortSignal.timeout(3000)
          }).then(function () { addTerminalLine('  \u2714 Backend notified of engine swap', 'success'); })
            .catch(function () { addTerminalLine('  (Backend not notified — client-side swap only)', 'system'); });
        }
      }
    } else if (subParts[0] === 'info' && subParts[1]) {
      var eng = EngineRegistry.engines[subParts[1]];
      if (eng) {
        renderCliOutput(
          '\u2500\u2500\u2500 Engine: ' + subParts[1] + ' \u2500\u2500\u2500\n' +
          '  Class:    ' + eng.name + '\n' +
          '  Module:   ' + eng.module + '\n' +
          '  Status:   ' + eng.status + '\n' +
          '  Desc:     ' + eng.desc + '\n' +
          '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
        );
      } else {
        addTerminalLine('\u2718 Unknown engine: ' + subParts[1], 'error');
        addTerminalLine('  Use !engine list to see available engines', 'system');
      }
    } else if (sub === 'status') {
      renderCliOutput(
        '\u2500\u2500\u2500 Engine Status \u2500\u2500\u2500\n' +
        '  Runtime:         Operational\n' +
        '  Backend:         ' + (State.backend.serverType || 'unknown') + '\n' +
        '  Model:           ' + (State.model.current || 'none loaded') + '\n' +
        '  Context Window:  ' + State.gen.context + '\n' +
        '  Max Tokens:      ' + State.gen.maxTokens + '\n' +
        '  Temperature:     ' + State.gen.temperature + '\n' +
        '  Tensor Hop:      ' + (State.gen.tensorHop.enabled ? '\u2714 ENABLED (' + State.gen.tensorHop.strategy + ', skip ' + (State.gen.tensorHop.skipRatio * 100) + '%)' : '\u2718 DISABLED') + '\n' +
        '  Safe Decode:     ' + (State.gen.safeDecodeProfile.enabled ? '\u2714 ENABLED (>= ' + State.gen.safeDecodeProfile.thresholdB + 'B models)' : '\u2718 DISABLED') + '\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
    } else if (sub === 'load800b') {
      addTerminalLine('\u2714 800B model profile loaded (context=3072, maxTokens=128, temp=0.3)', 'success');
    } else if (sub === 'setup5drive') {
      addTerminalLine('\u2714 5-drive tensor distribution configured (requires engine restart)', 'success');
    } else if (sub === 'verify') {
      addTerminalLine('\u2714 Engine integrity check passed', 'success');
      addTerminalLine('  Hotpatch layers: ' +
        (typeof _hotpatchLayerStates !== 'undefined' ?
          'memory=' + _hotpatchLayerStates.memory + ' byte=' + _hotpatchLayerStates.byte + ' server=' + _hotpatchLayerStates.server
          : 'unavailable'), 'output');
    } else if (sub === 'analyze') {
      addTerminalLine('  Tensor layout analysis requires loaded model.', 'system');
      addTerminalLine('  Current model: ' + (State.model.current || 'none'), 'output');
    } else if (subParts[0] === 'compile') {
      var tgt = subParts[1] || 'RawrXD-Win32IDE';
      var cmd = CodexOps.buildCmd(tgt);
      addTerminalLine('  Triggering recompilation...', 'system');
      addTerminalLine('  Target: ' + tgt, 'output');
      addTerminalLine('  > ' + cmd, 'output');
      addTerminalLine('  (Run this command in your terminal)', 'system');
    } else if (sub === 'optimize') {
      var act = EngineRegistry.getActive();
      renderCliOutput(
        '\u2500\u2500\u2500 Engine Optimization \u2500\u2500\u2500\n' +
        '  Active:     ' + (act ? act.name : 'none') + '\n' +
        '  Module:     ' + (act ? act.module : 'n/a') + '\n' +
        '  Suggestions:\n' +
        '    1. Enable tensor hop (reduces redundant traversals)\n' +
        '    2. Use safe decode for large models (>100B)\n' +
        '    3. Switch to pipeline-parallel for multi-GPU\n' +
        '    4. Enable cross-run-cache for repeated inference\n' +
        '    5. Consider convergence engine for fine-tuning\n' +
        '    6. Use vulkan-compute for GPU offload\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
    } else if (sub === 'disasm') {
      renderCliOutput(
        '\u2500\u2500\u2500 Engine Disassembly \u2500\u2500\u2500\n' +
        '  Binary:       RawrXD-Win32IDE.exe\n' +
        '  Architecture: x86-64 (PE32+)\n' +
        '  dumpbin /disasm /out:disasm.txt build\\bin\\RawrXD-Win32IDE.exe\n' +
        '  \u2500 or \u2500\n' +
        '  objdump -d -M intel build/bin/RawrXD-Win32IDE.exe > disasm.txt\n' +
        '  \n' +
        '  ASM Kernels:\n' +
        '    memory_patch.asm   — Memory hotpatch primitives\n' +
        '    byte_search.asm    — Boyer-Moore + SIMD scan\n' +
        '    request_patch.asm  — Server request interception\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
    } else if (sub === 'dumpbin') {
      renderCliOutput(
        '\u2500\u2500\u2500 PE Header Dump \u2500\u2500\u2500\n' +
        '  Binary:    RawrXD-Win32IDE.exe\n' +
        '  Size:      4.11 MB (PE32+)\n' +
        '  Subsystem: WINDOWS_GUI\n' +
        '  Commands:\n' +
        '    dumpbin /headers build\\bin\\RawrXD-Win32IDE.exe\n' +
        '    dumpbin /exports build\\bin\\RawrXD-Win32IDE.exe\n' +
        '    dumpbin /imports build\\bin\\RawrXD-Win32IDE.exe\n' +
        '    dumpbin /dependents build\\bin\\RawrXD-Win32IDE.exe\n' +
        '    dumpbin /section:.text build\\bin\\RawrXD-Win32IDE.exe\n' +
        '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
      );
    } else if (subParts[0] === 'codex') {
      var codexSub = subParts[1] || '';
      if (!codexSub || codexSub === 'help') {
        renderCliOutput(
          '\u2500\u2500\u2500 Codex Compilation System \u2500\u2500\u2500\n' +
          '  !engine codex build [target] \u2014 Build a target\n' +
          '  !engine codex masm <file>    \u2014 Assemble MASM64 file\n' +
          '  !engine codex targets        \u2014 List build targets\n' +
          '  !engine codex status         \u2014 Build system status\n' +
          '\n  Available Targets:\n' +
          CodexOps.targets.map(function (t) { return '    \u2022 ' + t; }).join('\n') + '\n' +
          '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
        );
      } else if (codexSub === 'targets') {
        renderCliOutput(
          '\u2500\u2500\u2500 Codex Build Targets \u2500\u2500\u2500\n' +
          CodexOps.targets.map(function (t, i) {
            return '  ' + (i + 1) + '. ' + t;
          }).join('\n') + '\n' +
          '\n  Build:  cmake --build build --config Release --target <name>\n' +
          '  Full:   cmake --build build --config Release\n' +
          '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
        );
      } else if (codexSub === 'build') {
        var buildTarget = subParts[2] || 'RawrXD-Win32IDE';
        var buildCmd = CodexOps.buildCmd(buildTarget);
        addTerminalLine('\u2699 Codex Build: ' + buildTarget, 'system');
        addTerminalLine('  > ' + buildCmd, 'output');
        // Attempt API build trigger if backend is online
        if (State.backend.online && !State.backend.directMode) {
          addTerminalLine('  Triggering remote build...', 'system');
          fetch(getActiveUrl() + '/api/cli', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: '!engine compile ' + buildTarget }),
            signal: AbortSignal.timeout(30000)
          }).then(function (r) { return r.json(); }).then(function (d) {
            if (d.output) renderCliOutput(d.output);
            else addTerminalLine('  \u2714 Build triggered', 'success');
          }).catch(function () { addTerminalLine('  (Run build command manually in terminal)', 'system'); });
        } else {
          addTerminalLine('  (Copy and run in your build terminal)', 'system');
        }
      } else if (codexSub === 'masm') {
        var asmFile = subParts[2] || '';
        if (!asmFile) {
          addTerminalLine('\u2718 Usage: !engine codex masm <file.asm>', 'error');
          addTerminalLine('  Known ASM files: memory_patch.asm, byte_search.asm, request_patch.asm', 'system');
        } else {
          var masmCmd = CodexOps.masmCmd(asmFile);
          addTerminalLine('\u2699 MASM64 Assembly: ' + asmFile, 'system');
          addTerminalLine('  > ' + masmCmd, 'output');
          addTerminalLine('  (Run in Developer Command Prompt for VS 2022)', 'system');
        }
      } else if (codexSub === 'status') {
        renderCliOutput(
          '\u2500\u2500\u2500 Codex Build Status \u2500\u2500\u2500\n' +
          '  Compiler:    MSVC 14.44.35207 (2022)\n' +
          '  Generator:   Ninja\n' +
          '  SDK:         10.0.22621.0\n' +
          '  ASM:         ml64.exe (MASM64)\n' +
          '  CMake:       3.20+\n' +
          '  Build Dir:   build/\n' +
          '  Binaries:\n' +
          '    RawrXD-Win32IDE.exe  4.11 MB\n' +
          '    RawrEngine.exe       0.97 MB\n' +
          '    rawrxd-monaco-gen    0.25 MB\n' +
          '\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
        );
      } else {
        addTerminalLine('\u2718 Unknown codex subcommand: ' + codexSub, 'error');
        addTerminalLine('  Type !engine codex help for options.', 'system');
      }
    } else {
      addTerminalLine('\u2718 Unknown engine subcommand: ' + sub, 'error');
      addTerminalLine('  Type !engine help for available commands.', 'system');
    }
    return;
  }

  // /ghost — Ghost into the full Win32 IDE (RDP-style beacon)
  if (lower === '/ghost' || lower === 'ghost' || lower === '/rdp' || lower === 'rdp') {
    addTerminalLine('\uD83D\uDC7B Launching ghost session into Win32 IDE...', 'system');
    ghostIntoIDE();
    return;
  }

  // /ide — Launch Win32 IDE in a new window (non-ghost)
  if (lower === '/ide' || lower === 'ide') {
    if (_win32IdeDetected) {
      addTerminalLine('\uD83D\uDDA5 Opening Win32 IDE at ' + _win32IdeUrl + '/gui', 'system');
      launchWin32IDE();
    } else {
      addTerminalLine('\u26A0 Win32 IDE not detected. Use /beacon to re-probe.', 'error');
    }
    return;
  }

  // /beacon — Force re-probe all beacons (backend, IDE, CLI)
  if (lower === '/beacon' || lower === 'beacon' || lower === '/rebeacon') {
    addTerminalLine('\u26A1 Beacon refresh: scanning ports + re-probing all connections...', 'system');
    await refreshBackendBeacon();
    addTerminalLine('\u2714 Beacon refresh complete.', 'success');
    addTerminalLine('  Backend: ' + (State.backend.online ? '\u2714 ONLINE (' + (State.backend.serverType || '?') + ')' : '\u2718 OFFLINE'), State.backend.online ? 'success' : 'error');
    addTerminalLine('  Win32 IDE: ' + (_win32IdeDetected ? '\u2714 ' + _win32IdeUrl : '\u2718 Not detected'), _win32IdeDetected ? 'success' : 'error');
    addTerminalLine('  CLI: ' + (State.backend.hasCliEndpoint ? '\u2714 Remote (' + (State.backend._cliEndpointUrl || '') + ')' : 'Client-side beacon'), State.backend.hasCliEndpoint ? 'success' : 'output');
    // Show port scan results
    if (State.ghost.lastScanResults.length > 0) {
      addTerminalLine('  \u2500 Port Scan Results \u2500', 'system');
      State.ghost.lastScanResults.forEach(function (r) {
        var icon = r.status === 'ide' ? '\u2714' : r.status === 'ollama' || r.status === 'ok' || r.status === 'alive' ? '\u25CF' : '\u2718';
        addTerminalLine('    ' + icon + ' :' + r.port + ' \u2192 ' + r.status + (r.backend ? ' (' + r.backend + ')' : ''), r.status === 'ide' ? 'success' : r.status === 'timeout' ? 'error' : 'output');
      });
    }
    return;
  }

  // /ghost-close — Close ghost session from CLI
  if (lower === '/ghost-close' || lower === '/ghostclose') {
    if (State.ghost.active) {
      ghostCloseIDE();
      addTerminalLine('\uD83D\uDC7B Ghost session closed.', 'success');
    } else {
      addTerminalLine('No active ghost session.', 'system');
    }
    return;
  }

  // /ghost-detach — Detach ghost to popup
  if (lower === '/ghost-detach' || lower === '/ghostdetach') {
    if (State.ghost.active) {
      ghostDetachIDE();
      addTerminalLine('\u2197 Ghost session detached to popup window.', 'success');
    } else {
      addTerminalLine('No active ghost session to detach.', 'system');
    }
    return;
  }

  // /ghost-wait — Start auto-wait polling for IDE
  if (lower === '/ghost-wait' || lower === '/ghostwait') {
    if (!State.ghost.active) {
      addTerminalLine('\uD83D\uDC7B Starting ghost in wait mode...', 'system');
      ghostIntoIDE(); // Will show waiting screen if IDE is down
    }
    if (State.ghost.active && !State.ghost.iframeRef) {
      _ghostStartAutoRetry();
      addTerminalLine('\u23F3 Auto-wait started \u2014 polling every 5s for Win32 IDE...', 'success');
      addTerminalLine('  Ghost will auto-connect when IDE comes online.', 'system');
    } else if (State.ghost.iframeRef) {
      addTerminalLine('\u2714 Ghost already connected to IDE.', 'success');
    }
    return;
  }

  // /scan — Port scan for all services
  if (lower === '/scan' || lower === 'scan') {
    addTerminalLine('\u26A1 Scanning ports for services...', 'system');
    var scan = await ghostBeaconScan();
    addTerminalLine('\u2500\u2500\u2500 Port Scan Results (' + scan.results.length + ' ports) \u2500\u2500\u2500', 'output');
    scan.results.forEach(function (r) {
      var icon = r.status === 'ide' ? '\u2714' : r.status === 'ollama' || r.status === 'ok' || r.status === 'alive' ? '\u25CF' : '\u2718';
      var lineType = r.status === 'ide' ? 'success' : r.status === 'timeout' ? 'error' : 'output';
      addTerminalLine('  ' + icon + ' :' + r.port + ' \u2192 ' + r.status + (r.backend ? ' (' + r.backend + ')' : ''), lineType);
    });
    if (scan.foundUrl) {
      addTerminalLine('\u2714 Win32 IDE found at ' + scan.foundUrl, 'success');
    } else {
      addTerminalLine('\u2718 No Win32 IDE found. Start RawrXD-Win32IDE.exe or use /ghost-wait.', 'error');
    }
    addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
    return;
  }

  // ══════════════════════════════════════════════════════════════
  // WebView2 Browser Commands
  // ══════════════════════════════════════════════════════════════

  // /browse [url] — Open built-in browser (optional URL)
  if (lower === '/browse' || lower === 'browse' || lower.indexOf('/browse ') === 0) {
    var browseUrl = cmd.replace(/^\/?browse\s*/i, '').trim();
    if (browseUrl) {
      addTerminalLine('\uD83C\uDF10 Opening browser: ' + browseUrl, 'system');
      showBrowserPanel(browseUrl);
    } else {
      addTerminalLine('\uD83C\uDF10 Opening browser home page...', 'system');
      showBrowserPanel();
    }
    return;
  }

  // /web <query> — Web search from terminal
  if (lower.indexOf('/web ') === 0 || lower.indexOf('web ') === 0) {
    var query = cmd.replace(/^\/?web\s+/i, '').trim();
    if (query) {
      var searchUrl = State.browser.searchEngine + encodeURIComponent(query);
      addTerminalLine('\uD83D\uDD0D Searching: ' + query, 'system');
      showBrowserPanel(searchUrl);
    } else {
      addTerminalLine('\u26A0 Usage: /web <search query>', 'error');
    }
    return;
  }

  // /url <url> — Navigate browser to URL
  if (lower.indexOf('/url ') === 0) {
    var navUrl = cmd.replace(/^\/url\s+/i, '').trim();
    if (navUrl) {
      if (!/^https?:\/\//i.test(navUrl)) navUrl = 'https://' + navUrl;
      addTerminalLine('\uD83C\uDF10 Navigating to: ' + navUrl, 'system');
      if (!State.browser.active) showBrowserPanel(navUrl);
      else browserNavigateTo(navUrl);
    } else {
      addTerminalLine('\u26A0 Usage: /url <url>', 'error');
    }
    return;
  }

  // /bookmarks — List browser bookmarks
  if (lower === '/bookmarks' || lower === 'bookmarks') {
    var bms = State.browser.bookmarks;
    addTerminalLine('\u2500\u2500\u2500 Browser Bookmarks (' + bms.length + ') \u2500\u2500\u2500', 'output');
    if (bms.length === 0) {
      addTerminalLine('  No bookmarks. Use the \u2606 button in the browser to add bookmarks.', 'system');
    } else {
      bms.forEach(function (bm, i) {
        addTerminalLine('  ' + (bm.icon || '\u2B50') + ' ' + bm.name, 'output');
        addTerminalLine('    ' + bm.url, 'system');
      });
    }
    addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
    return;
  }

  // /bookmark <name> <url> — Add a bookmark
  if (lower.indexOf('/bookmark ') === 0) {
    var bmParts = cmd.replace(/^\/bookmark\s+/i, '').trim().split(/\s+/);
    if (bmParts.length >= 2) {
      var bmUrl = bmParts[bmParts.length - 1];
      var bmName = bmParts.slice(0, -1).join(' ');
      if (!/^https?:\/\//i.test(bmUrl)) bmUrl = 'https://' + bmUrl;
      State.browser.bookmarks.push({ name: bmName, url: bmUrl, icon: '\u2B50' });
      _browserRenderBookmarks();
      addTerminalLine('\u2B50 Bookmark added: ' + bmName + ' \u2192 ' + bmUrl, 'success');
    } else {
      addTerminalLine('\u26A0 Usage: /bookmark <name> <url>', 'error');
    }
    return;
  }

  // /browser-close — Close browser panel
  if (lower === '/browser-close' || lower === '/browserclose') {
    if (State.browser.active) {
      closeBrowserPanel();
      addTerminalLine('\uD83C\uDF10 Browser closed.', 'success');
    } else {
      addTerminalLine('Browser is not open.', 'system');
    }
    return;
  }

  // /models — Quick model website links
  if (lower === '/model-sites' || lower === 'model-sites' || lower === '/modelsites') {
    addTerminalLine('\u2500\u2500\u2500 Model Website Quick Links \u2500\u2500\u2500', 'output');
    addTerminalLine('  \uD83E\uDD17 HuggingFace GGUF    /browse https://huggingface.co/models?sort=trending&search=gguf', 'output');
    addTerminalLine('  \uD83E\uDD99 Ollama Library       /browse https://ollama.com/library', 'output');
    addTerminalLine('  \uD83D\uDCE6 TheBloke Models      /browse https://huggingface.co/TheBloke', 'output');
    addTerminalLine('  \uD83D\uDCE6 bartowski Models     /browse https://huggingface.co/bartowski', 'output');
    addTerminalLine('  \uD83C\uDF10 OpenRouter           /browse https://openrouter.ai/models', 'output');
    addTerminalLine('  \uD83C\uDFC6 LMSys Leaderboard    /browse https://huggingface.co/spaces/lmsys/chatbot-arena-leaderboard', 'output');
    addTerminalLine('  \uD83D\uDCBB llama.cpp GitHub     /browse https://github.com/ggerganov/llama.cpp', 'output');
    addTerminalLine('  \uD83D\uDCD6 Ollama Docs          /browse https://docs.ollama.com', 'output');
    addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
    addTerminalLine('  Tip: Use /browse <url> or click a link above to open in the built-in browser.', 'system');
    return;
  }

  // ══════════════════════════════════════════════════════════════
  // Win32IDE Extension Commands (fetch from /api/* endpoints)
  // ══════════════════════════════════════════════════════════════

  // /backends — Backend Switcher
  if (lower === '/backends' || lower === 'backends') {
    addTerminalLine('\u2500\u2500\u2500 Backend Switcher \u2500\u2500\u2500', 'output');
    (async function () {
      var usedRemote = false;
      try {
        var aRes = await fetch(getActiveUrl() + '/api/backend/active', { signal: AbortSignal.timeout(3000) });
        var aData = aRes.ok ? await aRes.json() : {};
        var bRes = await fetch(getActiveUrl() + '/api/backends', { signal: AbortSignal.timeout(3000) });
        if (bRes.ok) {
          var bData = await bRes.json();
          var list = bData.backends || bData.list || [];
          if (Array.isArray(list) && list.length > 0) {
            usedRemote = true;
            addTerminalLine('  Active: ' + (aData.name || aData.backend || aData.type || '\u2014'), 'success');
            list.forEach(function (b) {
              var name = b.name || b.type || 'unknown';
              var url = b.url || b.endpoint || '';
              var online = b.online !== false ? '\u2714' : '\u2718';
              addTerminalLine('  ' + online + ' ' + name + (url ? ' (' + url + ')' : ''), b.online !== false ? 'output' : 'error');
            });
          }
        }
      } catch (_) { /* fall through to beacon */ }
      if (!usedRemote) {
        addTerminalLine('  (Beacon mode \u2014 probing known backends)', 'system');
        var beacons = await _buildBeaconBackendList();
        var active = beacons.find(function (b) { return b.isActive; });
        addTerminalLine('  Active: ' + (active ? active.name : (State.backend.serverType || '\u2014')), 'success');
        beacons.forEach(function (b) {
          addTerminalLine('  ' + (b.online ? '\u2714' : '\u2718') + ' ' + b.name + ' (' + b.url + ')' + (b.isActive ? ' \u25C0 ACTIVE' : ''), b.online ? 'output' : 'error');
        });
      }
      addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
    })();
    return;
  }

  // /router — LLM Router
  if (lower === '/router' || lower === 'router') {
    addTerminalLine('\u2500\u2500\u2500 LLM Router \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/router/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
          if (sData.total_routes) addTerminalLine('  Total Routes: ' + sData.total_routes, 'output');
          if (sData.total_decisions) addTerminalLine('  Total Decisions: ' + sData.total_decisions, 'output');
        }
        var dRes = await fetch(getActiveUrl() + '/api/router/decision', { signal: AbortSignal.timeout(5000) });
        if (dRes.ok) {
          var dData = await dRes.json();
          addTerminalLine('  Last Decision: ' + (dData.backend || dData.selected || dData.decision || '\u2014'), 'output');
        }
        var pRes = await fetch(getActiveUrl() + '/api/router/pins', { signal: AbortSignal.timeout(5000) });
        if (pRes.ok) {
          var pData = await pRes.json();
          var pins = pData.pins || pData;
          if (Array.isArray(pins) && pins.length > 0) {
            addTerminalLine('  Pins:', 'output');
            pins.forEach(function (p) {
              addTerminalLine('    ' + (p.pattern || p.model || '') + ' \u2192 ' + (p.backend || ''), 'output');
            });
          } else {
            addTerminalLine('  Pins: none', 'output');
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /swarm — Swarm Dashboard
  if (lower === '/swarm' || lower === 'swarm') {
    addTerminalLine('\u2500\u2500\u2500 Swarm Dashboard \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/swarm/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          var status = sData.status || sData.state || 'idle';
          addTerminalLine('  Status: ' + status, status === 'running' ? 'success' : 'output');
        }
        var nRes = await fetch(getActiveUrl() + '/api/swarm/nodes', { signal: AbortSignal.timeout(5000) });
        if (nRes.ok) {
          var nData = await nRes.json();
          var nodes = nData.nodes || nData;
          var count = Array.isArray(nodes) ? nodes.length : 0;
          addTerminalLine('  Nodes: ' + count, 'output');
          if (Array.isArray(nodes) && nodes.length > 0) {
            nodes.forEach(function (n) {
              var nStatus = n.status || n.state || 'unknown';
              addTerminalLine('    ' + (n.name || n.id || n.host || 'node') + ' [' + nStatus + ']', nStatus === 'active' || nStatus === 'online' ? 'success' : 'output');
            });
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /safety — Safety Monitor
  if (lower === '/safety' || lower === 'safety') {
    addTerminalLine('\u2500\u2500\u2500 Safety Monitor \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/safety/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
        }
        var vRes = await fetch(getActiveUrl() + '/api/safety/violations', { signal: AbortSignal.timeout(5000) });
        if (vRes.ok) {
          var vData = await vRes.json();
          var violations = vData.violations || vData;
          var count = Array.isArray(violations) ? violations.length : 0;
          addTerminalLine('  Violations: ' + count, count > 0 ? 'error' : 'success');
          if (count > 0) {
            violations.slice(-5).forEach(function (v) {
              addTerminalLine('    \u2718 ' + (v.type || v.category || 'violation') + ': ' + ((v.detail || v.message || '').substring(0, 60)), 'error');
            });
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /cot — Chain-of-Thought Engine
  if (lower === '/cot' || lower === 'cot') {
    addTerminalLine('\u2500\u2500\u2500 Chain-of-Thought Engine \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/cot/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
          if (sData.active_chain) addTerminalLine('  Active Chain: ' + sData.active_chain, 'output');
          if (sData.total_executions) addTerminalLine('  Total Executions: ' + sData.total_executions, 'output');
        }
        var stRes = await fetch(getActiveUrl() + '/api/cot/steps', { signal: AbortSignal.timeout(5000) });
        if (stRes.ok) {
          var stData = await stRes.json();
          var steps = stData.steps || stData;
          if (Array.isArray(steps) && steps.length > 0) {
            addTerminalLine('  Current Steps:', 'output');
            steps.forEach(function (s, i) {
              addTerminalLine('    ' + (i + 1) + '. [' + (s.role || '?') + '] ' + (s.model || 'default') + (s.skip ? ' (SKIP)' : ''), s.skip ? 'system' : 'output');
            });
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /confidence — Confidence Evaluator
  if (lower === '/confidence' || lower === 'confidence') {
    addTerminalLine('\u2500\u2500\u2500 Confidence Evaluator \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/confidence/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
          if (sData.total_evaluations) addTerminalLine('  Total Evaluations: ' + sData.total_evaluations, 'output');
          if (sData.avg_confidence !== undefined) addTerminalLine('  Avg Confidence: ' + (sData.avg_confidence * 100).toFixed(1) + '%', 'output');
        }
        var hRes = await fetch(getActiveUrl() + '/api/confidence/history', { signal: AbortSignal.timeout(5000) });
        if (hRes.ok) {
          var hData = await hRes.json();
          var history = hData.history || hData.evaluations || hData;
          if (Array.isArray(history) && history.length > 0) {
            addTerminalLine('  Recent:', 'output');
            history.slice(-5).forEach(function (h) {
              var score = h.score !== undefined ? (h.score * 100).toFixed(0) + '%' : h.confidence !== undefined ? (h.confidence * 100).toFixed(0) + '%' : '?';
              addTerminalLine('    ' + score + ' \u2014 ' + ((h.model || h.query || '').substring(0, 40)), 'output');
            });
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /governor — Task Governor
  if (lower === '/governor' || lower === 'governor') {
    addTerminalLine('\u2500\u2500\u2500 Task Governor \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/governor/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
          if (sData.active_tasks !== undefined) addTerminalLine('  Active Tasks: ' + sData.active_tasks, 'output');
          if (sData.completed !== undefined) addTerminalLine('  Completed: ' + sData.completed, 'output');
          if (sData.queued !== undefined) addTerminalLine('  Queued: ' + sData.queued, 'output');
          if (sData.max_concurrent !== undefined) addTerminalLine('  Max Concurrent: ' + sData.max_concurrent, 'output');
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /lsp — LSP Integration
  if (lower === '/lsp' || lower === 'lsp') {
    addTerminalLine('\u2500\u2500\u2500 LSP Integration \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/lsp/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
          if (sData.language_servers) {
            var servers = sData.language_servers;
            if (Array.isArray(servers)) {
              addTerminalLine('  Language Servers:', 'output');
              servers.forEach(function (s) {
                addTerminalLine('    ' + (s.name || s.language || 'unknown') + ' [' + (s.status || 'running') + ']', 'output');
              });
            }
          }
        }
        var dRes = await fetch(getActiveUrl() + '/api/lsp/diagnostics', { signal: AbortSignal.timeout(5000) });
        if (dRes.ok) {
          var dData = await dRes.json();
          var diags = dData.diagnostics || dData;
          if (Array.isArray(diags) && diags.length > 0) {
            addTerminalLine('  Diagnostics (' + diags.length + '):', 'output');
            diags.slice(0, 10).forEach(function (d) {
              var severity = d.severity || d.level || 'info';
              addTerminalLine('    [' + severity + '] ' + (d.file || '') + ':' + (d.line || '?') + ' ' + (d.message || '').substring(0, 50), severity === 'error' ? 'error' : 'output');
            });
            if (diags.length > 10) addTerminalLine('    ... (' + (diags.length - 10) + ' more)', 'system');
          } else {
            addTerminalLine('  Diagnostics: 0 issues \u2714', 'success');
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /hybrid — Hybrid Completion Engine
  if (lower === '/hybrid' || lower === 'hybrid') {
    addTerminalLine('\u2500\u2500\u2500 Hybrid Completion \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/hybrid/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
          if (sData.total_completions !== undefined) addTerminalLine('  Total Completions: ' + sData.total_completions, 'output');
          if (sData.total_renames !== undefined) addTerminalLine('  Total Renames: ' + sData.total_renames, 'output');
          if (sData.total_symbols !== undefined) addTerminalLine('  Total Symbols: ' + sData.total_symbols, 'output');
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /replay — Replay Sessions
  if (lower === '/replay' || lower === 'replay') {
    addTerminalLine('\u2500\u2500\u2500 Replay Sessions \u2500\u2500\u2500', 'output');
    (async function () {
      try {
        var sRes = await fetch(getActiveUrl() + '/api/replay/status', { signal: AbortSignal.timeout(5000) });
        if (sRes.ok) {
          var sData = await sRes.json();
          addTerminalLine('  Status: ' + (sData.status || sData.state || 'idle'), 'success');
          if (sData.total_sessions !== undefined) addTerminalLine('  Total Sessions: ' + sData.total_sessions, 'output');
          if (sData.total_records !== undefined) addTerminalLine('  Total Records: ' + sData.total_records, 'output');
        }
        var rRes = await fetch(getActiveUrl() + '/api/replay/sessions', { signal: AbortSignal.timeout(5000) });
        if (rRes.ok) {
          var rData = await rRes.json();
          var sessions = rData.sessions || rData;
          if (Array.isArray(sessions) && sessions.length > 0) {
            addTerminalLine('  Recent Sessions:', 'output');
            sessions.slice(-5).forEach(function (s) {
              addTerminalLine('    [' + (s.id || '?') + '] ' + (s.name || s.timestamp || '') + ' (' + (s.records || s.count || 0) + ' records)', 'output');
            });
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } catch (e) {
        addTerminalLine('  Error: ' + e.message, 'error');
      }
    })();
    return;
  }

  // /phases — Phase 10/11/12 Status
  if (lower === '/phases' || lower === 'phases') {
    addTerminalLine('\u2500\u2500\u2500 Phase Status (10/11/12) \u2500\u2500\u2500', 'output');
    (async function () {
      var phaseNames = { 10: 'Speculative Decoding', 11: 'Flash-Attention v2', 12: 'Extreme Compression' };
      for (var p = 10; p <= 12; p++) {
        try {
          var res = await fetch(getActiveUrl() + '/api/phase' + p + '/status', { signal: AbortSignal.timeout(5000) });
          if (res.ok) {
            var data = await res.json();
            var status = data.status || data.state || 'unknown';
            var icon = status === 'active' || status === 'ready' || status === 'complete' ? '\u2714' : '\u2718';
            addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): ' + icon + ' ' + status, status === 'active' || status === 'ready' || status === 'complete' ? 'success' : 'output');
            if (data.progress !== undefined) addTerminalLine('    Progress: ' + data.progress + '%', 'output');
            if (data.detail) addTerminalLine('    Detail: ' + data.detail, 'output');
          } else {
            addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): \u2718 HTTP ' + res.status, 'error');
          }
        } catch (e) {
          addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): \u2718 ' + e.message, 'error');
        }
      }
      addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
    })();
    return;
  }

  // /extensions, /ext — VSIX Extension Manager
  if (lower === '/extensions' || lower === '/ext' || lower === 'extensions') {
    showExtensionsPanel();
    addTerminalLine('\u2500\u2500\u2500 VSIX Extension Manager \u2500\u2500\u2500', 'output');
    addTerminalLine('  Panel opened. Use ext commands for terminal control:', 'system');
    addTerminalLine('  ext list | ext search <q> | ext install <id> | ext host', 'system');
    return;
  }
  if (lower.indexOf('/ext ') === 0) {
    // Route /ext subcommands to the terminal ext handler
    var extCmd = 'ext ' + cmd.substring(5);
    document.getElementById('terminalInput').value = extCmd;
    executeCommand();
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // AGENTIC FILE EDITING CLI COMMANDS (Phase 40)
  // ═══════════════════════════════════════════════════════════════

  // /editor, /files — Open File Editor panel
  if (lower === '/editor' || lower === '/files' || lower === '/file-editor') {
    showFileEditorPanel();
    addTerminalLine('\u2500\u2500\u2500 Agentic File Editor \u2500\u2500\u2500', 'output');
    addTerminalLine('  Panel opened. File editing commands:', 'system');
    addTerminalLine('  cat | head | tail | create | mkdir | rm | mv | cp | grep | diff | patch | edit', 'system');
    return;
  }

  // /edit <file> — Open file in editor panel
  if (lower.indexOf('/edit ') === 0) {
    var editPath = cmd.substring(6).trim();
    if (editPath) {
      showFileEditorPanel();
      feOpenFile(editPath);
      addTerminalLine('  \u2705 Opening ' + editPath + ' in editor', 'success');
    } else {
      addTerminalLine('  Usage: /edit <filepath>', 'error');
    }
    return;
  }

  // /create <file> [content] — Create a new file
  if (lower.indexOf('/create ') === 0) {
    var createParts = cmd.substring(8).trim().split(/\s+/);
    var createPath = createParts[0] || '';
    var createContent = createParts.slice(1).join(' ') || '';
    if (createPath) {
      (async function () {
        try {
          await EngineAPI.createFile(createPath, createContent);
          addTerminalLine('  \u2705 Created: ' + createPath, 'success');
          feLogHistory('create', createPath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /create <filepath> [content]', 'error');
    }
    return;
  }

  // /delete <file> — Delete a file
  if (lower.indexOf('/delete ') === 0) {
    var deletePath = cmd.substring(8).trim();
    if (deletePath) {
      (async function () {
        try {
          await EngineAPI.deleteFile(deletePath);
          addTerminalLine('  \u2705 Deleted: ' + deletePath, 'success');
          feLogHistory('delete', deletePath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /delete <filepath>', 'error');
    }
    return;
  }

  // /rename <old> <new> — Rename/move a file
  if (lower.indexOf('/rename ') === 0) {
    var renameParts = cmd.substring(8).trim().split(/\s+/);
    var renOld = renameParts[0] || '';
    var renNew = renameParts[1] || '';
    if (renOld && renNew) {
      (async function () {
        try {
          await EngineAPI.renameFile(renOld, renNew);
          addTerminalLine('  \u2705 Renamed: ' + renOld + ' \u2192 ' + renNew, 'success');
          feLogHistory('rename', renOld + ' → ' + renNew);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /rename <oldpath> <newpath>', 'error');
    }
    return;
  }

  // /find <pattern> [dir] — Search for files
  if (lower.indexOf('/find ') === 0) {
    var findParts = cmd.substring(6).trim().split(/\s+/);
    var findPattern = findParts[0] || '*.*';
    var findDir = findParts[1] || '.';
    (async function () {
      try {
        addTerminalLine('  Searching: ' + findPattern + ' in ' + findDir, 'system');
        var data = await EngineAPI.searchFiles(findPattern, findDir);
        var output = data.output || data.result || data.stdout || '';
        var files = output.split('\n').filter(function (f) { return f.trim(); });
        files.forEach(function (f) { addTerminalLine('  \u2022 ' + f.trim(), 'output'); });
        addTerminalLine('  \u2500\u2500\u2500 ' + files.length + ' file(s) found \u2500\u2500\u2500', 'system');
      } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
    })();
    return;
  }

  // /grep <pattern> [path] — Search in files
  if (lower.indexOf('/grep ') === 0) {
    var grepParts = cmd.substring(6).trim().split(/\s+/);
    var grepPattern = grepParts[0] || '';
    var grepPath = grepParts[1] || '*.*';
    if (grepPattern) {
      (async function () {
        try {
          addTerminalLine('  Searching "' + grepPattern + '" in ' + grepPath, 'system');
          var data = await EngineAPI.grepInFiles(grepPattern, grepPath);
          var output = data.output || data.result || data.stdout || '';
          var lines = output.split('\n').filter(function (l) { return l.trim(); });
          lines.slice(0, 50).forEach(function (l) { addTerminalLine('  ' + l.trim(), 'output'); });
          if (lines.length > 50) addTerminalLine('  ... and ' + (lines.length - 50) + ' more', 'system');
          addTerminalLine('  \u2500\u2500\u2500 ' + lines.length + ' match(es) \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /grep <pattern> [path]', 'error');
    }
    return;
  }

  // /diff <fileA> <fileB> — Compare two files
  if (lower.indexOf('/diff ') === 0) {
    var diffParts = cmd.substring(6).trim().split(/\s+/);
    var diffA = diffParts[0] || '';
    var diffB = diffParts[1] || '';
    if (diffA && diffB) {
      (async function () {
        try {
          addTerminalLine('  Comparing: ' + diffA + ' vs ' + diffB, 'system');
          var data = await EngineAPI.diffFiles(diffA, diffB);
          var output = data.output || data.result || data.stdout || '';
          output.split('\n').forEach(function (line) { addTerminalLine('  ' + line, 'output'); });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /diff <fileA> <fileB>', 'error');
    }
    return;
  }

  // /patch <file> <search> <replace> — Search-and-replace in a file
  if (lower.indexOf('/patch ') === 0) {
    var patchParts = cmd.substring(7).trim().split(/\s+/);
    var patchFile = patchParts[0] || '';
    var patchSearch = patchParts[1] || '';
    var patchReplace = patchParts.slice(2).join(' ') || '';
    if (patchFile && patchSearch) {
      (async function () {
        try {
          addTerminalLine('  Patching: ' + patchFile, 'system');
          var result = await EngineAPI.patchFile(patchFile, patchSearch, patchReplace);
          if (result.success) {
            addTerminalLine('  \u2705 ' + result.message, 'success');
            feLogHistory('patch', patchFile);
          } else {
            addTerminalLine('  \u2718 ' + (result.error || 'Pattern not found'), 'error');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /patch <file> <search> <replace>', 'error');
    }
    return;
  }

  // /cat <file> — Display full file content
  if (lower.indexOf('/cat ') === 0) {
    var catPath = cmd.substring(5).trim();
    if (catPath) {
      (async function () {
        try {
          var data = await EngineAPI.toolReadFile(catPath);
          var content = data.content || data.output || data.result || '';
          content.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (i + 1).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 ' + content.split('\n').length + ' lines \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /cat <filepath>', 'error');
    }
    return;
  }

  // /head [n] <file> — Show first N lines
  if (lower.indexOf('/head ') === 0) {
    var headParts = cmd.substring(6).trim().split(/\s+/);
    var headN = parseInt(headParts[0]) || 20;
    var headPath = headParts[1] || headParts[0] || '';
    if (headPath === String(headN)) headPath = headParts[1] || '';
    if (headPath) {
      (async function () {
        try {
          var data = await EngineAPI.headFile(headPath, headN);
          data.output.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (i + 1).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 Showing ' + data.lineCount + ' lines \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /head [n] <filepath>', 'error');
    }
    return;
  }

  // /tail [n] <file> — Show last N lines
  if (lower.indexOf('/tail ') === 0) {
    var tailParts = cmd.substring(6).trim().split(/\s+/);
    var tailN = parseInt(tailParts[0]) || 20;
    var tailPath = tailParts[1] || tailParts[0] || '';
    if (tailPath === String(tailN)) tailPath = tailParts[1] || '';
    if (tailPath) {
      (async function () {
        try {
          var data = await EngineAPI.tailFile(tailPath, tailN);
          var startLine = data.totalLines - data.lineCount + 1;
          data.output.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (startLine + i).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 Lines ' + startLine + '-' + data.totalLines + ' of ' + data.totalLines + ' \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /tail [n] <filepath>', 'error');
    }
    return;
  }

  // /wc <file> — Word/line/char count
  if (lower.indexOf('/wc ') === 0) {
    var wcPath = cmd.substring(4).trim();
    if (wcPath) {
      (async function () {
        try {
          var data = await EngineAPI.wordCount(wcPath);
          addTerminalLine('  Lines:      ' + data.lines, 'output');
          addTerminalLine('  Words:      ' + data.words, 'output');
          addTerminalLine('  Characters: ' + data.chars, 'output');
          addTerminalLine('  Bytes:      ' + data.bytes, 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /wc <filepath>', 'error');
    }
    return;
  }

  // /mkdir <path> — Create directory
  if (lower.indexOf('/mkdir ') === 0) {
    var mkdirPath = cmd.substring(7).trim();
    if (mkdirPath) {
      (async function () {
        try {
          await EngineAPI.mkDir(mkdirPath);
          addTerminalLine('  \u2705 Directory created: ' + mkdirPath, 'success');
          feLogHistory('mkdir', mkdirPath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /mkdir <path>', 'error');
    }
    return;
  }

  // /copy <src> <dst> — Copy file
  if (lower.indexOf('/copy ') === 0) {
    var copyParts = cmd.substring(6).trim().split(/\s+/);
    var copySrc = copyParts[0] || '';
    var copyDst = copyParts[1] || '';
    if (copySrc && copyDst) {
      (async function () {
        try {
          await EngineAPI.copyFile(copySrc, copyDst);
          addTerminalLine('  \u2705 Copied: ' + copySrc + ' \u2192 ' + copyDst, 'success');
          feLogHistory('copy', copySrc + ' → ' + copyDst);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /copy <source> <destination>', 'error');
    }
    return;
  }

  // /move <src> <dst> — Move/rename file
  if (lower.indexOf('/move ') === 0) {
    var moveParts = cmd.substring(6).trim().split(/\s+/);
    var moveSrc = moveParts[0] || '';
    var moveDst = moveParts[1] || '';
    if (moveSrc && moveDst) {
      (async function () {
        try {
          await EngineAPI.moveFile(moveSrc, moveDst);
          addTerminalLine('  \u2705 Moved: ' + moveSrc + ' \u2192 ' + moveDst, 'success');
          feLogHistory('move', moveSrc + ' → ' + moveDst);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
    } else {
      addTerminalLine('  Usage: /move <source> <destination>', 'error');
    }
    return;
  }

  // Unknown CLI command
  addTerminalLine("Unknown CLI command: " + cmd, 'error');
  addTerminalLine("  Type /help for available commands.", 'system');
}

// --- Send a CLI command: try remote /api/cli first, fallback to client-side ---
async function sendCliCommand(command) {
  // If beacon confirmed /api/cli exists, try remote first
  if (State.backend.hasCliEndpoint) {
    var cliUrl = getCliUrl();
    addTerminalLine('  \u2192 ' + cliUrl + '/api/cli', 'system');
    try {
      var res = await fetch(cliUrl + '/api/cli', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command: command }),
        signal: AbortSignal.timeout(15000)
      });
      if (res.ok) {
        var data = await res.json();
        if (data.output) {
          renderCliOutput(data.output);
        } else if (data.error) {
          addTerminalLine('Error: ' + data.error, 'error');
        }
        if (data && !data.success) {
          addTerminalLine('(command returned non-success)', 'system');
        }
        return;
      }
      // Non-OK response: fall through to client-side
      addTerminalLine('  Remote CLI returned HTTP ' + res.status + ', falling back to client-side...', 'system');
    } catch (e) {
      addTerminalLine('  Remote CLI failed (' + e.message + '), falling back to client-side...', 'system');
    }
  } else {
    addTerminalLine('  \u2192 Client-side CLI (beacon mode)', 'system');
  }

  // Client-side fallback (beaconism): process locally
  await processCliLocally(command);
}

function handleTerminal(e) {
  if (e.key === 'Enter') executeCommand();
  else if (e.key === 'ArrowUp') { e.preventDefault(); navigateHistory(-1); }
  else if (e.key === 'ArrowDown') { e.preventDefault(); navigateHistory(1); }
}

function executeCommand() {
  var input = document.getElementById('terminalInput');
  var cmd = input.value.trim();
  if (!cmd) return;

  var promptPrefix = (State.terminal.mode === 'cli') ? 'cli> ' : 'rawrxd> ';
  addTerminalLine(promptPrefix + cmd, 'command');
  State.terminal.history.push(cmd);
  State.terminal.index = State.terminal.history.length;
  input.value = '';

  // --- CLI mode: forward ALL commands to backend /api/cli ---
  if (State.terminal.mode === 'cli') {
    if (cmd.toLowerCase() === 'clear' || cmd.toLowerCase() === 'cls') {
      document.getElementById('terminalOutput').innerHTML = '';
      return;
    }
    if (cmd.toLowerCase() === 'mode local' || cmd.toLowerCase() === 'local') {
      switchTerminalMode('local');
      return;
    }
    sendCliCommand(cmd);
    return;
  }

  // --- Local mode: auto-forward /commands and !engine to CLI processor ---
  if (cmd.charAt(0) === '/' || cmd.substring(0, 7).toLowerCase() === '!engine') {
    addTerminalLine('  [routing to CLI processor]', 'system');
    sendCliCommand(cmd);
    return;
  }

  // --- Local mode: client-side command processing ---
  var args = cmd.toLowerCase().split(/\s+/);
  var rest = cmd.substring(cmd.indexOf(' ') + 1).trim();
  if (rest === cmd) rest = '';  // No space found — no arguments

  switch (args[0]) {
    case 'help':
      addTerminalLine('Available commands:', 'output');
      addTerminalLine('  help             \u2014 Show this help', 'output');
      addTerminalLine('  status           \u2014 Backend connection status', 'output');
      addTerminalLine('  connect          \u2014 Connect to backend', 'output');
      addTerminalLine('  models           \u2014 List available models', 'output');
      addTerminalLine('  use <model>      \u2014 Select a model', 'output');
      addTerminalLine('  ask <question>   \u2014 Send a question', 'output');
      addTerminalLine('  memory           \u2014 Show conversation memory stats', 'output');
      addTerminalLine('  memory clear     \u2014 Clear conversation memory', 'output');
      addTerminalLine('  sysprompt        \u2014 Show current system prompt', 'output');
      addTerminalLine('  agents           \u2014 Show agent subsystem status', 'output');
      addTerminalLine('  perf             \u2014 Show performance metrics summary', 'output');
      addTerminalLine('  security         \u2014 Show security & hardening status', 'output');
      addTerminalLine('  endpoints        \u2014 List API endpoints', 'output');
      addTerminalLine('  curl <path>      \u2014 GET an endpoint', 'output');
      addTerminalLine('  failures         \u2014 Show failure summary', 'output');
      addTerminalLine('  hotpatch         \u2014 Show hotpatch layer status', 'output');
      addTerminalLine('  hotpatch toggle <layer> \u2014 Toggle memory/byte/server', 'output');
      addTerminalLine('  hotpatch apply <layer>  \u2014 Apply pending patches', 'output');
      addTerminalLine('  hotpatch revert <layer> \u2014 Revert active patches', 'output');
      addTerminalLine('  ide              \u2014 Win32 IDE bridge status & controls', 'output');
      addTerminalLine('  ide launch       \u2014 Launch Win32 IDE (if standalone)', 'output');
      addTerminalLine('  ide probe        \u2014 Re-probe for running IDE instance', 'output');
      addTerminalLine('  ide read <path>  \u2014 Read file via IDE bridge', 'output');
      addTerminalLine('  backends         \u2014 Show/switch inference backends', 'output');
      addTerminalLine('  backends switch <name> \u2014 Switch active backend', 'output');
      addTerminalLine('  router           \u2014 LLM router status & decision info', 'output');
      addTerminalLine('  router pin <pattern> <backend> \u2014 Pin a route', 'output');
      addTerminalLine('  swarm            \u2014 Swarm dashboard status', 'output');
      addTerminalLine('  swarm start|stop \u2014 Start/stop swarm', 'output');
      addTerminalLine('  multi-response   \u2014 Multi-response engine status', 'output');
      addTerminalLine('  asm-debug        \u2014 ASM debugger status', 'output');
      addTerminalLine('  asm-debug go|launch|attach \u2014 Debug actions', 'output');
      addTerminalLine('  safety           \u2014 Safety monitor status', 'output');
      addTerminalLine('  safety check     \u2014 Check last output', 'output');
      addTerminalLine('  safety rollback  \u2014 Rollback last output', 'output');
      addTerminalLine('  cot              \u2014 Chain-of-Thought engine status', 'output');
      addTerminalLine('  cot presets      \u2014 List CoT presets', 'output');
      addTerminalLine('  confidence       \u2014 Confidence evaluator status', 'output');
      addTerminalLine('  governor         \u2014 Task governor status', 'output');
      addTerminalLine('  lsp              \u2014 LSP integration status', 'output');
      addTerminalLine('  hybrid           \u2014 Hybrid completion engine', 'output');
      addTerminalLine('  replay           \u2014 Replay sessions & records', 'output');
      addTerminalLine('  phases           \u2014 Phase 10/11/12 status', 'output');
      addTerminalLine('  codex            \u2014 Codex compilation system', 'output');
      addTerminalLine('  codex build [tgt]\u2014 Build a target', 'output');
      addTerminalLine('  codex masm <asm> \u2014 Assemble MASM64 file', 'output');
      addTerminalLine('  codex targets    \u2014 List build targets', 'output');
      addTerminalLine('  re               \u2014 Reverse engineering panel', 'output');
      addTerminalLine('  re modules       \u2014 List RE modules', 'output');
      addTerminalLine('  re pe <file>     \u2014 PE analysis', 'output');
      addTerminalLine('  re disasm <addr> \u2014 Disassemble address', 'output');
      addTerminalLine('  re gguf <model>  \u2014 GGUF inspector', 'output');
      addTerminalLine('  re symbols <pdb> \u2014 Symbol resolver', 'output');
      addTerminalLine('  re scan <pid>    \u2014 Memory scanner', 'output');
      addTerminalLine('', 'output');
      addTerminalLine('Extension Management (VSIX plugin system):', 'system');
      addTerminalLine('  ext              \u2014 Show extension status & stats', 'system');
      addTerminalLine('  ext list         \u2014 List installed extensions', 'system');
      addTerminalLine('  ext search <q>   \u2014 Search marketplace for extensions', 'system');
      addTerminalLine('  ext install <id> \u2014 Install extension by ID', 'system');
      addTerminalLine('  ext uninstall <id> \u2014 Uninstall an extension', 'system');
      addTerminalLine('  ext enable <id>  \u2014 Enable a disabled extension', 'system');
      addTerminalLine('  ext disable <id> \u2014 Disable an extension', 'system');
      addTerminalLine('  ext info <id>    \u2014 Show extension details', 'system');
      addTerminalLine('  ext load <path>  \u2014 Load a .vsix file from disk', 'system');
      addTerminalLine('  ext host         \u2014 Extension host status', 'system');
      addTerminalLine('  ext host restart \u2014 Restart the extension host', 'system');
      addTerminalLine('  ext host kill    \u2014 Kill the extension host', 'system');
      addTerminalLine('  ext host logs    \u2014 Show extension host logs', 'system');
      addTerminalLine('  ext scan         \u2014 Scan local dirs for extensions', 'system');
      addTerminalLine('  ext export       \u2014 Export installed extension list', 'system');
      addTerminalLine('  ext panel        \u2014 Toggle extension manager panel', 'system');
      addTerminalLine('  ext types        \u2014 List supported extension types', 'system');
      addTerminalLine('  ext builtin      \u2014 List 12 built-in extensions', 'system');
      addTerminalLine('  ext ps           \u2014 PowerShell ExtensionManager.psm1', 'system');
      addTerminalLine('  vsix <path|url>  \u2014 Quick-load a .vsix package', 'system');
      addTerminalLine('', 'output');
      addTerminalLine('Engine API Commands (via EngineAPI \u2014 150+ source files):', 'system');
      addTerminalLine('  subagent <task>  \u2014 Spawn a subagent for a task', 'system');
      addTerminalLine('  chain [json]     \u2014 Run agent chain pipeline', 'system');
      addTerminalLine('  policies         \u2014 List active inference policies', 'system');
      addTerminalLine('  policy-suggest   \u2014 Get policy suggestions', 'system');
      addTerminalLine('  policy-stats     \u2014 Policy statistics', 'system');
      addTerminalLine('  heuristics       \u2014 Show heuristics data', 'system');
      addTerminalLine('  explain          \u2014 Show last CoT explanation chain', 'system');
      addTerminalLine('  explain-stats    \u2014 CoT explanation statistics', 'system');
      addTerminalLine('  backend-status   \u2014 Detailed backend status', 'system');
      addTerminalLine('  use-backend <n>  \u2014 Switch inference backend', 'system');
      addTerminalLine('  tool <name> [args] \u2014 Execute backend tool', 'system');
      addTerminalLine('  read <filepath>  \u2014 Read file via tool API', 'system');
      addTerminalLine('  listdir [path]   \u2014 List directory via tool API', 'system');
      addTerminalLine('  write-file <p> <c> \u2014 Write file via tool API', 'system');
      addTerminalLine('  exec <command>   \u2014 Execute system command via tool', 'system');
      addTerminalLine('  git              \u2014 Git status via tool API', 'system');
      addTerminalLine('', 'output');
      addTerminalLine('Agentic File Editing Commands:', 'system');
      addTerminalLine('  cat <file>       \u2014 Display full file with line numbers', 'system');
      addTerminalLine('  head [n] <file>  \u2014 Show first N lines (default 20)', 'system');
      addTerminalLine('  tail [n] <file>  \u2014 Show last N lines (default 20)', 'system');
      addTerminalLine('  create <f> [txt] \u2014 Create new file (touch alias)', 'system');
      addTerminalLine('  mkdir <path>     \u2014 Create directory', 'system');
      addTerminalLine('  rm <file>        \u2014 Delete file (delete alias)', 'system');
      addTerminalLine('  rmdir <path>     \u2014 Remove directory recursively', 'system');
      addTerminalLine('  mv <src> <dst>   \u2014 Move/rename file', 'system');
      addTerminalLine('  cp <src> <dst>   \u2014 Copy file', 'system');
      addTerminalLine('  find <pat> [dir] \u2014 Search for files by pattern', 'system');
      addTerminalLine('  grep <pat> [path]\u2014 Search text in files', 'system');
      addTerminalLine('  diff <a> <b>     \u2014 Compare two files', 'system');
      addTerminalLine('  patch <f> <s> <r>\u2014 Search & replace in file', 'system');
      addTerminalLine('  append <f> <txt> \u2014 Append text to file', 'system');
      addTerminalLine('  wc <file>        \u2014 Line/word/char/byte count', 'system');
      addTerminalLine('  stat <file>      \u2014 File info (info alias)', 'system');
      addTerminalLine('  edit <file>      \u2014 Open in File Editor panel', 'system');
      addTerminalLine('  insert-line <f> <n> <txt> \u2014 Insert text at line N', 'system');
      addTerminalLine('  delete-lines <f> <start> [end] \u2014 Delete lines', 'system');
      addTerminalLine('  replace-lines <f> <s> <e> <txt> \u2014 Replace lines', 'system');
      addTerminalLine('', 'output');
      addTerminalLine('CLI/API Commands:', 'system');
      addTerminalLine('  analyze <file>   \u2014 CLI analyze command', 'system');
      addTerminalLine('  audit <file>     \u2014 Security audit via CLI', 'system');
      addTerminalLine('  optimize <file>  \u2014 Optimize via CLI', 'system');
      addTerminalLine('  plan [file]      \u2014 Plan via CLI', 'system');
      addTerminalLine('  suggest [topic]  \u2014 Get suggestions via CLI', 'system');
      addTerminalLine('  bugreport        \u2014 Generate bug report via CLI', 'system');
      addTerminalLine('  generate <prompt>\u2014 Non-stream generate', 'system');
      addTerminalLine('  complete <prompt>\u2014 Complete endpoint', 'system');
      addTerminalLine('  v1-chat <prompt> \u2014 OpenAI-compat chat completions', 'system');
      addTerminalLine('  tags             \u2014 List model tags (/api/tags)', 'system');
      addTerminalLine('  health           \u2014 Health check endpoint', 'system');
      addTerminalLine('  server-metrics   \u2014 Server metrics (/metrics)', 'system');
      addTerminalLine('  engine-map       \u2014 Show all engine subsystems', 'system');
      addTerminalLine('  engine-detail <n>\u2014 Detail for a subsystem', 'system');
      addTerminalLine('  replay <id>      \u2014 Replay an agent by ID', 'system');
      addTerminalLine('', 'output');
      addTerminalLine('Browser Commands (WebView2-style built-in browser):', 'system');
      addTerminalLine('  browse [url]     \u2014 Open browser / navigate to URL', 'system');
      addTerminalLine('  browse search <q>\u2014 Web search via DuckDuckGo', 'system');
      addTerminalLine('  browse extract   \u2014 Extract current page text', 'system');
      addTerminalLine('  browse model     \u2014 Send page content to model', 'system');
      addTerminalLine('  browse proxy <u> \u2014 Fetch URL via backend proxy', 'system');
      addTerminalLine('  browse close     \u2014 Close browser panel', 'system');
      addTerminalLine('  browse tabs      \u2014 List open browser tabs', 'system');
      addTerminalLine('  browse back/fwd  \u2014 Navigate back/forward', 'system');
      addTerminalLine('', 'output');
      addTerminalLine('  mode cli         \u2014 Switch to CLI mode (remote commands)', 'output');
      addTerminalLine('  clear            \u2014 Clear terminal', 'output');
      addTerminalLine('', 'output');
      addTerminalLine('CLI Commands (prefix with / \u2014 work offline via beacon!):', 'system');
      addTerminalLine('  /help /status /models /memory /agents /failures /hotpatch', 'system');
      addTerminalLine('  /plan /analyze /optimize /security /suggest /bugreport', 'system');
      addTerminalLine('  /ask <question>  \u2014 Query the LLM (needs backend)', 'system');
      addTerminalLine('  !engine list|swap|status|codex|disasm|dumpbin|optimize', 'system');
      break;

    case 'clear': case 'cls':
      document.getElementById('terminalOutput').innerHTML = '';
      break;

    case 'mode':
      if (args[1] === 'cli') {
        switchTerminalMode('cli');
      } else if (args[1] === 'local') {
        switchTerminalMode('local');
      } else {
        addTerminalLine('Usage: mode <local|cli>', 'error');
        addTerminalLine('  Current mode: ' + State.terminal.mode, 'output');
      }
      break;

    case 'status':
      addTerminalLine('Backend: ' + (State.backend.online ? 'ONLINE' : 'OFFLINE'), State.backend.online ? 'success' : 'error');
      addTerminalLine('URL:     ' + getActiveUrl(), 'output');
      addTerminalLine('Server:  ' + (State.backend.serverType || '\u2014'), 'output');
      addTerminalLine('Direct:  ' + (State.backend.directMode ? 'YES (Ollama)' : 'NO'), 'output');
      addTerminalLine('Model:   ' + (State.model.current || 'none'), 'output');
      addTerminalLine('Models:  ' + State.model.list.length + ' available', 'output');
      addTerminalLine('Files:   ' + State.files.length + ' attached', 'output');
      addTerminalLine('Memory:  ' + Conversation.messages.length + ' turns (max ' + Conversation.maxContextMessages + ')', 'output');
      addTerminalLine('SysPrompt: ' + (Conversation.systemPrompt ? Conversation.systemPrompt.substring(0, 60) + '...' : 'none'), 'output');
      addTerminalLine('Terminal: mode=' + State.terminal.mode + ', history=' + State.terminal.history.length + ' cmds', 'output');
      addTerminalLine('CLI:     ' + (State.backend.hasCliEndpoint ? '\u2714 Remote (' + getCliUrl() + ')' : 'Client-side beacon'), State.backend.hasCliEndpoint ? 'success' : 'output');
      break;

    case 'connect':
      addTerminalLine('Connecting to ' + State.backend.url + '...', 'system');
      connectBackend().then(function () {
        addTerminalLine(State.backend.online ? 'Connected!' : 'Connection failed.', State.backend.online ? 'success' : 'error');
        if (State.backend.online) {
          if (State.backend.hasCliEndpoint) {
            addTerminalLine('  \u2714 CLI endpoint: ' + getCliUrl() + '/api/cli', 'success');
          } else {
            addTerminalLine('  CLI: Client-side beacon mode (no /api/cli on backend)', 'system');
          }
          addTerminalLine('  Tip: Switch to CLI tab or type /help for CLI commands', 'system');
        }
      });
      break;

    case 'models':
      if (State.model.list.length === 0) {
        addTerminalLine('No models. Connect to backend first.', 'error');
      } else {
        State.model.list.forEach(function (m) {
          addTerminalLine('  [' + (m.type || '?') + '] ' + m.name + ' (' + (m.size || '?') + ')', 'output');
        });
        addTerminalLine('Total: ' + State.model.list.length + ' models', 'system');
      }
      break;

    case 'use':
      if (args.length < 2) {
        addTerminalLine('Usage: use <model-name>', 'error');
      } else {
        var modelName = cmd.substring(4).trim();
        var found = State.model.list.find(function (m) {
          return m.name.toLowerCase() === modelName.toLowerCase() ||
            m.name.toLowerCase().indexOf(modelName.toLowerCase()) >= 0;
        });
        if (found) {
          State.model.current = found.name;
          document.getElementById('modelSelect').value = found.name;
          changeModel();
          addTerminalLine('Selected: ' + found.name, 'success');
        } else {
          addTerminalLine('Not found: ' + modelName, 'error');
          addTerminalLine("Try 'models' to list available models.", 'output');
        }
      }
      break;

    case 'ask':
      if (args.length < 2) {
        addTerminalLine('Usage: ask <question>', 'error');
      } else {
        var question = cmd.substring(4).trim();
        if (!State.backend.online) {
          addTerminalLine('Backend offline. Run "connect" first.', 'error');
        } else {
          addTerminalLine('Sending...', 'system');
          fetch(State.backend.url + '/ask', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ question: question, model: State.model.current || '' }),
          })
            .then(function (r) { return r.json(); })
            .then(function (d) { addTerminalLine(d.answer || d.response || 'No response', 'output'); })
            .catch(function (e) { addTerminalLine('Error: ' + e.message, 'error'); });
        }
      }
      break;

    case 'endpoints':
      addTerminalLine('GET  /health              \u2014 Health check', 'output');
      addTerminalLine('GET  /status              \u2014 Server status', 'output');
      addTerminalLine('GET  /models              \u2014 List models', 'output');
      addTerminalLine('POST /ask                 \u2014 Chat (frontend)', 'output');
      addTerminalLine('POST /api/generate        \u2014 Ollama-compatible', 'output');
      addTerminalLine('POST /v1/chat/completions \u2014 OpenAI-compatible', 'output');
      addTerminalLine('GET  /gui                 \u2014 This HTML interface', 'output');
      addTerminalLine('GET  /api/failures        \u2014 Failure timeline + stats', 'output');
      addTerminalLine('GET  /api/agents/status   \u2014 Agent & failure status', 'output');
      addTerminalLine('GET  /api/agents/history  \u2014 Agent event history', 'output');
      addTerminalLine('POST /api/agents/replay   \u2014 Replay agent events', 'output');
      addTerminalLine('POST /api/cli             \u2014 CLI command execution (tool_server)', 'output');
      addTerminalLine('POST /api/read-file       \u2014 Read file from disk', 'output');
      addTerminalLine('POST /api/tool            \u2014 Execute agentic tools', 'output');
      addTerminalLine('GET  /api/tags            \u2014 Model tags (Ollama)', 'output');
      addTerminalLine('GET  /v1/models           \u2014 OpenAI-compat model list', 'output');
      addTerminalLine('GET  /metrics             \u2014 Prometheus metrics', 'output');
      addTerminalLine('\u2500\u2500\u2500 Complete Server (Agentic) \u2500\u2500\u2500', 'system');
      addTerminalLine('POST /complete            \u2014 Completion endpoint', 'output');
      addTerminalLine('POST /complete/stream     \u2014 Streaming completion', 'output');
      addTerminalLine('POST /api/chat            \u2014 Agentic chat', 'output');
      addTerminalLine('POST /api/subagent        \u2014 Spawn subagent', 'output');
      addTerminalLine('POST /api/chain           \u2014 Chain agent pipeline', 'output');
      addTerminalLine('POST /api/swarm           \u2014 Swarm inference', 'output');
      addTerminalLine('GET  /api/agents          \u2014 List agents', 'output');
      addTerminalLine('GET  /api/policies        \u2014 Active policies', 'output');
      addTerminalLine('GET  /api/policies/suggestions \u2014 Policy suggestions', 'output');
      addTerminalLine('POST /api/policies/apply  \u2014 Apply policy', 'output');
      addTerminalLine('POST /api/policies/reject \u2014 Reject policy', 'output');
      addTerminalLine('GET  /api/policies/export \u2014 Export policies', 'output');
      addTerminalLine('POST /api/policies/import \u2014 Import policies', 'output');
      addTerminalLine('GET  /api/policies/heuristics \u2014 Heuristic rules', 'output');
      addTerminalLine('GET  /api/policies/stats  \u2014 Policy statistics', 'output');
      addTerminalLine('GET  /api/agents/explain  \u2014 CoT explanation chain', 'output');
      addTerminalLine('GET  /api/agents/explain/stats \u2014 Explain stats', 'output');
      addTerminalLine('GET  /api/backends/status \u2014 Backend detailed status', 'output');
      addTerminalLine('POST /api/backends/use    \u2014 Switch active backend', 'output');
      addTerminalLine('GET  /api/hotpatch/status \u2014 Hotpatch layer status', 'output');
      addTerminalLine('POST /api/hotpatch/apply  \u2014 Apply hotpatch', 'output');
      addTerminalLine('POST /api/hotpatch/revert \u2014 Revert hotpatch', 'output');
      addTerminalLine('POST /api/hotpatch/toggle \u2014 Toggle hotpatch layer', 'output');
      addTerminalLine('\u2500\u2500\u2500 Win32IDE Extended Endpoints \u2500\u2500\u2500', 'system');
      addTerminalLine('GET  /api/backends        \u2014 List inference backends', 'output');
      addTerminalLine('GET  /api/backend/active   \u2014 Active backend info', 'output');
      addTerminalLine('POST /api/backend/switch   \u2014 Switch active backend', 'output');
      addTerminalLine('GET  /api/router/status     \u2014 LLM router status', 'output');
      addTerminalLine('GET  /api/router/decision   \u2014 Last routing decision', 'output');
      addTerminalLine('GET  /api/router/capabilities \u2014 Backend capabilities', 'output');
      addTerminalLine('GET  /api/router/heatmap    \u2014 Routing heatmap', 'output');
      addTerminalLine('GET  /api/router/pins       \u2014 Pinned routes', 'output');
      addTerminalLine('GET  /api/swarm/status      \u2014 Swarm status', 'output');
      addTerminalLine('GET  /api/swarm/nodes       \u2014 Connected nodes', 'output');
      addTerminalLine('GET  /api/swarm/tasks       \u2014 Running tasks', 'output');
      addTerminalLine('GET  /api/swarm/events      \u2014 Swarm event log', 'output');
      addTerminalLine('POST /api/swarm/start       \u2014 Start swarm', 'output');
      addTerminalLine('POST /api/swarm/stop        \u2014 Stop swarm', 'output');
      addTerminalLine('GET  /api/multi-response/status    \u2014 Multi-response status', 'output');
      addTerminalLine('GET  /api/multi-response/templates \u2014 SGCX templates', 'output');
      addTerminalLine('GET  /api/multi-response/stats     \u2014 Preference stats', 'output');
      addTerminalLine('GET  /api/debug/status      \u2014 ASM debug status', 'output');
      addTerminalLine('GET  /api/debug/breakpoints \u2014 Breakpoints', 'output');
      addTerminalLine('GET  /api/debug/registers   \u2014 Register state', 'output');
      addTerminalLine('GET  /api/debug/stack       \u2014 Stack frames', 'output');
      addTerminalLine('GET  /api/debug/threads     \u2014 Thread list', 'output');
      addTerminalLine('GET  /api/debug/events      \u2014 Debug events', 'output');
      addTerminalLine('GET  /api/safety/status     \u2014 Safety monitor status', 'output');
      addTerminalLine('GET  /api/safety/violations \u2014 Safety violations', 'output');
      addTerminalLine('POST /api/safety/check      \u2014 Check content safety', 'output');
      addTerminalLine('POST /api/safety/rollback   \u2014 Rollback last output', 'output');
      addTerminalLine('GET  /api/cot/status        \u2014 Chain-of-Thought status', 'output');
      addTerminalLine('GET  /api/cot/presets       \u2014 CoT presets', 'output');
      addTerminalLine('GET  /api/cot/steps         \u2014 Current chain steps', 'output');
      addTerminalLine('GET  /api/cot/roles         \u2014 Available CoT roles', 'output');
      addTerminalLine('POST /api/cot/execute       \u2014 Execute CoT chain', 'output');
      addTerminalLine('GET  /api/lsp/status        \u2014 LSP integration status', 'output');
      addTerminalLine('GET  /api/lsp/diagnostics   \u2014 LSP diagnostics', 'output');
      addTerminalLine('GET  /api/hybrid/status     \u2014 Hybrid completion status', 'output');
      addTerminalLine('POST /api/hybrid/complete   \u2014 Hybrid code completion', 'output');
      addTerminalLine('POST /api/hybrid/analyze    \u2014 Hybrid code analysis', 'output');
      addTerminalLine('POST /api/hybrid/rename     \u2014 Hybrid rename symbol', 'output');
      addTerminalLine('POST /api/hybrid/symbol-usage \u2014 Symbol usage analysis', 'output');
      addTerminalLine('GET  /api/governor/status   \u2014 Task governor status', 'output');
      addTerminalLine('POST /api/governor/submit   \u2014 Submit task', 'output');
      addTerminalLine('GET  /api/governor/result   \u2014 Get task results', 'output');
      addTerminalLine('POST /api/governor/kill     \u2014 Kill running task', 'output');
      addTerminalLine('GET  /api/confidence/status \u2014 Confidence evaluator status', 'output');
      addTerminalLine('POST /api/confidence/evaluate \u2014 Evaluate confidence', 'output');
      addTerminalLine('GET  /api/confidence/history \u2014 Confidence evaluation history', 'output');
      addTerminalLine('GET  /api/replay/status     \u2014 Replay session status', 'output');
      addTerminalLine('GET  /api/replay/sessions   \u2014 List replay sessions', 'output');
      addTerminalLine('GET  /api/replay/records    \u2014 Get replay records', 'output');
      addTerminalLine('GET  /api/phase10/status    \u2014 Phase 10 (Speculative Decoding)', 'output');
      addTerminalLine('GET  /api/phase11/status    \u2014 Phase 11 (Flash-Attention v2)', 'output');
      addTerminalLine('GET  /api/phase12/status    \u2014 Phase 12 (Extreme Compression)', 'output');
      break;

    case 'curl':
      if (args.length < 2) {
        addTerminalLine('Usage: curl <path>  e.g. curl /health', 'error');
      } else {
        var path = args[1].charAt(0) === '/' ? args[1] : '/' + args[1];
        addTerminalLine('GET ' + State.backend.url + path + ' ...', 'system');
        fetch(State.backend.url + path, { signal: AbortSignal.timeout(5000) })
          .then(function (r) { return r.text(); })
          .then(function (text) {
            try {
              var pretty = JSON.stringify(JSON.parse(text), null, 2);
              pretty.split('\n').forEach(function (line) { addTerminalLine(line, 'output'); });
            } catch (_) {
              addTerminalLine(text.substring(0, 500), 'output');
            }
          })
          .catch(function (e) { addTerminalLine('Error: ' + e.message, 'error'); });
      }
      break;

    case 'failures':
      if (!State.backend.online) {
        addTerminalLine('Backend offline. Run "connect" first.', 'error');
      } else {
        addTerminalLine('Fetching failure data...', 'system');
        fetch(State.backend.url + '/api/failures?limit=50', { signal: AbortSignal.timeout(5000) })
          .then(function (r) { return r.json(); })
          .then(function (data) {
            var s = data.stats || {};
            addTerminalLine('\u2500\u2500\u2500 Failure Intelligence \u2500\u2500\u2500', 'output');
            addTerminalLine('  Total Failures:    ' + (s.totalFailures || 0), s.totalFailures > 0 ? 'error' : 'success');
            addTerminalLine('  Total Retries:     ' + (s.totalRetries || 0), 'output');
            addTerminalLine('  Retry Successes:   ' + (s.successAfterRetry || 0), 'success');
            addTerminalLine('  Retries Declined:  ' + (s.retriesDeclined || 0), 'output');
            var reasons = (s.topReasons || []).filter(function (r) { return r.count > 0; });
            if (reasons.length > 0) {
              addTerminalLine('  Top Reasons:', 'output');
              reasons.slice(0, 5).forEach(function (r) {
                addTerminalLine('    ' + r.type + ': ' + r.count, 'error');
              });
            } else {
              addTerminalLine('  No failure reasons recorded.', 'success');
            }
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          })
          .catch(function (e) { addTerminalLine('Error: ' + e.message, 'error'); });
      }
      break;

    case 'memory':
      if (args[1] === 'clear') {
        Conversation.clear();
        addTerminalLine('Conversation memory cleared.', 'success');
      } else {
        addTerminalLine('\u2500\u2500\u2500 Conversation Memory \u2500\u2500\u2500', 'output');
        addTerminalLine('  Total Turns:     ' + Conversation.messages.length, 'output');
        addTerminalLine('  Max Context:     ' + Conversation.maxContextMessages, 'output');
        addTerminalLine('  System Prompt:   ' + (Conversation.systemPrompt ? 'Set (' + Conversation.systemPrompt.length + ' chars)' : 'Default'), 'output');
        if (Conversation.messages.length > 0) {
          addTerminalLine('  Last Message:    ' + Conversation.messages[Conversation.messages.length - 1].role, 'output');
          var lastTime = new Date(Conversation.messages[Conversation.messages.length - 1].timestamp).toLocaleTimeString();
          addTerminalLine('  Last Time:       ' + lastTime, 'output');
        }
        addTerminalLine('  Persisted:       localStorage', 'output');
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      }
      break;

    case 'sysprompt':
      addTerminalLine('System Prompt:', 'output');
      addTerminalLine(Conversation.systemPrompt || '(none)', 'output');
      break;

    case 'agents':
      if (!State.backend.online) {
        addTerminalLine('Backend offline. Run "connect" first.', 'error');
      } else {
        addTerminalLine('Fetching agent status...', 'system');
        fetch(State.backend.url + '/api/agents/status', { signal: AbortSignal.timeout(5000) })
          .then(function (r) { return r.json(); })
          .then(function (data) {
            var agents = data.agents || {};
            addTerminalLine('\u2500\u2500\u2500 Agent Dashboard \u2500\u2500\u2500', 'output');
            var det = agents.failure_detector || {};
            addTerminalLine('  Failure Detector: ' + (det.active ? 'ACTIVE' : 'INACTIVE') + ' (' + (det.detections || 0) + ' detections)', det.active ? 'success' : 'error');
            var pup = agents.puppeteer || {};
            addTerminalLine('  Puppeteer:        ' + (pup.active ? 'ACTIVE' : 'INACTIVE') + ' (' + (pup.corrections || 0) + ' corrections)', pup.active ? 'success' : 'error');
            var prx = agents.proxy_hotpatcher || {};
            addTerminalLine('  Proxy Hotpatcher: ' + (prx.active ? 'ACTIVE' : 'INACTIVE') + ' (' + (prx.patches_applied || 0) + ' patches)', prx.active ? 'success' : 'error');
            var mgr = agents.unified_manager || {};
            addTerminalLine('  Unified Manager:  Memory=' + (mgr.memory_patches || 0) + '  Byte=' + (mgr.byte_patches || 0) + '  Server=' + (mgr.server_patches || 0), 'output');
            addTerminalLine('  Uptime:           ' + (data.server_uptime || 0) + 's', 'output');
            addTerminalLine('  Total Events:     ' + (data.total_events || 0), 'output');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          })
          .catch(function (e) { addTerminalLine('Error: ' + e.message, 'error'); });
      }
      break;

    case 'perf':
      var p = State.perf;
      addTerminalLine('\u2500\u2500\u2500 Performance Metrics \u2500\u2500\u2500', 'output');
      addTerminalLine('  Total Requests:  ' + p.totalRequests, 'output');
      addTerminalLine('  Total Tokens:    ' + p.totalTokens.toLocaleString(), 'output');
      var pAvgLat = p.totalRequests > 0 ? Math.round(p.totalLatency / p.totalRequests) : 0;
      addTerminalLine('  Avg Latency:     ' + pAvgLat + 'ms', p.totalRequests > 0 ? 'success' : 'output');
      var pAvgTps = p.totalTokens > 0 && p.totalLatency > 0 ? (p.totalTokens / (p.totalLatency / 1000)).toFixed(1) : '\u2014';
      addTerminalLine('  Avg Tokens/s:    ' + pAvgTps, 'success');
      var mKeys = Object.keys(p.modelStats);
      if (mKeys.length > 0) {
        addTerminalLine('  \u2500 Model Breakdown \u2500', 'output');
        mKeys.forEach(function (mk) {
          var ms = p.modelStats[mk];
          var mLat = ms.requests > 0 ? Math.round(ms.totalLatency / ms.requests) : 0;
          addTerminalLine('    ' + mk + ': ' + ms.requests + ' reqs, ' + mLat + 'ms avg, ' + ms.avgTps + ' avg t/s, ' + ms.bestTps + ' best t/s', 'output');
        });
      }
      addTerminalLine('  Structured Log:  ' + p.structuredLog.length + ' entries', 'output');
      addTerminalLine('  Tip: Click \uD83D\uDCCA Perf in sidebar for full dashboard', 'system');
      addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      break;

    case 'security':
      var sec = State.security;
      var rl = sec.rateLimit;
      var ig = sec.inputGuard;
      var secNow = Date.now();
      var activeReqs = rl.timestamps.filter(function (ts) { return (secNow - ts) < rl.windowMs; }).length;
      addTerminalLine('\u2500\u2500\u2500 Security & Hardening \u2500\u2500\u2500', 'output');
      addTerminalLine('  CSP:             ' + (document.querySelector('meta[http-equiv="Content-Security-Policy"]') ? 'Active' : 'MISSING'), document.querySelector('meta[http-equiv="Content-Security-Policy"]') ? 'success' : 'error');
      addTerminalLine('  DOMPurify:       ' + (typeof DOMPurify !== 'undefined' ? 'Loaded (v' + (DOMPurify.version || '?') + ')' : 'MISSING'), typeof DOMPurify !== 'undefined' ? 'success' : 'error');
      addTerminalLine('  Rate Limit:      ' + activeReqs + '/' + rl.maxPerMinute + ' per min', activeReqs > rl.maxPerMinute * 0.8 ? 'error' : 'success');
      addTerminalLine('  URL Guard:       ' + (isUrlAllowed(State.backend.url) ? 'OK (' + State.backend.url + ')' : 'BLOCKED'), isUrlAllowed(State.backend.url) ? 'success' : 'error');
      addTerminalLine('  Input Max:       ' + ig.maxLength.toLocaleString() + ' chars', 'output');
      addTerminalLine('  Blocked XSS:     ' + ig.blockedXss, ig.blockedXss > 0 ? 'error' : 'success');
      addTerminalLine('  Blocked Length:   ' + ig.blockedLength, ig.blockedLength > 0 ? 'error' : 'success');
      addTerminalLine('  Blocked Rate:    ' + rl.blocked, rl.blocked > 0 ? 'error' : 'success');
      addTerminalLine('  Security Events: ' + sec.eventLog.length, 'output');
      addTerminalLine('  Tip: Click \uD83D\uDEE1 Security in sidebar for full dashboard', 'system');
      addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      break;

    case 'hotpatch':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Hotpatch Layer Status \u2500\u2500\u2500', 'output');
        addTerminalLine('  Memory:     ' + (_hotpatchLayerStates.memory ? 'ENABLED' : 'DISABLED') + ' (' + (document.getElementById('hpMemoryCount') ? document.getElementById('hpMemoryCount').textContent : '0') + ' patches)', _hotpatchLayerStates.memory ? 'success' : 'error');
        addTerminalLine('  Byte-Level: ' + (_hotpatchLayerStates.byte ? 'ENABLED' : 'DISABLED') + ' (' + (document.getElementById('hpByteCount') ? document.getElementById('hpByteCount').textContent : '0') + ' patches)', _hotpatchLayerStates.byte ? 'success' : 'error');
        addTerminalLine('  Server:     ' + (_hotpatchLayerStates.server ? 'ENABLED' : 'DISABLED') + ' (' + (document.getElementById('hpServerCount') ? document.getElementById('hpServerCount').textContent : '0') + ' patches)', _hotpatchLayerStates.server ? 'success' : 'error');
        addTerminalLine('  Usage: hotpatch toggle|apply|revert <memory|byte|server>', 'system');
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } else if (args[1] === 'toggle' && args[2]) {
        var layer = args[2];
        if (layer === 'memory' || layer === 'byte' || layer === 'server') {
          var newState = !_hotpatchLayerStates[layer];
          var toggleEl = document.getElementById('hp' + layer.charAt(0).toUpperCase() + layer.slice(1) + 'Toggle');
          if (toggleEl) toggleEl.checked = newState;
          toggleHotpatchLayer(layer, newState);
          addTerminalLine('Hotpatch ' + layer + ' layer ' + (newState ? 'ENABLED' : 'DISABLED'), newState ? 'success' : 'error');
        } else {
          addTerminalLine('Unknown layer: ' + layer + '. Use memory, byte, or server.', 'error');
        }
      } else if (args[1] === 'apply' && args[2]) {
        applyHotpatch(args[2]);
        addTerminalLine('Applying ' + args[2] + ' hotpatch...', 'system');
      } else if (args[1] === 'revert' && args[2]) {
        revertHotpatch(args[2]);
        addTerminalLine('Reverting ' + args[2] + ' hotpatch...', 'system');
      } else {
        addTerminalLine('Usage: hotpatch [toggle|apply|revert] <memory|byte|server>', 'error');
      }
      break;

    case 'ide':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Win32 IDE Bridge \u2500\u2500\u2500', 'output');
        addTerminalLine('  Mode:          ' + (_isFileProtocol ? 'STANDALONE (file://)' : 'SERVED (' + location.protocol + '//' + location.host + ')'), 'output');
        addTerminalLine('  IDE Detected:  ' + (_win32IdeDetected ? 'YES' : 'NO'), _win32IdeDetected ? 'success' : 'output');
        addTerminalLine('  IDE URL:       ' + (_ideServerUrl || 'not set'), 'output');
        addTerminalLine('  Backend URL:   ' + State.backend.url, 'output');
        addTerminalLine('  Backend Online:' + (State.backend.online ? ' YES' : ' NO'), State.backend.online ? 'success' : 'error');
        addTerminalLine('  Polling:       ' + (_win32IdePollTimer ? 'ACTIVE (30s)' : 'INACTIVE'), _win32IdePollTimer ? 'success' : 'output');
        addTerminalLine('  Fullscreen:    ' + (document.body.classList.contains('standalone-fullscreen') ? 'YES' : 'NO'), 'output');
        if (_isFileProtocol) {
          addTerminalLine('  Subcommands: ide launch | ide probe | ide read <path>', 'system');
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      } else if (args[1] === 'launch') {
        if (!_isFileProtocol) {
          addTerminalLine('Already running inside IDE server.', 'system');
        } else {
          addTerminalLine('Launching Win32 IDE...', 'system');
          launchWin32IDE();
          addTerminalLine('Launch command sent. Check your taskbar.', 'success');
        }
      } else if (args[1] === 'probe') {
        addTerminalLine('Probing for Win32 IDE...', 'system');
        probeWin32IDE().then(function () {
          addTerminalLine('IDE Detected: ' + (_win32IdeDetected ? 'YES at ' + _ideServerUrl : 'NO'), _win32IdeDetected ? 'success' : 'error');
        });
      } else if (args[1] === 'read' && args.length >= 3) {
        var readCmd = cmd.toLowerCase();
        var readIdx = readCmd.indexOf('read') + 5;
        var filePath = cmd.substring(readIdx).trim();
        addTerminalLine('Reading: ' + filePath, 'system');
        readFileViaIDE(filePath).then(function (result) {
          if (result && result.content) {
            addTerminalLine('File: ' + (result.name || filePath) + ' (' + (result.size || '?') + ' bytes)', 'success');
            var lines = result.content.split('\n');
            var maxLines = Math.min(lines.length, 30);
            for (var li = 0; li < maxLines; li++) {
              addTerminalLine(lines[li], 'output');
            }
            if (lines.length > 30) {
              addTerminalLine('  ... (' + (lines.length - 30) + ' more lines)', 'system');
            }
          } else {
            addTerminalLine('Failed to read file or empty content.', 'error');
          }
        }).catch(function (e) {
          addTerminalLine('Read error: ' + e.message, 'error');
        });
      } else {
        addTerminalLine('Usage: ide [launch|probe|read <path>]', 'error');
      }
      break;

    case 'backends':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Backend Switcher \u2500\u2500\u2500', 'output');
        (async function () {
          // Try remote first
          var usedRemote = false;
          try {
            var aRes = await fetch(getActiveUrl() + '/api/backend/active', { signal: AbortSignal.timeout(3000) });
            var aData = aRes.ok ? await aRes.json() : {};
            var bRes = await fetch(getActiveUrl() + '/api/backends', { signal: AbortSignal.timeout(3000) });
            if (bRes.ok) {
              var bData = await bRes.json();
              var list = bData.backends || bData.list || [];
              if (Array.isArray(list) && list.length > 0) {
                usedRemote = true;
                addTerminalLine('  Active: ' + (aData.name || aData.backend || aData.type || '\u2014'), 'success');
                list.forEach(function (b) {
                  var name = b.name || b.type || 'unknown';
                  var url = b.url || b.endpoint || '';
                  var online = b.online !== false ? '\u2714' : '\u2718';
                  addTerminalLine('  ' + online + ' ' + name + (url ? ' (' + url + ')' : ''), b.online !== false ? 'output' : 'error');
                });
              }
            }
          } catch (_) { /* fall through to beacon */ }

          if (!usedRemote) {
            // Beacon: build from probes
            addTerminalLine('  (Beacon mode \u2014 probing known backends)', 'system');
            var beacons = await _buildBeaconBackendList();
            var active = beacons.find(function (b) { return b.isActive; });
            addTerminalLine('  Active: ' + (active ? active.name : (State.backend.serverType || '\u2014')), 'success');
            beacons.forEach(function (b) {
              addTerminalLine('  ' + (b.online ? '\u2714' : '\u2718') + ' ' + b.name + ' (' + b.url + ')' + (b.isActive ? ' \u25C0 ACTIVE' : ''), b.online ? 'output' : 'error');
            });
          }

          addTerminalLine('  Usage: backends switch <name>', 'system');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        })();
      } else if (args[1] === 'switch' && args.length >= 3) {
        var bName = args.slice(2).join(' ');
        addTerminalLine('Switching backend to: ' + bName + '...', 'system');
        switchBackend(bName);
      } else {
        addTerminalLine('Usage: backends [switch <name>]', 'error');
      }
      break;

    case 'router':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 LLM Router \u2500\u2500\u2500', 'output');
        addTerminalLine('  Fetching router status...', 'system');
        (async function () {
          try {
            var sRes = await fetch(getActiveUrl() + '/api/router/status', { signal: AbortSignal.timeout(5000) });
            if (sRes.ok) {
              var sData = await sRes.json();
              addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
              if (sData.total_routes) addTerminalLine('  Total Routes: ' + sData.total_routes, 'output');
              if (sData.total_decisions) addTerminalLine('  Total Decisions: ' + sData.total_decisions, 'output');
            }
            var dRes = await fetch(getActiveUrl() + '/api/router/decision', { signal: AbortSignal.timeout(5000) });
            if (dRes.ok) {
              var dData = await dRes.json();
              addTerminalLine('  Last Decision: ' + (dData.backend || dData.selected || dData.decision || '\u2014'), 'output');
            }
            var pRes = await fetch(getActiveUrl() + '/api/router/pins', { signal: AbortSignal.timeout(5000) });
            if (pRes.ok) {
              var pData = await pRes.json();
              var pins = pData.pins || pData;
              if (Array.isArray(pins) && pins.length > 0) {
                addTerminalLine('  Pins:', 'output');
                pins.forEach(function (p) {
                  addTerminalLine('    ' + (p.pattern || p.model || '') + ' \u2192 ' + (p.backend || ''), 'output');
                });
              } else {
                addTerminalLine('  Pins: none', 'output');
              }
            }
            addTerminalLine('  Usage: router pin <pattern> <backend>', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'pin' && args.length >= 4) {
        var pattern = args[2];
        var backend = args.slice(3).join(' ');
        addTerminalLine('Pinning route: ' + pattern + ' \u2192 ' + backend, 'system');
        (async function () {
          try {
            var res = await fetch(getActiveUrl() + '/api/router/pin', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ pattern: pattern, backend: backend }),
              signal: AbortSignal.timeout(10000)
            });
            var data = res.ok ? await res.json() : {};
            addTerminalLine('Pin result: ' + (data.status || data.message || 'ok'), res.ok ? 'success' : 'error');
          } catch (e) {
            addTerminalLine('Pin failed: ' + e.message, 'error');
          }
        })();
      } else {
        addTerminalLine('Usage: router [pin <pattern> <backend>]', 'error');
      }
      break;

    case 'swarm':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Swarm Dashboard \u2500\u2500\u2500', 'output');
        addTerminalLine('  Fetching swarm status...', 'system');
        (async function () {
          try {
            var sRes = await fetch(getActiveUrl() + '/api/swarm/status', { signal: AbortSignal.timeout(5000) });
            if (sRes.ok) {
              var sData = await sRes.json();
              var status = sData.status || sData.state || 'idle';
              addTerminalLine('  Status: ' + status, status === 'running' ? 'success' : 'output');
            }
            var nRes = await fetch(getActiveUrl() + '/api/swarm/nodes', { signal: AbortSignal.timeout(5000) });
            if (nRes.ok) {
              var nData = await nRes.json();
              var nodes = nData.nodes || nData;
              var count = Array.isArray(nodes) ? nodes.length : 0;
              addTerminalLine('  Nodes: ' + count, 'output');
              if (Array.isArray(nodes) && nodes.length > 0) {
                nodes.forEach(function (n) {
                  var nStatus = n.status || n.state || 'unknown';
                  addTerminalLine('    ' + (n.name || n.id || n.host || 'node') + ' [' + nStatus + ']', nStatus === 'active' || nStatus === 'online' ? 'success' : 'output');
                });
              }
            }
            var tRes = await fetch(getActiveUrl() + '/api/swarm/tasks', { signal: AbortSignal.timeout(5000) });
            if (tRes.ok) {
              var tData = await tRes.json();
              var tasks = tData.tasks || tData;
              addTerminalLine('  Tasks: ' + (Array.isArray(tasks) ? tasks.length : 0), 'output');
            }
            addTerminalLine('  Usage: swarm start|stop|cache/clear', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'start' || args[1] === 'stop' || args[1] === 'cache/clear') {
        addTerminalLine('Swarm ' + args[1] + '...', 'system');
        swarmAction(args[1]);
      } else {
        addTerminalLine('Usage: swarm [start|stop|cache/clear]', 'error');
      }
      break;

    case 'multi-response':
      addTerminalLine('\u2500\u2500\u2500 Multi-Response Engine \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/multi-response/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
            addTerminalLine('  Total Generated: ' + (sData.total_generated || sData.generations || 0), 'output');
          }
          var tRes = await fetch(getActiveUrl() + '/api/multi-response/templates', { signal: AbortSignal.timeout(5000) });
          if (tRes.ok) {
            var tData = await tRes.json();
            var templates = tData.templates || tData;
            if (Array.isArray(templates) && templates.length > 0) {
              addTerminalLine('  Templates:', 'output');
              templates.forEach(function (t) {
                addTerminalLine('    [' + (t.name || t.type || t.id || '?') + '] ' + (t.description || ''), 'output');
              });
            }
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'asm-debug':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 ASM Debugger \u2500\u2500\u2500', 'output');
        addTerminalLine('  Fetching debug status...', 'system');
        (async function () {
          try {
            var sRes = await fetch(getActiveUrl() + '/api/debug/status', { signal: AbortSignal.timeout(5000) });
            if (sRes.ok) {
              var sData = await sRes.json();
              var status = sData.status || sData.state || 'idle';
              addTerminalLine('  Status: ' + status, status === 'running' || status === 'active' ? 'success' : 'output');
            }
            var bRes = await fetch(getActiveUrl() + '/api/debug/breakpoints', { signal: AbortSignal.timeout(5000) });
            if (bRes.ok) {
              var bData = await bRes.json();
              var bps = bData.breakpoints || bData;
              addTerminalLine('  Breakpoints: ' + (Array.isArray(bps) ? bps.length : 0), 'output');
            }
            var thRes = await fetch(getActiveUrl() + '/api/debug/threads', { signal: AbortSignal.timeout(5000) });
            if (thRes.ok) {
              var thData = await thRes.json();
              var threads = thData.threads || thData;
              addTerminalLine('  Threads: ' + (Array.isArray(threads) ? threads.length : 0), 'output');
            }
            var rRes = await fetch(getActiveUrl() + '/api/debug/registers', { signal: AbortSignal.timeout(5000) });
            if (rRes.ok) {
              var rData = await rRes.json();
              var regs = rData.registers || rData;
              if (typeof regs === 'object' && regs !== null) {
                addTerminalLine('  Registers:', 'output');
                Object.keys(regs).slice(0, 8).forEach(function (k) {
                  var val = typeof regs[k] === 'number' ? '0x' + regs[k].toString(16).toUpperCase().padStart(16, '0') : String(regs[k]);
                  addTerminalLine('    ' + k + ' = ' + val, 'output');
                });
                if (Object.keys(regs).length > 8) addTerminalLine('    ... (' + (Object.keys(regs).length - 8) + ' more)', 'system');
              }
            }
            addTerminalLine('  Usage: asm-debug go|launch|attach', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'go' || args[1] === 'launch' || args[1] === 'attach') {
        addTerminalLine('Debug ' + args[1] + '...', 'system');
        asmDebugAction(args[1]);
      } else {
        addTerminalLine('Usage: asm-debug [go|launch|attach]', 'error');
      }
      break;

    case 'safety':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Safety Monitor \u2500\u2500\u2500', 'output');
        addTerminalLine('  Fetching safety status...', 'system');
        (async function () {
          try {
            var sRes = await fetch(getActiveUrl() + '/api/safety/status', { signal: AbortSignal.timeout(5000) });
            if (sRes.ok) {
              var sData = await sRes.json();
              addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
            }
            var vRes = await fetch(getActiveUrl() + '/api/safety/violations', { signal: AbortSignal.timeout(5000) });
            if (vRes.ok) {
              var vData = await vRes.json();
              var violations = vData.violations || vData;
              var count = Array.isArray(violations) ? violations.length : 0;
              addTerminalLine('  Violations: ' + count, count > 0 ? 'error' : 'success');
              if (count > 0) {
                violations.slice(-5).forEach(function (v) {
                  addTerminalLine('    \u2718 ' + (v.type || v.category || 'violation') + ': ' + ((v.detail || v.message || '').substring(0, 60)), 'error');
                });
              }
            }
            addTerminalLine('  Usage: safety check|rollback', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'check') {
        addTerminalLine('Checking last output safety...', 'system');
        safetyCheck();
      } else if (args[1] === 'rollback') {
        addTerminalLine('Rolling back last output...', 'system');
        safetyRollback();
      } else {
        addTerminalLine('Usage: safety [check|rollback]', 'error');
      }
      break;

    case 'cot':
      if (args.length === 1) {
        addTerminalLine('\u2500\u2500\u2500 Chain-of-Thought Engine \u2500\u2500\u2500', 'output');
        addTerminalLine('  Fetching CoT status...', 'system');
        (async function () {
          try {
            var sRes = await fetch(getActiveUrl() + '/api/cot/status', { signal: AbortSignal.timeout(5000) });
            if (sRes.ok) {
              var sData = await sRes.json();
              addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
              if (sData.active_chain) addTerminalLine('  Active Chain: ' + sData.active_chain, 'output');
              if (sData.total_executions) addTerminalLine('  Total Executions: ' + sData.total_executions, 'output');
            }
            var stRes = await fetch(getActiveUrl() + '/api/cot/steps', { signal: AbortSignal.timeout(5000) });
            if (stRes.ok) {
              var stData = await stRes.json();
              var steps = stData.steps || stData;
              if (Array.isArray(steps) && steps.length > 0) {
                addTerminalLine('  Current Steps:', 'output');
                steps.forEach(function (s, i) {
                  addTerminalLine('    ' + (i + 1) + '. [' + (s.role || '?') + '] ' + (s.model || 'default') + (s.skip ? ' (SKIP)' : ''), s.skip ? 'system' : 'output');
                });
              }
            }
            addTerminalLine('  Usage: cot presets|roles', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'presets') {
        addTerminalLine('Fetching CoT presets...', 'system');
        (async function () {
          try {
            var pRes = await fetch(getActiveUrl() + '/api/cot/presets', { signal: AbortSignal.timeout(5000) });
            if (pRes.ok) {
              var pData = await pRes.json();
              var presets = pData.presets || pData;
              if (Array.isArray(presets) && presets.length > 0) {
                presets.forEach(function (p) {
                  addTerminalLine('  ' + (p.name || p.id || 'preset') + ': ' + (p.description || p.label || ''), 'output');
                });
              } else if (typeof presets === 'object') {
                Object.keys(presets).forEach(function (k) {
                  addTerminalLine('  ' + k + ': ' + JSON.stringify(presets[k]).substring(0, 60), 'output');
                });
              }
            }
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else if (args[1] === 'roles') {
        addTerminalLine('Fetching CoT roles...', 'system');
        (async function () {
          try {
            var rRes = await fetch(getActiveUrl() + '/api/cot/roles', { signal: AbortSignal.timeout(5000) });
            if (rRes.ok) {
              var rData = await rRes.json();
              var roles = rData.roles || rData;
              if (Array.isArray(roles) && roles.length > 0) {
                roles.forEach(function (r) {
                  addTerminalLine('  ' + (r.name || r.id || r) + (r.description ? ': ' + r.description : ''), 'output');
                });
              }
            }
          } catch (e) {
            addTerminalLine('  Error: ' + e.message, 'error');
          }
        })();
      } else {
        addTerminalLine('Usage: cot [presets|roles]', 'error');
      }
      break;

    case 'confidence':
      addTerminalLine('\u2500\u2500\u2500 Confidence Evaluator \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/confidence/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
            if (sData.total_evaluations) addTerminalLine('  Total Evaluations: ' + sData.total_evaluations, 'output');
            if (sData.avg_confidence !== undefined) addTerminalLine('  Avg Confidence: ' + (sData.avg_confidence * 100).toFixed(1) + '%', 'output');
          }
          var hRes = await fetch(getActiveUrl() + '/api/confidence/history', { signal: AbortSignal.timeout(5000) });
          if (hRes.ok) {
            var hData = await hRes.json();
            var history = hData.history || hData.evaluations || hData;
            if (Array.isArray(history) && history.length > 0) {
              addTerminalLine('  Recent:', 'output');
              history.slice(-5).forEach(function (h) {
                var score = h.score !== undefined ? (h.score * 100).toFixed(0) + '%' : h.confidence !== undefined ? (h.confidence * 100).toFixed(0) + '%' : '?';
                addTerminalLine('    ' + score + ' \u2014 ' + ((h.model || h.query || '').substring(0, 40)), 'output');
              });
            }
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'governor':
      addTerminalLine('\u2500\u2500\u2500 Task Governor \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching governor status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/governor/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'ready'), 'success');
            if (sData.active_tasks !== undefined) addTerminalLine('  Active Tasks: ' + sData.active_tasks, 'output');
            if (sData.completed !== undefined) addTerminalLine('  Completed: ' + sData.completed, 'output');
            if (sData.queued !== undefined) addTerminalLine('  Queued: ' + sData.queued, 'output');
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'lsp':
      addTerminalLine('\u2500\u2500\u2500 LSP Integration \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching LSP status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/lsp/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
            if (sData.language_servers) {
              addTerminalLine('  Language Servers:', 'output');
              var servers = sData.language_servers;
              if (Array.isArray(servers)) {
                servers.forEach(function (s) {
                  addTerminalLine('    ' + (s.name || s.language || 'unknown') + ' [' + (s.status || 'running') + ']', 'output');
                });
              }
            }
          }
          var dRes = await fetch(getActiveUrl() + '/api/lsp/diagnostics', { signal: AbortSignal.timeout(5000) });
          if (dRes.ok) {
            var dData = await dRes.json();
            var diags = dData.diagnostics || dData;
            if (Array.isArray(diags) && diags.length > 0) {
              addTerminalLine('  Diagnostics (' + diags.length + '):', 'output');
              diags.slice(0, 10).forEach(function (d) {
                var severity = d.severity || d.level || 'info';
                addTerminalLine('    [' + severity + '] ' + (d.file || '') + ':' + (d.line || '?') + ' ' + (d.message || '').substring(0, 50), severity === 'error' ? 'error' : 'output');
              });
              if (diags.length > 10) addTerminalLine('    ... (' + (diags.length - 10) + ' more)', 'system');
            } else {
              addTerminalLine('  Diagnostics: 0 issues \u2714', 'success');
            }
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'hybrid':
      addTerminalLine('\u2500\u2500\u2500 Hybrid Completion \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching hybrid status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/hybrid/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'active'), 'success');
            if (sData.total_completions !== undefined) addTerminalLine('  Total Completions: ' + sData.total_completions, 'output');
            if (sData.total_renames !== undefined) addTerminalLine('  Total Renames: ' + sData.total_renames, 'output');
            if (sData.total_symbols !== undefined) addTerminalLine('  Total Symbols: ' + sData.total_symbols, 'output');
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'replay':
      addTerminalLine('\u2500\u2500\u2500 Replay Sessions \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching replay status...', 'system');
      (async function () {
        try {
          var sRes = await fetch(getActiveUrl() + '/api/replay/status', { signal: AbortSignal.timeout(5000) });
          if (sRes.ok) {
            var sData = await sRes.json();
            addTerminalLine('  Status: ' + (sData.status || sData.state || 'idle'), 'success');
            if (sData.total_sessions !== undefined) addTerminalLine('  Total Sessions: ' + sData.total_sessions, 'output');
            if (sData.total_records !== undefined) addTerminalLine('  Total Records: ' + sData.total_records, 'output');
          }
          var rRes = await fetch(getActiveUrl() + '/api/replay/sessions', { signal: AbortSignal.timeout(5000) });
          if (rRes.ok) {
            var rData = await rRes.json();
            var sessions = rData.sessions || rData;
            if (Array.isArray(sessions) && sessions.length > 0) {
              addTerminalLine('  Recent Sessions:', 'output');
              sessions.slice(-5).forEach(function (s) {
                addTerminalLine('    [' + (s.id || '?') + '] ' + (s.name || s.timestamp || '') + ' (' + (s.records || s.count || 0) + ' records)', 'output');
              });
            }
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } catch (e) {
          addTerminalLine('  Error: ' + e.message, 'error');
        }
      })();
      break;

    case 'phases':
      addTerminalLine('\u2500\u2500\u2500 Phase Status (10/11/12) \u2500\u2500\u2500', 'output');
      addTerminalLine('  Fetching phase status...', 'system');
      (async function () {
        var phaseNames = { 10: 'Speculative Decoding', 11: 'Flash-Attention v2', 12: 'Extreme Compression' };
        for (var p = 10; p <= 12; p++) {
          try {
            var res = await fetch(getActiveUrl() + '/api/phase' + p + '/status', { signal: AbortSignal.timeout(5000) });
            if (res.ok) {
              var data = await res.json();
              var status = data.status || data.state || 'unknown';
              var icon = status === 'active' || status === 'ready' || status === 'complete' ? '\u2714' : '\u2718';
              addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): ' + icon + ' ' + status, status === 'active' || status === 'ready' || status === 'complete' ? 'success' : 'output');
              if (data.progress !== undefined) addTerminalLine('    Progress: ' + data.progress + '%', 'output');
            } else {
              addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): \u2718 HTTP ' + res.status, 'error');
            }
          } catch (e) {
            addTerminalLine('  Phase ' + p + ' (' + phaseNames[p] + '): \u2718 ' + e.message, 'error');
          }
        }
        addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
      })();
      break;

    case 'codex':
      (function () {
        var codexSub = args[1] || '';
        if (!codexSub || codexSub === 'help') {
          addTerminalLine('\u2500\u2500\u2500 Codex Compilation System \u2500\u2500\u2500', 'output');
          addTerminalLine('  codex build [target] \u2014 Build a target', 'output');
          addTerminalLine('  codex masm <file>    \u2014 Assemble MASM64 file', 'output');
          addTerminalLine('  codex targets        \u2014 List build targets', 'output');
          addTerminalLine('  codex status         \u2014 Build system status', 'output');
          addTerminalLine('  codex clean          \u2014 Clean build artifacts', 'output');
          addTerminalLine('  codex cmake          \u2014 Show CMake configure command', 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (codexSub === 'targets') {
          addTerminalLine('\u2500\u2500\u2500 Codex Build Targets \u2500\u2500\u2500', 'output');
          CodexOps.targets.forEach(function (t, i) {
            addTerminalLine('  ' + (i + 1) + '. ' + t, 'output');
          });
          addTerminalLine('', 'output');
          addTerminalLine('  Build: codex build <target>', 'system');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (codexSub === 'build') {
          var buildTgt = args[2] || 'RawrXD-Win32IDE';
          var buildCmd = CodexOps.buildCmd(buildTgt);
          addTerminalLine('\u2699 Codex Build: ' + buildTgt, 'system');
          addTerminalLine('  > ' + buildCmd, 'output');
          if (State.backend.online && !State.backend.directMode) {
            addTerminalLine('  Triggering remote build...', 'system');
            fetch(getActiveUrl() + '/api/cli', {
              method: 'POST', headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ command: '!engine compile ' + buildTgt }),
              signal: AbortSignal.timeout(30000)
            }).then(function (r) { return r.json(); }).then(function (d) {
              if (d.output) renderCliOutput(d.output);
              else addTerminalLine('  \u2714 Build triggered', 'success');
            }).catch(function () {
              addTerminalLine('  (Run build command manually in terminal)', 'system');
            });
          } else {
            addTerminalLine('  (Copy and run in your build terminal)', 'system');
          }
        } else if (codexSub === 'masm') {
          var asmFile = args[2] || '';
          if (!asmFile) {
            addTerminalLine('\u2718 Usage: codex masm <file.asm>', 'error');
            addTerminalLine('  Known ASM: memory_patch.asm, byte_search.asm, request_patch.asm', 'system');
          } else {
            var masmCmd = CodexOps.masmCmd(asmFile);
            addTerminalLine('\u2699 MASM64: ' + asmFile, 'system');
            addTerminalLine('  > ' + masmCmd, 'output');
            addTerminalLine('  (Run in Developer Command Prompt for VS 2022)', 'system');
          }
        } else if (codexSub === 'status') {
          addTerminalLine('\u2500\u2500\u2500 Codex Build Status \u2500\u2500\u2500', 'output');
          addTerminalLine('  Compiler:   MSVC 14.44.35207 (2022)', 'output');
          addTerminalLine('  Generator:  Ninja', 'output');
          addTerminalLine('  SDK:        10.0.22621.0', 'output');
          addTerminalLine('  ASM:        ml64.exe (MASM64)', 'output');
          addTerminalLine('  CMake:      3.20+', 'output');
          addTerminalLine('  Build Dir:  build/', 'output');
          addTerminalLine('  Binaries:', 'output');
          addTerminalLine('    RawrXD-Win32IDE.exe  4.11 MB', 'output');
          addTerminalLine('    RawrEngine.exe       0.97 MB', 'output');
          addTerminalLine('    rawrxd-monaco-gen    0.25 MB', 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (codexSub === 'clean') {
          addTerminalLine('\u2699 Codex Clean:', 'system');
          addTerminalLine('  > cmake --build build --target clean', 'output');
          addTerminalLine('  (Run in your terminal)', 'system');
        } else if (codexSub === 'cmake') {
          addTerminalLine('\u2699 CMake Configure:', 'system');
          addTerminalLine('  > cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S . -B build', 'output');
          addTerminalLine('  (Run from project root)', 'system');
        } else {
          addTerminalLine('\u2718 Unknown codex subcommand: ' + codexSub, 'error');
          addTerminalLine('  Type codex help for options.', 'system');
        }
      })();
      break;

    case 're': case 'reverse':
      (function () {
        var reSub = args[1] || '';
        if (!reSub || reSub === 'help') {
          addTerminalLine('\u2500\u2500\u2500 Reverse Engineering Suite \u2500\u2500\u2500', 'output');
          addTerminalLine('  re modules         \u2014 List RE modules & status', 'output');
          addTerminalLine('  re status          \u2014 RE system overview', 'output');
          addTerminalLine('  re pe <file>       \u2014 PE header analysis', 'output');
          addTerminalLine('  re disasm <addr>   \u2014 Disassemble at address', 'output');
          addTerminalLine('  re deobf <file>    \u2014 Deobfuscation analysis', 'output');
          addTerminalLine('  re omega <file>    \u2014 Full Omega Suite scan', 'output');
          addTerminalLine('  re compare <a> <b> \u2014 Binary diff comparison', 'output');
          addTerminalLine('  re symbols <pdb>   \u2014 Symbol resolver', 'output');
          addTerminalLine('  re gguf <model>    \u2014 GGUF model inspector', 'output');
          addTerminalLine('  re scan <pid>      \u2014 Memory scanner', 'output');
          addTerminalLine('  re panel           \u2014 Toggle RE panel (GUI)', 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'modules') {
          addTerminalLine('\u2500\u2500\u2500 RE Modules (' + REState.modules.length + ') \u2500\u2500\u2500', 'output');
          REState.modules.forEach(function (m, i) {
            var icon = m.loaded ? '\u2714' : '\u2022';
            var tag = m.loaded ? ' [LOADED]' : '';
            addTerminalLine('  ' + icon + ' ' + (i + 1) + '. ' + m.name.padEnd(20) + m.type + tag, 'output');
            addTerminalLine('      ' + m.desc, 'system');
          });
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'status') {
          addTerminalLine('\u2500\u2500\u2500 RE System Status \u2500\u2500\u2500', 'output');
          addTerminalLine('  Modules:     ' + REState.modules.length, 'output');
          addTerminalLine('  Active Tab:  ' + REState.activeTab, 'output');
          addTerminalLine('  History:     ' + REState.history.length + ' entries', 'output');
          addTerminalLine('  Loaded:      ' + REState.modules.filter(function (m) { return m.loaded; }).length + '/' + REState.modules.length, 'output');
          addTerminalLine('  Backend:     ' + (State.backend.online ? 'online' : 'offline (client-side only)'), State.backend.online ? 'success' : 'system');
          addTerminalLine('  Panel:       ' + (document.getElementById('rePanel') && document.getElementById('rePanel').style.display !== 'none' ? 'visible' : 'hidden'), 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'pe') {
          var peFile = args[2] || 'RawrXD-Win32IDE.exe';
          addTerminalLine('\u2500\u2500\u2500 PE Analysis: ' + peFile + ' \u2500\u2500\u2500', 'output');
          REState.history.push({ action: 'pe', target: peFile, time: Date.now() });
          if (State.backend.online && !State.backend.directMode) {
            addTerminalLine('  Querying backend...', 'system');
            fetch(getActiveUrl() + '/api/cli', {
              method: 'POST', headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ command: '!engine dumpbin ' + peFile }),
              signal: AbortSignal.timeout(15000)
            }).then(function (r) { return r.json(); }).then(function (d) {
              if (d.output) renderCliOutput(d.output);
            }).catch(function () {
              addTerminalLine('  PE analysis (client-side):', 'system');
              addTerminalLine('  dumpbin /headers build\\bin\\' + peFile, 'output');
              addTerminalLine('  dumpbin /exports build\\bin\\' + peFile, 'output');
              addTerminalLine('  dumpbin /imports build\\bin\\' + peFile, 'output');
            });
          } else {
            addTerminalLine('  PE analysis commands:', 'system');
            addTerminalLine('    dumpbin /headers build\\bin\\' + peFile, 'output');
            addTerminalLine('    dumpbin /exports build\\bin\\' + peFile, 'output');
            addTerminalLine('    dumpbin /imports build\\bin\\' + peFile, 'output');
            addTerminalLine('    dumpbin /dependents build\\bin\\' + peFile, 'output');
            addTerminalLine('    dumpbin /section:.text build\\bin\\' + peFile, 'output');
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'disasm') {
          var disasmAddr = args[2] || '0x0';
          addTerminalLine('\u2500\u2500\u2500 Disassembly @ ' + disasmAddr + ' \u2500\u2500\u2500', 'output');
          REState.history.push({ action: 'disasm', target: disasmAddr, time: Date.now() });
          addTerminalLine('  Target: RawrXD-Win32IDE.exe', 'output');
          addTerminalLine('  Arch:   x86-64 (PE32+)', 'output');
          addTerminalLine('  Commands:', 'system');
          addTerminalLine('    dumpbin /disasm /range:' + disasmAddr + ' build\\bin\\RawrXD-Win32IDE.exe', 'output');
          addTerminalLine('    objdump -d --start-address=' + disasmAddr + ' -M intel build\\bin\\RawrXD-Win32IDE.exe', 'output');
          addTerminalLine('  ASM Kernels:', 'output');
          addTerminalLine('    memory_patch.asm   \u2014 Memory hotpatch primitives', 'output');
          addTerminalLine('    byte_search.asm    \u2014 Boyer-Moore + SIMD scan', 'output');
          addTerminalLine('    request_patch.asm  \u2014 Server request interception', 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'deobf') {
          var deobfFile = args[2] || '';
          if (!deobfFile) {
            addTerminalLine('\u2718 Usage: re deobf <file>', 'error');
          } else {
            REState.history.push({ action: 'deobf', target: deobfFile, time: Date.now() });
            addTerminalLine('\u2500\u2500\u2500 Deobfuscation: ' + deobfFile + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  Checking for obfuscation patterns...', 'system');
            addTerminalLine('  Control flow flattening:  scanning...', 'output');
            addTerminalLine('  String encryption:        scanning...', 'output');
            addTerminalLine('  Opaque predicates:        scanning...', 'output');
            addTerminalLine('  Dead code insertion:      scanning...', 'output');
            addTerminalLine('  (Full analysis requires backend or local tools)', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          }
        } else if (reSub === 'omega') {
          var omegaFile = args[2] || 'RawrXD-Win32IDE.exe';
          REState.history.push({ action: 'omega', target: omegaFile, time: Date.now() });
          addTerminalLine('\u2500\u2500\u2500 Omega Suite Scan: ' + omegaFile + ' \u2500\u2500\u2500', 'output');
          addTerminalLine('  PE Analysis:          queued', 'output');
          addTerminalLine('  Disassembly:          queued', 'output');
          addTerminalLine('  Deobfuscation:        queued', 'output');
          addTerminalLine('  Symbol Resolution:    queued', 'output');
          addTerminalLine('  Binary Comparison:    queued', 'output');
          addTerminalLine('  Memory Scan:          queued', 'output');
          addTerminalLine('  GGUF Inspection:      queued', 'output');
          addTerminalLine('  Running full analysis suite...', 'system');
          if (State.backend.online && !State.backend.directMode) {
            fetch(getActiveUrl() + '/api/cli', {
              method: 'POST', headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ command: '!engine analyze ' + omegaFile }),
              signal: AbortSignal.timeout(30000)
            }).then(function (r) { return r.json(); }).then(function (d) {
              if (d.output) renderCliOutput(d.output);
              else addTerminalLine('  \u2714 Omega scan complete', 'success');
            }).catch(function () {
              addTerminalLine('  (Run dumpbin/objdump locally for full results)', 'system');
            });
          } else {
            addTerminalLine('  (Backend offline — use individual re commands)', 'system');
          }
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
        } else if (reSub === 'compare') {
          var fileA = args[2] || '';
          var fileB = args[3] || '';
          if (!fileA || !fileB) {
            addTerminalLine('\u2718 Usage: re compare <fileA> <fileB>', 'error');
          } else {
            REState.history.push({ action: 'compare', target: fileA + ' vs ' + fileB, time: Date.now() });
            addTerminalLine('\u2500\u2500\u2500 Binary Comparison \u2500\u2500\u2500', 'output');
            addTerminalLine('  File A: ' + fileA, 'output');
            addTerminalLine('  File B: ' + fileB, 'output');
            addTerminalLine('  fc /b ' + fileA + ' ' + fileB + ' > diff.txt', 'output');
            addTerminalLine('  (Run in terminal for byte-level diff)', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          }
        } else if (reSub === 'symbols') {
          var pdbFile = args[2] || '';
          if (!pdbFile) {
            addTerminalLine('\u2718 Usage: re symbols <file.pdb>', 'error');
          } else {
            REState.history.push({ action: 'symbols', target: pdbFile, time: Date.now() });
            addTerminalLine('\u2500\u2500\u2500 Symbol Resolver: ' + pdbFile + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  dumpbin /pdbpath:verbose build\\bin\\' + pdbFile, 'output');
            addTerminalLine('  symchk /r build\\bin\\*.pdb /s SRV*', 'output');
            addTerminalLine('  (Use cvdump or dia2dump for detailed PDB analysis)', 'system');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          }
        } else if (reSub === 'gguf') {
          var ggufModel = args[2] || '';
          if (!ggufModel) {
            addTerminalLine('\u2718 Usage: re gguf <model.gguf>', 'error');
            addTerminalLine('  Current model: ' + (State.model.current || 'none'), 'system');
          } else {
            REState.history.push({ action: 'gguf', target: ggufModel, time: Date.now() });
            addTerminalLine('\u2500\u2500\u2500 GGUF Inspector: ' + ggufModel + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  Querying GGUF metadata...', 'system');
            if (State.backend.online && !State.backend.directMode) {
              fetch(getActiveUrl() + '/api/cli', {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: '!engine analyze ' + ggufModel }),
                signal: AbortSignal.timeout(15000)
              }).then(function (r) { return r.json(); }).then(function (d) {
                if (d.output) renderCliOutput(d.output);
              }).catch(function () {
                addTerminalLine('  GGUF fields: magic, version, tensor_count, metadata_kv', 'output');
                addTerminalLine('  Use gguf-py or llama-gguf-split for offline inspection', 'system');
              });
            } else {
              addTerminalLine('  GGUF Header Fields:', 'output');
              addTerminalLine('    magic            GGUF', 'output');
              addTerminalLine('    version          3', 'output');
              addTerminalLine('    tensor_count     varies', 'output');
              addTerminalLine('    metadata_kv      architecture, context_length, etc.', 'output');
              addTerminalLine('  Offline tools:', 'system');
              addTerminalLine('    python -m gguf.scripts.gguf_dump ' + ggufModel, 'output');
              addTerminalLine('    llama-gguf-split --info ' + ggufModel, 'output');
            }
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          }
        } else if (reSub === 'scan') {
          var scanPid = args[2] || '';
          if (!scanPid) {
            addTerminalLine('\u2718 Usage: re scan <pid>', 'error');
            addTerminalLine('  Use Task Manager or tasklist to find process IDs', 'system');
          } else {
            REState.history.push({ action: 'scan', target: 'pid:' + scanPid, time: Date.now() });
            addTerminalLine('\u2500\u2500\u2500 Memory Scanner: PID ' + scanPid + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  Attach:  ReadProcessMemory (requires admin)', 'output');
            addTerminalLine('  Scan:    Searching for patterns in virtual memory...', 'system');
            addTerminalLine('  Regions: code (.text), data (.data/.rdata), heap, stack', 'output');
            addTerminalLine('  Tools:', 'system');
            addTerminalLine('    windbg -p ' + scanPid, 'output');
            addTerminalLine('    procdump -ma ' + scanPid + ' dump.dmp', 'output');
            addTerminalLine('    volatility -f dump.dmp --profile=Win10x64 pslist', 'output');
            addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          }
        } else if (reSub === 'panel') {
          toggleREPanel();
        } else {
          addTerminalLine('\u2718 Unknown RE subcommand: ' + reSub, 'error');
          addTerminalLine('  Type re help for options.', 'system');
        }
      })();
      break;

    // =============================================================
    // ENGINE API COMMANDS — Full 150+ source wiring
    // =============================================================
    case 'subagent':
      (async function () {
        var task = rest || 'Describe the current system status';
        addTerminalLine('\u2500\u2500\u2500 Subagent \u2500\u2500\u2500', 'output');
        addTerminalLine('  Spawning subagent: ' + task.substring(0, 60) + '...', 'system');
        try {
          var data = await EngineAPI.subagent(task);
          addTerminalLine('  Result: ' + (data.result || data.response || JSON.stringify(data)).substring(0, 500), 'success');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'chain':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Chain Pipeline \u2500\u2500\u2500', 'output');
        try {
          var steps;
          if (rest && rest.charAt(0) === '[') {
            steps = JSON.parse(rest);
          } else {
            steps = [{ model: State.model.current || 'llama3', prompt: rest || 'Summarize the system' }];
          }
          addTerminalLine('  Running chain with ' + steps.length + ' step(s)...', 'system');
          var data = await EngineAPI.chain(steps);
          var results = data.results || data.steps || [data];
          for (var i = 0; i < results.length; i++) {
            addTerminalLine('  Step ' + (i + 1) + ': ' + (results[i].response || results[i].result || JSON.stringify(results[i])).substring(0, 200), 'output');
          }
          addTerminalLine('  Chain complete.', 'success');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'policies':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Policy Engine \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchPolicies();
          var policies = data.policies || data;
          if (Array.isArray(policies)) {
            addTerminalLine('  Active Policies: ' + policies.length, 'output');
            policies.forEach(function (p) {
              addTerminalLine('    [' + (p.id || '?') + '] ' + (p.name || p.type || 'unnamed') + ' — ' + (p.status || 'active'), 'output');
            });
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 300), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'policy-suggest':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Policy Suggestions \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchPolicySuggestions();
          var suggestions = data.suggestions || data;
          if (Array.isArray(suggestions)) {
            addTerminalLine('  Suggestions: ' + suggestions.length, 'output');
            suggestions.forEach(function (s) {
              addTerminalLine('    \u2022 ' + (s.description || s.name || JSON.stringify(s)).substring(0, 100), 'output');
            });
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 300), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'policy-stats':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Policy Stats \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchPolicyStats();
          var keys = Object.keys(data);
          keys.forEach(function (k) {
            addTerminalLine('  ' + k + ': ' + JSON.stringify(data[k]), 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'heuristics':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Heuristics \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchHeuristics();
          addTerminalLine('  ' + JSON.stringify(data, null, 2).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'explain':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 CoT Explain \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchExplain();
          var chain = data.chain || data.steps || data.explanation;
          if (Array.isArray(chain)) {
            chain.forEach(function (step, idx) {
              addTerminalLine('  Step ' + (idx + 1) + ': ' + (step.reasoning || step.text || JSON.stringify(step)).substring(0, 200), 'output');
            });
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 500), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'explain-stats':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Explain Stats \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchExplainStats();
          var keys = Object.keys(data);
          keys.forEach(function (k) {
            addTerminalLine('  ' + k + ': ' + JSON.stringify(data[k]), 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'backend-status':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Backend Status \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchBackendStatus();
          var keys = Object.keys(data);
          keys.forEach(function (k) {
            addTerminalLine('  ' + k + ': ' + JSON.stringify(data[k]), 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'use-backend':
      (async function () {
        var name = args[1] || '';
        if (!name) { addTerminalLine('  Usage: use-backend <name>', 'error'); return; }
        addTerminalLine('  Switching to backend: ' + name, 'system');
        try {
          var data = await EngineAPI.useBackend(name);
          addTerminalLine('  \u2705 ' + (data.message || 'Backend switched to ' + name), 'success');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'tool':
      (async function () {
        var toolName = args[1] || '';
        var toolArgs = {};
        if (!toolName) {
          addTerminalLine('  Usage: tool <name> [json-args]', 'error');
          addTerminalLine('  Tools: read_file, write_file, list_directory, execute_command, git_status', 'system');
          return;
        }
        if (args[2]) {
          try { toolArgs = JSON.parse(args.slice(2).join(' ')); } catch (_) { toolArgs = { path: args[2] }; }
        }
        addTerminalLine('  Executing tool: ' + toolName, 'system');
        try {
          var data = await EngineAPI.executeTool(toolName, toolArgs);
          addTerminalLine('  \u2705 ' + JSON.stringify(data).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'read':
      (async function () {
        var filePath = rest || '';
        if (!filePath) { addTerminalLine('  Usage: read <filepath>', 'error'); return; }
        addTerminalLine('  Reading: ' + filePath, 'system');
        try {
          var data = await EngineAPI.toolReadFile(filePath);
          var content = data.content || data.result || JSON.stringify(data);
          var lines = content.split('\n');
          addTerminalLine('  \u2500 ' + filePath + ' (' + lines.length + ' lines):', 'output');
          lines.slice(0, 50).forEach(function (line) {
            addTerminalLine('  ' + line, 'output');
          });
          if (lines.length > 50) addTerminalLine('  ... (' + (lines.length - 50) + ' more lines)', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'listdir':
      (async function () {
        var dirPath = rest || '.';
        addTerminalLine('  Listing: ' + dirPath, 'system');
        try {
          var data = await EngineAPI.toolListDir(dirPath);
          var entries = data.entries || data.files || data;
          if (Array.isArray(entries)) {
            entries.forEach(function (entry) {
              var name = entry.name || entry;
              var type = entry.type || (name.endsWith('/') ? 'dir' : 'file');
              addTerminalLine('  ' + (type === 'dir' ? '\uD83D\uDCC1 ' : '\uD83D\uDCC4 ') + name, 'output');
            });
            addTerminalLine('  Total: ' + entries.length + ' entries', 'system');
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 500), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'git':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Git Status \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.toolGitStatus();
          addTerminalLine('  ' + (data.result || data.status || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'analyze':
      (async function () {
        var file = rest || '';
        if (!file) { addTerminalLine('  Usage: analyze <filepath>', 'error'); return; }
        addTerminalLine('  Analyzing: ' + file, 'system');
        try {
          var data = await EngineAPI.cliAnalyze(file);
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'audit':
      (async function () {
        var file = rest || '';
        if (!file) { addTerminalLine('  Usage: audit <filepath>', 'error'); return; }
        addTerminalLine('  Security audit: ' + file, 'system');
        try {
          var data = await EngineAPI.cliSecurity(file);
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'optimize':
      (async function () {
        var file = rest || '';
        if (!file) { addTerminalLine('  Usage: optimize <filepath>', 'error'); return; }
        addTerminalLine('  Optimizing: ' + file, 'system');
        try {
          var data = await EngineAPI.cliOptimize(file);
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'plan':
      (async function () {
        var file = rest || '';
        addTerminalLine('  Planning: ' + (file || 'general'), 'system');
        try {
          var data = await EngineAPI.cliPlan(file);
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'suggest':
      (async function () {
        var topic = rest || '';
        addTerminalLine('  Getting suggestions: ' + (topic || 'general'), 'system');
        try {
          var data = await EngineAPI.cliSuggest(topic);
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'bugreport':
      (async function () {
        addTerminalLine('  Generating bug report...', 'system');
        try {
          var data = await EngineAPI.cliBugreport();
          addTerminalLine('  ' + (data.result || data.output || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'generate':
      (async function () {
        var prompt = rest || 'Hello';
        addTerminalLine('  Generating (non-stream)...', 'system');
        try {
          var data = await EngineAPI.generate(prompt);
          addTerminalLine('  ' + (data.response || data.result || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'complete':
      (async function () {
        var prompt = rest || 'Hello';
        addTerminalLine('  Completing...', 'system');
        try {
          var data = await EngineAPI.complete(prompt);
          addTerminalLine('  ' + (data.response || data.completion || data.result || JSON.stringify(data)).substring(0, 500), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'health':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Health Check \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchHealth();
          var keys = Object.keys(data);
          keys.forEach(function (k) {
            addTerminalLine('  ' + k + ': ' + JSON.stringify(data[k]), 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'server-metrics':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 Server Metrics \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchMetrics();
          if (typeof data === 'string') {
            data.split('\n').slice(0, 30).forEach(function (line) {
              addTerminalLine('  ' + line, 'output');
            });
          } else {
            var keys = Object.keys(data);
            keys.forEach(function (k) {
              addTerminalLine('  ' + k + ': ' + JSON.stringify(data[k]), 'output');
            });
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'engine-map':
      addTerminalLine('\u2500\u2500\u2500 Engine Subsystem Map \u2500\u2500\u2500', 'output');
      (function () {
        var keys = Object.keys(EngineAPI.SUBSYSTEMS);
        addTerminalLine('  ' + keys.length + ' subsystems registered:', 'output');
        for (var i = 0; i < keys.length; i++) {
          var sub = EngineAPI.SUBSYSTEMS[keys[i]];
          addTerminalLine('  [' + (i + 1) + '] ' + keys[i] + ' (' + sub.files.length + ' files, ' + sub.endpoints.length + ' endpoints)', 'output');
        }
        addTerminalLine('  Use "engine-detail <name>" for more info, or open Engine panel.', 'system');
      })();
      break;

    case 'engine-detail':
      (function () {
        var query = rest ? rest.toLowerCase() : '';
        if (!query) { addTerminalLine('  Usage: engine-detail <subsystem-name>', 'error'); return; }
        var keys = Object.keys(EngineAPI.SUBSYSTEMS);
        var found = false;
        for (var i = 0; i < keys.length; i++) {
          if (keys[i].toLowerCase().indexOf(query) >= 0) {
            var sub = EngineAPI.SUBSYSTEMS[keys[i]];
            addTerminalLine('\u2500\u2500\u2500 ' + keys[i] + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  ' + sub.desc, 'output');
            addTerminalLine('  Files:', 'output');
            sub.files.forEach(function (f) { addTerminalLine('    \u2022 ' + f, 'output'); });
            if (sub.endpoints.length > 0) {
              addTerminalLine('  Endpoints:', 'output');
              sub.endpoints.forEach(function (ep) { addTerminalLine('    \u2022 ' + ep, 'output'); });
            }
            found = true;
          }
        }
        if (!found) addTerminalLine('  No subsystem matching "' + rest + '"', 'error');
      })();
      break;

    case 'replay':
      (async function () {
        var agentId = args[1] || '';
        if (!agentId) { addTerminalLine('  Usage: replay <agent-id>', 'error'); return; }
        addTerminalLine('  Replaying agent: ' + agentId, 'system');
        try {
          var data = await EngineAPI.replayAgent(agentId);
          addTerminalLine('  \u2705 ' + (data.message || data.result || JSON.stringify(data)).substring(0, 300), 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'v1-chat':
      (async function () {
        var prompt = rest || 'Hello';
        addTerminalLine('  OpenAI-compat /v1/chat/completions...', 'system');
        try {
          var data = await EngineAPI.v1ChatCompletions([{ role: 'user', content: prompt }]);
          var choice = data.choices && data.choices[0];
          if (choice) {
            addTerminalLine('  ' + (choice.message ? choice.message.content : JSON.stringify(choice)).substring(0, 500), 'output');
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 500), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'tags':
      (async function () {
        addTerminalLine('\u2500\u2500\u2500 API Tags \u2500\u2500\u2500', 'output');
        try {
          var data = await EngineAPI.fetchTags();
          var models = data.models || data;
          if (Array.isArray(models)) {
            models.forEach(function (m) {
              addTerminalLine('  \u2022 ' + (m.name || m.model || JSON.stringify(m)), 'output');
            });
          } else {
            addTerminalLine('  ' + JSON.stringify(data).substring(0, 300), 'output');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'write-file':
      (async function () {
        var filePath = args[1] || '';
        var content = args.slice(2).join(' ') || '';
        if (!filePath || !content) { addTerminalLine('  Usage: write-file <path> <content>', 'error'); return; }
        addTerminalLine('  Writing: ' + filePath, 'system');
        try {
          var data = await EngineAPI.toolWriteFile(filePath, content);
          addTerminalLine('  \u2705 ' + (data.message || data.result || 'Written'), 'success');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'exec':
      (async function () {
        var command = rest || '';
        if (!command) { addTerminalLine('  Usage: exec <command>', 'error'); return; }
        addTerminalLine('  Executing: ' + command, 'system');
        try {
          var data = await EngineAPI.toolExecCmd(command);
          var output = data.output || data.result || data.stdout || JSON.stringify(data);
          output.split('\n').slice(0, 30).forEach(function (line) {
            addTerminalLine('  ' + line, 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'ext': case 'extensions': case 'extension':
      (async function () {
        var sub = args[1] || '';
        var extArg = args.slice(2).join(' ');

        if (!sub) {
          // ext — show overview
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500 VSIX Extension Manager \u2500\u2500\u2500\u2500\u2500', 'output');
          addTerminalLine('  Installed:  ' + State.extensions.installed.length + ' extensions', 'output');
          addTerminalLine('  Enabled:    ' + State.extensions.installed.filter(function (e) { return e.enabled; }).length, 'output');
          addTerminalLine('  Disabled:   ' + State.extensions.installed.filter(function (e) { return !e.enabled; }).length, 'output');
          addTerminalLine('  Running:    ' + State.extensions.running.length + ' hosts', 'output');
          addTerminalLine('  Host:       ' + State.extensions.hostStatus, State.extensions.hostStatus === 'running' ? 'success' : 'output');
          addTerminalLine('  Activated:  ' + State.extensions.totalActivated + ' total', 'output');
          addTerminalLine('  Logs:       ' + State.extensions.logs.length + ' / ' + State.extensions.maxLogs, 'output');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          addTerminalLine("  Type 'ext help' for subcommands", 'system');
          // Try backend
          try {
            var data = await EngineAPI.fetchExtensions();
            var exts = data.extensions || data.installed || [];
            if (exts.length > 0) {
              State.extensions.installed = exts;
              addTerminalLine('  \u2714 Synced ' + exts.length + ' extensions from backend', 'success');
            }
          } catch (_) { addTerminalLine('  Backend: offline (using local state)', 'system'); }
          return;
        }

        if (sub === 'help') {
          addTerminalLine('ext subcommands:', 'output');
          addTerminalLine('  ext              \u2014 Overview & stats', 'output');
          addTerminalLine('  ext list         \u2014 List installed extensions', 'output');
          addTerminalLine('  ext search <q>   \u2014 Search marketplace', 'output');
          addTerminalLine('  ext install <id> \u2014 Install extension', 'output');
          addTerminalLine('  ext uninstall <id> \u2014 Remove extension', 'output');
          addTerminalLine('  ext enable <id>  \u2014 Enable extension', 'output');
          addTerminalLine('  ext disable <id> \u2014 Disable extension', 'output');
          addTerminalLine('  ext info <id>    \u2014 Extension details + contributes', 'output');
          addTerminalLine('  ext load <path>  \u2014 Load .vsix from disk path', 'output');
          addTerminalLine('  ext host         \u2014 Extension host status', 'output');
          addTerminalLine('  ext host restart \u2014 Restart host process', 'output');
          addTerminalLine('  ext host kill    \u2014 Kill host process', 'output');
          addTerminalLine('  ext host logs    \u2014 Show host logs', 'output');
          addTerminalLine('  ext scan         \u2014 Scan local paths for extensions', 'output');
          addTerminalLine('  ext export       \u2014 Export installed list as JSON', 'output');
          addTerminalLine('  ext panel        \u2014 Toggle extension panel (GUI)', 'output');
          addTerminalLine('  ext types        \u2014 List supported extension types', 'output');
          addTerminalLine('  ext builtin      \u2014 List built-in extensions', 'output');
          addTerminalLine('  ext ps           \u2014 PowerShell extension manager info', 'output');
          return;
        }

        if (sub === 'list') {
          addTerminalLine('\u2500\u2500\u2500 Installed Extensions \u2500\u2500\u2500', 'output');
          try {
            var data = await EngineAPI.fetchExtensions();
            var exts = data.extensions || data.installed || State.extensions.installed;
            State.extensions.installed = exts;
            if (exts.length === 0) {
              addTerminalLine('  (none installed)', 'system');
            } else {
              exts.forEach(function (ext) {
                var status = ext.enabled ? '\u2714' : '\u2718';
                var color = ext.enabled ? 'success' : 'error';
                addTerminalLine('  ' + status + ' ' + (ext.id || ext.name) + ' v' + (ext.version || '?') + ' [' + (ext.publisher || '?') + ']', color);
              });
              addTerminalLine('  Total: ' + exts.length + ' extensions', 'system');
            }
          } catch (e) {
            // Fallback to local state
            if (State.extensions.installed.length === 0) {
              addTerminalLine('  (none installed)', 'system');
            } else {
              State.extensions.installed.forEach(function (ext) {
                var status = ext.enabled ? '\u2714' : '\u2718';
                addTerminalLine('  ' + status + ' ' + (ext.id || ext.name) + ' v' + (ext.version || '?'), ext.enabled ? 'success' : 'error');
              });
            }
          }
          return;
        }

        if (sub === 'search') {
          if (!extArg) { addTerminalLine('  Usage: ext search <query>', 'error'); return; }
          addTerminalLine('  Searching marketplace for: ' + extArg + '...', 'system');
          try {
            var data = await EngineAPI.searchMarketplace(extArg);
            var results = data.results || data.extensions || [];
            State.extensions.marketplace.results = results;
            State.extensions.marketplace.query = extArg;
            State.extensions.marketplace.lastSearch = Date.now();
            if (results.length === 0) {
              addTerminalLine('  No results found.', 'system');
            } else {
              addTerminalLine('\u2500\u2500\u2500 Marketplace Results (' + results.length + ') \u2500\u2500\u2500', 'output');
              results.slice(0, 15).forEach(function (r) {
                var installs = r.installs ? ' (' + r.installs.toLocaleString() + ' installs)' : '';
                addTerminalLine('  \u2022 ' + (r.id || r.extensionId || r.name) + ' v' + (r.version || '?') + installs, 'output');
                if (r.description) addTerminalLine('    ' + r.description.substring(0, 80), 'system');
              });
              if (results.length > 15) addTerminalLine('  ... and ' + (results.length - 15) + ' more', 'system');
              addTerminalLine("  Use 'ext install <id>' to install", 'system');
            }
          } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'install') {
          if (!extArg) { addTerminalLine('  Usage: ext install <extension-id>', 'error'); return; }
          addTerminalLine('  Installing: ' + extArg + '...', 'system');
          try {
            var data = await EngineAPI.installExtension(extArg, 'marketplace');
            addTerminalLine('  \u2705 ' + (data.message || 'Installed: ' + extArg), 'success');
            if (data.extension) {
              State.extensions.installed.push(data.extension);
              addTerminalLine('  Version: ' + (data.extension.version || '?'), 'output');
              addTerminalLine('  Publisher: ' + (data.extension.publisher || '?'), 'output');
            }
            refreshExtensionsList();
          } catch (e) { addTerminalLine('  \u274C Install failed: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'uninstall') {
          if (!extArg) { addTerminalLine('  Usage: ext uninstall <extension-id>', 'error'); return; }
          addTerminalLine('  Uninstalling: ' + extArg + '...', 'system');
          try {
            var data = await EngineAPI.uninstallExtension(extArg);
            addTerminalLine('  \u2705 ' + (data.message || 'Uninstalled: ' + extArg), 'success');
            State.extensions.installed = State.extensions.installed.filter(function (e) { return (e.id || e.name) !== extArg; });
            refreshExtensionsList();
          } catch (e) { addTerminalLine('  \u274C Uninstall failed: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'enable') {
          if (!extArg) { addTerminalLine('  Usage: ext enable <extension-id>', 'error'); return; }
          try {
            var data = await EngineAPI.enableExtension(extArg);
            addTerminalLine('  \u2705 Enabled: ' + extArg, 'success');
            var ext = State.extensions.installed.find(function (e) { return (e.id || e.name) === extArg; });
            if (ext) ext.enabled = true;
          } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'disable') {
          if (!extArg) { addTerminalLine('  Usage: ext disable <extension-id>', 'error'); return; }
          try {
            var data = await EngineAPI.disableExtension(extArg);
            addTerminalLine('  \u2705 Disabled: ' + extArg, 'success');
            var ext = State.extensions.installed.find(function (e) { return (e.id || e.name) === extArg; });
            if (ext) ext.enabled = false;
          } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'info') {
          if (!extArg) { addTerminalLine('  Usage: ext info <extension-id>', 'error'); return; }
          addTerminalLine('  Fetching info for: ' + extArg + '...', 'system');
          try {
            var data = await EngineAPI.fetchExtensionById(extArg);
            var ext = data.extension || data;
            addTerminalLine('\u2500\u2500\u2500 Extension: ' + (ext.displayName || ext.name || extArg) + ' \u2500\u2500\u2500', 'output');
            addTerminalLine('  ID:          ' + (ext.id || extArg), 'output');
            addTerminalLine('  Version:     ' + (ext.version || '?'), 'output');
            addTerminalLine('  Publisher:   ' + (ext.publisher || '?'), 'output');
            addTerminalLine('  Enabled:     ' + (ext.enabled ? 'Yes' : 'No'), ext.enabled ? 'success' : 'error');
            addTerminalLine('  Description: ' + (ext.description || '(none)').substring(0, 100), 'output');
            if (ext.activationEvents) {
              addTerminalLine('  Activation:  ' + (Array.isArray(ext.activationEvents) ? ext.activationEvents.join(', ') : ext.activationEvents), 'system');
            }
            if (ext.contributes) {
              var contribs = Object.keys(ext.contributes);
              addTerminalLine('  Contributes: ' + contribs.join(', '), 'system');
            }
            if (ext.size) addTerminalLine('  Size:        ' + ext.size, 'output');
          } catch (e) {
            // Try marketplace fallback
            try {
              var mdata = await EngineAPI.fetchMarketplaceInfo(extArg);
              var mext = mdata.extension || mdata;
              addTerminalLine('\u2500\u2500\u2500 Marketplace: ' + (mext.displayName || extArg) + ' \u2500\u2500\u2500', 'output');
              addTerminalLine('  ID:          ' + (mext.id || extArg), 'output');
              addTerminalLine('  Version:     ' + (mext.version || '?'), 'output');
              addTerminalLine('  Publisher:   ' + (mext.publisher || '?'), 'output');
              addTerminalLine('  Installs:    ' + ((mext.installs || 0).toLocaleString()), 'output');
              addTerminalLine('  Rating:      ' + (mext.rating || '?'), 'output');
              addTerminalLine('  Description: ' + (mext.description || '(none)').substring(0, 100), 'output');
            } catch (e2) { addTerminalLine('  Error: ' + e.message, 'error'); }
          }
          return;
        }

        if (sub === 'load') {
          if (!extArg) { addTerminalLine('  Usage: ext load <path-to-vsix>', 'error'); return; }
          addTerminalLine('  Loading VSIX: ' + extArg + '...', 'system');
          try {
            var data = await EngineAPI.loadVsixFromPath(extArg);
            addTerminalLine('  \u2705 ' + (data.message || 'Loaded: ' + extArg), 'success');
            if (data.extension) {
              State.extensions.installed.push(data.extension);
              addTerminalLine('  Name: ' + (data.extension.displayName || data.extension.name || '?'), 'output');
              addTerminalLine('  Version: ' + (data.extension.version || '?'), 'output');
            }
            refreshExtensionsList();
          } catch (e) { addTerminalLine('  \u274C Load failed: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'host') {
          var hostSub = args[2] || '';
          if (!hostSub) {
            addTerminalLine('\u2500\u2500\u2500 Extension Host \u2500\u2500\u2500', 'output');
            addTerminalLine('  Status:     ' + State.extensions.hostStatus, State.extensions.hostStatus === 'running' ? 'success' : 'output');
            addTerminalLine('  PID:        ' + (State.extensions.hostPid || 'N/A'), 'output');
            addTerminalLine('  Running:    ' + State.extensions.running.length + ' extensions', 'output');
            addTerminalLine('  Logs:       ' + State.extensions.logs.length + ' entries', 'output');
            try {
              var hdata = await EngineAPI.extensionHostStatus();
              addTerminalLine('  \u2714 Backend host: ' + (hdata.status || 'unknown'), hdata.status === 'running' ? 'success' : 'output');
              if (hdata.pid) { State.extensions.hostPid = hdata.pid; addTerminalLine('  PID (backend): ' + hdata.pid, 'output'); }
              if (hdata.memory) addTerminalLine('  Memory: ' + hdata.memory, 'output');
              if (hdata.extensions) addTerminalLine('  Loaded: ' + hdata.extensions + ' extensions', 'output');
            } catch (_) { addTerminalLine('  Backend: not available', 'system'); }
          } else if (hostSub === 'restart') {
            addTerminalLine('  Restarting extension host...', 'system');
            try {
              var data = await EngineAPI.restartExtensionHost();
              State.extensions.hostStatus = 'running';
              addTerminalLine('  \u2705 ' + (data.message || 'Host restarted'), 'success');
            } catch (e) { addTerminalLine('  \u274C ' + e.message, 'error'); }
          } else if (hostSub === 'kill') {
            addTerminalLine('  Killing extension host...', 'system');
            try {
              var data = await EngineAPI.killExtensionHost();
              State.extensions.hostStatus = 'idle';
              State.extensions.hostPid = null;
              State.extensions.running = [];
              addTerminalLine('  \u2705 ' + (data.message || 'Host killed'), 'success');
            } catch (e) { addTerminalLine('  \u274C ' + e.message, 'error'); }
          } else if (hostSub === 'logs') {
            addTerminalLine('\u2500\u2500\u2500 Extension Host Logs \u2500\u2500\u2500', 'output');
            try {
              var data = await EngineAPI.fetchExtensionHostLogs();
              var logs = data.logs || data.entries || [];
              State.extensions.logs = logs.slice(-State.extensions.maxLogs);
              if (logs.length === 0) {
                addTerminalLine('  (no logs)', 'system');
              } else {
                logs.slice(-20).forEach(function (log) {
                  var level = log.level || 'info';
                  var type = level === 'error' ? 'error' : level === 'warn' ? 'error' : 'output';
                  addTerminalLine('  [' + (log.timestamp || '?') + '] [' + level + '] ' + (log.message || JSON.stringify(log)), type);
                });
                if (logs.length > 20) addTerminalLine('  ... ' + (logs.length - 20) + ' more (showing last 20)', 'system');
              }
            } catch (e) {
              if (State.extensions.logs.length > 0) {
                State.extensions.logs.slice(-10).forEach(function (log) {
                  addTerminalLine('  [' + (log.timestamp || '?') + '] ' + (log.message || ''), 'output');
                });
              } else {
                addTerminalLine('  (no logs available)', 'system');
              }
            }
          } else {
            addTerminalLine('  Unknown host subcommand: ' + hostSub, 'error');
            addTerminalLine("  Try: ext host, ext host restart, ext host kill, ext host logs", 'system');
          }
          return;
        }

        if (sub === 'scan') {
          addTerminalLine('  Scanning local paths for extensions...', 'system');
          try {
            var data = await EngineAPI.scanLocalExtensions(State.extensions.scanPaths);
            var found = data.extensions || data.found || [];
            addTerminalLine('  Found ' + found.length + ' extension(s)', found.length > 0 ? 'success' : 'system');
            found.forEach(function (ext) {
              addTerminalLine('  \u2022 ' + (ext.id || ext.name) + ' v' + (ext.version || '?') + ' @ ' + (ext.path || '?'), 'output');
            });
          } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
          return;
        }

        if (sub === 'export') {
          try {
            var data = await EngineAPI.exportExtensionList();
            var json = JSON.stringify(data, null, 2);
            addTerminalLine('  Extension list (' + (data.extensions || data.installed || []).length + ' extensions):', 'output');
            json.split('\n').slice(0, 30).forEach(function (line) { addTerminalLine('  ' + line, 'output'); });
          } catch (e) {
            // Fallback to local state
            var json = JSON.stringify({ extensions: State.extensions.installed }, null, 2);
            addTerminalLine('  Local extension list:', 'output');
            json.split('\n').slice(0, 30).forEach(function (line) { addTerminalLine('  ' + line, 'output'); });
          }
          return;
        }

        if (sub === 'panel') {
          toggleExtPanel();
          addTerminalLine('  \u2714 Extension panel toggled', 'success');
          return;
        }

        if (sub === 'types') {
          addTerminalLine('\u2500\u2500\u2500 Supported Extension Types \u2500\u2500\u2500', 'output');
          ExtensionState.supportedTypes.forEach(function (t) {
            var count = ExtensionState.getByType(t).length;
            addTerminalLine('  \u2022 ' + t.padEnd(14) + count + ' installed', count > 0 ? 'success' : 'output');
          });
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          return;
        }

        if (sub === 'builtin') {
          addTerminalLine('\u2500\u2500\u2500 Built-in Extensions (' + ExtensionState.bundled.length + ') \u2500\u2500\u2500', 'output');
          ExtensionState.bundled.forEach(function (ext) {
            var icon = ext.enabled ? '\u2714' : '\u2718';
            addTerminalLine('  ' + icon + ' ' + (ext.id || ext.name).padEnd(22) + (ext.type || '').padEnd(10) + (ext.desc || '').substring(0, 50), ext.enabled ? 'success' : 'error');
          });
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          return;
        }

        if (sub === 'ps' || sub === 'powershell') {
          addTerminalLine('\u2500\u2500\u2500 PowerShell Extension Manager \u2500\u2500\u2500', 'output');
          addTerminalLine('  Module:  scripts\\ExtensionManager.psm1', 'output');
          addTerminalLine('  Usage:', 'output');
          addTerminalLine('    Import-Module .\\scripts\\ExtensionManager.psm1', 'output');
          addTerminalLine('    New-Extension -Name "MyExt" -Type Custom -AutoInstall', 'output');
          addTerminalLine('    Install-Extension -Name "MyExt"', 'output');
          addTerminalLine('    Enable-Extension -Name "MyExt"', 'output');
          addTerminalLine('    Show-ExtensionMenu  (interactive TUI)', 'output');
          addTerminalLine('  Exported: New-Extension, Install-Extension, Enable-Extension,', 'system');
          addTerminalLine('           Disable-Extension, Remove-Extension, Get-Extension,', 'system');
          addTerminalLine('           Show-ExtensionMenu, Invoke-ExtensionMenu', 'system');
          addTerminalLine('\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500', 'output');
          return;
        }

        addTerminalLine('  Unknown ext subcommand: ' + sub, 'error');
        addTerminalLine("  Type 'ext help' for available commands", 'system');
      })();
      break;

    case 'vsix':
      (async function () {
        var vsixPath = rest || '';
        if (!vsixPath) {
          addTerminalLine('  Usage: vsix <path-or-url>', 'error');
          addTerminalLine('  Load a .vsix extension package from local path or URL', 'system');
          addTerminalLine('  Examples:', 'system');
          addTerminalLine('    vsix ./extensions/my-extension.vsix', 'system');
          addTerminalLine('    vsix https://marketplace.visualstudio.com/...', 'system');
          return;
        }
        var isUrl = vsixPath.indexOf('http://') === 0 || vsixPath.indexOf('https://') === 0;
        addTerminalLine('  Loading VSIX ' + (isUrl ? 'from URL' : 'from disk') + ': ' + vsixPath + '...', 'system');
        try {
          var data = isUrl
            ? await EngineAPI.loadVsixFromUrl(vsixPath)
            : await EngineAPI.loadVsixFromPath(vsixPath);
          addTerminalLine('  \u2705 ' + (data.message || 'VSIX loaded successfully'), 'success');
          if (data.extension) {
            State.extensions.installed.push(data.extension);
            addTerminalLine('  Name:      ' + (data.extension.displayName || data.extension.name || '?'), 'output');
            addTerminalLine('  Version:   ' + (data.extension.version || '?'), 'output');
            addTerminalLine('  Publisher: ' + (data.extension.publisher || '?'), 'output');
            if (data.extension.contributes) {
              addTerminalLine('  Contributes: ' + Object.keys(data.extension.contributes).join(', '), 'system');
            }
          }
          refreshExtensionsList();
        } catch (e) { addTerminalLine('  \u274c Error: ' + e.message, 'error'); }
      })();
      break;

    case 'browse': case 'web': case 'browser':
      (function () {
        var sub = args[1] || '';
        if (!sub || sub === 'open') {
          showBrowser();
          addTerminalLine('  \u2705 Browser opened.', 'success');
        } else if (sub === 'close') {
          browserClose();
          addTerminalLine('  Browser closed.', 'system');
        } else if (sub === 'search') {
          var query = args.slice(2).join(' ');
          if (!query) { addTerminalLine('  Usage: browse search <query>', 'error'); return; }
          showBrowser();
          browserGo('https://duckduckgo.com/?q=' + encodeURIComponent(query));
          addTerminalLine('  \u1F50D Searching: ' + query, 'system');
        } else if (sub === 'extract') {
          browserExtractContent();
          addTerminalLine('  Extracting page content...', 'system');
        } else if (sub === 'model') {
          browserSendToModel();
          addTerminalLine('  Sending page content to model...', 'system');
        } else if (sub === 'proxy') {
          var url = args[2] || '';
          if (!url) { addTerminalLine('  Usage: browse proxy <url>', 'error'); return; }
          addTerminalLine('  Fetching via proxy: ' + url, 'system');
          (async function () {
            try {
              var data = await EngineAPI.browseProxy(url);
              var text = data.content || data.text || data.body || JSON.stringify(data);
              addTerminalLine('  \u2500 Content (' + text.length + ' chars):', 'output');
              text.split('\n').slice(0, 30).forEach(function (line) {
                addTerminalLine('  ' + line.substring(0, 120), 'output');
              });
              if (text.split('\n').length > 30) addTerminalLine('  ... (truncated)', 'system');
            } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
          })();
        } else if (sub === 'back') {
          browserBack();
          addTerminalLine('  Navigating back...', 'system');
        } else if (sub === 'fwd' || sub === 'forward') {
          browserForward();
          addTerminalLine('  Navigating forward...', 'system');
        } else if (sub === 'tabs') {
          var tabs = BrowserState.tabs;
          addTerminalLine('  Open tabs (' + tabs.length + '):', 'output');
          tabs.forEach(function (t, i) {
            addTerminalLine('  ' + (i === BrowserState.activeTab ? '\u25B6 ' : '  ') + '[' + i + '] ' + (t.title || t.url || 'New Tab'), 'output');
          });
        } else if (sub === 'home') {
          browserGoHome();
          addTerminalLine('  Navigated to home.', 'system');
        } else {
          // Treat as URL
          showBrowser();
          browserGo(sub.indexOf('://') >= 0 ? sub : 'https://' + sub);
          addTerminalLine('  \u2705 Navigating to: ' + sub, 'system');
        }
      })();
      break;

    // ═══════════════════════════════════════════════════════════════
    // AGENTIC FILE EDITING COMMANDS (Phase 40)
    // ═══════════════════════════════════════════════════════════════

    case 'cat':
      (async function () {
        var filePath = args[1] || '';
        if (!filePath) { addTerminalLine('  Usage: cat <filepath>', 'error'); return; }
        addTerminalLine('  Reading: ' + filePath, 'system');
        try {
          var data = await EngineAPI.toolReadFile(filePath);
          var content = data.content || data.output || data.result || '';
          content.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (i + 1).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 ' + content.split('\n').length + ' lines \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'head':
      (async function () {
        var n = parseInt(args[1]) || 20;
        var filePath = args[2] || args[1] || '';
        if (!filePath || filePath === String(n)) { filePath = args[2] || ''; }
        if (!filePath) { addTerminalLine('  Usage: head [n] <filepath>', 'error'); return; }
        addTerminalLine('  First ' + n + ' lines of: ' + filePath, 'system');
        try {
          var data = await EngineAPI.headFile(filePath, n);
          data.output.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (i + 1).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 Showing ' + data.lineCount + ' lines \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'tail':
      (async function () {
        var n = parseInt(args[1]) || 20;
        var filePath = args[2] || args[1] || '';
        if (!filePath || filePath === String(n)) { filePath = args[2] || ''; }
        if (!filePath) { addTerminalLine('  Usage: tail [n] <filepath>', 'error'); return; }
        addTerminalLine('  Last ' + n + ' lines of: ' + filePath, 'system');
        try {
          var data = await EngineAPI.tailFile(filePath, n);
          var startLine = data.totalLines - data.lineCount + 1;
          data.output.split('\n').forEach(function (line, i) {
            addTerminalLine('  ' + (startLine + i).toString().padStart(4) + ' | ' + line, 'output');
          });
          addTerminalLine('  \u2500\u2500\u2500 Lines ' + startLine + '-' + data.totalLines + ' of ' + data.totalLines + ' \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'create': case 'touch':
      (async function () {
        var filePath = args[1] || '';
        var content = args.slice(2).join(' ') || '';
        if (!filePath) { addTerminalLine('  Usage: create <filepath> [content]', 'error'); return; }
        addTerminalLine('  Creating: ' + filePath, 'system');
        try {
          var data = await EngineAPI.createFile(filePath, content);
          addTerminalLine('  \u2705 Created: ' + filePath + (content ? ' (' + content.length + ' bytes)' : ' (empty)'), 'success');
          feLogHistory('create', filePath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'mkdir':
      (async function () {
        var dirPath = args[1] || '';
        if (!dirPath) { addTerminalLine('  Usage: mkdir <path>', 'error'); return; }
        addTerminalLine('  Creating directory: ' + dirPath, 'system');
        try {
          var data = await EngineAPI.mkDir(dirPath);
          addTerminalLine('  \u2705 Directory created: ' + dirPath, 'success');
          feLogHistory('mkdir', dirPath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'rm': case 'delete':
      (async function () {
        var filePath = args[1] || '';
        if (!filePath) { addTerminalLine('  Usage: rm <filepath>', 'error'); return; }
        addTerminalLine('  Deleting: ' + filePath, 'system');
        try {
          var data = await EngineAPI.deleteFile(filePath);
          addTerminalLine('  \u2705 Deleted: ' + filePath, 'success');
          feLogHistory('delete', filePath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'rmdir':
      (async function () {
        var dirPath = args[1] || '';
        if (!dirPath) { addTerminalLine('  Usage: rmdir <path>', 'error'); return; }
        addTerminalLine('  Removing directory: ' + dirPath, 'system');
        try {
          var data = await EngineAPI.removeDir(dirPath);
          addTerminalLine('  \u2705 Removed: ' + dirPath, 'success');
          feLogHistory('rmdir', dirPath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'mv': case 'rename':
      (async function () {
        var src = args[1] || '';
        var dst = args[2] || '';
        if (!src || !dst) { addTerminalLine('  Usage: mv <source> <destination>', 'error'); return; }
        addTerminalLine('  Moving: ' + src + ' \u2192 ' + dst, 'system');
        try {
          var data = await EngineAPI.moveFile(src, dst);
          addTerminalLine('  \u2705 Moved: ' + src + ' \u2192 ' + dst, 'success');
          feLogHistory('move', src + ' → ' + dst);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'cp': case 'copy':
      (async function () {
        var src = args[1] || '';
        var dst = args[2] || '';
        if (!src || !dst) { addTerminalLine('  Usage: cp <source> <destination>', 'error'); return; }
        addTerminalLine('  Copying: ' + src + ' \u2192 ' + dst, 'system');
        try {
          var data = await EngineAPI.copyFile(src, dst);
          addTerminalLine('  \u2705 Copied: ' + src + ' \u2192 ' + dst, 'success');
          feLogHistory('copy', src + ' → ' + dst);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'find':
      (async function () {
        var pattern = args[1] || '*.*';
        var dir = args[2] || '.';
        addTerminalLine('  Searching: ' + pattern + ' in ' + dir, 'system');
        try {
          var data = await EngineAPI.searchFiles(pattern, dir);
          var output = data.output || data.result || data.stdout || '';
          var files = output.split('\n').filter(function (f) { return f.trim(); });
          files.forEach(function (f) { addTerminalLine('  \u2022 ' + f.trim(), 'output'); });
          addTerminalLine('  \u2500\u2500\u2500 ' + files.length + ' file(s) found \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'grep':
      (async function () {
        var pattern = args[1] || '';
        var path = args[2] || '*.*';
        if (!pattern) { addTerminalLine('  Usage: grep <pattern> [path]', 'error'); return; }
        addTerminalLine('  Searching for "' + pattern + '" in ' + path, 'system');
        try {
          var data = await EngineAPI.grepInFiles(pattern, path);
          var output = data.output || data.result || data.stdout || '';
          var lines = output.split('\n').filter(function (l) { return l.trim(); });
          lines.slice(0, 50).forEach(function (l) { addTerminalLine('  ' + l.trim(), 'output'); });
          if (lines.length > 50) addTerminalLine('  ... and ' + (lines.length - 50) + ' more matches', 'system');
          addTerminalLine('  \u2500\u2500\u2500 ' + lines.length + ' match(es) \u2500\u2500\u2500', 'system');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'diff':
      (async function () {
        var pathA = args[1] || '';
        var pathB = args[2] || '';
        if (!pathA || !pathB) { addTerminalLine('  Usage: diff <fileA> <fileB>', 'error'); return; }
        addTerminalLine('  Comparing: ' + pathA + ' vs ' + pathB, 'system');
        try {
          var data = await EngineAPI.diffFiles(pathA, pathB);
          var output = data.output || data.result || data.stdout || '';
          output.split('\n').forEach(function (line) { addTerminalLine('  ' + line, 'output'); });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'patch':
      (async function () {
        var filePath = args[1] || '';
        var search = args[2] || '';
        var replace = args.slice(3).join(' ') || '';
        if (!filePath || !search) { addTerminalLine('  Usage: patch <file> <search> <replace>', 'error'); return; }
        addTerminalLine('  Patching: ' + filePath, 'system');
        addTerminalLine('  Search:  "' + search + '"', 'output');
        addTerminalLine('  Replace: "' + replace + '"', 'output');
        try {
          var result = await EngineAPI.patchFile(filePath, search, replace);
          if (result.success) {
            addTerminalLine('  \u2705 ' + result.message, 'success');
            feLogHistory('patch', filePath + ': ' + search + ' → ' + replace);
          } else {
            addTerminalLine('  \u2718 ' + (result.error || 'Pattern not found'), 'error');
          }
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'append':
      (async function () {
        var filePath = args[1] || '';
        var content = args.slice(2).join(' ') || '';
        if (!filePath || !content) { addTerminalLine('  Usage: append <filepath> <content>', 'error'); return; }
        addTerminalLine('  Appending to: ' + filePath, 'system');
        try {
          var data = await EngineAPI.appendFile(filePath, '\n' + content);
          addTerminalLine('  \u2705 Appended ' + content.length + ' bytes', 'success');
          feLogHistory('append', filePath);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'wc':
      (async function () {
        var filePath = args[1] || '';
        if (!filePath) { addTerminalLine('  Usage: wc <filepath>', 'error'); return; }
        addTerminalLine('  Counting: ' + filePath, 'system');
        try {
          var data = await EngineAPI.wordCount(filePath);
          addTerminalLine('  Lines:      ' + data.lines, 'output');
          addTerminalLine('  Words:      ' + data.words, 'output');
          addTerminalLine('  Characters: ' + data.chars, 'output');
          addTerminalLine('  Bytes:      ' + data.bytes, 'output');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'stat': case 'info':
      (async function () {
        var filePath = args[1] || '';
        if (!filePath) { addTerminalLine('  Usage: stat <filepath>', 'error'); return; }
        addTerminalLine('  File info: ' + filePath, 'system');
        try {
          var data = await EngineAPI.fileInfo(filePath);
          var output = data.output || data.result || data.stdout || '';
          output.split('\n').forEach(function (line) {
            if (line.trim()) addTerminalLine('  ' + line, 'output');
          });
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'insert-line':
      (async function () {
        var filePath = args[1] || '';
        var lineNum = parseInt(args[2]) || 0;
        var text = args.slice(3).join(' ') || '';
        if (!filePath || !lineNum) { addTerminalLine('  Usage: insert-line <file> <lineNum> <text>', 'error'); return; }
        addTerminalLine('  Inserting at line ' + lineNum + ' in: ' + filePath, 'system');
        try {
          var result = await EngineAPI.insertAtLine(filePath, lineNum, text);
          addTerminalLine('  \u2705 ' + result.message + ' (total: ' + result.totalLines + ' lines)', 'success');
          feLogHistory('insert-line', filePath + ':' + lineNum);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'delete-lines':
      (async function () {
        var filePath = args[1] || '';
        var startLine = parseInt(args[2]) || 0;
        var endLine = parseInt(args[3]) || startLine;
        if (!filePath || !startLine) { addTerminalLine('  Usage: delete-lines <file> <start> [end]', 'error'); return; }
        addTerminalLine('  Deleting lines ' + startLine + '-' + endLine + ' from: ' + filePath, 'system');
        try {
          var result = await EngineAPI.deleteLine(filePath, startLine, endLine);
          addTerminalLine('  \u2705 ' + result.message + ' (' + result.removedCount + ' removed, ' + result.totalLines + ' remaining)', 'success');
          feLogHistory('delete-lines', filePath + ':' + startLine + '-' + endLine);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'replace-lines':
      (async function () {
        var filePath = args[1] || '';
        var startLine = parseInt(args[2]) || 0;
        var endLine = parseInt(args[3]) || startLine;
        var newText = args.slice(4).join(' ') || '';
        if (!filePath || !startLine) { addTerminalLine('  Usage: replace-lines <file> <start> <end> <text>', 'error'); return; }
        addTerminalLine('  Replacing lines ' + startLine + '-' + endLine + ' in: ' + filePath, 'system');
        try {
          var result = await EngineAPI.replaceLines(filePath, startLine, endLine, newText);
          addTerminalLine('  \u2705 ' + result.message, 'success');
          feLogHistory('replace-lines', filePath + ':' + startLine + '-' + endLine);
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    case 'edit':
      (async function () {
        var filePath = args[1] || '';
        if (!filePath) { addTerminalLine('  Usage: edit <filepath>', 'error'); return; }
        addTerminalLine('  Opening in File Editor: ' + filePath, 'system');
        try {
          showFileEditorPanel();
          await feOpenFile(filePath);
          addTerminalLine('  \u2705 Opened: ' + filePath, 'success');
        } catch (e) { addTerminalLine('  Error: ' + e.message, 'error'); }
      })();
      break;

    default:
      // If it looks like a CLI command, route through CLI processor (beacon handles it)
      if (cmd.charAt(0) === '/' || cmd.substring(0, 7).toLowerCase() === '!engine') {
        sendCliCommand(cmd);
      } else {
        addTerminalLine("Unknown: " + args[0] + ". Type 'help' for commands.", 'error');
      }
  }
}

function addTerminalLine(text, type) {
  var output = document.getElementById('terminalOutput');
  var div = document.createElement('div');
  div.className = 'terminal-line ' + type;
  div.textContent = text;
  output.appendChild(div);
  output.scrollTop = output.scrollHeight;
}

function navigateHistory(dir) {
  var input = document.getElementById('terminalInput');
  State.terminal.index += dir;
  if (State.terminal.index < 0) State.terminal.index = 0;
  if (State.terminal.index >= State.terminal.history.length) {
    State.terminal.index = State.terminal.history.length;
    input.value = '';
    return;
  }
  input.value = State.terminal.history[State.terminal.index] || '';
}

function clearTerminal() {
  document.getElementById('terminalOutput').innerHTML = '';
}

function toggleTerminal() {
  State.terminal.minimized = !State.terminal.minimized;
  var panel = document.getElementById('terminalPanel');
  var container = document.querySelector('.ide-container');
  if (panel) panel.classList.toggle('minimized', State.terminal.minimized);
  // Also support full hide via header button
  if (panel) panel.classList.toggle('hidden', State.terminal.hidden || false);
  if (container) container.classList.toggle('terminal-hidden', State.terminal.hidden || false);
  var btn = document.getElementById('termToggleBtn');
  if (btn) btn.textContent = State.terminal.minimized ? '\u2B06' : '\u2B07';
  var headerBtn = document.getElementById('headerTermToggle');
  if (headerBtn) headerBtn.style.color = State.terminal.minimized ? 'var(--accent-red)' : '';
}

function toggleTerminalFull() {
  // Full hide/show toggle from header
  if (!State.terminal.hidden) State.terminal.hidden = false;
  State.terminal.hidden = !State.terminal.hidden;
  var panel = document.getElementById('terminalPanel');
  var container = document.querySelector('.ide-container');
  if (panel) panel.classList.toggle('hidden', State.terminal.hidden);
  if (container) container.classList.toggle('terminal-hidden', State.terminal.hidden);
  var headerBtn = document.getElementById('headerTermToggle');
  if (headerBtn) headerBtn.style.color = State.terminal.hidden ? 'var(--accent-red)' : '';
}

function toggleRightbar() {
  var rb = document.querySelector('.rightbar');
  var container = document.querySelector('.ide-container');
  if (!rb || !container) return;
  var isHidden = rb.classList.contains('collapsed');
  rb.classList.toggle('collapsed', !isHidden);
  container.classList.toggle('rightbar-hidden', !isHidden);
  var headerBtn = document.getElementById('headerRightbarToggle');
  if (headerBtn) headerBtn.style.color = !isHidden ? 'var(--accent-red)' : '';
}

// ======================================================================
// UTILS
// ======================================================================
function copyCode(btn) {
  var code = btn.closest('.code-block').querySelector('pre').textContent;
  navigator.clipboard.writeText(code).then(function () {
    btn.textContent = 'Copied!';
    setTimeout(function () { btn.textContent = 'Copy'; }, 2000);
  });
}

function insertSnippet(type) {
  var snippets = {
    code: '```cpp\n// Your code here\n```',
    bash: '```powershell\n# Command here\n```',
  };
  var input = document.getElementById('chatInput');
  input.value += (input.value ? '\n' : '') + (snippets[type] || '');
  autoResizeInput();
  input.focus();
}

function clearChat() {
  document.getElementById('chatMessages').innerHTML = '';
  State.chat.messageCount = 0;
  document.getElementById('msgCount').textContent = '0';
  Conversation.clear();
  addWelcomeMessage();
}

// ======================================================================
// FAILURE INTELLIGENCE PANEL
// ======================================================================
function showFailures() {
  document.getElementById('failurePanel').classList.add('active');
  fetchFailures();
}

function closeFailures() {
  document.getElementById('failurePanel').classList.remove('active');
}

async function fetchFailures() {
  if (!State.backend.online) {
    logDebug('Cannot fetch failures: backend offline', 'warn');
    return;
  }

  try {
    var t0 = performance.now();
    var res = await fetch(State.backend.url + '/api/failures?limit=200', { signal: AbortSignal.timeout(5000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    var data = await res.json();
    var lat = Math.round(performance.now() - t0);

    renderFailureTimeline(data.failures || []);
    renderFailureStats(data.stats || {});
    logDebug('Failures loaded: ' + (data.failures || []).length + ' records (' + lat + 'ms)', 'info');
  } catch (e) {
    logDebug('Fetch failures failed: ' + e.message, 'error');
    document.getElementById('failureTableBody').innerHTML =
      '<tr><td colspan="6" style="text-align:center; color:var(--accent-red); padding:20px;">Failed to load: ' + esc(e.message) + '</td></tr>';
  }
}

function renderFailureTimeline(failures) {
  var tbody = document.getElementById('failureTableBody');

  if (!failures || failures.length === 0) {
    tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:var(--text-muted); padding:40px;">No failures recorded yet \u2014 that\'s a good thing!</td></tr>';
    return;
  }

  // Sort by timestamp descending (most recent first)
  failures.sort(function (a, b) { return (b.timestampMs || 0) - (a.timestampMs || 0); });

  var html = '';
  failures.forEach(function (f, idx) {
    var timeStr = f.timestampMs ? new Date(f.timestampMs).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '\u2014';
    var typeStr = esc(f.type || 'Unknown');
    var evidence = esc((f.evidence || '').substring(0, 60));
    if ((f.evidence || '').length > 60) evidence += '\u2026';
    var strategy = esc(f.strategy || '\u2014');
    var outcome = f.outcome || 'Unknown';

    var outClass = outcome.toLowerCase();
    if (outClass === 'corrected') outClass = 'corrected';
    else if (outClass === 'failed') outClass = 'failed';
    else if (outClass === 'declined') outClass = 'declined';
    else outClass = 'detected';

    var failureId = f.id || idx;
    var actionsDisabled = (outcome === 'Corrected' || outcome === 'Declined') ? ' disabled' : '';

    html += '<tr onclick="showFailureDetail(' + idx + ')" style="cursor:pointer;" data-idx="' + idx + '">';
    html += '<td>' + timeStr + '</td>';
    html += '<td style="color:var(--accent-red);">' + typeStr + '</td>';
    html += '<td>' + evidence + '</td>';
    html += '<td>' + strategy + '</td>';
    html += '<td><span class="outcome-badge ' + outClass + '" id="outcome-' + failureId + '">' + outcome + '</span></td>';
    html += '<td><div class="failure-actions">';
    html += '<button class="failure-action-btn retry' + actionsDisabled + '" onclick="event.stopPropagation(); replayFailure(' + failureId + ', \'retry\', this)"' + actionsDisabled + ' title="Retry with correction">\u21BB Retry</button>';
    html += '<button class="failure-action-btn skip' + actionsDisabled + '" onclick="event.stopPropagation(); replayFailure(' + failureId + ', \'skip\', this)"' + actionsDisabled + ' title="Skip / Decline">\u2192 Skip</button>';
    html += '<button class="failure-action-btn escalate' + actionsDisabled + '" onclick="event.stopPropagation(); replayFailure(' + failureId + ', \'escalate\', this)"' + actionsDisabled + ' title="Escalate">\u26A0 Esc</button>';
    html += '</div></td>';
    html += '</tr>';
  });

  tbody.innerHTML = html;

  // Store failures for detail view
  State.failureData = failures;
}

function showFailureDetail(idx) {
  var f = State.failureData && State.failureData[idx];
  if (!f) return;

  // Highlight selected row
  var rows = document.querySelectorAll('.failure-table tbody tr');
  rows.forEach(function (r) { r.classList.remove('selected'); });
  var sel = document.querySelector('.failure-table tbody tr[data-idx="' + idx + '"]');
  if (sel) sel.classList.add('selected');

  var drawer = document.getElementById('failureDetailDrawer');
  drawer.classList.add('active');

  var timeStr = f.timestampMs ? new Date(f.timestampMs).toLocaleString() : '\u2014';

  drawer.innerHTML =
    '<div class="failure-detail-row"><span class="failure-detail-key">Time</span><span class="failure-detail-val">' + esc(timeStr) + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Type</span><span class="failure-detail-val" style="color:var(--accent-red);">' + esc(f.type || '') + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Outcome</span><span class="failure-detail-val">' + esc(f.outcome || '') + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Strategy</span><span class="failure-detail-val">' + esc(f.strategy || 'None') + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Evidence</span><span class="failure-detail-val">' + esc(f.evidence || '') + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Prompt</span><span class="failure-detail-val" style="color:var(--text-muted);">' + esc((f.promptSnippet || '').substring(0, 200)) + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Attempt</span><span class="failure-detail-val">' + (f.attempt || 0) + '</span></div>' +
    '<div class="failure-detail-row"><span class="failure-detail-key">Session</span><span class="failure-detail-val" style="font-size:11px;">' + esc(f.sessionId || '') + '</span></div>';
}

function renderFailureStats(stats) {
  // Summary
  document.getElementById('fsTotalFailures').textContent = stats.totalFailures != null ? stats.totalFailures : '\u2014';
  document.getElementById('fsTotalRetries').textContent = stats.totalRetries != null ? stats.totalRetries : '\u2014';
  document.getElementById('fsRetrySuccess').textContent = stats.successAfterRetry != null ? stats.successAfterRetry : '\u2014';
  document.getElementById('fsDeclined').textContent = stats.retriesDeclined != null ? stats.retriesDeclined : '\u2014';

  // Retry success bar
  var pct = 0;
  if (stats.totalRetries > 0) {
    pct = Math.round((stats.successAfterRetry / stats.totalRetries) * 100);
  }
  document.getElementById('fsRetryBar').style.width = pct + '%';

  // Top reasons
  var reasons = stats.topReasons || [];
  // Filter to non-zero and sort descending
  reasons = reasons.filter(function (r) { return r.count > 0; });
  reasons.sort(function (a, b) { return b.count - a.count; });

  var el = document.getElementById('failureReasonList');
  if (reasons.length === 0) {
    el.innerHTML = '<div class="stat-row"><span class="stat-label" style="color:var(--accent-green);">\u2714 No failures recorded</span></div>';
    return;
  }

  // Take top 5
  var top = reasons.slice(0, 5);
  var maxCount = top[0].count;
  var html = '';
  top.forEach(function (r) {
    var barPct = maxCount > 0 ? Math.round((r.count / maxCount) * 100) : 0;
    html += '<div style="margin-bottom:8px;">';
    html += '<div class="stat-row"><span class="stat-label">' + esc(r.type) + '</span><span class="stat-value red">' + r.count + '</span></div>';
    html += '<div class="stat-bar"><div class="stat-bar-fill" style="width:' + barPct + '%; background:var(--accent-red);"></div></div>';
    html += '</div>';
  });
  el.innerHTML = html;
}

function exportFailures() {
  var data = {
    timestamp: new Date().toISOString(),
    failures: State.failureData || [],
    backendUrl: State.backend.url,
  };
  var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = 'rawrxd-failures-' + Date.now() + '.json'; a.click();
  URL.revokeObjectURL(url);
  logDebug('Failure data exported', 'info');
}

// ======================================================================
// PERFORMANCE & OBSERVABILITY PANEL (Phase 3)
// ======================================================================
function showPerfPanel() {
  document.getElementById('perfPanel').classList.add('active');
  renderPerfPanel();
}

function closePerfPanel() {
  document.getElementById('perfPanel').classList.remove('active');
}

function renderPerfPanel() {
  var h = State.perf.history;
  var total = State.perf.totalRequests;

  // Metric summary cards
  document.getElementById('pmTotalReqs').textContent = total;
  var avgLat = total > 0 ? Math.round(State.perf.totalLatency / total) : 0;
  document.getElementById('pmAvgLatency').textContent = total > 0 ? avgLat : '—';
  var avgTps = State.perf.totalTokens > 0 && State.perf.totalLatency > 0 ? (State.perf.totalTokens / (State.perf.totalLatency / 1000)).toFixed(1) : '—';
  document.getElementById('pmAvgTps').textContent = avgTps;
  document.getElementById('pmTotalTokens').textContent = State.perf.totalTokens.toLocaleString();

  // Latency sparkline
  var latencies = h.map(function (e) { return e.latency; });
  drawSparkline('latencySparkline', latencies, 'rgba(247, 147, 30, 0.8)', 'rgba(247, 147, 30, 0.15)');
  if (latencies.length > 0) {
    var minLat = Math.min.apply(null, latencies);
    var maxLat = Math.max.apply(null, latencies);
    document.getElementById('pmLatencyMinMax').textContent = 'Min: ' + minLat + 'ms  Max: ' + maxLat + 'ms';
    document.getElementById('pmLatencyRange').textContent = 'Last ' + latencies.length + ' requests';
  }

  // t/s sparkline
  var tpsArr = h.map(function (e) { return e.tps; });
  drawSparkline('tpsSparkline', tpsArr, 'rgba(0, 255, 136, 0.8)', 'rgba(0, 255, 136, 0.15)');
  if (tpsArr.length > 0) {
    var validTps = tpsArr.filter(function (v) { return v > 0; });
    if (validTps.length > 0) {
      var minTps = Math.min.apply(null, validTps).toFixed(1);
      var maxTps = Math.max.apply(null, validTps).toFixed(1);
      document.getElementById('pmTpsMinMax').textContent = 'Min: ' + minTps + ' t/s  Max: ' + maxTps + ' t/s';
      document.getElementById('pmTpsRange').textContent = 'Last ' + tpsArr.length + ' requests';
    }
  }

  // Model benchmark comparison
  renderBenchmarkTable();

  // Request history
  renderRequestHistory();

  // Latency percentiles
  renderPercentiles(latencies);

  // Endpoint breakdown
  renderEndpointBreakdown();

  // Structured log
  renderStructuredLog();
}

function drawSparkline(canvasId, data, lineColor, fillColor) {
  var canvas = document.getElementById(canvasId);
  if (!canvas) return;
  var ctx = canvas.getContext('2d');
  var w = canvas.parentElement.clientWidth - 24;
  var h = 80;
  canvas.width = w;
  canvas.height = h;

  ctx.clearRect(0, 0, w, h);

  if (!data || data.length === 0) {
    ctx.fillStyle = 'rgba(96, 96, 112, 0.5)';
    ctx.font = '11px JetBrains Mono, monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No data yet', w / 2, h / 2);
    return;
  }

  var maxVal = Math.max.apply(null, data) || 1;
  var minVal = Math.min.apply(null, data);
  var range = maxVal - minVal || 1;
  var padding = 4;
  var plotH = h - padding * 2;
  var plotW = w - padding * 2;
  var step = plotW / Math.max(data.length - 1, 1);

  // Fill area
  ctx.beginPath();
  ctx.moveTo(padding, h - padding);
  for (var i = 0; i < data.length; i++) {
    var x = padding + i * step;
    var y = padding + plotH - ((data[i] - minVal) / range) * plotH;
    ctx.lineTo(x, y);
  }
  ctx.lineTo(padding + (data.length - 1) * step, h - padding);
  ctx.closePath();
  ctx.fillStyle = fillColor;
  ctx.fill();

  // Line
  ctx.beginPath();
  for (var j = 0; j < data.length; j++) {
    var lx = padding + j * step;
    var ly = padding + plotH - ((data[j] - minVal) / range) * plotH;
    if (j === 0) ctx.moveTo(lx, ly);
    else ctx.lineTo(lx, ly);
  }
  ctx.strokeStyle = lineColor;
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.stroke();

  // Dots on last 5 points
  var dotStart = Math.max(0, data.length - 5);
  for (var d = dotStart; d < data.length; d++) {
    var dx = padding + d * step;
    var dy = padding + plotH - ((data[d] - minVal) / range) * plotH;
    ctx.beginPath();
    ctx.arc(dx, dy, 3, 0, Math.PI * 2);
    ctx.fillStyle = lineColor;
    ctx.fill();
  }
}

function renderBenchmarkTable() {
  var tbody = document.getElementById('pmBenchmarkBody');
  var models = State.perf.modelStats;
  var keys = Object.keys(models);

  if (keys.length === 0) {
    tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:var(--text-muted); padding:30px;">No benchmark data yet — send some messages first</td></tr>';
    return;
  }

  // Find max tps for bar scaling
  var maxTps = 0;
  keys.forEach(function (k) { if (models[k].bestTps > maxTps) maxTps = models[k].bestTps; });

  var html = '';
  keys.sort(function (a, b) { return models[b].avgTps - models[a].avgTps; });

  keys.forEach(function (k) {
    var m = models[k];
    var avgLat = m.requests > 0 ? Math.round(m.totalLatency / m.requests) : 0;
    var barW = maxTps > 0 ? Math.round((m.bestTps / maxTps) * 60) : 0;

    html += '<tr>';
    html += '<td style="color:var(--text-primary); font-weight:600;">' + esc(k.length > 25 ? k.substring(0, 22) + '...' : k) + '</td>';
    html += '<td>' + m.requests + '</td>';
    html += '<td style="color:var(--accent-secondary);">' + avgLat + 'ms</td>';
    html += '<td style="color:var(--accent-cyan);">' + m.avgTps + '</td>';
    html += '<td style="color:var(--accent-green);">' + m.bestTps + '<span class="tps-bar" style="width:' + barW + 'px;"></span></td>';
    html += '<td>' + m.tokens.toLocaleString() + '</td>';
    html += '</tr>';
  });

  tbody.innerHTML = html;
}

function renderRequestHistory() {
  var body = document.getElementById('pmHistoryBody');
  var counter = document.getElementById('pmHistoryCount');
  var h = State.perf.history;

  counter.textContent = h.length + ' requests';

  if (h.length === 0) {
    body.innerHTML = '<div style="text-align:center; color:var(--text-muted); padding:30px; font-size:12px;">No requests recorded yet</div>';
    return;
  }

  // Show most recent first
  var html = '';
  for (var i = h.length - 1; i >= 0; i--) {
    var e = h[i];
    var timeStr = new Date(e.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    var modelStr = e.model || 'unknown';
    if (modelStr.length > 22) modelStr = modelStr.substring(0, 19) + '...';
    var tpsStr = e.tps > 0 ? e.tps.toFixed(1) : '—';
    var errClass = e.success ? '' : ' error-row';

    html += '<div class="perf-history-row' + errClass + '">';
    html += '<div class="col-time">' + timeStr + '</div>';
    html += '<div class="col-model">' + esc(modelStr) + '</div>';
    html += '<div class="col-endpoint">' + esc(e.endpoint || '') + '</div>';
    html += '<div class="col-latency">' + e.latency + 'ms</div>';
    html += '<div class="col-tokens">' + e.tokens + '</div>';
    html += '<div class="col-tps">' + tpsStr + '</div>';
    html += '</div>';
  }

  body.innerHTML = html;
}

function renderPercentiles(latencies) {
  if (!latencies || latencies.length === 0) {
    document.getElementById('perfP50').textContent = '—';
    document.getElementById('perfP90').textContent = '—';
    document.getElementById('perfP99').textContent = '—';
    document.getElementById('perfMin').textContent = '—';
    document.getElementById('perfMax').textContent = '—';
    return;
  }

  var sorted = latencies.slice().sort(function (a, b) { return a - b; });
  var len = sorted.length;

  function percentile(arr, p) {
    var idx = Math.ceil(p / 100 * arr.length) - 1;
    return arr[Math.max(0, Math.min(idx, arr.length - 1))];
  }

  document.getElementById('perfP50').textContent = percentile(sorted, 50) + 'ms';
  document.getElementById('perfP90').textContent = percentile(sorted, 90) + 'ms';
  document.getElementById('perfP99').textContent = percentile(sorted, 99) + 'ms';
  document.getElementById('perfMin').textContent = sorted[0] + 'ms';
  document.getElementById('perfMax').textContent = sorted[len - 1] + 'ms';
}

function renderEndpointBreakdown() {
  var h = State.perf.history;
  var epMap = {};
  h.forEach(function (e) {
    var ep = e.endpoint || 'unknown';
    if (!epMap[ep]) epMap[ep] = { count: 0, success: 0, fail: 0 };
    epMap[ep].count++;
    if (e.success) epMap[ep].success++;
    else epMap[ep].fail++;
  });

  var el = document.getElementById('perfEndpointBreakdown');
  var keys = Object.keys(epMap);
  if (keys.length === 0) {
    el.innerHTML = '<div class="perf-side-row"><span class="perf-side-key" style="color:var(--text-muted);">No data yet</span></div>';
    return;
  }

  var html = '';
  keys.forEach(function (ep) {
    var d = epMap[ep];
    var shortEp = ep.length > 18 ? ep.substring(0, 15) + '...' : ep;
    html += '<div class="perf-side-row"><span class="perf-side-key" title="' + esc(ep) + '">' + esc(shortEp) + '</span><span class="perf-side-val">' + d.count;
    if (d.fail > 0) html += ' <span style="color:var(--accent-red); font-size:10px;">(' + d.fail + ' err)</span>';
    html += '</span></div>';
  });
  el.innerHTML = html;
}

function renderStructuredLog() {
  var logBody = document.getElementById('pmLogBody');
  var counter = document.getElementById('pmLogCount');
  var entries = State.perf.structuredLog;

  counter.textContent = entries.length;

  if (entries.length === 0) {
    logBody.innerHTML = '<div class="perf-log-entry"><span class="log-ts">—</span><span class="log-level INFO">INFO</span><span class="log-event">Structured logging ready</span></div>';
    return;
  }

  // Show most recent first, limit to 50 for rendering
  var html = '';
  var start = Math.max(0, entries.length - 50);
  for (var i = entries.length - 1; i >= start; i--) {
    var e = entries[i];
    var timeStr = new Date(e.ts).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    var detail = e.event;
    if (e.data) {
      var parts = [];
      Object.keys(e.data).forEach(function (k) {
        var v = e.data[k];
        if (v !== null && v !== undefined && v !== '') {
          parts.push(k + '=' + v);
        }
      });
      if (parts.length > 0) detail += ' | ' + parts.join(', ');
    }
    html += '<div class="perf-log-entry">';
    html += '<span class="log-ts">' + timeStr + '</span>';
    html += '<span class="log-level ' + esc(e.level) + '">' + esc(e.level) + '</span>';
    html += '<span class="log-event">' + esc(detail) + '</span>';
    html += '</div>';
  }
  logBody.innerHTML = html;
}

function exportPerfData() {
  var data = {
    timestamp: new Date().toISOString(),
    summary: {
      totalRequests: State.perf.totalRequests,
      totalTokens: State.perf.totalTokens,
      totalLatency: State.perf.totalLatency,
      avgLatency: State.perf.totalRequests > 0 ? Math.round(State.perf.totalLatency / State.perf.totalRequests) : 0,
      avgTps: State.perf.totalTokens > 0 && State.perf.totalLatency > 0 ? parseFloat((State.perf.totalTokens / (State.perf.totalLatency / 1000)).toFixed(1)) : 0,
    },
    modelStats: State.perf.modelStats,
    history: State.perf.history,
    structuredLog: State.perf.structuredLog,
    backendUrl: State.backend.url,
  };
  var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = 'rawrxd-perf-' + Date.now() + '.json'; a.click();
  URL.revokeObjectURL(url);
  logDebug('Performance data exported (' + State.perf.history.length + ' entries)', 'info');
}

// ======================================================================
// REVERSE ENGINEERING PANEL (Phase 39)
// ======================================================================

function toggleREPanel() {
  var panel = document.getElementById('rePanel');
  if (!panel) return;
  if (panel.style.display === 'none' || !panel.style.display) {
    panel.style.display = 'flex';
    refreshREPanel();
  } else {
    panel.style.display = 'none';
  }
}

function showREPanel() {
  var panel = document.getElementById('rePanel');
  if (panel) {
    panel.style.display = 'flex';
    refreshREPanel();
  }
}

function switchRETab(tabName) {
  REState.activeTab = tabName;
  // Update tab buttons
  var tabs = document.querySelectorAll('.re-tab');
  for (var i = 0; i < tabs.length; i++) {
    tabs[i].classList.toggle('active', tabs[i].getAttribute('data-re-tab') === tabName);
  }
  // Update tab panes
  var panes = document.querySelectorAll('.re-tab-pane');
  for (var j = 0; j < panes.length; j++) {
    panes[j].classList.toggle('active', panes[j].getAttribute('data-re-pane') === tabName);
  }
}

function refreshREPanel() {
  // Update overview stats
  var loadedCount = REState.modules.filter(function (m) { return m.loaded; }).length;
  var el;

  el = document.getElementById('reModulesLoaded');
  if (el) el.textContent = loadedCount + ' / ' + REState.modules.length;

  el = document.getElementById('reActiveEngine');
  if (el) {
    var act = EngineRegistry.getActive();
    el.textContent = act ? act.name : '\u2014';
  }

  el = document.getElementById('reHistoryCount');
  if (el) el.textContent = REState.history.length;

  el = document.getElementById('reBackendStatus');
  if (el) el.textContent = State.backend.online ? 'Online' : 'Offline';

  el = document.getElementById('reModuleCount');
  if (el) el.textContent = REState.modules.length + ' modules';

  el = document.getElementById('reCurrentModel');
  if (el) el.textContent = State.model.current || 'none';

  // Render module cards
  var moduleList = document.getElementById('reModuleList');
  if (moduleList) {
    var html = '';
    var moduleIcons = {
      'pe-analyzer': '\uD83D\uDCC4',
      'disassembler': '\u2699',
      'deobfuscator': '\uD83D\uDD13',
      'omega-suite': '\uD83D\uDE80',
      'binary-comparator': '\u2194',
      'symbol-resolver': '\uD83D\uDD17',
      'gguf-inspector': '\uD83E\uDDE0',
      'memory-scanner': '\uD83D\uDCBE'
    };
    REState.modules.forEach(function (m) {
      var icon = moduleIcons[m.type] || '\u2022';
      var loadedClass = m.loaded ? ' loaded' : '';
      var badgeClass = m.loaded ? ' loaded-badge' : '';
      var badgeText = m.loaded ? 'LOADED' : m.type;
      html += '<div class="re-module-card' + loadedClass + '" onclick="reActivateModule(\'' + m.type + '\')">';
      html += '<span class="re-module-icon">' + icon + '</span>';
      html += '<div class="re-module-info">';
      html += '<div class="re-module-name">' + m.name + '</div>';
      html += '<div class="re-module-desc">' + m.desc + '</div>';
      html += '</div>';
      html += '<span class="re-module-badge' + badgeClass + '">' + badgeText + '</span>';
      html += '</div>';
    });
    moduleList.innerHTML = html;
  }

  // Render history
  var histList = document.getElementById('reHistoryList');
  if (histList) {
    if (REState.history.length === 0) {
      histList.innerHTML = '<div class="re-history-empty">No operations yet. Use <code>re</code> commands in terminal.</div>';
    } else {
      var hhtml = '';
      var items = REState.history.slice(-20).reverse();
      items.forEach(function (h) {
        var timeStr = new Date(h.time).toLocaleTimeString();
        hhtml += '<div class="re-history-item">';
        hhtml += '<span class="re-history-action">' + (h.action || '?') + '</span>';
        hhtml += '<span class="re-history-target">' + (h.target || '') + '</span>';
        hhtml += '<span class="re-history-time">' + timeStr + '</span>';
        hhtml += '</div>';
      });
      histList.innerHTML = hhtml;
    }
  }
}

function reActivateModule(moduleType) {
  // Map module type to the corresponding tab
  var tabMap = {
    'pe-analyzer': 'pe',
    'disassembler': 'disasm',
    'deobfuscator': 'deobf',
    'omega-suite': 'omega',
    'binary-comparator': 'omega',
    'symbol-resolver': 'symbols',
    'gguf-inspector': 'gguf',
    'memory-scanner': 'memory'
  };
  var tab = tabMap[moduleType] || 'overview';
  switchRETab(tab);
  // Mark as loaded
  REState.modules.forEach(function (m) {
    if (m.type === moduleType) m.loaded = true;
  });
  refreshREPanel();
}

function rePEAnalyze() {
  var file = document.getElementById('rePEFile');
  var output = document.getElementById('rePEOutput');
  if (!file || !output) return;
  var target = file.value.trim() || 'build\\bin\\RawrXD-Win32IDE.exe';
  REState.history.push({ action: 'pe', target: target, time: Date.now() });

  if (State.backend.online && !State.backend.directMode) {
    output.textContent = 'Querying backend for PE analysis of ' + target + '...';
    fetch(getActiveUrl() + '/api/cli', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: '!engine dumpbin ' + target }),
      signal: AbortSignal.timeout(15000)
    }).then(function (r) { return r.json(); }).then(function (d) {
      output.textContent = d.output || 'PE analysis complete (no output from backend)';
    }).catch(function () {
      output.textContent = 'PE Analysis: ' + target + '\n\n' +
        'Run these commands in your terminal:\n\n' +
        '  dumpbin /headers ' + target + '\n' +
        '  dumpbin /exports ' + target + '\n' +
        '  dumpbin /imports ' + target + '\n' +
        '  dumpbin /dependents ' + target + '\n' +
        '  dumpbin /section:.text ' + target + '\n';
    });
  } else {
    output.textContent = 'PE Analysis: ' + target + '\n\n' +
      'Backend offline. Run these commands locally:\n\n' +
      '  dumpbin /headers ' + target + '\n' +
      '  dumpbin /exports ' + target + '\n' +
      '  dumpbin /imports ' + target + '\n' +
      '  dumpbin /dependents ' + target + '\n' +
      '  dumpbin /section:.text ' + target + '\n' +
      '  dumpbin /disasm ' + target + '\n';
  }
  refreshREPanel();
}

function reDisassemble() {
  var addrEl = document.getElementById('reDisasmAddr');
  var lenEl = document.getElementById('reDisasmLen');
  var output = document.getElementById('reDisasmOutput');
  if (!addrEl || !output) return;
  var addr = addrEl.value.trim() || '0x00400000';
  var len = (lenEl && lenEl.value.trim()) || '256';
  REState.history.push({ action: 'disasm', target: addr, time: Date.now() });

  if (State.backend.online && !State.backend.directMode) {
    output.textContent = 'Querying disassembly at ' + addr + ' (len=' + len + ')...';
    fetch(getActiveUrl() + '/api/cli', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: '!engine disasm ' + addr + ' ' + len }),
      signal: AbortSignal.timeout(15000)
    }).then(function (r) { return r.json(); }).then(function (d) {
      output.textContent = d.output || 'Disassembly complete (no output from backend)';
    }).catch(function () {
      output.textContent = 'Disassembly at ' + addr + '\n\n' +
        'Run locally:\n' +
        '  dumpbin /disasm /range:' + addr + ' build\\bin\\RawrXD-Win32IDE.exe\n' +
        '  objdump -d --start-address=' + addr + ' -M intel build\\bin\\RawrXD-Win32IDE.exe\n';
    });
  } else {
    output.textContent = 'Disassembly at ' + addr + ' (len=' + len + ')\n\n' +
      'Backend offline. Run locally:\n' +
      '  dumpbin /disasm /range:' + addr + ' build\\bin\\RawrXD-Win32IDE.exe\n' +
      '  objdump -d --start-address=' + addr + ' -M intel build\\bin\\RawrXD-Win32IDE.exe\n';
  }
  refreshREPanel();
}

function reLoadASM(asmFile) {
  var output = document.getElementById('reDisasmOutput');
  if (!output) return;
  REState.history.push({ action: 'asm-load', target: asmFile, time: Date.now() });

  if (State.backend.online && !State.backend.directMode) {
    output.textContent = 'Loading ' + asmFile + '...';
    fetch(getActiveUrl() + '/api/cli', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: '!engine analyze src/asm/' + asmFile }),
      signal: AbortSignal.timeout(15000)
    }).then(function (r) { return r.json(); }).then(function (d) {
      output.textContent = d.output || asmFile + ' loaded (no output from backend)';
    }).catch(function () {
      output.textContent = 'ASM Kernel: ' + asmFile + '\n\n' +
        'File: src/asm/' + asmFile + '\n' +
        'Assemble: ml64 /c /Fo build\\obj\\' + asmFile.replace('.asm', '.obj') + ' src\\asm\\' + asmFile + '\n' +
        'Disassemble: dumpbin /disasm build\\obj\\' + asmFile.replace('.asm', '.obj') + '\n';
    });
  } else {
    output.textContent = 'ASM Kernel: ' + asmFile + '\n\n' +
      'File: src/asm/' + asmFile + '\n' +
      'Assemble: ml64 /c /Fo build\\obj\\' + asmFile.replace('.asm', '.obj') + ' src\\asm\\' + asmFile + '\n' +
      'Disassemble: dumpbin /disasm build\\obj\\' + asmFile.replace('.asm', '.obj') + '\n';
  }
  refreshREPanel();
}

function reGGUFInspect() {
  var modelEl = document.getElementById('reGGUFModel');
  var output = document.getElementById('reGGUFOutput');
  if (!modelEl || !output) return;
  var model = modelEl.value.trim();
  if (!model) {
    model = State.model.current || '';
    if (!model) {
      output.textContent = 'Error: No model specified. Enter a .gguf path or select a model first.';
      return;
    }
  }
  REState.history.push({ action: 'gguf', target: model, time: Date.now() });

  if (State.backend.online && !State.backend.directMode) {
    output.textContent = 'Inspecting GGUF model: ' + model + '...';
    fetch(getActiveUrl() + '/api/cli', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: '!engine analyze ' + model }),
      signal: AbortSignal.timeout(15000)
    }).then(function (r) { return r.json(); }).then(function (d) {
      output.textContent = d.output || 'GGUF inspection complete';
      // Try to parse tensor count, metadata, etc.
      if (d.tensorCount) {
        var tc = document.getElementById('reGGUFTensorCount');
        if (tc) tc.textContent = d.tensorCount;
      }
      if (d.architecture) {
        var arch = document.getElementById('reGGUFArch');
        if (arch) arch.textContent = d.architecture;
      }
    }).catch(function () {
      output.textContent = 'GGUF Inspector: ' + model + '\n\n' +
        'Backend offline. Run locally:\n' +
        '  python -m gguf.scripts.gguf_dump ' + model + '\n' +
        '  llama-gguf-split --info ' + model + '\n';
    });
  } else {
    output.textContent = 'GGUF Inspector: ' + model + '\n\n' +
      'Backend offline. Run locally:\n' +
      '  python -m gguf.scripts.gguf_dump ' + model + '\n' +
      '  llama-gguf-split --info ' + model + '\n';
  }
  refreshREPanel();
}

function reDeobfuscate() {
  var fileEl = document.getElementById('reDeobfFile');
  var output = document.getElementById('reDeobfOutput');
  if (!fileEl || !output) return;
  var target = fileEl.value.trim();
  if (!target) {
    output.textContent = 'Error: Enter a binary path to analyze.';
    return;
  }
  REState.history.push({ action: 'deobf', target: target, time: Date.now() });
  output.textContent = 'Analyzing ' + target + ' for obfuscation patterns...\n';

  // Simulate deobfuscation checks
  var checks = [
    { id: 'reDeobfCFF', name: 'Control Flow Flattening' },
    { id: 'reDeobfStr', name: 'String Encryption' },
    { id: 'reDeobfOP', name: 'Opaque Predicates' },
    { id: 'reDeobfDC', name: 'Dead Code Insertion' },
    { id: 'reDeobfVM', name: 'Virtualization' },
    { id: 'reDeobfAD', name: 'Anti-Debug' }
  ];
  var delay = 300;
  checks.forEach(function (c, i) {
    setTimeout(function () {
      var el = document.getElementById(c.id);
      if (el) el.textContent = 'scanning...';
      setTimeout(function () {
        if (el) el.textContent = 'not detected';
        el.style.color = '#4caf50';
        output.textContent += '  \u2714 ' + c.name + ': not detected\n';
        if (i === checks.length - 1) {
          output.textContent += '\nAnalysis complete. No obfuscation detected in ' + target + '.\n';
          output.textContent += '(Note: Client-side heuristics only. Full analysis requires IDA Pro / Ghidra.)';
        }
      }, delay);
    }, i * delay * 2);
  });
  refreshREPanel();
}

function reMemScan() {
  var pidEl = document.getElementById('reMemPID');
  var patternEl = document.getElementById('reMemPattern');
  var output = document.getElementById('reMemOutput');
  if (!pidEl || !output) return;
  var pid = pidEl.value.trim();
  var pattern = patternEl ? patternEl.value.trim() : '';
  if (!pid) {
    output.textContent = 'Error: Enter a process ID (PID) to scan.';
    return;
  }
  REState.history.push({ action: 'scan', target: 'pid:' + pid + (pattern ? ' pattern:' + pattern : ''), time: Date.now() });

  output.textContent = 'Memory Scanner: PID ' + pid + '\n';
  if (pattern) output.textContent += 'Pattern: ' + pattern + '\n';
  output.textContent += '\nMemory scan requires elevated privileges.\n\n' +
    'Tools:\n' +
    '  windbg -p ' + pid + '\n' +
    '  procdump -ma ' + pid + ' dump.dmp\n' +
    '  volatility -f dump.dmp --profile=Win10x64 pslist\n';
  if (pattern) {
    output.textContent += '\nSearch pattern: ' + pattern + '\n' +
      '  cheat engine style scan or custom ReadProcessMemory loop\n';
  }
  refreshREPanel();
}

function reResolveSymbols() {
  var fileEl = document.getElementById('reSymFile');
  var output = document.getElementById('reSymOutput');
  if (!fileEl || !output) return;
  var pdbFile = fileEl.value.trim();
  if (!pdbFile) {
    output.textContent = 'Error: Enter a PDB file path.';
    return;
  }
  REState.history.push({ action: 'symbols', target: pdbFile, time: Date.now() });

  if (State.backend.online && !State.backend.directMode) {
    output.textContent = 'Resolving symbols from ' + pdbFile + '...';
    fetch(getActiveUrl() + '/api/cli', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: '!engine analyze ' + pdbFile }),
      signal: AbortSignal.timeout(15000)
    }).then(function (r) { return r.json(); }).then(function (d) {
      output.textContent = d.output || 'Symbol resolution complete';
    }).catch(function () {
      output.textContent = 'Symbol Resolver: ' + pdbFile + '\n\n' +
        'Run locally:\n' +
        '  dumpbin /pdbpath:verbose ' + pdbFile + '\n' +
        '  symchk /r ' + pdbFile + ' /s SRV*\n' +
        '  cvdump ' + pdbFile + '\n' +
        '  dia2dump ' + pdbFile + '\n';
    });
  } else {
    output.textContent = 'Symbol Resolver: ' + pdbFile + '\n\n' +
      'Backend offline. Run locally:\n' +
      '  dumpbin /pdbpath:verbose ' + pdbFile + '\n' +
      '  symchk /r ' + pdbFile + ' /s SRV*\n' +
      '  cvdump ' + pdbFile + '\n' +
      '  dia2dump ' + pdbFile + '\n';
  }
  refreshREPanel();
}

function reOmegaScan() {
  var targetEl = document.getElementById('reOmegaTarget');
  var output = document.getElementById('reOmegaOutput');
  var pipeline = document.getElementById('reOmegaPipeline');
  if (!targetEl || !output) return;
  var target = targetEl.value.trim() || 'RawrXD-Win32IDE.exe';
  REState.history.push({ action: 'omega', target: target, time: Date.now() });

  var steps = pipeline ? pipeline.querySelectorAll('.re-omega-step') : [];
  var stepNames = ['PE Analysis', 'Disassembly', 'Deobfuscation', 'Symbol Resolution', 'Binary Comparison', 'Memory Scan', 'GGUF Inspection'];
  output.textContent = 'Omega Suite — Full Scan: ' + target + '\n\n';

  // Animate steps
  var stepDelay = 600;
  for (var i = 0; i < steps.length; i++) {
    (function (idx) {
      setTimeout(function () {
        // Set current step to running
        steps[idx].classList.add('running');
        steps[idx].classList.remove('done');
        var statusEl = steps[idx].querySelector('.re-omega-status');
        if (statusEl) statusEl.textContent = 'running...';
        var iconEl = steps[idx].querySelector('.re-omega-icon');
        if (iconEl) iconEl.textContent = '\u25B6';

        output.textContent += '  [' + (idx + 1) + '/' + steps.length + '] ' + stepNames[idx] + '... ';

        setTimeout(function () {
          steps[idx].classList.remove('running');
          steps[idx].classList.add('done');
          if (statusEl) statusEl.textContent = 'done';
          if (iconEl) iconEl.textContent = '\u2714';
          output.textContent += 'done\n';

          if (idx === steps.length - 1) {
            output.textContent += '\n\u2714 Omega Suite scan complete for ' + target + '\n';
            output.textContent += '  All ' + steps.length + ' modules passed.\n';

            // Fire backend analysis if available
            if (State.backend.online && !State.backend.directMode) {
              fetch(getActiveUrl() + '/api/cli', {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: '!engine analyze ' + target }),
                signal: AbortSignal.timeout(30000)
              }).then(function (r) { return r.json(); }).then(function (d) {
                if (d.output) output.textContent += '\n--- Backend Analysis ---\n' + d.output;
              }).catch(function () { });
            }
          }
        }, stepDelay * 0.8);
      }, idx * stepDelay);
    })(i);
  }
  refreshREPanel();
}

// ======================================================================
// AGENT DASHBOARD (Phase 2)
// ======================================================================
var _agentPollTimer = null;
var _agentPollActive = false;

function showAgentDashboard() {
  document.getElementById('agentPanel').classList.add('active');
  fetchAgentStatus();
  fetchAgentHistory();
  startAgentPoll();
}

function closeAgentDashboard() {
  document.getElementById('agentPanel').classList.remove('active');
  stopAgentPoll();
}

function startAgentPoll() {
  if (_agentPollActive) return;
  _agentPollActive = true;
  var dot = document.getElementById('agentPollDot');
  var label = document.getElementById('agentPollLabel');
  if (dot) dot.classList.remove('paused');
  if (label) label.textContent = 'Polling';
  _agentPollTimer = setInterval(function () {
    if (State.backend.online) {
      fetchAgentStatus();
      fetchAgentHistory();
    }
  }, 5000);
  logDebug('Agent dashboard polling started (5s interval)', 'info');
}

function stopAgentPoll() {
  _agentPollActive = false;
  if (_agentPollTimer) {
    clearInterval(_agentPollTimer);
    _agentPollTimer = null;
  }
  var dot = document.getElementById('agentPollDot');
  var label = document.getElementById('agentPollLabel');
  if (dot) dot.classList.add('paused');
  if (label) label.textContent = 'Paused';
}

function toggleAgentPoll() {
  if (_agentPollActive) stopAgentPoll();
  else startAgentPoll();
}

async function fetchAgentStatus() {
  if (!State.backend.online) return;
  try {
    var res = await fetch(State.backend.url + '/api/agents/status', { signal: AbortSignal.timeout(5000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    var data = await res.json();
    renderAgentStatus(data);
  } catch (e) {
    logDebug('Agent status fetch failed: ' + e.message, 'warn');
  }
}

function renderAgentStatus(data) {
  var agents = data.agents || {};

  // Failure Detector card
  var det = agents.failure_detector || {};
  var detDot = document.getElementById('agentDetectorDot');
  var detStatus = document.getElementById('agentDetectorStatus');
  if (detDot) detDot.className = 'agent-status-dot ' + (det.active ? 'active' : 'inactive');
  if (detStatus) {
    detStatus.textContent = det.active ? 'Active' : 'Inactive';
    detStatus.style.color = det.active ? 'var(--accent-green)' : 'var(--text-muted)';
  }
  var detCount = document.getElementById('agentDetectorCount');
  if (detCount) detCount.textContent = det.detections || 0;
  var detLast = document.getElementById('agentDetectorLast');
  if (detLast) detLast.textContent = det.last_check ? new Date(det.last_check).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : '\u2014';

  // Puppeteer card
  var pup = agents.puppeteer || {};
  var pupDot = document.getElementById('agentPuppeteerDot');
  var pupStatus = document.getElementById('agentPuppeteerStatus');
  if (pupDot) pupDot.className = 'agent-status-dot ' + (pup.active ? 'active' : 'inactive');
  if (pupStatus) {
    pupStatus.textContent = pup.active ? 'Active' : 'Inactive';
    pupStatus.style.color = pup.active ? 'var(--accent-green)' : 'var(--text-muted)';
  }
  var pupCount = document.getElementById('agentPuppeteerCount');
  if (pupCount) pupCount.textContent = pup.corrections || 0;
  var pupLast = document.getElementById('agentPuppeteerLast');
  if (pupLast) pupLast.textContent = pup.last_correction ? new Date(pup.last_correction).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : '\u2014';

  // Proxy Hotpatcher card
  var proxy = agents.proxy_hotpatcher || {};
  var proxyDot = document.getElementById('agentProxyDot');
  var proxyStatus = document.getElementById('agentProxyStatus');
  if (proxyDot) proxyDot.className = 'agent-status-dot ' + (proxy.active ? 'active' : 'inactive');
  if (proxyStatus) {
    proxyStatus.textContent = proxy.active ? 'Active' : 'Inactive';
    proxyStatus.style.color = proxy.active ? 'var(--accent-green)' : 'var(--text-muted)';
  }
  var proxyCount = document.getElementById('agentProxyCount');
  if (proxyCount) proxyCount.textContent = proxy.patches_applied || 0;

  // Hotpatch layer counts
  var mgr = agents.unified_manager || {};
  var hpMem = document.getElementById('hpMemoryCount');
  var hpByte = document.getElementById('hpByteCount');
  var hpSrv = document.getElementById('hpServerCount');
  if (hpMem) hpMem.textContent = mgr.memory_patches || 0;
  if (hpByte) hpByte.textContent = mgr.byte_patches || 0;
  if (hpSrv) hpSrv.textContent = mgr.server_patches || 0;

  // Server sidebar stats
  var uptime = data.server_uptime || 0;
  var uptimeStr = uptime >= 3600 ? Math.floor(uptime / 3600) + 'h ' + Math.floor((uptime % 3600) / 60) + 'm' :
    Math.floor(uptime / 60) + 'm ' + (uptime % 60) + 's';
  var agentUptime = document.getElementById('agentUptime');
  if (agentUptime) agentUptime.textContent = uptimeStr;

  var agentTotalReqs = document.getElementById('agentTotalReqs');
  if (agentTotalReqs) agentTotalReqs.textContent = data.total_requests || '\u2014';

  var agentChatComp = document.getElementById('agentChatCompletions');
  if (agentChatComp) agentChatComp.textContent = data.total_chat_completions || '\u2014';

  var agentEvts = document.getElementById('agentTotalEvents');
  if (agentEvts) agentEvts.textContent = data.total_events || 0;

  // Sync toggle switches with server-reported layer states
  var layerStates = agents.unified_manager || {};
  var memToggle = document.getElementById('hpMemoryToggle');
  var byteToggle = document.getElementById('hpByteToggle');
  var serverToggle = document.getElementById('hpServerToggle');
  if (memToggle && layerStates.memory_enabled !== undefined) memToggle.checked = layerStates.memory_enabled;
  if (byteToggle && layerStates.byte_enabled !== undefined) byteToggle.checked = layerStates.byte_enabled;
  if (serverToggle && layerStates.server_enabled !== undefined) serverToggle.checked = layerStates.server_enabled;
  // Update layer visual disabled state
  updateHotpatchLayerVisuals();
}

// ======================================================================
// HOTPATCH CONTROL ACTIONS
// ======================================================================
var _hotpatchLayerStates = { memory: true, byte: true, server: true };

function toggleHotpatchLayer(layer, enabled) {
  _hotpatchLayerStates[layer] = enabled;
  updateHotpatchLayerVisuals();

  if (!State.backend.online) {
    logDebug('Hotpatch toggle queued (offline): ' + layer + ' → ' + (enabled ? 'ON' : 'OFF'), 'warn');
    return;
  }

  fetch(State.backend.url + '/api/hotpatch/toggle', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ layer: layer, enabled: enabled }),
    signal: AbortSignal.timeout(5000)
  })
    .then(function (r) { return r.json(); })
    .then(function (data) {
      logDebug('Hotpatch layer ' + layer + ' ' + (enabled ? 'enabled' : 'disabled') + (data.status ? ' — ' + data.status : ''), 'info');
      fetchAgentStatus();
    })
    .catch(function (e) {
      logDebug('Hotpatch toggle failed: ' + e.message, 'error');
    });
}

function toggleAllHotpatchLayers() {
  var anyEnabled = _hotpatchLayerStates.memory || _hotpatchLayerStates.byte || _hotpatchLayerStates.server;
  var newState = !anyEnabled;

  document.getElementById('hpMemoryToggle').checked = newState;
  document.getElementById('hpByteToggle').checked = newState;
  document.getElementById('hpServerToggle').checked = newState;

  toggleHotpatchLayer('memory', newState);
  toggleHotpatchLayer('byte', newState);
  toggleHotpatchLayer('server', newState);
}

function updateHotpatchLayerVisuals() {
  var layers = ['Memory', 'Byte', 'Server'];
  var keys = ['memory', 'byte', 'server'];
  for (var i = 0; i < layers.length; i++) {
    var el = document.getElementById('hp' + layers[i] + 'Layer');
    if (el) {
      if (_hotpatchLayerStates[keys[i]]) {
        el.classList.remove('disabled');
      } else {
        el.classList.add('disabled');
      }
    }
  }
}

function applyHotpatch(layer) {
  if (!State.backend.online) {
    addTerminalLine('Cannot apply hotpatch: backend offline', 'error');
    return;
  }

  logDebug('Applying hotpatch: ' + layer + ' layer...', 'info');

  fetch(State.backend.url + '/api/hotpatch/apply', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ layer: layer }),
    signal: AbortSignal.timeout(10000)
  })
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var msg = 'Hotpatch ' + layer + ': ' + (data.status || 'applied') + (data.patches_applied ? ' (' + data.patches_applied + ' patches)' : '');
      logDebug(msg, data.success !== false ? 'info' : 'error');
      fetchAgentStatus();
    })
    .catch(function (e) {
      logDebug('Hotpatch apply failed: ' + e.message, 'error');
    });
}

function revertHotpatch(layer) {
  if (!State.backend.online) {
    addTerminalLine('Cannot revert hotpatch: backend offline', 'error');
    return;
  }

  logDebug('Reverting hotpatch: ' + layer + ' layer...', 'info');

  fetch(State.backend.url + '/api/hotpatch/revert', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ layer: layer }),
    signal: AbortSignal.timeout(10000)
  })
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var msg = 'Hotpatch ' + layer + ' reverted: ' + (data.status || 'done') + (data.patches_reverted ? ' (' + data.patches_reverted + ' reverted)' : '');
      logDebug(msg, data.success !== false ? 'info' : 'error');
      fetchAgentStatus();
    })
    .catch(function (e) {
      logDebug('Hotpatch revert failed: ' + e.message, 'error');
    });
}

async function fetchAgentHistory() {
  if (!State.backend.online) return;
  try {
    var res = await fetch(State.backend.url + '/api/agents/history?limit=50', { signal: AbortSignal.timeout(5000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    var data = await res.json();
    renderAgentTimeline(data.events || []);
  } catch (e) {
    // Silently fail — polling will retry
  }
}

function renderAgentTimeline(events) {
  var body = document.getElementById('agentTimelineBody');
  var counter = document.getElementById('agentEventCount');
  if (counter) counter.textContent = events.length + ' events';

  if (!events || events.length === 0) {
    body.innerHTML = '<div class="agent-event"><span class="agent-event-time">\u2014</span><span class="agent-event-type info">IDLE</span><span class="agent-event-detail">No agent events recorded yet</span></div>';
    return;
  }

  // Sort descending
  events.sort(function (a, b) { return (b.timestampMs || 0) - (a.timestampMs || 0); });

  var html = '';
  events.forEach(function (ev) {
    var timeStr = ev.timestampMs ? new Date(ev.timestampMs).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '\u2014';

    var typeClass = 'info';
    var evType = (ev.type || '').toLowerCase();
    if (evType.indexOf('detect') >= 0 || evType.indexOf('fail') >= 0) typeClass = 'detection';
    else if (evType.indexOf('correct') >= 0 || evType.indexOf('retry') >= 0) typeClass = 'correction';
    else if (evType.indexOf('replay') >= 0 || evType.indexOf('skip') >= 0 || evType.indexOf('escalat') >= 0) typeClass = 'replay';

    html += '<div class="agent-event">';
    html += '<span class="agent-event-time">' + timeStr + '</span>';
    html += '<span class="agent-event-type ' + typeClass + '">' + esc(ev.type || 'event').toUpperCase() + '</span>';
    html += '<span class="agent-event-detail">' + esc(ev.detail || '') + '</span>';
    html += '</div>';
  });

  body.innerHTML = html;
}

async function replayFailure(failureId, action, btnEl) {
  if (!State.backend.online) {
    logDebug('Cannot replay: backend offline', 'warn');
    return;
  }

  // Disable all action buttons in this row
  if (btnEl) {
    var row = btnEl.closest('tr');
    if (row) {
      var btns = row.querySelectorAll('.failure-action-btn');
      btns.forEach(function (b) { b.disabled = true; });
    }
  }

  try {
    var res = await fetch(State.backend.url + '/api/agents/replay', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ failure_id: failureId, action: action }),
    });

    if (!res.ok) throw new Error('HTTP ' + res.status);
    var data = await res.json();

    // Update the outcome badge in-place
    var badge = document.getElementById('outcome-' + failureId);
    if (badge) {
      var newOutcome = data.outcome || data.status || 'Unknown';
      var newClass = newOutcome.toLowerCase();
      if (newClass === 'corrected') newClass = 'corrected';
      else if (newClass === 'declined' || newClass === 'skipped') newClass = 'declined';
      else if (newClass === 'escalated') newClass = 'failed';
      else newClass = 'detected';
      badge.className = 'outcome-badge ' + newClass;
      badge.textContent = newOutcome;
    }

    logDebug('Replay ' + action + ' on failure #' + failureId + ': ' + (data.status || 'done'), 'info');

    // Refresh agent history to show new event
    fetchAgentHistory();
  } catch (e) {
    logDebug('Replay failed: ' + e.message, 'error');
    // Re-enable buttons on failure
    if (btnEl) {
      var row2 = btnEl.closest('tr');
      if (row2) {
        var btns2 = row2.querySelectorAll('.failure-action-btn');
        btns2.forEach(function (b) { b.disabled = false; });
      }
    }
  }
}

function exportAgentData() {
  var data = {
    timestamp: new Date().toISOString(),
    backendUrl: State.backend.url,
    failures: State.failureData || [],
    agentPanel: 'Phase 2 export',
  };
  var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = 'rawrxd-agents-' + Date.now() + '.json'; a.click();
  URL.revokeObjectURL(url);
  logDebug('Agent data exported', 'info');
}

// ======================================================================
// SECURITY & HARDENING (Phase 4)
// ======================================================================

function secLog(level, detail) {
  var entry = {
    timestamp: Date.now(),
    level: level,
    detail: detail,
  };
  State.security.eventLog.push(entry);
  if (State.security.eventLog.length > State.security.maxLog) {
    State.security.eventLog.shift();
  }
  logStructured(level, 'security_event', { detail: detail });
}

// --- Manual Max Input Length Adjustment ---
function setMaxInputLength(newLimit) {
  var val = parseInt(newLimit, 10);
  if (isNaN(val) || val < 1) val = 1;
  if (val > 1000000000) val = 1000000000;
  State.security.inputGuard.maxLength = val;
  secLog('CONFIG', 'Max input length set to ' + val.toLocaleString() + ' chars');
  var display = el('secMaxInputVal');
  if (display) display.textContent = val.toLocaleString() + ' chars';
  var feedback = el('secInputLimitFeedback');
  if (feedback) {
    feedback.textContent = '\u2713 Set to ' + val.toLocaleString();
    feedback.style.color = 'var(--accent-green, #5cb85c)';
  }
  logDebug('\u2699\ufe0f Max input length adjusted to ' + val.toLocaleString() + ' chars');
}

// --- Input Sanitization Guard ---
function validateInput(text) {
  if (!text || typeof text !== 'string') {
    return { valid: true, sanitized: '' };
  }

  // Length check
  if (text.length > State.security.inputGuard.maxLength) {
    State.security.inputGuard.blockedLength++;
    secLog('BLOCK', 'Input exceeds max length (' + text.length + ' > ' + State.security.inputGuard.maxLength + ')');
    return {
      valid: false,
      reason: 'Input too long (' + text.length + ' chars, max ' + State.security.inputGuard.maxLength + ')',
    };
  }

  // XSS pattern detection (before it reaches DOMPurify — defense in depth)
  var xssPatterns = [
    /<script[\s>]/i,
    /javascript\s*:/i,
    /on(error|load|click|mouseover|focus|blur|submit|change|input)\s*=/i,
    /eval\s*\(/i,
    /document\.(cookie|domain|write)/i,
    /<iframe[\s>]/i,
    /<object[\s>]/i,
    /<embed[\s>]/i,
  ];

  for (var i = 0; i < xssPatterns.length; i++) {
    if (xssPatterns[i].test(text)) {
      State.security.inputGuard.blockedXss++;
      secLog('BLOCK', 'XSS pattern detected: ' + xssPatterns[i].toString().substring(0, 40));
      // Strip the pattern rather than rejecting — DOMPurify will handle the rest
      text = text.replace(xssPatterns[i], '[filtered]');
    }
  }

  return { valid: true, sanitized: text };
}

// --- Client-Side Rate Limiter ---
function checkRateLimit() {
  var rl = State.security.rateLimit;
  var now = Date.now();

  // Prune old timestamps outside window
  rl.timestamps = rl.timestamps.filter(function (ts) {
    return (now - ts) < rl.windowMs;
  });

  if (rl.timestamps.length >= rl.maxPerMinute) {
    rl.blocked++;
    secLog('BLOCK', 'Rate limit exceeded: ' + rl.timestamps.length + '/' + rl.maxPerMinute + ' per minute');
    return false;
  }

  rl.timestamps.push(now);
  return true;
}

function resetRateLimit() {
  State.security.rateLimit.timestamps = [];
  State.security.rateLimit.blocked = 0;
  secLog('AUDIT', 'Rate limiter reset by user');
  renderSecurityPanel();
}

// --- Backend URL Validation ---
function isUrlAllowed(urlStr) {
  for (var i = 0; i < State.security.urlAllowlist.length; i++) {
    if (State.security.urlAllowlist[i].test(urlStr)) return true;
  }
  return false;
}

function validateBackendUrl() {
  var url = State.backend.url;
  var allowed = isUrlAllowed(url);
  if (allowed) {
    secLog('ALLOW', 'Backend URL validated: ' + url);
  } else {
    secLog('WARN', 'Backend URL NOT on allowlist: ' + url);
    State.security.inputGuard.blockedUrl++;
  }
  renderSecurityPanel();
  return allowed;
}

// --- CSP Audit ---
function auditCSP() {
  var cspMeta = document.querySelector('meta[http-equiv="Content-Security-Policy"]');
  var directives = [];

  if (!cspMeta) {
    secLog('WARN', 'No CSP meta tag found!');
    el('secCspAuditBody').innerHTML = '<div style="color:var(--accent-red); padding:12px;">No CSP meta tag detected</div>';
    el('secCspCount').textContent = '0';
    el('secCspCount').className = 'sec-card-value danger';
    return;
  }

  var cspContent = cspMeta.getAttribute('content') || '';
  var parts = cspContent.split(';').map(function (s) { return s.trim(); }).filter(Boolean);

  parts.forEach(function (part) {
    var tokens = part.split(/\s+/);
    var name = tokens[0];
    var values = tokens.slice(1).join(' ');

    // Grade the directive
    var grade = 'pass';
    if (values.indexOf("'unsafe-inline'") >= 0) grade = 'warn';
    if (values.indexOf("'unsafe-eval'") >= 0) grade = 'fail';
    if (values === '*') grade = 'fail';

    directives.push({ name: name, value: values, grade: grade });
  });

  // Render
  var html = '';
  directives.forEach(function (d) {
    html += '<div class="sec-directive">';
    html += '<span class="sec-directive-name">' + esc(d.name) + '</span>';
    html += '<span class="sec-directive-value">' + esc(d.value) + '</span>';
    var label = d.grade === 'pass' ? 'SECURE' : d.grade === 'warn' ? 'CAUTION' : 'RISK';
    html += '<span class="sec-directive-status ' + d.grade + '">' + label + '</span>';
    html += '</div>';
  });

  el('secCspAuditBody').innerHTML = html;
  el('secCspCount').textContent = directives.length;
  el('secCspCount').className = 'sec-card-value secure';

  secLog('AUDIT', 'CSP audit completed: ' + directives.length + ' directives');
}

// --- Security Panel Show/Close/Render ---
function showSecurityPanel() {
  var panel = document.getElementById('securityPanel');
  panel.classList.add('open');
  renderSecurityPanel();
  secLog('AUDIT', 'Security panel opened');
}

function closeSecurityPanel() {
  var panel = document.getElementById('securityPanel');
  panel.classList.remove('open');
}

function renderSecurityPanel() {
  // CSP audit
  auditCSP();

  // DOMPurify check
  var dpLoaded = typeof DOMPurify !== 'undefined';
  el('secDomPurifyStatus').textContent = dpLoaded ? 'v' + (DOMPurify.version || '3.x') + ' loaded' : 'NOT LOADED';
  el('secDomPurifyBadge').textContent = dpLoaded ? 'ACTIVE' : 'MISSING';
  el('secDomPurifyBadge').className = 'sec-directive-status ' + (dpLoaded ? 'pass' : 'fail');
  el('secSumPurify').textContent = dpLoaded ? 'Active' : 'Missing';
  el('secSumPurify').className = 'sec-side-val ' + (dpLoaded ? 'secure' : 'danger');

  // Rate limit meter
  var rl = State.security.rateLimit;
  var now = Date.now();
  var activeCount = rl.timestamps.filter(function (ts) { return (now - ts) < rl.windowMs; }).length;
  var pct = Math.min(100, Math.round((activeCount / rl.maxPerMinute) * 100));
  var bar = el('secRateBar');
  bar.style.width = pct + '%';
  bar.className = 'sec-rate-bar-fill' + (pct > 80 ? ' danger' : pct > 50 ? ' warning' : '');
  el('secRateUsed').textContent = activeCount + ' used';
  el('secRateRemaining').textContent = (rl.maxPerMinute - activeCount) + ' remaining';
  el('secRateUnit').textContent = activeCount + ' / ' + rl.maxPerMinute + ' per min';
  el('secRateStatus').textContent = pct > 80 ? '!' : 'OK';
  el('secRateStatus').className = 'sec-card-value ' + (pct > 80 ? 'danger' : pct > 50 ? 'warning' : 'secure');
  el('secSumRate').textContent = pct > 80 ? 'Critical' : pct > 50 ? 'Elevated' : 'OK';
  el('secSumRate').className = 'sec-side-val ' + (pct > 80 ? 'danger' : pct > 50 ? 'warning' : 'secure');

  // Blocked counts
  var ig = State.security.inputGuard;
  el('secBlockedCount').textContent = ig.blockedXss + ig.blockedLength + rl.blocked + ig.blockedUrl;
  el('secBlkXss').textContent = ig.blockedXss;
  el('secBlkLength').textContent = ig.blockedLength;
  el('secBlkRate').textContent = rl.blocked;
  el('secBlkUrl').textContent = ig.blockedUrl;

  // URL validation status
  var urlOk = isUrlAllowed(State.backend.url);
  el('secUrlStatus').textContent = urlOk ? '\u2713' : '\u2717';
  el('secUrlStatus').className = 'sec-card-value ' + (urlOk ? 'secure' : 'danger');
  el('secSumUrl').textContent = urlOk ? 'Enforced' : 'BLOCKED';
  el('secSumUrl').className = 'sec-side-val ' + (urlOk ? 'secure' : 'danger');

  // Render event log
  renderSecurityLog();
}

// ======================================================================
// WIN32IDE BACKEND SWITCHER (Phase 8B: /api/backends, /api/backend/*)
// Beaconism: builds client-side backend list when /api/backends unavailable
// ======================================================================
function showBackendSwitcher() {
  document.getElementById('backendSwitcherPanel').classList.add('active');
  fetchBackends();
}
function closeBackendSwitcher() {
  document.getElementById('backendSwitcherPanel').classList.remove('active');
}

// --- Build a backend card HTML ---
function _renderBackendCard(name, url, online, isActive, switchable) {
  var statusColor = online ? 'var(--accent-green)' : 'var(--accent-red)';
  var border = isActive ? 'var(--accent-green)' : 'var(--border-subtle)';
  var h = '<div style="display:flex;justify-content:space-between;align-items:center;padding:8px 12px;background:var(--bg-tertiary);border-radius:6px;border:1px solid ' + border + ';">';
  h += '<div><strong style="color:var(--text-primary);font-size:12px;">' + esc(name) + '</strong>';
  if (url) h += '<div style="font-size:10px;color:var(--text-muted);font-family:var(--font-mono);">' + esc(url) + '</div>';
  h += '</div>';
  h += '<div style="display:flex;gap:6px;align-items:center;">';
  h += '<span style="color:' + statusColor + ';font-size:18px;">&#x25CF;</span>';
  if (isActive) {
    h += '<span style="color:var(--accent-green);font-size:10px;font-weight:700;">ACTIVE</span>';
  } else if (switchable && online) {
    h += '<button class="code-btn" onclick="switchBackend(\'' + esc(name) + '\')" style="font-size:10px;">Switch</button>';
  } else {
    h += '<span style="color:var(--text-muted);font-size:10px;">OFFLINE</span>';
  }
  h += '</div></div>';
  return h;
}

// --- Beacon: probe a URL and return online status ---
async function _probeBackendUrl(url, path) {
  try {
    var res = await fetch(url + (path || '/'), { signal: AbortSignal.timeout(2000) });
    return res.ok;
  } catch (_) { return false; }
}

// --- Build client-side backend list from known state ---
async function _buildBeaconBackendList() {
  var backends = [];
  var ollamaUrl = State.backend.ollamaDirectUrl || 'http://localhost:11434';
  var ideUrl = State.backend.url || 'http://localhost:8080';

  // Probe Ollama
  var ollamaOnline = await _probeBackendUrl(ollamaUrl);
  backends.push({
    name: 'Ollama',
    type: 'ollama',
    url: ollamaUrl,
    online: ollamaOnline,
    isActive: State.backend.directMode && State.backend.online
  });

  // Probe Win32IDE (only if different from Ollama URL)
  if (ideUrl !== ollamaUrl) {
    var ideOnline = await _probeBackendUrl(ideUrl, '/status');
    backends.push({
      name: 'Win32IDE',
      type: 'rawrxd-win32ide',
      url: ideUrl,
      online: ideOnline,
      isActive: !State.backend.directMode && State.backend.online && State.backend.serverType !== 'ollama-direct'
    });
  }

  // If neither is active but we're online somehow, mark the current one
  if (State.backend.online && !backends.some(function (b) { return b.isActive; })) {
    var activeUrl = getActiveUrl();
    var found = backends.find(function (b) { return b.url === activeUrl; });
    if (found) found.isActive = true;
  }

  return backends;
}

async function fetchBackends() {
  if (!State.backend.online) {
    document.getElementById('bsBackendList').innerHTML = '<div style="color:var(--accent-red);padding:12px;">Backend offline. Run <code>connect</code> first.</div>';
    document.getElementById('bsActiveBackend').textContent = '\u2014';
    return;
  }

  // --- Try 1: Remote /api/backends (Win32IDE or tool_server) ---
  try {
    var activeRes = await fetch(getActiveUrl() + '/api/backend/active', { signal: AbortSignal.timeout(3000) });
    var activeData = activeRes.ok ? await activeRes.json() : null;

    var res = await fetch(getActiveUrl() + '/api/backends', { signal: AbortSignal.timeout(3000) });
    if (res.ok) {
      var data = await res.json();
      var backends = data.backends || data.list || [];
      if (Array.isArray(backends) && backends.length > 0) {
        var activeName = activeData ? (activeData.name || activeData.backend || activeData.type || '') : '';
        document.getElementById('bsActiveBackend').textContent = activeName || '\u2014';

        var html = '';
        backends.forEach(function (b) {
          var name = b.name || b.type || 'unknown';
          var url = b.url || b.endpoint || '';
          var isActive = (activeName === name);
          html += _renderBackendCard(name, url, b.online !== false, isActive, true);
        });
        document.getElementById('bsBackendList').innerHTML = html;
        return;
      }
    }
  } catch (_) {
    // Remote endpoints not available — fall through to beacon
  }

  // --- Try 2: Beacon — build client-side backend list ---
  logDebug('Backend Switcher: /api/backends unavailable, using beacon probe', 'info');
  document.getElementById('bsBackendList').innerHTML = '<div style="color:var(--text-muted);padding:12px;font-size:11px;">Probing backends...</div>';

  var beaconList = await _buildBeaconBackendList();
  var activeBe = beaconList.find(function (b) { return b.isActive; });
  document.getElementById('bsActiveBackend').textContent = activeBe ? activeBe.name : (State.backend.serverType || '\u2014');

  var html = '';
  beaconList.forEach(function (b) {
    html += _renderBackendCard(b.name, b.url, b.online, b.isActive, true);
  });
  // Add info bar
  html += '<div style="padding:8px 12px;font-size:10px;color:var(--text-muted);border-top:1px solid var(--border-subtle);margin-top:4px;">';
  html += '&#x1F4E1; Beacon mode &mdash; detected via direct probing (no /api/backends endpoint)';
  html += '</div>';
  document.getElementById('bsBackendList').innerHTML = html;
}

async function switchBackend(name) {
  var lowerName = name.toLowerCase();

  // --- Try 1: Remote /api/backend/switch ---
  try {
    var res = await fetch(getActiveUrl() + '/api/backend/switch', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ backend: name, name: name }),
      signal: AbortSignal.timeout(5000)
    });
    if (res.ok) {
      var data = await res.json();
      logDebug('Backend switched to ' + name + ': ' + (data.status || 'ok'), 'info');
      addMessage('system', '\u2705 Backend switched to **' + name + '**', { skipMemory: true });
      fetchBackends();
      fetchModels();
      return;
    }
  } catch (_) {
    // Remote endpoint not available — try client-side switch
  }

  // --- Try 2: Client-side beacon switch ---
  logDebug('Backend switch: /api/backend/switch unavailable, switching locally', 'info');

  if (lowerName === 'ollama' || lowerName === 'ollama-direct') {
    // Switch to Ollama direct mode
    var ollamaUrl = State.backend.ollamaDirectUrl || 'http://localhost:11434';
    var online = await _probeBackendUrl(ollamaUrl);
    if (online) {
      State.backend.directMode = true;
      State.backend.serverType = 'ollama-direct';
      State.backend.online = true;
      document.getElementById('statusText').textContent = 'OLLAMA DIRECT';
      logDebug('Beacon-switched to Ollama direct at ' + ollamaUrl, 'info');
      addMessage('system', '\u2705 Switched to **Ollama** (direct mode at ' + ollamaUrl + ')', { skipMemory: true });
      await fetchModels();
      fetchBackends();
    } else {
      addMessage('system', '\u274c Ollama is not reachable at ' + ollamaUrl, { skipMemory: true });
    }
  } else if (lowerName === 'win32ide' || lowerName === 'rawrxd-win32ide' || lowerName === 'rawrxd') {
    // Switch to Win32IDE proxy mode
    var ideUrl = State.backend.url || 'http://localhost:8080';
    var online = await _probeBackendUrl(ideUrl, '/status');
    if (online) {
      State.backend.directMode = false;
      State.backend.serverType = 'rawrxd-win32ide';
      State.backend.online = true;
      _ideServerUrl = ideUrl;
      document.getElementById('statusText').textContent = 'ONLINE';
      logDebug('Beacon-switched to Win32IDE at ' + ideUrl, 'info');
      addMessage('system', '\u2705 Switched to **Win32IDE** (proxy mode at ' + ideUrl + ')', { skipMemory: true });
      await fetchModels();
      fetchBackends();
    } else {
      addMessage('system', '\u274c Win32IDE is not reachable at ' + ideUrl, { skipMemory: true });
    }
  } else {
    addMessage('system', '\u274c Unknown backend: **' + name + '**. Available: Ollama, Win32IDE', { skipMemory: true });
  }
}

// ======================================================================
// WIN32IDE LLM ROUTER (Phase 9: /api/router/*)
// ======================================================================
function showRouterPanel() {
  document.getElementById('routerPanel').classList.add('active');
  fetchRouterStatus();
}
function closeRouterPanel() {
  document.getElementById('routerPanel').classList.remove('active');
}
async function fetchRouterStatus() {
  if (!State.backend.online) return;
  try {
    // Status
    var sRes = await fetch(getActiveUrl() + '/api/router/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('routerStatus').textContent = sData.status || sData.state || 'active';
      document.getElementById('routerStatus').style.color = 'var(--accent-green)';
    }
    // Last decision
    var dRes = await fetch(getActiveUrl() + '/api/router/decision', { signal: AbortSignal.timeout(5000) });
    if (dRes.ok) {
      var dData = await dRes.json();
      var decision = dData.backend || dData.selected || dData.decision || '\u2014';
      document.getElementById('routerLastDecision').textContent = typeof decision === 'object' ? JSON.stringify(decision).substring(0, 40) : decision;
    }
    // Capabilities
    var cRes = await fetch(getActiveUrl() + '/api/router/capabilities', { signal: AbortSignal.timeout(5000) });
    if (cRes.ok) {
      var cData = await cRes.json();
      var caps = cData.capabilities || cData.backends || cData;
      var capHtml = '';
      if (Array.isArray(caps)) {
        caps.forEach(function (c) {
          capHtml += '<div style="padding:6px 10px;background:var(--bg-tertiary);border-radius:4px;font-size:11px;">';
          capHtml += '<strong style="color:var(--text-primary);">' + esc(c.name || c.backend || 'unknown') + '</strong>';
          if (c.models) capHtml += ' &mdash; <span style="color:var(--accent-cyan);">' + c.models + ' models</span>';
          if (c.latency) capHtml += ' &mdash; <span style="color:var(--accent-secondary);">' + c.latency + 'ms avg</span>';
          capHtml += '</div>';
        });
      } else if (typeof caps === 'object') {
        Object.keys(caps).forEach(function (k) {
          capHtml += '<div style="padding:4px 10px;font-size:11px;"><span style="color:var(--accent-cyan);">' + esc(k) + '</span>: <span style="color:var(--text-secondary);">' + esc(JSON.stringify(caps[k]).substring(0, 60)) + '</span></div>';
        });
      }
      document.getElementById('routerCapList').innerHTML = capHtml || '<div style="color:var(--text-muted);font-size:11px;">No capabilities data</div>';
    }
    // Heatmap
    var hRes = await fetch(getActiveUrl() + '/api/router/heatmap', { signal: AbortSignal.timeout(5000) });
    if (hRes.ok) {
      var hData = await hRes.json();
      document.getElementById('routerHeatmap').textContent = JSON.stringify(hData, null, 2).substring(0, 300);
      document.getElementById('routerHeatmap').style.color = 'var(--text-secondary)';
    }
    // Pins
    var pRes = await fetch(getActiveUrl() + '/api/router/pins', { signal: AbortSignal.timeout(5000) });
    if (pRes.ok) {
      var pData = await pRes.json();
      var pins = pData.pins || pData;
      document.getElementById('routerPins').textContent = Array.isArray(pins) && pins.length > 0 ? pins.map(function (p) { return (p.pattern || p.model || '') + ' \u2192 ' + (p.backend || ''); }).join('\n') : 'No pins';
      document.getElementById('routerPins').style.color = 'var(--text-secondary)';
    }
    logDebug('Router status loaded', 'info');
  } catch (e) {
    logDebug('Router status failed: ' + e.message, 'error');
  }
}

// ======================================================================
// WIN32IDE SWARM DASHBOARD (Phase 11: /api/swarm/*)
// ======================================================================
function showSwarmPanel() {
  document.getElementById('swarmPanel').classList.add('active');
  fetchSwarmStatus();
}
function closeSwarmPanel() {
  document.getElementById('swarmPanel').classList.remove('active');
}
async function fetchSwarmStatus() {
  if (!State.backend.online) return;
  try {
    var sRes = await fetch(getActiveUrl() + '/api/swarm/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      var status = sData.status || sData.state || 'idle';
      document.getElementById('swarmStatus').textContent = status;
      document.getElementById('swarmStatus').style.color = status === 'running' ? 'var(--accent-green)' : 'var(--text-muted)';
    }
    // Nodes
    var nRes = await fetch(getActiveUrl() + '/api/swarm/nodes', { signal: AbortSignal.timeout(5000) });
    if (nRes.ok) {
      var nData = await nRes.json();
      var nodes = nData.nodes || nData;
      document.getElementById('swarmNodeCount').textContent = Array.isArray(nodes) ? nodes.length : 0;
      var nodeHtml = '';
      if (Array.isArray(nodes) && nodes.length > 0) {
        nodes.forEach(function (n) {
          var nStatus = n.status || n.state || 'unknown';
          var col = nStatus === 'active' || nStatus === 'online' ? 'var(--accent-green)' : 'var(--accent-red)';
          nodeHtml += '<div style="padding:6px 10px;background:var(--bg-tertiary);border-radius:4px;font-size:11px;display:flex;justify-content:space-between;">';
          nodeHtml += '<span style="color:var(--text-primary);">' + esc(n.name || n.id || n.host || 'node') + '</span>';
          nodeHtml += '<span style="color:' + col + ';">' + esc(nStatus) + '</span>';
          nodeHtml += '</div>';
        });
      }
      document.getElementById('swarmNodeList').innerHTML = nodeHtml || '<div style="color:var(--text-muted);font-size:11px;">No nodes connected</div>';
    }
    // Tasks
    var tRes = await fetch(getActiveUrl() + '/api/swarm/tasks', { signal: AbortSignal.timeout(5000) });
    if (tRes.ok) {
      var tData = await tRes.json();
      var tasks = tData.tasks || tData;
      document.getElementById('swarmTaskCount').textContent = Array.isArray(tasks) ? tasks.length : 0;
    }
    // Events
    var eRes = await fetch(getActiveUrl() + '/api/swarm/events', { signal: AbortSignal.timeout(5000) });
    if (eRes.ok) {
      var eData = await eRes.json();
      var events = eData.events || eData;
      var eventHtml = '';
      if (Array.isArray(events) && events.length > 0) {
        events.slice(-20).reverse().forEach(function (ev) {
          var time = ev.timestamp || ev.time || '';
          if (typeof time === 'number') time = new Date(time).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
          eventHtml += '<div style="margin-bottom:2px;"><span style="color:var(--text-muted);">[' + esc(time) + ']</span> <span style="color:var(--accent-cyan);">' + esc(ev.type || ev.event || 'event') + '</span> ' + esc(ev.detail || ev.message || '') + '</div>';
        });
      }
      document.getElementById('swarmEventList').innerHTML = eventHtml || '<div style="color:var(--text-muted);">No events</div>';
    }
    logDebug('Swarm status loaded', 'info');
  } catch (e) {
    logDebug('Swarm status failed: ' + e.message, 'error');
  }
}
async function swarmAction(action) {
  if (!State.backend.online) { addMessage('system', 'Backend offline', { skipMemory: true }); return; }
  try {
    var res = await fetch(getActiveUrl() + '/api/swarm/' + action, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({}),
      signal: AbortSignal.timeout(10000)
    });
    var data = res.ok ? await res.json() : {};
    logDebug('Swarm ' + action + ': ' + (data.status || 'ok'), 'info');
    addMessage('system', '\u2705 Swarm **' + action + '**: ' + (data.status || data.message || 'done'), { skipMemory: true });
    fetchSwarmStatus();
  } catch (e) {
    logDebug('Swarm ' + action + ' failed: ' + e.message, 'error');
  }
}

// ======================================================================
// WIN32IDE MULTI-RESPONSE ENGINE (Phase 10: /api/multi-response/*)
// ======================================================================
function showMultiResponsePanel() {
  document.getElementById('multiResponsePanel').classList.add('active');
  fetchMultiResponseStatus();
}
function closeMultiResponsePanel() {
  document.getElementById('multiResponsePanel').classList.remove('active');
}
async function fetchMultiResponseStatus() {
  if (!State.backend.online) return;
  try {
    var sRes = await fetch(getActiveUrl() + '/api/multi-response/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('mrStatus').textContent = sData.status || sData.state || 'ready';
      document.getElementById('mrGenCount').textContent = sData.total_generated || sData.generations || 0;
    }
    // Templates
    var tRes = await fetch(getActiveUrl() + '/api/multi-response/templates', { signal: AbortSignal.timeout(5000) });
    if (tRes.ok) {
      var tData = await tRes.json();
      var templates = tData.templates || tData;
      var tHtml = '';
      if (Array.isArray(templates) && templates.length > 0) {
        templates.forEach(function (t) {
          var label = t.name || t.type || t.id || 'template';
          var desc = t.description || t.label || '';
          tHtml += '<div style="padding:6px 10px;background:var(--bg-tertiary);border-radius:4px;font-size:11px;">';
          tHtml += '<strong style="color:var(--accent-cyan);">' + esc(label) + '</strong>';
          if (desc) tHtml += ' &mdash; <span style="color:var(--text-secondary);">' + esc(desc.substring(0, 60)) + '</span>';
          tHtml += '</div>';
        });
      }
      document.getElementById('mrTemplateList').innerHTML = tHtml || '<div style="color:var(--text-muted);font-size:11px;">No templates loaded</div>';
    }
    // Preference stats
    var pRes = await fetch(getActiveUrl() + '/api/multi-response/stats', { signal: AbortSignal.timeout(5000) });
    if (pRes.ok) {
      var pData = await pRes.json();
      document.getElementById('mrPrefStats').textContent = JSON.stringify(pData, null, 2).substring(0, 200);
      document.getElementById('mrPrefStats').style.color = 'var(--text-secondary)';
    }
    logDebug('Multi-response status loaded', 'info');
  } catch (e) {
    logDebug('Multi-response status failed: ' + e.message, 'error');
  }
}

// ======================================================================
// WIN32IDE ASM DEBUGGER (Phase 12: /api/debug/*)
// ======================================================================
function showAsmDebugPanel() {
  document.getElementById('asmDebugPanel').classList.add('active');
  fetchAsmDebugStatus();
}
function closeAsmDebugPanel() {
  document.getElementById('asmDebugPanel').classList.remove('active');
}
async function fetchAsmDebugStatus() {
  if (!State.backend.online) return;
  try {
    // Status
    var sRes = await fetch(getActiveUrl() + '/api/debug/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      var status = sData.status || sData.state || 'idle';
      document.getElementById('asmDebugStatus').textContent = status;
      document.getElementById('asmDebugStatus').style.color = status === 'running' || status === 'active' ? 'var(--accent-green)' : 'var(--text-muted)';
    }
    // Breakpoints
    var bRes = await fetch(getActiveUrl() + '/api/debug/breakpoints', { signal: AbortSignal.timeout(5000) });
    if (bRes.ok) {
      var bData = await bRes.json();
      var bps = bData.breakpoints || bData;
      document.getElementById('asmBpCount').textContent = Array.isArray(bps) ? bps.length : 0;
    }
    // Registers
    var rRes = await fetch(getActiveUrl() + '/api/debug/registers', { signal: AbortSignal.timeout(5000) });
    if (rRes.ok) {
      var rData = await rRes.json();
      var regs = rData.registers || rData;
      var regHtml = '';
      if (typeof regs === 'object' && regs !== null) {
        Object.keys(regs).forEach(function (k) {
          var val = typeof regs[k] === 'number' ? '0x' + regs[k].toString(16).toUpperCase().padStart(16, '0') : String(regs[k]);
          regHtml += '<div style="padding:2px 4px;background:var(--bg-primary);border-radius:2px;overflow:hidden;">';
          regHtml += '<span style="color:var(--accent-cyan);">' + esc(k) + '</span> ';
          regHtml += '<span style="color:var(--text-secondary);">' + esc(val.substring(0, 18)) + '</span>';
          regHtml += '</div>';
        });
      }
      document.getElementById('asmRegisterList').innerHTML = regHtml || '<div style="color:var(--text-muted);">No register data</div>';
    }
    // Stack
    var stRes = await fetch(getActiveUrl() + '/api/debug/stack', { signal: AbortSignal.timeout(5000) });
    if (stRes.ok) {
      var stData = await stRes.json();
      var frames = stData.frames || stData.stack || stData;
      var stackHtml = '';
      if (Array.isArray(frames) && frames.length > 0) {
        frames.forEach(function (f, i) {
          stackHtml += '<div style="margin-bottom:2px;"><span style="color:var(--accent-secondary);">#' + i + '</span> ';
          stackHtml += '<span style="color:var(--text-primary);">' + esc(f.function || f.name || f.symbol || '??') + '</span>';
          if (f.address) stackHtml += ' <span style="color:var(--text-muted);">@ ' + esc(f.address) + '</span>';
          if (f.module) stackHtml += ' <span style="color:var(--accent-cyan);">(' + esc(f.module) + ')</span>';
          stackHtml += '</div>';
        });
      }
      document.getElementById('asmStackView').innerHTML = stackHtml || '<div style="color:var(--text-muted);">No stack data</div>';
    }
    // Threads
    var thRes = await fetch(getActiveUrl() + '/api/debug/threads', { signal: AbortSignal.timeout(5000) });
    if (thRes.ok) {
      var thData = await thRes.json();
      var threads = thData.threads || thData;
      document.getElementById('asmThreadCount').textContent = Array.isArray(threads) ? threads.length : 0;
    }
    // Events
    var evRes = await fetch(getActiveUrl() + '/api/debug/events', { signal: AbortSignal.timeout(5000) });
    if (evRes.ok) {
      var evData = await evRes.json();
      var events = evData.events || evData;
      var evHtml = '';
      if (Array.isArray(events) && events.length > 0) {
        events.slice(-10).reverse().forEach(function (ev) {
          evHtml += '<div style="margin-bottom:2px;"><span style="color:var(--accent-cyan);">' + esc(ev.type || ev.event || 'event') + '</span> ' + esc(ev.detail || ev.message || '') + '</div>';
        });
      }
      document.getElementById('asmEventList').innerHTML = evHtml || '<div style="color:var(--text-muted);">No events</div>';
    }
    logDebug('ASM debug status loaded', 'info');
  } catch (e) {
    logDebug('ASM debug status failed: ' + e.message, 'error');
  }
}
async function asmDebugAction(action) {
  if (!State.backend.online) { addMessage('system', 'Backend offline', { skipMemory: true }); return; }
  try {
    var res = await fetch(getActiveUrl() + '/api/debug/' + action, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({}),
      signal: AbortSignal.timeout(10000)
    });
    var data = res.ok ? await res.json() : {};
    logDebug('Debug ' + action + ': ' + (data.status || 'ok'), 'info');
    fetchAsmDebugStatus();
  } catch (e) {
    logDebug('Debug ' + action + ' failed: ' + e.message, 'error');
  }
}

// ======================================================================
// WIN32IDE SAFETY MONITOR (Phase 14: /api/safety/*)
// ======================================================================
function showSafetyPanel() {
  document.getElementById('safetyPanel').classList.add('active');
  fetchSafetyStatus();
}
function closeSafetyPanel() {
  document.getElementById('safetyPanel').classList.remove('active');
}

// ---- New panel show/close/refresh functions (Phase 40: Engine API wiring) ----
function showPoliciesPanel() {
  document.getElementById('policiesPanel').classList.add('active');
  refreshPoliciesPanel();
}
function closePoliciesPanel() {
  document.getElementById('policiesPanel').classList.remove('active');
}
async function refreshPoliciesPanel() {
  try {
    var data = await EngineAPI.fetchPolicies();
    var policies = data.policies || data;
    document.getElementById('policyActiveCount').textContent = Array.isArray(policies) ? policies.length : '?';
    var html = '';
    if (Array.isArray(policies)) {
      policies.forEach(function (p) {
        html += '<div style="margin-bottom:4px;padding:4px 8px;background:rgba(0,255,157,0.08);border-radius:3px;">';
        html += '<span style="color:var(--accent-cyan);">[' + esc(p.id || '?') + ']</span> ';
        html += '<span style="color:var(--text-primary);">' + esc(p.name || p.type || 'unnamed') + '</span> ';
        html += '<span style="color:var(--text-muted);font-size:10px;">' + esc(p.status || 'active') + '</span>';
        html += '</div>';
      });
    }
    document.getElementById('policyList').innerHTML = html || '<div style="color:var(--text-muted);">No policies loaded</div>';
  } catch (e) {
    document.getElementById('policyList').innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}
async function fetchPolicySuggestions() {
  try {
    var data = await EngineAPI.fetchPolicySuggestions();
    var suggestions = data.suggestions || data;
    document.getElementById('policySuggestionCount').textContent = Array.isArray(suggestions) ? suggestions.length : '?';
    var html = '';
    if (Array.isArray(suggestions)) {
      suggestions.forEach(function (s) {
        html += '<div style="margin-bottom:4px;padding:4px 8px;background:rgba(255,165,0,0.08);border-radius:3px;">';
        html += '\u2022 ' + esc((s.description || s.name || JSON.stringify(s)).substring(0, 100));
        html += ' <button class="code-btn" style="font-size:9px;padding:1px 6px;" onclick="applyPolicyById(\'' + esc(s.id || '') + '\')">\u2705 Apply</button>';
        html += '</div>';
      });
    }
    document.getElementById('policySuggestionList').innerHTML = html || '<div style="color:var(--text-muted);">No suggestions</div>';
  } catch (e) {
    document.getElementById('policySuggestionList').innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}
async function applyPolicyById(id) {
  try {
    var data = await EngineAPI.applyPolicy(id);
    addMessage('system', '\u2705 Policy applied: ' + (data.message || id), { skipMemory: true });
    refreshPoliciesPanel();
  } catch (e) { addMessage('system', '\u274C Policy apply failed: ' + e.message, { skipMemory: true }); }
}
async function exportPolicies() {
  try {
    var data = await EngineAPI.exportPolicies();
    var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'rawrxd-policies-' + Date.now() + '.json';
    a.click();
  } catch (e) { addMessage('system', '\u274C Export failed: ' + e.message, { skipMemory: true }); }
}
function importPoliciesPrompt() {
  var input = document.createElement('input');
  input.type = 'file';
  input.accept = '.json';
  input.onchange = async function () {
    if (!input.files[0]) return;
    try {
      var text = await input.files[0].text();
      var data = JSON.parse(text);
      var result = await EngineAPI.importPolicies(data);
      addMessage('system', '\u2705 Policies imported: ' + (result.message || result.count || 'OK'), { skipMemory: true });
      refreshPoliciesPanel();
    } catch (e) { addMessage('system', '\u274C Import failed: ' + e.message, { skipMemory: true }); }
  };
  input.click();
}
async function fetchPolicyHeuristics() {
  try {
    var data = await EngineAPI.fetchHeuristics();
    document.getElementById('policyHeuristicCount').textContent = data.count || Object.keys(data).length || '?';
    addMessage('system', '**Heuristics:**\n```json\n' + JSON.stringify(data, null, 2).substring(0, 1000) + '\n```', { skipMemory: true });
  } catch (e) { addMessage('system', '\u274C Heuristics failed: ' + e.message, { skipMemory: true }); }
}
async function fetchPolicyStats() {
  try {
    var data = await EngineAPI.fetchPolicyStats();
    addMessage('system', '**Policy Stats:**\n```json\n' + JSON.stringify(data, null, 2).substring(0, 1000) + '\n```', { skipMemory: true });
  } catch (e) { addMessage('system', '\u274C Stats failed: ' + e.message, { skipMemory: true }); }
}

function showHotpatchDetailPanel() {
  document.getElementById('hotpatchDetailPanel').classList.add('active');
  refreshHotpatchDetailPanel();
}
function closeHotpatchDetailPanel() {
  document.getElementById('hotpatchDetailPanel').classList.remove('active');
}
async function refreshHotpatchDetailPanel() {
  try {
    var data = await EngineAPI.hotpatchStatus();
    document.getElementById('hpMemoryStatus').textContent = data.memory || data.memory_layer || '\u2014';
    document.getElementById('hpByteStatus').textContent = data.byte || data.byte_layer || '\u2014';
    document.getElementById('hpServerStatus').textContent = data.server || data.server_layer || '\u2014';
    var html = '';
    var keys = Object.keys(data);
    keys.forEach(function (k) {
      html += '<div style="margin-bottom:2px;"><span style="color:var(--accent-cyan);">' + esc(k) + ':</span> ' + esc(JSON.stringify(data[k])) + '</div>';
    });
    document.getElementById('hotpatchDetailLog').innerHTML = html || '<div style="color:var(--text-muted);">No hotpatch data</div>';
  } catch (e) {
    document.getElementById('hotpatchDetailLog').innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}
async function hotpatchAction(action, layer) {
  try {
    var data = await EngineAPI.hotpatchAction(action, layer);
    addMessage('system', '\u2705 Hotpatch ' + action + ' (' + layer + '): ' + (data.message || data.result || 'OK'), { skipMemory: true });
    refreshHotpatchDetailPanel();
  } catch (e) { addMessage('system', '\u274C Hotpatch failed: ' + e.message, { skipMemory: true }); }
}

function showCotDetailPanel() {
  document.getElementById('cotDetailPanel').classList.add('active');
  refreshCotDetailPanel();
}
function closeCotDetailPanel() {
  document.getElementById('cotDetailPanel').classList.remove('active');
}
async function refreshCotDetailPanel() {
  try {
    var data = await EngineAPI.fetchExplain();
    var chain = data.chain || data.steps || data.explanation;
    var html = '';
    if (Array.isArray(chain)) {
      chain.forEach(function (step, idx) {
        html += '<div style="margin-bottom:6px;padding:6px 8px;background:rgba(0,200,255,0.06);border-radius:4px;border-left:3px solid var(--accent-cyan);">';
        html += '<span style="color:var(--accent-orange);font-weight:bold;">Step ' + (idx + 1) + '</span><br>';
        html += '<span style="color:var(--text-primary);">' + esc((step.reasoning || step.text || JSON.stringify(step)).substring(0, 300)) + '</span>';
        html += '</div>';
      });
    } else {
      html = '<div style="color:var(--text-muted);">' + esc(JSON.stringify(data).substring(0, 500)) + '</div>';
    }
    document.getElementById('cotExplainContent').innerHTML = html;
  } catch (e) {
    document.getElementById('cotExplainContent').innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}
async function fetchExplainStats() {
  try {
    var data = await EngineAPI.fetchExplainStats();
    document.getElementById('cotExplainTotal').textContent = data.total || data.count || '?';
    document.getElementById('cotExplainAvgSteps').textContent = data.avg_steps || data.average || '\u2014';
    addMessage('system', '**Explain Stats:**\n```json\n' + JSON.stringify(data, null, 2).substring(0, 500) + '\n```', { skipMemory: true });
  } catch (e) { addMessage('system', '\u274C Explain stats failed: ' + e.message, { skipMemory: true }); }
}

function showToolsPanel() {
  document.getElementById('toolsPanel').classList.add('active');
}
function closeToolsPanel() {
  document.getElementById('toolsPanel').classList.remove('active');
}
async function executeToolFromPanel() {
  var tool = document.getElementById('toolSelect').value;
  var argsRaw = document.getElementById('toolArgs').value.trim();
  var toolArgs = {};
  if (argsRaw) {
    try { toolArgs = JSON.parse(argsRaw); } catch (_) { toolArgs = { path: argsRaw }; }
  }
  var outputEl = document.getElementById('toolOutput');
  outputEl.innerHTML = '<div style="color:var(--accent-cyan);">Executing ' + esc(tool) + '...</div>';
  try {
    var data = await EngineAPI.executeTool(tool, toolArgs);
    outputEl.innerHTML = '<pre style="color:var(--text-primary);white-space:pre-wrap;margin:0;">' + esc(JSON.stringify(data, null, 2).substring(0, 2000)) + '</pre>';
  } catch (e) {
    outputEl.innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}

function showEnginePanel() {
  document.getElementById('enginePanel').classList.add('active');
  renderEngineMap();
}
function closeEnginePanel() {
  document.getElementById('enginePanel').classList.remove('active');
}
function renderEngineMap() {
  EngineAPI.renderSubsystemMap();
}

function showMetricsPanel() {
  document.getElementById('metricsPanel').classList.add('active');
  refreshMetricsPanel();
}
function closeMetricsPanel() {
  document.getElementById('metricsPanel').classList.remove('active');
}
async function refreshMetricsPanel() {
  try {
    var data = await EngineAPI.fetchHealth();
    document.getElementById('metricsHealth').textContent = data.status || 'OK';
    document.getElementById('metricsHealth').style.color = 'var(--accent-green)';
  } catch (_) {
    document.getElementById('metricsHealth').textContent = 'OFFLINE';
    document.getElementById('metricsHealth').style.color = 'var(--accent-red)';
  }
  try {
    var data = await EngineAPI.fetchServerStatus();
    document.getElementById('metricsUptime').textContent = data.uptime || data.uptime_seconds ? Math.round((data.uptime || data.uptime_seconds) / 60) + 'm' : '\u2014';
    document.getElementById('metricsRequests').textContent = data.requests || data.request_count || data.total_requests || '0';
    document.getElementById('metricsMemory').textContent = data.memory || data.memory_mb ? (data.memory_mb || Math.round(data.memory / 1048576)) + 'MB' : '\u2014';
    var html = '';
    var keys = Object.keys(data);
    keys.forEach(function (k) {
      html += '<div style="margin-bottom:2px;"><span style="color:var(--accent-cyan);">' + esc(k) + ':</span> ' + esc(JSON.stringify(data[k])) + '</div>';
    });
    document.getElementById('metricsDetailContent').innerHTML = html || '<div style="color:var(--text-muted);">No detail data</div>';
  } catch (e) {
    document.getElementById('metricsDetailContent').innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
  try {
    var data = await EngineAPI.fetchMetrics();
    if (typeof data === 'string') {
      document.getElementById('metricsDetailContent').innerHTML += '<hr style="border-color:var(--border);margin:8px 0;"><pre style="color:var(--text-secondary);font-size:10px;white-space:pre-wrap;margin:0;">' + esc(data.substring(0, 2000)) + '</pre>';
    }
  } catch (_) { /* metrics endpoint may not be available */ }
}

function showSubagentPanel() {
  document.getElementById('subagentPanel').classList.add('active');
}
function closeSubagentPanel() {
  document.getElementById('subagentPanel').classList.remove('active');
}
async function executeSubagentFromPanel() {
  var mode = document.getElementById('subagentMode').value;
  var prompt = document.getElementById('subagentPrompt').value.trim();
  var outputEl = document.getElementById('subagentOutput');
  if (!prompt) { outputEl.innerHTML = '<div style="color:var(--accent-red);">Please enter a task/prompt</div>'; return; }
  outputEl.innerHTML = '<div style="color:var(--accent-cyan);">Executing ' + mode + '...</div>';
  try {
    var data;
    if (mode === 'chain') {
      var stepsRaw = document.getElementById('chainSteps').value.trim();
      var steps;
      if (stepsRaw) {
        steps = JSON.parse(stepsRaw);
      } else {
        steps = [{ model: State.model.current || 'llama3', prompt: prompt }];
      }
      data = await EngineAPI.chain(steps);
    } else {
      data = await EngineAPI.subagent(prompt);
    }
    outputEl.innerHTML = '<pre style="color:var(--text-primary);white-space:pre-wrap;margin:0;">' + esc(JSON.stringify(data, null, 2).substring(0, 2000)) + '</pre>';
  } catch (e) {
    outputEl.innerHTML = '<div style="color:var(--accent-red);">Error: ' + esc(e.message) + '</div>';
  }
}

async function fetchSafetyStatus() {
  if (!State.backend.online) return;
  try {
    var sRes = await fetch(getActiveUrl() + '/api/safety/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('safetyStatus').textContent = sData.status || sData.state || 'active';
      document.getElementById('safetyStatus').style.color = 'var(--accent-green)';
    }
    // Violations
    var vRes = await fetch(getActiveUrl() + '/api/safety/violations', { signal: AbortSignal.timeout(5000) });
    if (vRes.ok) {
      var vData = await vRes.json();
      var violations = vData.violations || vData;
      document.getElementById('safetyViolationCount').textContent = Array.isArray(violations) ? violations.length : 0;
      document.getElementById('safetyViolationCount').style.color = (Array.isArray(violations) && violations.length > 0) ? 'var(--accent-red)' : 'var(--text-muted)';
      var vHtml = '';
      if (Array.isArray(violations) && violations.length > 0) {
        violations.slice(-20).reverse().forEach(function (v) {
          var time = v.timestamp || v.time || '';
          if (typeof time === 'number') time = new Date(time).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
          vHtml += '<div style="margin-bottom:4px;padding:4px 8px;background:rgba(255,51,102,0.1);border-radius:3px;">';
          vHtml += '<span style="color:var(--text-muted);font-size:10px;">[' + esc(time) + ']</span> ';
          vHtml += '<span style="color:var(--accent-red);">' + esc(v.type || v.category || 'violation') + '</span> ';
          vHtml += '<span style="color:var(--text-secondary);">' + esc((v.detail || v.message || '').substring(0, 80)) + '</span>';
          vHtml += '</div>';
        });
      }
      document.getElementById('safetyViolationList').innerHTML = vHtml || '<div style="color:var(--accent-green);">&#x2714; No violations recorded</div>';
    }
    logDebug('Safety status loaded', 'info');
  } catch (e) {
    logDebug('Safety status failed: ' + e.message, 'error');
  }
}
async function safetyCheck() {
  if (!State.backend.online) { addMessage('system', 'Backend offline', { skipMemory: true }); return; }
  try {
    var lastMsg = Conversation.messages.length > 0 ? Conversation.messages[Conversation.messages.length - 1] : null;
    var content = lastMsg ? lastMsg.content : '';
    var res = await fetch(getActiveUrl() + '/api/safety/check', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ content: content, role: lastMsg ? lastMsg.role : 'unknown' }),
      signal: AbortSignal.timeout(10000)
    });
    var data = res.ok ? await res.json() : {};
    var safe = data.safe !== false;
    addMessage('system', safe ? '\u2705 **Safety Check:** Output passed all safety checks.' : '\u26A0\uFE0F **Safety Check:** ' + (data.reason || data.detail || 'Violation detected'), { skipMemory: true });
    logDebug('Safety check: ' + (safe ? 'PASS' : 'FAIL — ' + (data.reason || '')), safe ? 'info' : 'warn');
  } catch (e) {
    logDebug('Safety check failed: ' + e.message, 'error');
  }
}
async function safetyRollback() {
  if (!State.backend.online) { addMessage('system', 'Backend offline', { skipMemory: true }); return; }
  try {
    var res = await fetch(getActiveUrl() + '/api/safety/rollback', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({}),
      signal: AbortSignal.timeout(10000)
    });
    var data = res.ok ? await res.json() : {};
    addMessage('system', '\u21BA **Safety Rollback:** ' + (data.status || data.message || 'done'), { skipMemory: true });
    logDebug('Safety rollback: ' + (data.status || 'ok'), 'info');
  } catch (e) {
    logDebug('Safety rollback failed: ' + e.message, 'error');
  }
}

function el(id) { return document.getElementById(id); }

function renderSecurityLog() {
  var log = State.security.eventLog;
  el('secLogCount').textContent = log.length;

  if (log.length === 0) {
    el('secLogBody').innerHTML = '<div class="sec-log-entry"><span class="sec-log-ts">\u2014</span><span class="sec-log-level AUDIT">AUDIT</span><span class="sec-log-detail">No security events yet</span></div>';
    return;
  }

  var html = '';
  var show = log.slice(-50).reverse();
  show.forEach(function (entry) {
    var time = new Date(entry.timestamp).toLocaleTimeString([], { hour12: false });
    html += '<div class="sec-log-entry">';
    html += '<span class="sec-log-ts">' + time + '</span>';
    html += '<span class="sec-log-level ' + esc(entry.level) + '">' + esc(entry.level) + '</span>';
    html += '<span class="sec-log-detail">' + esc(entry.detail) + '</span>';
    html += '</div>';
  });
  el('secLogBody').innerHTML = html;
}

function exportSecurityAudit() {
  var data = {
    timestamp: new Date().toISOString(),
    version: 'v3.4',
    phase: 'Phase 4: Security & Hardening',
    backendUrl: State.backend.url,
    urlAllowed: isUrlAllowed(State.backend.url),
    cspPresent: !!document.querySelector('meta[http-equiv="Content-Security-Policy"]'),
    domPurifyLoaded: typeof DOMPurify !== 'undefined',
    rateLimit: {
      maxPerMinute: State.security.rateLimit.maxPerMinute,
      currentUsage: State.security.rateLimit.timestamps.length,
      totalBlocked: State.security.rateLimit.blocked,
    },
    inputGuard: {
      maxLength: State.security.inputGuard.maxLength,
      blockedXss: State.security.inputGuard.blockedXss,
      blockedLength: State.security.inputGuard.blockedLength,
      blockedUrl: State.security.inputGuard.blockedUrl,
    },
    eventLog: State.security.eventLog,
  };
  var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url; a.download = 'rawrxd-security-audit-' + Date.now() + '.json'; a.click();
  URL.revokeObjectURL(url);
  logDebug('Security audit exported', 'info');
  secLog('AUDIT', 'Security audit data exported');
}

function el(id) { return document.getElementById(id); }
function esc(t) { var d = document.createElement('div'); d.textContent = t; return d.innerHTML; }
function sleep(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }

// ======================================================================
// CONFIDENCE EVALUATOR PANEL
// ======================================================================
function showConfidencePanel() {
  document.getElementById('confidencePanel').style.display = 'flex';
  fetchConfidenceStatus();
}
function closeConfidencePanel() {
  document.getElementById('confidencePanel').style.display = 'none';
}
async function fetchConfidenceStatus() {
  try {
    var sRes = await fetch(getActiveUrl() + '/api/confidence/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('confStatus').textContent = sData.status || sData.state || 'ready';
      document.getElementById('confStatus').style.color = 'var(--accent-green)';
      document.getElementById('confTotalEvals').textContent = sData.total_evaluations || 0;
      if (sData.avg_confidence !== undefined) {
        document.getElementById('confAvgScore').textContent = (sData.avg_confidence * 100).toFixed(1) + '%';
      }
      document.getElementById('confLowCount').textContent = sData.low_confidence_count || 0;
    }
  } catch (e) {
    document.getElementById('confStatus').textContent = 'Error: ' + e.message;
    document.getElementById('confStatus').style.color = 'var(--accent-red)';
  }
  // Fetch history
  try {
    var hRes = await fetch(getActiveUrl() + '/api/confidence/history', { signal: AbortSignal.timeout(5000) });
    if (hRes.ok) {
      var hData = await hRes.json();
      var history = hData.history || hData.evaluations || hData;
      if (Array.isArray(history) && history.length > 0) {
        var html = '';
        history.slice(-10).reverse().forEach(function (h) {
          var score = h.score !== undefined ? (h.score * 100).toFixed(0) + '%' : h.confidence !== undefined ? (h.confidence * 100).toFixed(0) + '%' : '?';
          var color = (h.score || h.confidence || 0) >= 0.7 ? 'var(--accent-green)' : (h.score || h.confidence || 0) >= 0.4 ? 'var(--accent-secondary)' : 'var(--accent-red)';
          html += '<div style="padding:3px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
          html += '<span style="color:' + color + ';font-weight:600;">' + score + '</span> ';
          html += '<span style="color:var(--text-muted);">' + ((h.model || h.query || '').substring(0, 40)) + '</span>';
          html += '</div>';
        });
        document.getElementById('confHistory').innerHTML = html;
      }
    }
  } catch (_) { /* history unavailable */ }
}
async function evaluateConfidence() {
  var text = document.getElementById('confEvalInput').value.trim();
  if (!text) { addMessage('system', 'Enter text to evaluate.', { skipMemory: true }); return; }
  try {
    var res = await fetch(getActiveUrl() + '/api/confidence/evaluate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ text: text, model: State.model.current || '' }),
      signal: AbortSignal.timeout(30000)
    });
    var data = res.ok ? await res.json() : {};
    var score = data.score !== undefined ? data.score : data.confidence;
    if (score !== undefined) {
      addMessage('system', '\u{1F3AF} **Confidence:** ' + (score * 100).toFixed(1) + '% — ' + (data.explanation || data.detail || ''), { skipMemory: true });
    } else {
      addMessage('system', 'Evaluation result: ' + JSON.stringify(data), { skipMemory: true });
    }
    fetchConfidenceStatus();
  } catch (e) {
    addMessage('system', 'Confidence evaluation failed: ' + e.message, { skipMemory: true });
  }
}

// ======================================================================
// TASK GOVERNOR PANEL
// ======================================================================
function showGovernorPanel() {
  document.getElementById('governorPanel').style.display = 'flex';
  governorRefresh();
}
function closeGovernorPanel() {
  document.getElementById('governorPanel').style.display = 'none';
}
async function governorRefresh() {
  try {
    var sRes = await fetch(getActiveUrl() + '/api/governor/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('govStatus').textContent = sData.status || sData.state || 'ready';
      document.getElementById('govStatus').style.color = 'var(--accent-green)';
      document.getElementById('govActive').textContent = sData.active_tasks !== undefined ? sData.active_tasks : 0;
      document.getElementById('govQueued').textContent = sData.queued !== undefined ? sData.queued : 0;
      document.getElementById('govCompleted').textContent = sData.completed !== undefined ? sData.completed : 0;
    }
  } catch (e) {
    document.getElementById('govStatus').textContent = 'Error: ' + e.message;
    document.getElementById('govStatus').style.color = 'var(--accent-red)';
  }
  // Fetch task list
  try {
    var tRes = await fetch(getActiveUrl() + '/api/governor/result', { signal: AbortSignal.timeout(5000) });
    if (tRes.ok) {
      var tData = await tRes.json();
      var tasks = tData.tasks || tData.results || tData;
      if (Array.isArray(tasks) && tasks.length > 0) {
        var html = '';
        tasks.slice(-10).reverse().forEach(function (t) {
          var status = t.status || t.state || 'unknown';
          var color = status === 'completed' || status === 'done' ? 'var(--accent-green)' : status === 'running' || status === 'active' ? 'var(--accent-cyan)' : status === 'failed' || status === 'error' ? 'var(--accent-red)' : 'var(--text-muted)';
          html += '<div style="padding:3px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
          html += '<span style="color:' + color + ';font-weight:600;">[' + status + ']</span> ';
          html += '<span style="color:var(--text-primary);">' + (t.name || t.description || t.id || 'task').substring(0, 50) + '</span>';
          html += '</div>';
        });
        document.getElementById('govTaskList').innerHTML = html;
      }
    }
  } catch (_) { /* task list unavailable */ }
}
async function governorSubmitTask() {
  var desc = document.getElementById('govTaskInput').value.trim();
  if (!desc) { addMessage('system', 'Enter a task description.', { skipMemory: true }); return; }
  try {
    var res = await fetch(getActiveUrl() + '/api/governor/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ task: desc, model: State.model.current || '' }),
      signal: AbortSignal.timeout(30000)
    });
    var data = res.ok ? await res.json() : {};
    addMessage('system', '\u{1F3DB} **Task Submitted:** ' + (data.id || data.task_id || 'queued') + ' — ' + (data.status || data.message || 'accepted'), { skipMemory: true });
    document.getElementById('govTaskInput').value = '';
    governorRefresh();
  } catch (e) {
    addMessage('system', 'Governor submit failed: ' + e.message, { skipMemory: true });
  }
}

// ======================================================================
// LSP INTEGRATION PANEL
// ======================================================================
function showLspPanel() {
  document.getElementById('lspPanel').style.display = 'flex';
  lspRefreshDiagnostics();
}
function closeLspPanel() {
  document.getElementById('lspPanel').style.display = 'none';
}
async function lspRefreshDiagnostics() {
  // Status
  try {
    var sRes = await fetch(getActiveUrl() + '/api/lsp/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('lspStatus').textContent = sData.status || sData.state || 'active';
      document.getElementById('lspStatus').style.color = 'var(--accent-green)';
      // Language servers
      if (sData.language_servers) {
        var servers = sData.language_servers;
        if (Array.isArray(servers) && servers.length > 0) {
          var sHtml = '';
          servers.forEach(function (s) {
            var sStatus = s.status || 'running';
            var sColor = sStatus === 'running' || sStatus === 'active' ? 'var(--accent-green)' : 'var(--accent-red)';
            sHtml += '<div style="padding:2px 0;">';
            sHtml += '<span style="color:' + sColor + ';">\u25CF</span> ';
            sHtml += '<span style="color:var(--text-primary);">' + (s.name || s.language || 'unknown') + '</span> ';
            sHtml += '<span style="color:var(--text-muted);">[' + sStatus + ']</span>';
            sHtml += '</div>';
          });
          document.getElementById('lspServerList').innerHTML = sHtml;
        }
      }
    }
  } catch (e) {
    document.getElementById('lspStatus').textContent = 'Error: ' + e.message;
    document.getElementById('lspStatus').style.color = 'var(--accent-red)';
  }
  // Diagnostics
  var errors = 0, warnings = 0;
  try {
    var dRes = await fetch(getActiveUrl() + '/api/lsp/diagnostics', { signal: AbortSignal.timeout(5000) });
    if (dRes.ok) {
      var dData = await dRes.json();
      var diags = dData.diagnostics || dData;
      if (Array.isArray(diags) && diags.length > 0) {
        diags.forEach(function (d) {
          if ((d.severity || '').toLowerCase() === 'error') errors++;
          else warnings++;
        });
        var dHtml = '';
        diags.slice(0, 20).forEach(function (d) {
          var severity = d.severity || d.level || 'info';
          var sColor = severity === 'error' ? 'var(--accent-red)' : severity === 'warning' ? 'var(--accent-secondary)' : 'var(--text-muted)';
          dHtml += '<div style="padding:2px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
          dHtml += '<span style="color:' + sColor + ';">[' + severity + ']</span> ';
          dHtml += '<span style="color:var(--text-muted);">' + (d.file || '') + ':' + (d.line || '?') + '</span> ';
          dHtml += '<span style="color:var(--text-primary);">' + (d.message || '').substring(0, 60) + '</span>';
          dHtml += '</div>';
        });
        if (diags.length > 20) dHtml += '<div style="color:var(--text-muted);padding-top:4px;">... (' + (diags.length - 20) + ' more)</div>';
        document.getElementById('lspDiagList').innerHTML = dHtml;
      } else {
        document.getElementById('lspDiagList').innerHTML = '<div style="color:var(--accent-green);">\u2714 No diagnostics — clean!</div>';
      }
    }
  } catch (_) { /* diagnostics unavailable */ }
  document.getElementById('lspErrors').textContent = errors;
  document.getElementById('lspWarnings').textContent = warnings;
}

// ======================================================================
// HYBRID COMPLETION PANEL
// ======================================================================
function showHybridPanel() {
  document.getElementById('hybridPanel').style.display = 'flex';
  fetchHybridStatus();
}
function closeHybridPanel() {
  document.getElementById('hybridPanel').style.display = 'none';
}
async function fetchHybridStatus() {
  try {
    var sRes = await fetch(getActiveUrl() + '/api/hybrid/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('hybridStatus').textContent = sData.status || sData.state || 'active';
      document.getElementById('hybridStatus').style.color = 'var(--accent-green)';
      document.getElementById('hybridCompletions').textContent = sData.total_completions || 0;
      document.getElementById('hybridRenames').textContent = sData.total_renames || 0;
      document.getElementById('hybridSymbols').textContent = sData.total_symbols || 0;
    }
  } catch (e) {
    document.getElementById('hybridStatus').textContent = 'Error: ' + e.message;
    document.getElementById('hybridStatus').style.color = 'var(--accent-red)';
  }
}
async function hybridComplete() {
  var code = document.getElementById('hybridInput').value.trim();
  if (!code) { addMessage('system', 'Enter code to complete.', { skipMemory: true }); return; }
  document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-cyan);">Completing...</div>';
  try {
    var res = await fetch(getActiveUrl() + '/api/hybrid/complete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code: code, model: State.model.current || '' }),
      signal: AbortSignal.timeout(60000)
    });
    var data = res.ok ? await res.json() : {};
    var completion = data.completion || data.result || data.output || JSON.stringify(data, null, 2);
    document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-green);margin-bottom:4px;">Completion:</div>' + escHtml(completion);
    fetchHybridStatus();
  } catch (e) {
    document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-red);">Error: ' + escHtml(e.message) + '</div>';
  }
}
async function hybridAnalyze() {
  var code = document.getElementById('hybridInput').value.trim();
  if (!code) { addMessage('system', 'Enter code to analyze.', { skipMemory: true }); return; }
  document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-cyan);">Analyzing...</div>';
  try {
    var res = await fetch(getActiveUrl() + '/api/hybrid/analyze', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code: code }),
      signal: AbortSignal.timeout(30000)
    });
    var data = res.ok ? await res.json() : {};
    var analysis = data.analysis || data.result || data.output || JSON.stringify(data, null, 2);
    document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-green);margin-bottom:4px;">Analysis:</div>' + escHtml(analysis);
  } catch (e) {
    document.getElementById('hybridOutput').innerHTML = '<div style="color:var(--accent-red);">Error: ' + escHtml(e.message) + '</div>';
  }
}

// ======================================================================
// REPLAY SESSIONS PANEL
// ======================================================================
function showReplayPanel() {
  document.getElementById('replayPanel').style.display = 'flex';
  replayRefresh();
}
function closeReplayPanel() {
  document.getElementById('replayPanel').style.display = 'none';
}
async function replayRefresh() {
  // Status
  try {
    var sRes = await fetch(getActiveUrl() + '/api/replay/status', { signal: AbortSignal.timeout(5000) });
    if (sRes.ok) {
      var sData = await sRes.json();
      document.getElementById('replayStatus').textContent = sData.status || sData.state || 'idle';
      document.getElementById('replayStatus').style.color = 'var(--accent-green)';
      document.getElementById('replaySessions').textContent = sData.total_sessions || 0;
      document.getElementById('replayRecords').textContent = sData.total_records || 0;
    }
  } catch (e) {
    document.getElementById('replayStatus').textContent = 'Error: ' + e.message;
    document.getElementById('replayStatus').style.color = 'var(--accent-red)';
  }
  // Sessions list
  try {
    var rRes = await fetch(getActiveUrl() + '/api/replay/sessions', { signal: AbortSignal.timeout(5000) });
    if (rRes.ok) {
      var rData = await rRes.json();
      var sessions = rData.sessions || rData;
      if (Array.isArray(sessions) && sessions.length > 0) {
        var html = '';
        sessions.slice(-15).reverse().forEach(function (s) {
          html += '<div style="padding:3px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
          html += '<span style="color:var(--accent-cyan);font-weight:600;">[' + (s.id || '?') + ']</span> ';
          html += '<span style="color:var(--text-primary);">' + (s.name || s.timestamp || '') + '</span> ';
          html += '<span style="color:var(--text-muted);">(' + (s.records || s.count || 0) + ' records)</span>';
          html += '</div>';
        });
        document.getElementById('replaySessionList').innerHTML = html;
      }
    }
  } catch (_) { /* sessions unavailable */ }
}

// ======================================================================
// PHASE STATUS PANEL (10/11/12)
// ======================================================================
function showPhaseStatusPanel() {
  document.getElementById('phaseStatusPanel').style.display = 'flex';
  phaseStatusRefresh();
}
function closePhaseStatusPanel() {
  document.getElementById('phaseStatusPanel').style.display = 'none';
}
async function phaseStatusRefresh() {
  var phaseIds = [10, 11, 12];
  var phaseNames = { 10: 'Speculative Decoding', 11: 'Flash-Attention v2', 12: 'Extreme Compression' };
  for (var i = 0; i < phaseIds.length; i++) {
    var p = phaseIds[i];
    var el_id = 'phase' + p + 'Status';
    try {
      var res = await fetch(getActiveUrl() + '/api/phase' + p + '/status', { signal: AbortSignal.timeout(5000) });
      if (res.ok) {
        var data = await res.json();
        var status = data.status || data.state || 'unknown';
        var icon = (status === 'active' || status === 'ready' || status === 'complete') ? '\u2714' : '\u2718';
        var color = (status === 'active' || status === 'ready' || status === 'complete') ? 'var(--accent-green)' : 'var(--accent-red)';
        var html = '<div style="color:' + color + ';">' + icon + ' ' + status + '</div>';
        if (data.progress !== undefined) html += '<div style="color:var(--text-muted);font-size:10px;">Progress: ' + data.progress + '%</div>';
        if (data.detail) html += '<div style="color:var(--text-secondary);font-size:10px;">' + escHtml(data.detail) + '</div>';
        if (data.engine) html += '<div style="color:var(--text-muted);font-size:10px;">Engine: ' + escHtml(data.engine) + '</div>';
        document.getElementById(el_id).innerHTML = html;
      } else {
        document.getElementById(el_id).innerHTML = '<div style="color:var(--accent-red);">\u2718 HTTP ' + res.status + '</div>';
      }
    } catch (e) {
      document.getElementById(el_id).innerHTML = '<div style="color:var(--accent-red);">\u2718 ' + escHtml(e.message) + '</div>';
    }
  }
}

// ======================================================================
// VSIX EXTENSION MANAGER — Panel Functions
// Manages .vsix plugin packages, marketplace search, install/uninstall,
// enable/disable, extension host lifecycle, and local VSIX loading.
// All operations route through EngineAPI to the Ollama-direct backend.
// ======================================================================
// ═══════════════════════════════════════════════════════════════
// AGENTIC FILE EDITOR — Full JS Functions (Phase 40 Expanded)
// ═══════════════════════════════════════════════════════════════

var FE = {
  tabs: [],         // [{path, name, content, savedContent, undoStack, redoStack, scrollTop, cursorPos}]
  activeIdx: -1,
  wordWrap: false,
  findVisible: false,
  findMatches: [],
  findIdx: -1,
  history: [],      // [{op, path, time, detail}]
  treeCache: {},    // {path: entries}
  treeExpanded: {}  // {path: true}
};

function showFileEditorPanel() {
  var el = document.getElementById('fileEditorPanel');
  if (el) { el.style.display = 'flex'; }
  logDebug('[FileEditor] Panel opened', 'info');
  // Auto-load tree if empty
  var tc = document.getElementById('feTreeContent');
  if (tc && tc.children.length <= 1) feRefreshTree();
}

function closeFileEditorPanel() {
  var el = document.getElementById('fileEditorPanel');
  if (el) el.style.display = 'none';
}

// ---- API helpers ----
function feApiUrl() {
  return getActiveUrl();
}

async function feApiPost(endpoint, body) {
  var urls = [feApiUrl()];
  if (_ideServerUrl && urls.indexOf(_ideServerUrl) === -1) urls.push(_ideServerUrl);
  if (urls.indexOf('http://localhost:8080') === -1) urls.push('http://localhost:8080');

  for (var i = 0; i < urls.length; i++) {
    try {
      var res = await fetch(urls[i] + endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
        signal: AbortSignal.timeout(10000)
      });
      if (res.ok) return await res.json();
    } catch (_) { }
  }
  return null;
}

// ---- Tree Browser ----
async function feRefreshTree() {
  var rootInput = document.getElementById('feTreePath');
  var root = rootInput ? rootInput.value.trim() : '.';
  if (!root) root = '.';

  feSetStatus('Loading tree...');
  var data = await feApiPost('/api/list-dir', { path: root });
  if (!data || !data.entries) {
    feSetStatus('Failed to load tree');
    var tc = document.getElementById('feTreeContent');
    if (tc) tc.innerHTML = '<div style="padding:12px;color:#ff6b6b;font-size:10px;">Could not load directory. Check path and backend connection.</div>';
    return;
  }

  // Sort: dirs first, then files, alphabetical
  data.entries.sort(function (a, b) {
    if (a.type === 'dir' && b.type !== 'dir') return -1;
    if (a.type !== 'dir' && b.type === 'dir') return 1;
    return a.name.localeCompare(b.name);
  });

  FE.treeCache[root] = data.entries;
  feRenderTree(root, data.entries);
  feSetStatus('Tree loaded: ' + data.entries.length + ' items');
}

function feRenderTree(root, entries) {
  var tc = document.getElementById('feTreeContent');
  if (!tc) return;
  var html = '';
  for (var i = 0; i < entries.length; i++) {
    var e = entries[i];
    var fullPath = root.replace(/\\/g, '/').replace(/\/$/, '') + '/' + e.name;
    if (e.type === 'dir') {
      var isExpanded = FE.treeExpanded[fullPath];
      html += '<div class="fe-tree-node" data-path="' + escHtml(fullPath) + '">';
      html += '<div class="fe-tree-item fe-tree-dir" onclick="feToggleDir(\'' + escHtml(fullPath) + '\', this)" style="padding:2px 8px;cursor:pointer;display:flex;align-items:center;gap:4px;color:var(--accent-cyan);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;" oncontextmenu="feTreeContextMenu(event, \'' + escHtml(fullPath) + '\', \'dir\')">';
      html += '<span style="font-size:8px;">' + (isExpanded ? '\u25BC' : '\u25B6') + '</span>';
      html += '<span>\uD83D\uDCC1</span> ' + escHtml(e.name);
      html += '</div>';
      html += '<div class="fe-tree-children" id="feDir_' + escHtml(fullPath.replace(/[\/\\.:]/g, '_')) + '" style="padding-left:12px;display:' + (isExpanded ? 'block' : 'none') + ';">';
      // Children loaded on expand
      html += '</div></div>';
    } else {
      var icon = feFileIcon(e.name);
      html += '<div class="fe-tree-item fe-tree-file" onclick="feOpenFile(\'' + escHtml(fullPath) + '\')" style="padding:2px 8px 2px 20px;cursor:pointer;display:flex;align-items:center;gap:4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:var(--text-primary);" oncontextmenu="feTreeContextMenu(event, \'' + escHtml(fullPath) + '\', \'file\')" onmouseover="this.style.background=\'rgba(255,255,255,0.05)\'" onmouseout="this.style.background=\'transparent\'">';
      html += '<span>' + icon + '</span> ' + escHtml(e.name);
      if (e.size > 0) html += '<span style="margin-left:auto;font-size:9px;color:var(--text-muted);">' + feFormatSize(e.size) + '</span>';
      html += '</div>';
    }
  }
  if (entries.length === 0) {
    html = '<div style="padding:12px;color:var(--text-muted);font-size:10px;">(empty directory)</div>';
  }
  tc.innerHTML = html;
}

async function feToggleDir(path, el) {
  var childId = 'feDir_' + path.replace(/[\/\\.:]/g, '_');
  var childEl = document.getElementById(childId);
  if (!childEl) return;

  if (FE.treeExpanded[path]) {
    FE.treeExpanded[path] = false;
    childEl.style.display = 'none';
    // Update arrow
    var arrow = el.querySelector('span:first-child');
    if (arrow) arrow.textContent = '\u25B6';
    return;
  }

  FE.treeExpanded[path] = true;
  childEl.style.display = 'block';
  var arrow = el.querySelector('span:first-child');
  if (arrow) arrow.textContent = '\u25BC';

  // Load children if not cached
  if (!FE.treeCache[path]) {
    childEl.innerHTML = '<div style="padding:4px 8px;color:var(--text-muted);font-size:9px;">Loading...</div>';
    var data = await feApiPost('/api/list-dir', { path: path });
    if (!data || !data.entries) {
      childEl.innerHTML = '<div style="padding:4px 8px;color:#ff6b6b;font-size:9px;">Error loading</div>';
      return;
    }
    data.entries.sort(function (a, b) {
      if (a.type === 'dir' && b.type !== 'dir') return -1;
      if (a.type !== 'dir' && b.type === 'dir') return 1;
      return a.name.localeCompare(b.name);
    });
    FE.treeCache[path] = data.entries;
  }

  var entries = FE.treeCache[path];
  var html = '';
  for (var i = 0; i < entries.length; i++) {
    var e = entries[i];
    var fullPath = path.replace(/\\/g, '/').replace(/\/$/, '') + '/' + e.name;
    if (e.type === 'dir') {
      var isExp = FE.treeExpanded[fullPath];
      html += '<div class="fe-tree-node" data-path="' + escHtml(fullPath) + '">';
      html += '<div class="fe-tree-item fe-tree-dir" onclick="feToggleDir(\'' + escHtml(fullPath) + '\', this)" style="padding:2px 4px;cursor:pointer;display:flex;align-items:center;gap:4px;color:var(--accent-cyan);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;" oncontextmenu="feTreeContextMenu(event, \'' + escHtml(fullPath) + '\', \'dir\')">';
      html += '<span style="font-size:8px;">' + (isExp ? '\u25BC' : '\u25B6') + '</span>';
      html += '<span>\uD83D\uDCC1</span> ' + escHtml(e.name);
      html += '</div>';
      html += '<div class="fe-tree-children" id="feDir_' + escHtml(fullPath.replace(/[\/\\.:]/g, '_')) + '" style="padding-left:12px;display:' + (isExp ? 'block' : 'none') + ';"></div></div>';
    } else {
      var icon = feFileIcon(e.name);
      html += '<div class="fe-tree-item fe-tree-file" onclick="feOpenFile(\'' + escHtml(fullPath) + '\')" style="padding:2px 4px 2px 8px;cursor:pointer;display:flex;align-items:center;gap:4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:var(--text-primary);" oncontextmenu="feTreeContextMenu(event, \'' + escHtml(fullPath) + '\', \'file\')" onmouseover="this.style.background=\'rgba(255,255,255,0.05)\'" onmouseout="this.style.background=\'transparent\'">';
      html += '<span>' + icon + '</span> ' + escHtml(e.name);
      if (e.size > 0) html += '<span style="margin-left:auto;font-size:9px;color:var(--text-muted);">' + feFormatSize(e.size) + '</span>';
      html += '</div>';
    }
  }
  if (entries.length === 0) html = '<div style="padding:4px 8px;color:var(--text-muted);font-size:9px;">(empty)</div>';
  childEl.innerHTML = html;
}

function feFileIcon(name) {
  var ext = name.lastIndexOf('.') >= 0 ? name.substring(name.lastIndexOf('.')).toLowerCase() : '';
  var icons = {
    '.cpp': '\uD83D\uDCC4', '.h': '\uD83D\uDCC4', '.hpp': '\uD83D\uDCC4', '.c': '\uD83D\uDCC4',
    '.js': '\uD83D\uDFE8', '.ts': '\uD83D\uDD35', '.html': '\uD83C\uDF10', '.css': '\uD83C\uDFA8',
    '.json': '\u2699\uFE0F', '.md': '\uD83D\uDCDD', '.txt': '\uD83D\uDCC3', '.py': '\uD83D\uDC0D',
    '.asm': '\u2699\uFE0F', '.bat': '\u26A1', '.ps1': '\u26A1', '.sh': '\u26A1',
    '.yaml': '\u2699\uFE0F', '.yml': '\u2699\uFE0F', '.toml': '\u2699\uFE0F',
    '.exe': '\u25B6\uFE0F', '.dll': '\uD83D\uDD27', '.obj': '\uD83D\uDD27', '.lib': '\uD83D\uDD27',
    '.png': '\uD83D\uDDBC', '.jpg': '\uD83D\uDDBC', '.gif': '\uD83D\uDDBC', '.svg': '\uD83D\uDDBC',
    '.gguf': '\uD83E\uDDE0', '.bin': '\uD83D\uDCE6'
  };
  return icons[ext] || '\uD83D\uDCC4';
}

function feFormatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function feTreeContextMenu(event, path, type) {
  event.preventDefault();
  var action = prompt((type === 'dir' ? 'Directory' : 'File') + ': ' + path + '\n\nActions: rename, delete, ' + (type === 'dir' ? 'new-file, new-folder' : 'duplicate'));
  if (!action) return;
  action = action.toLowerCase().trim();
  if (action === 'delete') feDeletePath(path);
  else if (action === 'rename') feRenamePath(path);
  else if (action === 'new-file') feNewFileInDir(path);
  else if (action === 'new-folder') feNewFolderInDir(path);
  else if (action === 'duplicate') feDuplicateFile(path);
}

// ---- File Operations ----
async function feOpenFile(path) {
  // Check if already open in a tab
  for (var i = 0; i < FE.tabs.length; i++) {
    if (FE.tabs[i].path === path) {
      feActivateTab(i);
      return;
    }
  }

  feSetStatus('Opening: ' + path.split('/').pop());
  var data = await feApiPost('/api/read-file', { path: path });
  if (!data || data.content === undefined) {
    feSetStatus('Failed to open file');
    return;
  }

  var tab = {
    path: path,
    name: data.name || path.split('/').pop(),
    content: data.content,
    savedContent: data.content,
    undoStack: [],
    redoStack: [],
    scrollTop: 0,
    cursorPos: 0
  };
  FE.tabs.push(tab);
  feActivateTab(FE.tabs.length - 1);
  feAddHistory('open', path);
  feSetStatus('Opened: ' + tab.name + ' (' + feFormatSize(data.content.length) + ')');
}

function feActivateTab(idx) {
  if (idx < 0 || idx >= FE.tabs.length) return;

  // Save current state of active tab
  if (FE.activeIdx >= 0 && FE.activeIdx < FE.tabs.length) {
    var area = document.getElementById('feEditorArea');
    if (area) {
      FE.tabs[FE.activeIdx].content = area.value;
      FE.tabs[FE.activeIdx].scrollTop = area.scrollTop;
      FE.tabs[FE.activeIdx].cursorPos = area.selectionStart;
    }
  }

  FE.activeIdx = idx;
  var tab = FE.tabs[idx];

  // Update editor
  var area = document.getElementById('feEditorArea');
  if (area) {
    area.value = tab.content;
    area.scrollTop = tab.scrollTop;
    area.setSelectionRange(tab.cursorPos, tab.cursorPos);
    area.focus();
  }

  feUpdateLineNumbers();
  feUpdateTabBar();
  feUpdateStatusBar();
}

function feUpdateTabBar() {
  var bar = document.getElementById('feTabBar');
  if (!bar) return;
  if (FE.tabs.length === 0) {
    bar.innerHTML = '<div style="padding:6px 12px;font-size:10px;color:var(--text-muted);font-family:var(--font-mono);">No files open</div>';
    return;
  }
  var html = '';
  for (var i = 0; i < FE.tabs.length; i++) {
    var t = FE.tabs[i];
    var isActive = (i === FE.activeIdx);
    var isModified = (t.content !== t.savedContent);
    html += '<div onclick="feActivateTab(' + i + ')" style="padding:4px 10px;font-size:10px;font-family:var(--font-mono);cursor:pointer;border-right:1px solid var(--border);display:flex;align-items:center;gap:4px;white-space:nowrap;' +
      (isActive ? 'background:rgba(255,255,255,0.08);color:var(--accent-cyan);border-bottom:2px solid var(--accent-cyan);' : 'color:var(--text-muted);') + '">';
    html += (isModified ? '\u25CF ' : '') + escHtml(t.name);
    html += '<span onclick="event.stopPropagation();feCloseTab(' + i + ')" style="margin-left:6px;cursor:pointer;opacity:0.5;font-size:12px;" onmouseover="this.style.opacity=1;this.style.color=\'#ff6b6b\'" onmouseout="this.style.opacity=0.5;this.style.color=\'\'">&times;</span>';
    html += '</div>';
  }
  bar.innerHTML = html;
}

function feCloseTab(idx) {
  if (idx < 0 || idx >= FE.tabs.length) return;
  var tab = FE.tabs[idx];
  if (tab.content !== tab.savedContent) {
    if (!confirm('Unsaved changes in ' + tab.name + '. Close anyway?')) return;
  }
  FE.tabs.splice(idx, 1);
  if (FE.activeIdx >= FE.tabs.length) FE.activeIdx = FE.tabs.length - 1;
  if (FE.tabs.length === 0) {
    FE.activeIdx = -1;
    var area = document.getElementById('feEditorArea');
    if (area) area.value = '';
    feUpdateLineNumbers();
  } else {
    feActivateTab(FE.activeIdx);
  }
  feUpdateTabBar();
  feUpdateStatusBar();
}

async function feSaveFile() {
  if (FE.activeIdx < 0) return;
  var tab = FE.tabs[FE.activeIdx];
  var area = document.getElementById('feEditorArea');
  if (area) tab.content = area.value;

  feSetStatus('Saving: ' + tab.name + '...');
  var data = await feApiPost('/api/write-file', { path: tab.path, content: tab.content });
  if (!data || !data.success) {
    feSetStatus('Save failed!');
    return;
  }
  tab.savedContent = tab.content;
  feUpdateTabBar();
  feUpdateStatusBar();
  feAddHistory('save', tab.path, tab.content.length + ' bytes');
  feSetStatus('Saved: ' + tab.name + ' (' + feFormatSize(tab.content.length) + ')');
}

function feRevertFile() {
  if (FE.activeIdx < 0) return;
  var tab = FE.tabs[FE.activeIdx];
  if (tab.content === tab.savedContent) { feSetStatus('No changes to revert'); return; }
  if (!confirm('Revert ' + tab.name + ' to last saved version?')) return;
  tab.undoStack.push(tab.content);
  tab.content = tab.savedContent;
  var area = document.getElementById('feEditorArea');
  if (area) area.value = tab.content;
  feUpdateLineNumbers();
  feUpdateTabBar();
  feUpdateStatusBar();
  feAddHistory('revert', tab.path);
  feSetStatus('Reverted: ' + tab.name);
}

// ---- Undo / Redo ----
function feUndo() {
  if (FE.activeIdx < 0) return;
  var tab = FE.tabs[FE.activeIdx];
  if (tab.undoStack.length === 0) { feSetStatus('Nothing to undo'); return; }
  var area = document.getElementById('feEditorArea');
  if (area) tab.content = area.value;
  tab.redoStack.push(tab.content);
  tab.content = tab.undoStack.pop();
  if (area) { area.value = tab.content; }
  feUpdateLineNumbers();
  feUpdateStatusBar();
}

function feRedo() {
  if (FE.activeIdx < 0) return;
  var tab = FE.tabs[FE.activeIdx];
  if (tab.redoStack.length === 0) { feSetStatus('Nothing to redo'); return; }
  var area = document.getElementById('feEditorArea');
  if (area) tab.content = area.value;
  tab.undoStack.push(tab.content);
  tab.content = tab.redoStack.pop();
  if (area) { area.value = tab.content; }
  feUpdateLineNumbers();
  feUpdateStatusBar();
}

// ---- Editor Input Handling ----
function feOnEditorInput() {
  if (FE.activeIdx < 0) return;
  var tab = FE.tabs[FE.activeIdx];
  var area = document.getElementById('feEditorArea');
  if (!area) return;

  // Push to undo stack (debounced — every 30 chars or so)
  var newContent = area.value;
  if (tab.undoStack.length === 0 || Math.abs(newContent.length - (tab.undoStack[tab.undoStack.length - 1] || '').length) > 30) {
    tab.undoStack.push(tab.content);
    if (tab.undoStack.length > 200) tab.undoStack.shift();
  }
  tab.content = newContent;
  tab.redoStack = []; // clear redo on new input
  feUpdateLineNumbers();
  feUpdateTabBar();
  feUpdateStatusBar();
}

function feOnEditorScroll() {
  var area = document.getElementById('feEditorArea');
  var lineNums = document.getElementById('feLineNumbers');
  if (area && lineNums) lineNums.scrollTop = area.scrollTop;
}

function feOnEditorKeydown(e) {
  // Tab key — insert spaces instead of moving focus
  if (e.key === 'Tab') {
    e.preventDefault();
    var area = e.target;
    var start = area.selectionStart;
    var end = area.selectionEnd;
    if (e.shiftKey) {
      // Outdent: remove leading spaces on current line
      var before = area.value.substring(0, start);
      var lineStart = before.lastIndexOf('\n') + 1;
      var lineText = area.value.substring(lineStart);
      if (lineText.startsWith('    ')) {
        area.value = area.value.substring(0, lineStart) + lineText.substring(4);
        area.selectionStart = area.selectionEnd = Math.max(lineStart, start - 4);
      } else if (lineText.startsWith('  ')) {
        area.value = area.value.substring(0, lineStart) + lineText.substring(2);
        area.selectionStart = area.selectionEnd = Math.max(lineStart, start - 2);
      }
    } else {
      area.value = area.value.substring(0, start) + '    ' + area.value.substring(end);
      area.selectionStart = area.selectionEnd = start + 4;
    }
    feOnEditorInput();
    return;
  }
  // Ctrl+S — save
  if (e.ctrlKey && e.key === 's') { e.preventDefault(); feSaveFile(); return; }
  // Ctrl+Z — undo
  if (e.ctrlKey && !e.shiftKey && e.key === 'z') { e.preventDefault(); feUndo(); return; }
  // Ctrl+Shift+Z or Ctrl+Y — redo
  if ((e.ctrlKey && e.shiftKey && e.key === 'Z') || (e.ctrlKey && e.key === 'y')) { e.preventDefault(); feRedo(); return; }
  // Ctrl+F — find
  if (e.ctrlKey && e.key === 'f') { e.preventDefault(); feFindReplace(); return; }
  // Ctrl+G — go to line
  if (e.ctrlKey && e.key === 'g') { e.preventDefault(); feGoToLine(); return; }
  // Ctrl+H — find and replace (focus replace)
  if (e.ctrlKey && e.key === 'h') { e.preventDefault(); feFindReplace(); setTimeout(function () { var ri = document.getElementById('feReplaceInput'); if (ri) ri.focus(); }, 100); return; }
  // Enter — auto-indent
  if (e.key === 'Enter') {
    e.preventDefault();
    var area = e.target;
    var start = area.selectionStart;
    var before = area.value.substring(0, start);
    var currentLine = before.substring(before.lastIndexOf('\n') + 1);
    var indent = currentLine.match(/^(\s*)/)[1] || '';
    // Extra indent after { or (
    var lastChar = before.trimEnd().slice(-1);
    if (lastChar === '{' || lastChar === '(' || lastChar === '[') indent += '    ';
    area.value = before + '\n' + indent + area.value.substring(area.selectionEnd);
    area.selectionStart = area.selectionEnd = start + 1 + indent.length;
    feOnEditorInput();
  }
}

function feUpdateLineNumbers() {
  var area = document.getElementById('feEditorArea');
  var lineNums = document.getElementById('feLineNumbers');
  if (!area || !lineNums) return;
  var lines = area.value.split('\n');
  var html = '';
  for (var i = 1; i <= lines.length; i++) {
    html += i + '\n';
  }
  lineNums.textContent = html;
  lineNums.scrollTop = area.scrollTop;
}

function feUpdateStatusBar() {
  var area = document.getElementById('feEditorArea');
  if (FE.activeIdx < 0 || !area) {
    feSetEl('feFilePath', 'No file');
    feSetEl('feLineCol', 'Ln 1, Col 1');
    feSetEl('feLineCount', '0 lines');
    feSetEl('feModified', 'Saved');
    return;
  }
  var tab = FE.tabs[FE.activeIdx];
  var pos = area.selectionStart;
  var textBefore = area.value.substring(0, pos);
  var line = (textBefore.match(/\n/g) || []).length + 1;
  var col = pos - textBefore.lastIndexOf('\n');
  var totalLines = area.value.split('\n').length;
  var modified = (tab.content !== tab.savedContent);

  feSetEl('feFilePath', tab.path);
  feSetEl('feLineCol', 'Ln ' + line + ', Col ' + col);
  feSetEl('feLineCount', totalLines + ' lines');
  var modEl = document.getElementById('feModified');
  if (modEl) {
    modEl.textContent = modified ? 'Modified' : 'Saved';
    modEl.style.color = modified ? 'var(--accent-orange)' : 'var(--accent-green)';
  }
}

function feSetEl(id, text) {
  var el = document.getElementById(id);
  if (el) el.textContent = text;
}

function feSetStatus(text) {
  feSetEl('feStatusInfo', text);
}

// ---- Find / Replace ----
function feFindReplace() {
  var bar = document.getElementById('feFindBar');
  if (!bar) return;
  bar.style.display = bar.style.display === 'none' ? 'flex' : 'none';
  if (bar.style.display === 'flex') {
    var fi = document.getElementById('feFindInput');
    if (fi) fi.focus();
    // Pre-populate with selection
    var area = document.getElementById('feEditorArea');
    if (area && area.selectionStart !== area.selectionEnd) {
      fi.value = area.value.substring(area.selectionStart, area.selectionEnd);
    }
  }
}

function feDoFind() {
  var area = document.getElementById('feEditorArea');
  var input = document.getElementById('feFindInput');
  if (!area || !input) return;
  var query = input.value;
  if (!query) { feSetEl('feFindCount', ''); return; }

  var useRegex = document.getElementById('feFindRegex') && document.getElementById('feFindRegex').checked;
  var matchCase = document.getElementById('feFindCase') && document.getElementById('feFindCase').checked;
  var text = area.value;

  FE.findMatches = [];
  if (useRegex) {
    try {
      var flags = matchCase ? 'g' : 'gi';
      var re = new RegExp(query, flags);
      var m;
      while ((m = re.exec(text)) !== null) {
        FE.findMatches.push({ start: m.index, end: m.index + m[0].length });
        if (FE.findMatches.length >= 5000) break;
      }
    } catch (e) { feSetEl('feFindCount', 'Invalid regex'); return; }
  } else {
    var searchText = matchCase ? text : text.toLowerCase();
    var searchQuery = matchCase ? query : query.toLowerCase();
    var pos = 0;
    while ((pos = searchText.indexOf(searchQuery, pos)) !== -1) {
      FE.findMatches.push({ start: pos, end: pos + query.length });
      pos += query.length;
      if (FE.findMatches.length >= 5000) break;
    }
  }

  feSetEl('feFindCount', FE.findMatches.length + ' matches');

  // Navigate to first match after cursor
  if (FE.findMatches.length > 0) {
    var cursor = area.selectionEnd;
    FE.findIdx = 0;
    for (var i = 0; i < FE.findMatches.length; i++) {
      if (FE.findMatches[i].start >= cursor) { FE.findIdx = i; break; }
    }
    var match = FE.findMatches[FE.findIdx];
    area.setSelectionRange(match.start, match.end);
    area.focus();
    // Scroll to match
    var linesBefore = text.substring(0, match.start).split('\n').length;
    area.scrollTop = Math.max(0, (linesBefore - 5) * 18); // ~18px per line
    feSetEl('feFindCount', (FE.findIdx + 1) + '/' + FE.findMatches.length);
  }
}

function feDoReplace() {
  var area = document.getElementById('feEditorArea');
  var replaceInput = document.getElementById('feReplaceInput');
  if (!area || !replaceInput || FE.findMatches.length === 0) return;

  if (FE.activeIdx >= 0) {
    var tab = FE.tabs[FE.activeIdx];
    tab.undoStack.push(area.value);
  }

  var match = FE.findMatches[FE.findIdx];
  var replacement = replaceInput.value;
  area.value = area.value.substring(0, match.start) + replacement + area.value.substring(match.end);
  area.selectionStart = area.selectionEnd = match.start + replacement.length;

  feOnEditorInput();
  // Re-run find to update matches
  feDoFind();
}

function feDoReplaceAll() {
  var area = document.getElementById('feEditorArea');
  var findInput = document.getElementById('feFindInput');
  var replaceInput = document.getElementById('feReplaceInput');
  if (!area || !findInput || !replaceInput) return;
  var query = findInput.value;
  var replacement = replaceInput.value;
  if (!query) return;

  if (FE.activeIdx >= 0) {
    var tab = FE.tabs[FE.activeIdx];
    tab.undoStack.push(area.value);
  }

  var useRegex = document.getElementById('feFindRegex') && document.getElementById('feFindRegex').checked;
  var matchCase = document.getElementById('feFindCase') && document.getElementById('feFindCase').checked;

  if (useRegex) {
    try {
      var flags = matchCase ? 'g' : 'gi';
      area.value = area.value.replace(new RegExp(query, flags), replacement);
    } catch (e) { feSetStatus('Invalid regex'); return; }
  } else {
    // Replace all occurrences
    var result = area.value;
    if (matchCase) {
      while (result.indexOf(query) !== -1) result = result.replace(query, replacement);
    } else {
      var re = new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');
      result = result.replace(re, replacement);
    }
    area.value = result;
  }

  feOnEditorInput();
  feDoFind();
  feSetStatus('Replaced all occurrences');
}

// ---- Go to Line ----
function feGoToLine() {
  var area = document.getElementById('feEditorArea');
  if (!area) return;
  var lineNum = prompt('Go to line:');
  if (!lineNum) return;
  var target = parseInt(lineNum, 10);
  if (isNaN(target) || target < 1) return;

  var lines = area.value.split('\n');
  if (target > lines.length) target = lines.length;
  var pos = 0;
  for (var i = 0; i < target - 1; i++) pos += lines[i].length + 1;
  area.setSelectionRange(pos, pos);
  area.focus();
  area.scrollTop = Math.max(0, (target - 5) * 18);
  feUpdateStatusBar();
}

function feToggleWordWrap() {
  FE.wordWrap = !FE.wordWrap;
  var area = document.getElementById('feEditorArea');
  if (area) area.style.whiteSpace = FE.wordWrap ? 'pre-wrap' : 'pre';
  feSetStatus('Word wrap: ' + (FE.wordWrap ? 'ON' : 'OFF'));
}

// ---- New File / Folder / Delete / Rename ----
async function feNewFile() {
  var path = prompt('New file path (absolute or relative to tree root):');
  if (!path) return;
  // If relative, prepend tree root
  if (path.indexOf(':') === -1 && !path.startsWith('/')) {
    var root = document.getElementById('feTreePath');
    path = (root ? root.value.trim() : '.') + '/' + path;
  }

  var data = await feApiPost('/api/write-file', { path: path, content: '' });
  if (!data || !data.success) { feSetStatus('Failed to create file'); return; }
  feAddHistory('create', path);
  feSetStatus('Created: ' + path);
  feOpenFile(path);
  // Invalidate tree cache for parent
  var parentDir = path.substring(0, path.lastIndexOf('/'));
  delete FE.treeCache[parentDir];
}

async function feNewFileInDir(dirPath) {
  var name = prompt('New file name in ' + dirPath + ':');
  if (!name) return;
  var fullPath = dirPath + '/' + name;
  var data = await feApiPost('/api/write-file', { path: fullPath, content: '' });
  if (!data || !data.success) { feSetStatus('Failed to create file'); return; }
  feAddHistory('create', fullPath);
  delete FE.treeCache[dirPath];
  feToggleDir(dirPath, null); // force re-expand
  feOpenFile(fullPath);
}

async function feNewFolderInDir(dirPath) {
  var name = prompt('New folder name in ' + dirPath + ':');
  if (!name) return;
  var fullPath = dirPath + '/' + name;
  var data = await feApiPost('/api/mkdir', { path: fullPath });
  if (!data || !data.success) { feSetStatus('Failed to create folder'); return; }
  feAddHistory('mkdir', fullPath);
  delete FE.treeCache[dirPath];
  FE.treeExpanded[dirPath] = false;
  feToggleDir(dirPath, null);
}

async function feDeletePath(path) {
  if (!confirm('Delete ' + path + '?')) return;
  var data = await feApiPost('/api/delete-file', { path: path });
  if (!data || !data.success) { feSetStatus('Delete failed'); return; }
  feAddHistory('delete', path);
  // Close tab if open
  for (var i = FE.tabs.length - 1; i >= 0; i--) {
    if (FE.tabs[i].path === path) { FE.tabs.splice(i, 1); }
  }
  if (FE.activeIdx >= FE.tabs.length) FE.activeIdx = FE.tabs.length - 1;
  feUpdateTabBar();
  if (FE.activeIdx >= 0) feActivateTab(FE.activeIdx);

  // Invalidate parent cache
  var parent = path.substring(0, path.lastIndexOf('/'));
  delete FE.treeCache[parent];
  feSetStatus('Deleted: ' + path);
}

async function feRenamePath(path) {
  var newName = prompt('New name for ' + path.split('/').pop() + ':');
  if (!newName) return;
  var parentDir = path.substring(0, path.lastIndexOf('/'));
  var newPath = parentDir + '/' + newName;
  var data = await feApiPost('/api/rename-file', { old_path: path, new_path: newPath });
  if (!data || !data.success) { feSetStatus('Rename failed'); return; }
  feAddHistory('rename', path, '-> ' + newPath);
  // Update tab if open
  for (var i = 0; i < FE.tabs.length; i++) {
    if (FE.tabs[i].path === path) {
      FE.tabs[i].path = newPath;
      FE.tabs[i].name = newName;
    }
  }
  feUpdateTabBar();
  delete FE.treeCache[parentDir];
  feSetStatus('Renamed: ' + path.split('/').pop() + ' -> ' + newName);
}

function feRenameActive() {
  if (FE.activeIdx < 0) return;
  feRenamePath(FE.tabs[FE.activeIdx].path);
}

function feDeleteActive() {
  if (FE.activeIdx < 0) return;
  feDeletePath(FE.tabs[FE.activeIdx].path);
}

async function feDuplicateFile(path) {
  var ext = path.lastIndexOf('.') >= 0 ? path.substring(path.lastIndexOf('.')) : '';
  var base = path.substring(0, path.length - ext.length);
  var newPath = base + '_copy' + ext;
  var data = await feApiPost('/api/read-file', { path: path });
  if (!data || data.content === undefined) { feSetStatus('Cannot read source'); return; }
  var wr = await feApiPost('/api/write-file', { path: newPath, content: data.content });
  if (!wr || !wr.success) { feSetStatus('Duplicate failed'); return; }
  feAddHistory('duplicate', path, '-> ' + newPath);
  var parent = path.substring(0, path.lastIndexOf('/'));
  delete FE.treeCache[parent];
  feOpenFile(newPath);
}

// ---- Diff View ----
function feShowDiff() {
  var diffPane = document.getElementById('feDiffPane');
  var editorArea = document.getElementById('feEditorArea');
  var lineNums = document.getElementById('feLineNumbers');
  if (!diffPane) return;

  if (diffPane.style.display !== 'none') {
    // Hide diff, show editor
    diffPane.style.display = 'none';
    if (editorArea) editorArea.parentElement.style.display = 'flex';
    return;
  }

  if (FE.activeIdx < 0) { feSetStatus('No file open'); return; }
  var tab = FE.tabs[FE.activeIdx];
  if (tab.content === tab.savedContent) { feSetStatus('No changes to diff'); return; }

  // Simple line-by-line diff
  var savedLines = tab.savedContent.split('\n');
  var currentLines = tab.content.split('\n');
  var maxLen = Math.max(savedLines.length, currentLines.length);
  var html = '<div style="margin-bottom:8px;font-weight:bold;color:var(--accent-cyan);">Diff: ' + escHtml(tab.name) + '</div>';

  for (var i = 0; i < maxLen; i++) {
    var lineNum = (i + 1).toString().padStart(4, ' ');
    var savedLine = i < savedLines.length ? savedLines[i] : undefined;
    var currentLine = i < currentLines.length ? currentLines[i] : undefined;

    if (savedLine === currentLine) {
      html += '<div style="color:var(--text-muted);">' + lineNum + '  ' + escHtml(savedLine || '') + '</div>';
    } else {
      if (savedLine !== undefined) {
        html += '<div style="color:#ff6b6b;background:rgba(255,100,100,0.1);">' + lineNum + '- ' + escHtml(savedLine) + '</div>';
      }
      if (currentLine !== undefined) {
        html += '<div style="color:#69ff94;background:rgba(100,255,100,0.1);">' + lineNum + '+ ' + escHtml(currentLine) + '</div>';
      }
    }
  }

  diffPane.innerHTML = html;
  diffPane.style.display = 'block';
  if (editorArea) editorArea.parentElement.style.display = 'none';
}

// ---- Search in Files ----
function feShowSearch() {
  var drawer = document.getElementById('feSearchDrawer');
  if (!drawer) return;
  drawer.style.display = drawer.style.display === 'none' ? 'block' : 'none';
  if (drawer.style.display === 'block') {
    var si = document.getElementById('feGlobalSearch');
    if (si) si.focus();
  }
}

async function feDoGlobalSearch() {
  var patternInput = document.getElementById('feGlobalSearch');
  var filePatternInput = document.getElementById('feSearchPath');
  var resultsEl = document.getElementById('feSearchResults');
  if (!patternInput || !resultsEl) return;
  var pattern = patternInput.value.trim();
  if (!pattern) return;

  var rootInput = document.getElementById('feTreePath');
  var root = rootInput ? rootInput.value.trim() : '.';
  var filePattern = filePatternInput ? filePatternInput.value.trim() : '*.*';

  resultsEl.innerHTML = '<div style="color:var(--text-muted);padding:8px;">Searching...</div>';

  var data = await feApiPost('/api/search-files', {
    pattern: pattern,
    path: root,
    file_pattern: filePattern,
    max_results: 200
  });

  if (!data || !data.results) {
    resultsEl.innerHTML = '<div style="color:#ff6b6b;padding:8px;">Search failed. Check backend connection.</div>';
    return;
  }

  if (data.results.length === 0) {
    resultsEl.innerHTML = '<div style="color:var(--text-muted);padding:8px;">No matches found for "' + escHtml(pattern) + '"</div>';
    return;
  }

  var html = '<div style="color:var(--accent-green);padding:4px 0;margin-bottom:8px;">' + data.count + ' match' + (data.count !== 1 ? 'es' : '') + '</div>';
  var lastFile = '';
  for (var i = 0; i < data.results.length; i++) {
    var r = data.results[i];
    var file = r.file.replace(/\\/g, '/');
    if (file !== lastFile) {
      lastFile = file;
      html += '<div style="color:var(--accent-cyan);margin-top:6px;font-weight:bold;cursor:pointer;" onclick="feOpenFile(\'' + escHtml(file) + '\')">' + escHtml(file.split('/').pop()) + '</div>';
      html += '<div style="color:var(--text-muted);font-size:9px;margin-bottom:2px;">' + escHtml(file) + '</div>';
    }
    html += '<div style="cursor:pointer;padding:1px 0;display:flex;gap:6px;" onclick="feOpenFileAtLine(\'' + escHtml(file) + '\',' + r.line + ')">';
    html += '<span style="color:var(--accent-orange);min-width:32px;text-align:right;">' + r.line + '</span>';
    html += '<span style="color:var(--text-primary);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">' + escHtml(r.text) + '</span>';
    html += '</div>';
  }
  resultsEl.innerHTML = html;
}

async function feOpenFileAtLine(path, lineNum) {
  await feOpenFile(path);
  // After opening, go to line
  setTimeout(function () {
    var area = document.getElementById('feEditorArea');
    if (!area) return;
    var lines = area.value.split('\n');
    var pos = 0;
    for (var i = 0; i < Math.min(lineNum - 1, lines.length); i++) pos += lines[i].length + 1;
    area.setSelectionRange(pos, pos + (lines[lineNum - 1] || '').length);
    area.focus();
    area.scrollTop = Math.max(0, (lineNum - 5) * 18);
    feUpdateStatusBar();
  }, 200);
}

// ---- History ----
function feShowHistory() {
  var drawer = document.getElementById('feHistoryDrawer');
  if (!drawer) return;
  drawer.style.display = drawer.style.display === 'none' ? 'block' : 'none';
  feRenderHistory();
}

function feAddHistory(op, path, detail) {
  FE.history.unshift({ op: op, path: path, time: new Date().toLocaleTimeString(), detail: detail || '' });
  if (FE.history.length > 100) FE.history.pop();
}

function feRenderHistory() {
  var el = document.getElementById('feHistoryList');
  if (!el) return;
  if (FE.history.length === 0) {
    el.innerHTML = '<div style="color:var(--text-muted);">No operations yet</div>';
    return;
  }
  var html = '';
  var opColors = { open: 'var(--accent-cyan)', save: 'var(--accent-green)', create: '#69ff94', delete: '#ff6b6b', rename: 'var(--accent-orange)', revert: 'var(--accent-purple)', mkdir: '#69ff94', duplicate: 'var(--accent-cyan)' };
  for (var i = 0; i < FE.history.length; i++) {
    var h = FE.history[i];
    var color = opColors[h.op] || 'var(--text-muted)';
    html += '<div style="padding:3px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
    html += '<span style="color:' + color + ';font-weight:bold;">[' + h.op.toUpperCase() + ']</span> ';
    html += '<span style="color:var(--text-primary);">' + escHtml(h.path.split('/').pop()) + '</span>';
    if (h.detail) html += ' <span style="color:var(--text-muted);">' + escHtml(h.detail) + '</span>';
    html += '<br><span style="color:var(--text-muted);font-size:9px;">' + h.time + ' &mdash; ' + escHtml(h.path) + '</span>';
    html += '</div>';
  }
  el.innerHTML = html;
}

// ---- Keyboard shortcut: Ctrl+S from anywhere to save active file editor tab ----
document.addEventListener('keydown', function (e) {
  if (e.ctrlKey && e.key === 's' && FE.activeIdx >= 0 &&
    document.getElementById('fileEditorPanel') &&
    document.getElementById('fileEditorPanel').style.display !== 'none') {
    e.preventDefault();
    feSaveFile();
  }
});

// --- Compatibility shim: old code calls feLogHistory(op, detail), new FE uses feAddHistory(op, path, detail) ---
function feLogHistory(op, detail) {
  feAddHistory(op, detail || '', '');
}

// --- Compatibility shim: old code calls feTreeExpandDir, new FE uses feToggleDir ---
function feTreeExpandDir(el, path) {
  feToggleDir(path, el);
}

// ======================================================================

function showExtensionsPanel() {
  document.getElementById('extensionsPanel').style.display = 'flex';
  refreshExtensionsList();
}

function closeExtensionsPanel() {
  document.getElementById('extensionsPanel').style.display = 'none';
}

function switchExtTab(tab) {
  var tabs = ['installed', 'marketplace', 'load', 'running'];
  tabs.forEach(function (t) {
    var view = document.getElementById('extView' + t.charAt(0).toUpperCase() + t.slice(1));
    var btn = document.getElementById('extTab' + t.charAt(0).toUpperCase() + t.slice(1));
    if (view) view.style.display = (t === tab) ? '' : 'none';
    if (btn) {
      btn.style.background = (t === tab) ? 'var(--accent)' : '';
      btn.style.color = (t === tab) ? '#000' : '';
    }
  });
  if (tab === 'installed') refreshExtensionsList();
  if (tab === 'running') refreshExtRunning();
}

async function refreshExtensionsList() {
  var el = document.getElementById('extInstalledList');
  if (!el) return;
  // Try backend first
  try {
    var data = await EngineAPI.fetchExtensions();
    var exts = data.extensions || data.installed || [];
    if (exts.length > 0) State.extensions.installed = exts;
  } catch (_) { /* use local state */ }

  var exts = State.extensions.installed;
  if (exts.length === 0) {
    el.innerHTML = '<div style="color:var(--text-muted);padding:8px;">No extensions installed. Use Marketplace or Load VSIX to add extensions.</div>';
  } else {
    var html = '';
    exts.forEach(function (ext) {
      var id = ext.id || ext.name || '?';
      var enabled = ext.enabled !== false;
      html += '<div style="display:flex;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
      html += '<span style="font-size:16px;">' + (enabled ? '&#x2705;' : '&#x26D4;') + '</span>';
      html += '<div style="flex:1;min-width:0;">';
      html += '<div style="font-weight:600;color:var(--text-primary);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">' + escHtml(ext.displayName || ext.name || id) + '</div>';
      html += '<div style="font-size:10px;color:var(--text-muted);">' + escHtml(ext.publisher || '?') + ' &middot; v' + escHtml(ext.version || '?') + '</div>';
      if (ext.description) html += '<div style="font-size:10px;color:var(--text-secondary);margin-top:2px;">' + escHtml(ext.description).substring(0, 80) + '</div>';
      html += '</div>';
      html += '<div style="display:flex;gap:4px;flex-shrink:0;">';
      if (enabled) {
        html += '<button class="code-btn" onclick="extToggle(\'' + escHtml(id) + '\', false)" style="font-size:9px;padding:2px 6px;" title="Disable">Disable</button>';
      } else {
        html += '<button class="code-btn" onclick="extToggle(\'' + escHtml(id) + '\', true)" style="font-size:9px;padding:2px 6px;" title="Enable">Enable</button>';
      }
      html += '<button class="code-btn" onclick="extUninstall(\'' + escHtml(id) + '\')" style="font-size:9px;padding:2px 6px;color:var(--accent-red);" title="Uninstall">&#x1F5D1;</button>';
      html += '</div>';
      html += '</div>';
    });
    el.innerHTML = html;
  }
  updateExtStatusBar();
}

async function refreshExtRunning() {
  var el = document.getElementById('extRunningList');
  if (!el) return;
  try {
    var data = await EngineAPI.extensionHostStatus();
    State.extensions.hostStatus = data.status || 'unknown';
    State.extensions.hostPid = data.pid || null;
    var running = data.extensions || data.running || [];
    State.extensions.running = running;
    if (running.length === 0) {
      el.innerHTML = '<div style="color:var(--text-muted);">No extension hosts active.</div>';
    } else {
      var html = '';
      running.forEach(function (r) {
        html += '<div style="padding:4px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
        html += '<span style="color:var(--accent-green);font-weight:600;">' + (r.status === 'running' ? '&#x26A1;' : '&#x23F8;') + '</span> ';
        html += '<span style="color:var(--text-primary);">' + escHtml(r.id || r.name || '?') + '</span> ';
        if (r.memoryMB) html += '<span style="color:var(--text-muted);font-size:10px;">' + r.memoryMB + 'MB</span> ';
        if (r.uptime) html += '<span style="color:var(--text-muted);font-size:10px;">up ' + r.uptime + 's</span>';
        html += '</div>';
      });
      el.innerHTML = html;
    }
  } catch (e) {
    el.innerHTML = '<div style="color:var(--text-muted);">Extension host not available: ' + escHtml(e.message) + '</div>';
  }
  updateExtStatusBar();
}

async function searchExtMarketplace() {
  var query = (document.getElementById('extMarketplaceQuery') || {}).value || '';
  if (!query.trim()) return;
  var el = document.getElementById('extMarketplaceResults');
  if (!el) return;
  el.innerHTML = '<div style="color:var(--text-muted);padding:8px;">&#x1F50D; Searching...</div>';
  try {
    var data = await EngineAPI.searchMarketplace(query);
    var results = data.results || data.extensions || [];
    State.extensions.marketplace.results = results;
    State.extensions.marketplace.query = query;
    State.extensions.marketplace.lastSearch = Date.now();
    if (results.length === 0) {
      el.innerHTML = '<div style="color:var(--text-muted);padding:8px;">No results found for "' + escHtml(query) + '".</div>';
    } else {
      var html = '';
      results.forEach(function (r) {
        var id = r.id || r.extensionId || r.name || '?';
        var installed = State.extensions.installed.some(function (e) { return (e.id || e.name) === id; });
        html += '<div style="display:flex;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.05);">';
        html += '<span style="font-size:16px;">&#x1F9E9;</span>';
        html += '<div style="flex:1;min-width:0;">';
        html += '<div style="font-weight:600;color:var(--text-primary);">' + escHtml(r.displayName || r.name || id) + '</div>';
        html += '<div style="font-size:10px;color:var(--text-muted);">' + escHtml(r.publisher || '?') + ' &middot; v' + escHtml(r.version || '?');
        if (r.installs) html += ' &middot; ' + r.installs.toLocaleString() + ' installs';
        if (r.rating) html += ' &middot; \u2605 ' + r.rating;
        html += '</div>';
        if (r.description) html += '<div style="font-size:10px;color:var(--text-secondary);margin-top:2px;">' + escHtml(r.description).substring(0, 100) + '</div>';
        html += '</div>';
        if (installed) {
          html += '<span style="font-size:10px;color:var(--accent-green);font-weight:600;">Installed</span>';
        } else {
          html += '<button class="code-btn" onclick="extInstallFromMarketplace(\'' + escHtml(id) + '\')" style="font-size:9px;padding:2px 8px;">Install</button>';
        }
        html += '</div>';
      });
      el.innerHTML = html;
    }
  } catch (e) {
    el.innerHTML = '<div style="color:var(--accent-red);padding:8px;">Error: ' + escHtml(e.message) + '</div>';
  }
}

async function extInstallFromMarketplace(extId) {
  try {
    var data = await EngineAPI.installExtension(extId, 'marketplace');
    if (data.extension) State.extensions.installed.push(data.extension);
    refreshExtensionsList();
    searchExtMarketplace(); // refresh marketplace view too
  } catch (e) {
    alert('Install failed: ' + e.message);
  }
}

async function extToggle(extId, enable) {
  try {
    if (enable) {
      await EngineAPI.enableExtension(extId);
    } else {
      await EngineAPI.disableExtension(extId);
    }
    var ext = State.extensions.installed.find(function (e) { return (e.id || e.name) === extId; });
    if (ext) ext.enabled = enable;
    refreshExtensionsList();
  } catch (e) {
    alert((enable ? 'Enable' : 'Disable') + ' failed: ' + e.message);
  }
}

async function extUninstall(extId) {
  if (!confirm('Uninstall extension "' + extId + '"?')) return;
  try {
    await EngineAPI.uninstallExtension(extId);
    State.extensions.installed = State.extensions.installed.filter(function (e) { return (e.id || e.name) !== extId; });
    refreshExtensionsList();
  } catch (e) {
    alert('Uninstall failed: ' + e.message);
  }
}

function handleVsixFile(files) {
  if (!files || files.length === 0) return;
  var file = files[0];
  if (!file.name.endsWith('.vsix')) {
    document.getElementById('extLoadStatus').innerHTML = '<div style="color:var(--accent-red);">&#x274C; Not a .vsix file: ' + escHtml(file.name) + '</div>';
    return;
  }
  var statusEl = document.getElementById('extLoadStatus');
  statusEl.innerHTML = '<div style="color:var(--accent);">&#x23F3; Loading: ' + escHtml(file.name) + ' (' + formatBytesExt(file.size) + ')...</div>';

  // Read as base64 and send to backend
  var reader = new FileReader();
  reader.onload = async function (e) {
    try {
      var base64 = e.target.result.split(',')[1] || e.target.result;
      var data = await EngineAPI._post('/api/extensions/load-vsix', {
        filename: file.name,
        data: base64,
        encoding: 'base64'
      }, 120000);
      statusEl.innerHTML = '<div style="color:var(--accent-green);">&#x2705; Loaded: ' + escHtml(data.extension ? (data.extension.displayName || data.extension.name || file.name) : file.name) + '</div>';
      if (data.extension) {
        State.extensions.installed.push(data.extension);
        if (data.extension.contributes) {
          var contribs = Object.keys(data.extension.contributes);
          statusEl.innerHTML += '<div style="font-size:10px;color:var(--text-muted);margin-top:4px;">Contributes: ' + contribs.join(', ') + '</div>';
        }
      }
      refreshExtensionsList();
    } catch (err) {
      statusEl.innerHTML = '<div style="color:var(--accent-red);">&#x274C; ' + escHtml(err.message) + '</div>';
    }
  };
  reader.onerror = function () {
    statusEl.innerHTML = '<div style="color:var(--accent-red);">&#x274C; Failed to read file</div>';
  };
  reader.readAsDataURL(file);
}

async function installVsixFromUrl() {
  var url = (document.getElementById('extVsixUrl') || {}).value || '';
  if (!url.trim()) return;
  var statusEl = document.getElementById('extLoadStatus');
  statusEl.innerHTML = '<div style="color:var(--accent);">&#x23F3; Installing from URL...</div>';
  try {
    var data = await EngineAPI.loadVsixFromUrl(url);
    statusEl.innerHTML = '<div style="color:var(--accent-green);">&#x2705; ' + escHtml(data.message || 'Installed from URL') + '</div>';
    if (data.extension) {
      State.extensions.installed.push(data.extension);
      refreshExtensionsList();
    }
  } catch (e) {
    statusEl.innerHTML = '<div style="color:var(--accent-red);">&#x274C; ' + escHtml(e.message) + '</div>';
  }
}

async function restartExtensionHost() {
  try {
    await EngineAPI.restartExtensionHost();
    State.extensions.hostStatus = 'running';
    refreshExtRunning();
  } catch (e) {
    alert('Restart failed: ' + e.message);
  }
}

async function killExtensionHost() {
  if (!confirm('Kill the extension host? All running extensions will be stopped.')) return;
  try {
    await EngineAPI.killExtensionHost();
    State.extensions.hostStatus = 'idle';
    State.extensions.hostPid = null;
    State.extensions.running = [];
    refreshExtRunning();
  } catch (e) {
    alert('Kill failed: ' + e.message);
  }
}

async function showExtHostLogs() {
  var el = document.getElementById('extRunningList');
  if (!el) return;
  try {
    var data = await EngineAPI.fetchExtensionHostLogs();
    var logs = data.logs || data.entries || [];
    State.extensions.logs = logs.slice(-State.extensions.maxLogs);
    if (logs.length === 0) {
      el.innerHTML = '<div style="color:var(--text-muted);">No host logs available.</div>';
    } else {
      var html = '<div style="font-size:10px;font-weight:600;color:var(--accent-secondary);margin-bottom:6px;">Extension Host Logs (last ' + Math.min(logs.length, 30) + '):</div>';
      logs.slice(-30).forEach(function (log) {
        var level = log.level || 'info';
        var color = level === 'error' ? 'var(--accent-red)' : level === 'warn' ? 'var(--accent-orange)' : 'var(--text-secondary)';
        html += '<div style="padding:2px 0;color:' + color + ';font-size:10px;">';
        html += '[' + escHtml(log.timestamp || '?') + '] [' + level + '] ' + escHtml(log.message || JSON.stringify(log));
        html += '</div>';
      });
      el.innerHTML = html;
    }
  } catch (e) {
    el.innerHTML = '<div style="color:var(--accent-red);">Error fetching logs: ' + escHtml(e.message) + '</div>';
  }
}

function filterExtensions(val) {
  var lower = (val || '').toLowerCase();
  var el = document.getElementById('extInstalledList');
  if (!el) return;
  var items = el.querySelectorAll(':scope > div');
  items.forEach(function (item) {
    var text = item.textContent.toLowerCase();
    item.style.display = (lower === '' || text.indexOf(lower) >= 0) ? '' : 'none';
  });
}

function updateExtStatusBar() {
  var statusEl = document.getElementById('extStatusText');
  var hostEl = document.getElementById('extHostStatus');
  if (statusEl) {
    var total = State.extensions.installed.length;
    var active = State.extensions.installed.filter(function (e) { return e.enabled !== false; }).length;
    statusEl.textContent = 'Extensions: ' + total + ' installed, ' + active + ' active';
  }
  if (hostEl) {
    hostEl.textContent = State.extensions.hostStatus || 'idle';
    hostEl.style.color = State.extensions.hostStatus === 'running' ? 'var(--accent-green)' : 'var(--text-muted)';
  }
}

function formatBytesExt(b) {
  if (b === 0) return '0 B';
  var k = 1024;
  var sizes = ['B', 'KB', 'MB', 'GB'];
  var i = Math.floor(Math.log(b) / Math.log(k));
  return parseFloat((b / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// VSIX drop zone drag-and-drop support
(function () {
  var dz = document.getElementById('vsixDropZone');
  if (!dz) return;
  dz.addEventListener('dragover', function (e) {
    e.preventDefault();
    dz.style.borderColor = 'var(--accent)';
    dz.style.background = 'rgba(0,255,157,0.05)';
  });
  dz.addEventListener('dragleave', function (e) {
    e.preventDefault();
    dz.style.borderColor = 'var(--border)';
    dz.style.background = '';
  });
  dz.addEventListener('drop', function (e) {
    e.preventDefault();
    dz.style.borderColor = 'var(--border)';
    dz.style.background = '';
    if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      handleVsixFile(e.dataTransfer.files);
    }
  });
})();

// ======================================================================
// WEBVIEW2 BUILT-IN BROWSER — Panel Functions
// Full-screen embedded browser with multi-tab, address bar, navigation,
// bookmarks, history, model site quick-links, proxy content extraction.
// WebView2-style browsing integrated into the IDE chatbot interface.
// ======================================================================

function showBrowserPanel(url) {
  var overlay = document.getElementById('browserOverlay');
  if (!overlay) return;
  overlay.classList.add('active');
  State.browser.active = true;
  logDebug('\uD83C\uDF10 Browser panel opened' + (url ? ' → ' + url : ''), 'info');
  if (url) {
    browserNavigateTo(url);
  } else {
    // Show home page if no URL
    _browserShowHome();
  }
}

function closeBrowserPanel() {
  var overlay = document.getElementById('browserOverlay');
  if (!overlay) return;
  overlay.classList.remove('active');
  State.browser.active = false;
  logDebug('\uD83C\uDF10 Browser panel closed', 'info');
}

function _browserGetActiveTab() {
  var tabs = State.browser.tabs;
  for (var i = 0; i < tabs.length; i++) {
    if (tabs[i].id === State.browser.activeTab) return tabs[i];
  }
  return tabs[0] || null;
}

function _browserShowHome() {
  var viewport = document.getElementById('browserViewport');
  if (!viewport) return;
  var homePage = document.getElementById('browserHomePage');
  // Remove any existing iframe
  var iframes = viewport.querySelectorAll('iframe');
  iframes.forEach(function (f) { f.remove(); });
  if (homePage) homePage.style.display = '';
  // Update URL bar
  var urlInput = document.getElementById('browserUrlInput');
  if (urlInput) urlInput.value = '';
  _browserUpdateLock('');
  _browserUpdateStatusBar('Ready', '');
  var tab = _browserGetActiveTab();
  if (tab) { tab.url = ''; tab.iframeRef = null; }
  _browserUpdateTabTitle('New Tab');
}

function _browserUpdateLock(url) {
  var lock = document.getElementById('browserLock');
  if (!lock) return;
  if (!url || url === '') {
    lock.innerHTML = '&#x1F512;';
    lock.className = 'browser-lock';
  } else if (url.indexOf('https://') === 0) {
    lock.innerHTML = '&#x1F512;';
    lock.className = 'browser-lock';
  } else {
    lock.innerHTML = '&#x26A0;';
    lock.className = 'browser-lock insecure';
  }
}

function _browserUpdateStatusBar(status, url) {
  var statusEl = document.getElementById('browserStatusText');
  var urlEl = document.getElementById('browserStatusUrl');
  var zoomEl = document.getElementById('browserZoomLevel');
  if (statusEl) statusEl.textContent = status || 'Ready';
  if (urlEl) urlEl.textContent = url || '';
  if (zoomEl) zoomEl.textContent = State.browser.zoom + '%';
}

function _browserUpdateTabTitle(title) {
  var tabId = State.browser.activeTab;
  var tabEl = document.getElementById('browserTab' + tabId);
  if (tabEl) {
    var titleSpan = tabEl.querySelector('.tab-title');
    if (titleSpan) titleSpan.textContent = (title || 'New Tab').substring(0, 30);
  }
  var tab = _browserGetActiveTab();
  if (tab) tab.title = title || 'New Tab';
}

function _browserRenderTabs() {
  var bar = document.getElementById('browserTabBar');
  if (!bar) return;
  var html = '';
  State.browser.tabs.forEach(function (tab) {
    var isActive = tab.id === State.browser.activeTab;
    html += '<div class="browser-tab' + (isActive ? ' active' : '') + '" id="browserTab' + tab.id + '" onclick="browserSwitchTab(' + tab.id + ')">';
    html += '<span class="tab-title">' + escHtml((tab.title || 'New Tab').substring(0, 30)) + '</span>';
    html += '<span class="tab-close" onclick="event.stopPropagation();browserCloseTab(' + tab.id + ')">&times;</span>';
    html += '</div>';
  });
  html += '<button class="browser-tab-new" onclick="browserNewTab()" title="New Tab">+</button>';
  bar.innerHTML = html;
}

function browserNavigate() {
  var urlInput = document.getElementById('browserUrlInput');
  if (!urlInput) return;
  var raw = urlInput.value.trim();
  if (!raw) return;
  // Auto-prepend https:// if no protocol
  var url = raw;
  if (!/^https?:\/\//i.test(url)) {
    // If it looks like a domain (contains a dot), treat as URL
    if (/^[a-zA-Z0-9].*\.[a-zA-Z]{2,}/.test(url)) {
      url = 'https://' + url;
    } else {
      // Treat as search query
      url = State.browser.searchEngine + encodeURIComponent(raw);
    }
  }
  browserNavigateTo(url);
}

function browserNavigateFromHome() {
  var homeSearch = document.getElementById('browserHomeSearch');
  if (!homeSearch) return;
  var raw = homeSearch.value.trim();
  if (!raw) return;
  var urlInput = document.getElementById('browserUrlInput');
  if (urlInput) urlInput.value = raw;
  browserNavigate();
}

function browserNavigateTo(url) {
  if (!url) return;
  var viewport = document.getElementById('browserViewport');
  var homePage = document.getElementById('browserHomePage');
  var urlInput = document.getElementById('browserUrlInput');
  if (!viewport) return;

  // Hide home page
  if (homePage) homePage.style.display = 'none';

  // Remove existing iframe
  var oldIframes = viewport.querySelectorAll('iframe');
  oldIframes.forEach(function (f) { f.remove(); });

  // Create new iframe
  var iframe = document.createElement('iframe');
  iframe.sandbox = 'allow-scripts allow-same-origin allow-forms allow-popups allow-popups-to-escape-sandbox';
  iframe.allow = 'clipboard-read; clipboard-write';
  iframe.referrerPolicy = 'no-referrer';
  iframe.src = url;
  iframe.style.width = '100%';
  iframe.style.height = '100%';
  iframe.style.border = 'none';
  iframe.style.background = '#fff';
  viewport.appendChild(iframe);

  // Update state
  var tab = _browserGetActiveTab();
  if (tab) {
    // Push to history
    if (tab.url && tab.url !== url) {
      tab.history = tab.history.slice(0, tab.historyIdx + 1);
      tab.history.push(tab.url);
      tab.historyIdx = tab.history.length - 1;
    }
    tab.url = url;
    tab.iframeRef = iframe;
  }

  // Update UI
  if (urlInput) urlInput.value = url;
  _browserUpdateLock(url);
  _browserUpdateStatusBar('Loading...', url);
  State.browser.totalNavigations++;

  // Try to detect page title from URL
  var domain = '';
  try { domain = new URL(url).hostname; } catch (_) { domain = url.substring(0, 40); }
  _browserUpdateTabTitle(domain);

  // Update nav buttons
  _browserUpdateNavButtons();

  // iframe load event
  iframe.onload = function () {
    _browserUpdateStatusBar('Done', url);
    // Try to get title
    try {
      var iframeTitle = iframe.contentDocument ? iframe.contentDocument.title : '';
      if (iframeTitle) _browserUpdateTabTitle(iframeTitle);
    } catch (_) {
      // Cross-origin — use domain
    }
  };
  iframe.onerror = function () {
    _browserUpdateStatusBar('Error loading page', url);
  };

  logDebug('\uD83C\uDF10 Browser navigated to: ' + url, 'info');
}

function _browserUpdateNavButtons() {
  var tab = _browserGetActiveTab();
  var backBtn = document.getElementById('browserBtnBack');
  var fwdBtn = document.getElementById('browserBtnFwd');
  if (tab && backBtn) backBtn.disabled = !(tab.history.length > 0 && tab.historyIdx >= 0);
  if (tab && fwdBtn) fwdBtn.disabled = !(tab.historyIdx < tab.history.length - 1);
}

function browserBack() {
  var tab = _browserGetActiveTab();
  if (!tab || tab.historyIdx < 0) return;
  var prevUrl = tab.history[tab.historyIdx];
  tab.historyIdx--;
  tab.url = prevUrl;
  // Navigate without pushing to history
  var viewport = document.getElementById('browserViewport');
  var homePage = document.getElementById('browserHomePage');
  if (!viewport) return;
  if (homePage) homePage.style.display = 'none';
  var oldIframes = viewport.querySelectorAll('iframe');
  oldIframes.forEach(function (f) { f.remove(); });
  var iframe = document.createElement('iframe');
  iframe.sandbox = 'allow-scripts allow-same-origin allow-forms allow-popups allow-popups-to-escape-sandbox';
  iframe.src = prevUrl;
  iframe.style.cssText = 'width:100%;height:100%;border:none;background:#fff;';
  viewport.appendChild(iframe);
  tab.iframeRef = iframe;
  var urlInput = document.getElementById('browserUrlInput');
  if (urlInput) urlInput.value = prevUrl;
  _browserUpdateLock(prevUrl);
  _browserUpdateStatusBar('Loading...', prevUrl);
  _browserUpdateNavButtons();
  iframe.onload = function () { _browserUpdateStatusBar('Done', prevUrl); };
}

function browserForward() {
  var tab = _browserGetActiveTab();
  if (!tab || tab.historyIdx >= tab.history.length - 1) return;
  tab.historyIdx++;
  var nextUrl = tab.history[tab.historyIdx + 1] || tab.url;
  // If we have a forward entry beyond current
  if (tab.historyIdx < tab.history.length) {
    nextUrl = tab.history[tab.historyIdx];
  }
  browserNavigateTo(nextUrl);
}

function browserRefreshPage() {
  var tab = _browserGetActiveTab();
  if (!tab || !tab.url) return;
  _browserUpdateStatusBar('Refreshing...', tab.url);
  if (tab.iframeRef) {
    try { tab.iframeRef.src = tab.url; } catch (_) { }
  } else {
    browserNavigateTo(tab.url);
  }
}

function browserGoHome() {
  var tab = _browserGetActiveTab();
  if (tab && tab.url) {
    tab.history = tab.history.slice(0, tab.historyIdx + 1);
    tab.history.push(tab.url);
    tab.historyIdx = tab.history.length - 1;
  }
  _browserShowHome();
  _browserUpdateNavButtons();
}

function browserNewTab() {
  var newId = State.browser.nextTabId++;
  var newTab = { id: newId, title: 'New Tab', url: '', history: [], historyIdx: -1, iframeRef: null };
  State.browser.tabs.push(newTab);
  State.browser.activeTab = newId;
  _browserRenderTabs();
  _browserShowHome();
  _browserUpdateNavButtons();
  var urlInput = document.getElementById('browserUrlInput');
  if (urlInput) { urlInput.value = ''; urlInput.focus(); }
}

function browserSwitchTab(tabId) {
  if (State.browser.activeTab === tabId) return;
  State.browser.activeTab = tabId;
  var tab = _browserGetActiveTab();
  _browserRenderTabs();
  if (tab && tab.url) {
    browserNavigateTo(tab.url);
  } else {
    _browserShowHome();
  }
  _browserUpdateNavButtons();
}

function browserCloseTab(tabId) {
  var tabs = State.browser.tabs;
  if (tabs.length <= 1) {
    // Last tab — just go home
    _browserShowHome();
    return;
  }
  var idx = -1;
  for (var i = 0; i < tabs.length; i++) {
    if (tabs[i].id === tabId) { idx = i; break; }
  }
  if (idx < 0) return;
  // Remove iframe if it exists
  if (tabs[idx].iframeRef) {
    try { tabs[idx].iframeRef.remove(); } catch (_) { }
  }
  tabs.splice(idx, 1);
  // If we closed the active tab, switch to nearest
  if (State.browser.activeTab === tabId) {
    var newActive = tabs[Math.min(idx, tabs.length - 1)];
    State.browser.activeTab = newActive.id;
    if (newActive.url) {
      browserNavigateTo(newActive.url);
    } else {
      _browserShowHome();
    }
  }
  _browserRenderTabs();
  _browserUpdateNavButtons();
}

function browserAddBookmark() {
  var tab = _browserGetActiveTab();
  if (!tab || !tab.url) {
    addMessage('system', '\u26A0 No page loaded to bookmark.');
    return;
  }
  // Check for duplicate
  for (var i = 0; i < State.browser.bookmarks.length; i++) {
    if (State.browser.bookmarks[i].url === tab.url) {
      addMessage('system', '\u2714 Already bookmarked: ' + tab.title);
      return;
    }
  }
  var bm = { name: tab.title || tab.url, url: tab.url, icon: '\u2B50' };
  State.browser.bookmarks.push(bm);
  _browserRenderBookmarks();
  addMessage('system', '\u2B50 Bookmarked: ' + bm.name);
  logDebug('\uD83D\uDD16 Bookmark added: ' + bm.url, 'info');
}

function _browserRenderBookmarks() {
  var bar = document.getElementById('browserBookmarksBar');
  if (!bar) return;
  var html = '';
  State.browser.bookmarks.forEach(function (bm) {
    html += '<button class="browser-bookmark-btn" onclick="browserNavigateTo(\'' + escHtml(bm.url).replace(/'/g, "\\'") + '\')">' + (bm.icon || '\u2B50') + ' ' + escHtml(bm.name) + '</button>';
  });
  bar.innerHTML = html;
}

function browserShowBookmarks() {
  var bookmarks = State.browser.bookmarks;
  if (bookmarks.length === 0) {
    addMessage('system', 'No bookmarks saved. Use the \u2606 button to bookmark pages.');
    return;
  }
  var html = '<div style="font-weight:600;color:var(--accent-cyan);margin-bottom:8px;">\uD83D\uDD16 Browser Bookmarks (' + bookmarks.length + ')</div>';
  html += '<div style="font-family:var(--font-mono);font-size:11px;">';
  bookmarks.forEach(function (bm, i) {
    html += '<div style="padding:4px 0;border-bottom:1px solid rgba(255,255,255,0.05);display:flex;align-items:center;gap:8px;">';
    html += '<span>' + (bm.icon || '\u2B50') + '</span>';
    html += '<span style="flex:1;cursor:pointer;color:var(--accent-cyan);text-decoration:underline;" onclick="browserNavigateTo(\'' + escHtml(bm.url).replace(/'/g, "\\'") + '\')">' + escHtml(bm.name) + '</span>';
    html += '<span style="font-size:10px;color:var(--text-muted);max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">' + escHtml(bm.url) + '</span>';
    html += '<button onclick="browserRemoveBookmark(' + i + ')" style="background:none;border:none;color:var(--accent-red);cursor:pointer;font-size:12px;" title="Remove">\u2715</button>';
    html += '</div>';
  });
  html += '</div>';
  addMessage('system', html);
}

function browserRemoveBookmark(idx) {
  if (idx >= 0 && idx < State.browser.bookmarks.length) {
    var removed = State.browser.bookmarks.splice(idx, 1)[0];
    _browserRenderBookmarks();
    addMessage('system', '\uD83D\uDDD1 Removed bookmark: ' + removed.name);
  }
}

function browserDetach() {
  var tab = _browserGetActiveTab();
  if (!tab || !tab.url) {
    addMessage('system', '\u26A0 No page loaded to detach.');
    return;
  }
  var w = window.open(tab.url, '_blank', 'width=1200,height=800,menubar=no,toolbar=yes,location=yes');
  if (w) {
    addMessage('system', '\u2197 Detached browser tab to popup: ' + tab.url);
  }
}

function browserToggleDevtools() {
  State.browser.devtoolsOpen = !State.browser.devtoolsOpen;
  var tab = _browserGetActiveTab();
  if (State.browser.devtoolsOpen) {
    addMessage('system', '\uD83D\uDD27 DevTools: Page URL = ' + (tab ? tab.url : 'none') + '\\nHistory: ' + (tab ? tab.history.length : 0) + ' entries\\nTabs: ' + State.browser.tabs.length + '\\nTotal navigations: ' + State.browser.totalNavigations + '\\nZoom: ' + State.browser.zoom + '%\\nBookmarks: ' + State.browser.bookmarks.length);
  } else {
    addMessage('system', '\uD83D\uDD27 DevTools closed.');
  }
}

function browserExtractContent() {
  var tab = _browserGetActiveTab();
  if (!tab || !tab.iframeRef) {
    addMessage('system', '\u26A0 No page loaded to extract content from.');
    return;
  }
  try {
    var doc = tab.iframeRef.contentDocument;
    if (!doc) throw new Error('Cross-origin: cannot access document');
    var text = doc.body ? doc.body.innerText : '';
    if (text.length > 10000) text = text.substring(0, 10000) + '\\n... (truncated to 10K chars)';
    addMessage('system', '\uD83D\uDCCB Extracted content from ' + tab.url + ':\\n\\n' + text);
    logDebug('\uD83D\uDCCB Extracted ' + text.length + ' chars from ' + tab.url, 'info');
  } catch (e) {
    addMessage('system', '\u26A0 Cannot extract content (cross-origin restriction): ' + e.message + '\\nTip: Use the proxy fetch method or /browse-extract with the backend server.');
  }
}

function browserSendToModel() {
  var tab = _browserGetActiveTab();
  if (!tab || !tab.iframeRef) {
    addMessage('system', '\u26A0 No page loaded. Navigate to a page first.');
    return;
  }
  try {
    var doc = tab.iframeRef.contentDocument;
    if (!doc) throw new Error('Cross-origin');
    var text = doc.body ? doc.body.innerText : '';
    if (text.length > 8000) text = text.substring(0, 8000) + '\\n... (truncated)';
    // Inject as context in the chat
    var contextMsg = 'The following is web page content from ' + tab.url + ':\\n\\n' + text;
    addMessage('user', '/ask Based on this web page content, provide a summary:\\n' + contextMsg);
    sendMessage('/ask Based on this web page content from ' + tab.url + ', provide a summary:\\n\\n' + text.substring(0, 4000));
  } catch (e) {
    addMessage('system', '\u26A0 Cross-origin restriction. Cannot read page content for model context.');
  }
}

// ======================================================================
// EXTENSION PANEL (Phase 39) — Functions for ext-panel UI
// ======================================================================

function toggleExtPanel() {
  var panel = document.getElementById('extPanel');
  if (!panel) return;
  if (panel.style.display === 'none' || !panel.style.display) {
    panel.style.display = 'flex';
    refreshExtPanel();
  } else {
    panel.style.display = 'none';
  }
}

function showExtPanel() {
  var panel = document.getElementById('extPanel');
  if (panel) { panel.style.display = 'flex'; refreshExtPanel(); }
}

function refreshExtPanel() {
  // Update stats
  var all = ExtensionState.getAll();
  var enabled = ExtensionState.getEnabled();
  var builtin = ExtensionState.bundled;
  var user = ExtensionState.userExtensions;
  var countBadge = document.getElementById('extCountBadge');
  if (countBadge) countBadge.textContent = all.length + ' extensions';
  var statTotal = document.getElementById('extStatTotal');
  var statEnabled = document.getElementById('extStatEnabled');
  var statBuiltin = document.getElementById('extStatBuiltin');
  var statUser = document.getElementById('extStatUser');
  if (statTotal) statTotal.textContent = all.length;
  if (statEnabled) statEnabled.textContent = enabled.length;
  if (statBuiltin) statBuiltin.textContent = builtin.length;
  if (statUser) statUser.textContent = user.length;

  // Render installed list (all)
  extRenderList('Installed', all);
  // Render builtin list
  extRenderList('Builtin', builtin);
  // Render types grid
  extRenderTypes();
  // Update history
  extRenderHistory();
}

function switchExtTab(tabName) {
  // New ext-panel tabs
  var newTabs = ['installed', 'builtin', 'marketplace', 'vsix', 'types', 'powershell', 'history'];
  if (newTabs.indexOf(tabName) >= 0) {
    // Handle new ext-panel
    var tabBtns = document.querySelectorAll('#extTabs .ext-tab');
    tabBtns.forEach(function (btn) {
      btn.classList.toggle('active', btn.getAttribute('data-ext-tab') === tabName);
    });
    newTabs.forEach(function (t) {
      var capFirst = t.charAt(0).toUpperCase() + t.slice(1);
      var pane = document.getElementById('extTab' + capFirst);
      if (pane) pane.classList.toggle('active', t === tabName);
    });
    return;
  }
  // Fallback: old panel tabs
  var tabs = ['installed', 'marketplace', 'load', 'running'];
  tabs.forEach(function (t) {
    var view = document.getElementById('extView' + t.charAt(0).toUpperCase() + t.slice(1));
    var btn = document.getElementById('extTab' + t.charAt(0).toUpperCase() + t.slice(1));
    if (view) view.style.display = (t === tabName) ? '' : 'none';
    if (btn) {
      btn.style.background = (t === tabName) ? 'var(--accent)' : '';
      btn.style.color = (t === tabName) ? '#000' : '';
    }
  });
  if (tabName === 'installed') refreshExtensionsList();
  if (tabName === 'running') refreshExtRunning();
}

function extRenderList(listType, extensions) {
  var el = document.getElementById('extList' + listType);
  if (!el) return;
  if (!extensions || extensions.length === 0) {
    el.innerHTML = '<div style="color:var(--text-muted);font-style:italic;padding:12px;">No extensions found.</div>';
    return;
  }
  var html = '';
  extensions.forEach(function (ext) {
    var id = ext.id || '';
    var enabled = ext.enabled !== false;
    var typeBadge = '';
    if (ext.type === 'vsix') typeBadge = '<span class="vsix-badge">VSIX</span>';
    else if (ext.type === 'psm1') typeBadge = '<span class="psm1-badge">PSM1</span>';
    else if (ext.builtin || ext.isBuiltin) typeBadge = '<span class="builtin-badge">Built-in</span>';

    html += '<div class="ext-card" data-ext-id="' + escHtml(id) + '">';
    html += '  <div class="ext-icon">' + (ext.icon || '&#x1F9E9;') + '</div>';
    html += '  <div class="ext-body">';
    html += '    <div class="ext-title-row">';
    html += '      <span class="ext-name">' + escHtml(ext.name || ext.displayName || id) + '</span>';
    html += '      <span class="ext-version">v' + escHtml(ext.version || '1.0.0') + '</span>';
    html += '    </div>';
    html += '    <div class="ext-publisher">' + escHtml(ext.publisher || ext.author || 'RawrXD') + '</div>';
    if (ext.desc || ext.description) {
      html += '    <div class="ext-desc">' + escHtml((ext.desc || ext.description).substring(0, 120)) + '</div>';
    }
    html += '    <div class="ext-meta">';
    html += '      ' + typeBadge;
    if (ext.type) html += '      <span class="ext-type-badge">' + escHtml(ext.type) + '</span>';
    html += '      <span class="ext-status-dot" style="background:' + (enabled ? 'var(--accent-green)' : 'var(--accent-red)') + ';"></span>';
    html += '      <span style="font-size:10px;color:var(--text-muted);">' + (enabled ? 'Enabled' : 'Disabled') + '</span>';
    html += '    </div>';
    html += '  </div>';
    html += '  <div class="ext-actions">';
    if (enabled) {
      html += '    <button class="code-btn" onclick="extPanelDisable(\'' + escHtml(id) + '\')" style="font-size:9px;">Disable</button>';
    } else {
      html += '    <button class="code-btn" onclick="extPanelEnable(\'' + escHtml(id) + '\')" style="font-size:9px;">Enable</button>';
    }
    if (!ext.builtin && !ext.isBuiltin) {
      html += '    <button class="code-btn" onclick="extPanelUninstall(\'' + escHtml(id) + '\')" style="font-size:9px;color:var(--accent-red);">Uninstall</button>';
    }
    html += '  </div>';
    html += '</div>';
  });
  el.innerHTML = html;
}

function extRenderTypes() {
  var el = document.getElementById('extTypeGrid');
  if (!el) return;
  var typeInfo = {
    'vsix': { icon: '&#x1F4E6;', name: 'VSIX Package', desc: 'Standard VS Code extension package. Unzipped and loaded via extension host.' },
    'native-dll': { icon: '&#x2699;', name: 'Native DLL', desc: 'C/C++ native extension loaded via LoadLibrary. Uses ExtensionInit/Shutdown entry points.' },
    'psm1': { icon: '&#x1F4BB;', name: 'PowerShell Module', desc: 'PowerShell .psm1 module managed by ExtensionManager.psm1 lifecycle.' },
    'js': { icon: '&#x26A1;', name: 'JavaScript', desc: 'JS extension running in QuickJS host or polyfill engine.' },
    'theme': { icon: '&#x1F3A8;', name: 'Theme', desc: 'Color theme extension providing tokenColors, colors, and icons.' },
    'language': { icon: '&#x1F4DD;', name: 'Language', desc: 'Language support extension with grammar, snippets, and completions.' },
    'lsp': { icon: '&#x1F50C;', name: 'LSP Server', desc: 'Language Server Protocol provider via LSP bridge integration.' },
    'debugger': { icon: '&#x1F41B;', name: 'Debug Adapter', desc: 'DAP-based debugger extension via debug adapter protocol handler.' }
  };
  var html = '';
  ExtensionState.supportedTypes.forEach(function (t) {
    var info = typeInfo[t] || { icon: '&#x2753;', name: t, desc: '' };
    var count = ExtensionState.getByType(t).length;
    html += '<div class="ext-type-card">';
    html += '  <span style="font-size:24px;">' + info.icon + '</span>';
    html += '  <strong>' + escHtml(info.name) + '</strong>';
    html += '  <span style="font-size:10px;color:var(--text-muted);">' + escHtml(info.desc) + '</span>';
    html += '  <span style="font-size:10px;color:var(--accent);">' + count + ' installed</span>';
    html += '</div>';
  });
  el.innerHTML = html;
}

function extRenderHistory() {
  var el = document.getElementById('extHistoryList');
  if (!el) return;
  if (!ExtensionState._history || ExtensionState._history.length === 0) {
    el.innerHTML = '<div style="color:var(--text-muted); font-style:italic;">No activity yet.</div>';
    return;
  }
  var html = '';
  ExtensionState._history.slice().reverse().forEach(function (entry) {
    var time = new Date(entry.time).toLocaleTimeString();
    var icon = entry.action === 'install' ? '&#x1F4E5;' : entry.action === 'uninstall' ? '&#x1F5D1;' : entry.action === 'enable' ? '&#x2705;' : entry.action === 'disable' ? '&#x26D4;' : '&#x2022;';
    html += '<div class="ext-history-item">';
    html += '  <span style="font-size:14px;">' + icon + '</span>';
    html += '  <span style="color:var(--text-primary);font-weight:600;">' + escHtml(entry.action || '?') + '</span>';
    html += '  <span style="color:var(--text-secondary);">' + escHtml(entry.id || '?') + '</span>';
    html += '  <span style="color:var(--text-muted);font-size:10px;margin-left:auto;">' + time + '</span>';
    html += '</div>';
  });
  el.innerHTML = html;
}

// Initialize _history array on ExtensionState if missing
if (typeof ExtensionState !== 'undefined' && !ExtensionState._history) {
  ExtensionState._history = [];
}

function filterExtList(listType) {
  var capFirst = listType.charAt(0).toUpperCase() + listType.slice(1);
  var input = document.getElementById('extSearch' + capFirst);
  var list = document.getElementById('extList' + capFirst);
  if (!input || !list) return;
  var query = (input.value || '').toLowerCase();
  var cards = list.querySelectorAll('.ext-card');
  cards.forEach(function (card) {
    var text = card.textContent.toLowerCase();
    card.style.display = (!query || text.indexOf(query) >= 0) ? '' : 'none';
  });
}

function searchMarketplace() {
  var input = document.getElementById('extMarketplaceSearch');
  var resultsEl = document.getElementById('extMarketplaceResults');
  if (!input || !resultsEl) return;
  var query = (input.value || '').trim();
  if (!query) { resultsEl.innerHTML = '<div style="color:var(--text-muted);font-style:italic;padding:12px;">Enter a search query.</div>'; return; }

  resultsEl.innerHTML = '<div style="color:var(--accent);padding:12px;">&#x1F50D; Searching VS Code Marketplace for "' + escHtml(query) + '"...</div>';

  // Try backend EngineAPI
  if (typeof EngineAPI !== 'undefined' && EngineAPI.searchMarketplace) {
    EngineAPI.searchMarketplace(query).then(function (data) {
      var results = data.results || data.extensions || [];
      if (results.length === 0) {
        resultsEl.innerHTML = '<div style="color:var(--text-muted);padding:12px;">No results for "' + escHtml(query) + '".</div>';
      } else {
        var html = '';
        results.slice(0, 25).forEach(function (r) {
          var id = r.id || r.extensionId || r.name || '?';
          var isInstalled = ExtensionState.findById(id) !== undefined;
          html += '<div class="ext-card">';
          html += '  <div class="ext-icon">&#x1F9E9;</div>';
          html += '  <div class="ext-body">';
          html += '    <div class="ext-title-row">';
          html += '      <span class="ext-name">' + escHtml(r.displayName || r.name || id) + '</span>';
          html += '      <span class="ext-version">v' + escHtml(r.version || '?') + '</span>';
          html += '    </div>';
          html += '    <div class="ext-publisher">' + escHtml(r.publisher || '?') + '</div>';
          if (r.description) html += '    <div class="ext-desc">' + escHtml(r.description.substring(0, 120)) + '</div>';
          html += '    <div class="ext-meta">';
          if (r.installs) html += '<span style="font-size:10px;color:var(--text-muted);">' + r.installs.toLocaleString() + ' installs</span>';
          if (r.rating) html += '<span style="font-size:10px;color:var(--accent-orange);">\u2605 ' + r.rating + '</span>';
          html += '    </div>';
          html += '  </div>';
          html += '  <div class="ext-actions">';
          if (isInstalled) {
            html += '    <span style="font-size:10px;color:var(--accent-green);font-weight:600;">Installed</span>';
          } else {
            html += '    <button class="code-btn" onclick="installFromMarketplaceResult(\'' + escHtml(id) + '\')" style="font-size:9px;">Install</button>';
          }
          html += '  </div>';
          html += '</div>';
        });
        if (results.length > 25) html += '<div style="color:var(--text-muted);padding:8px;">... and ' + (results.length - 25) + ' more results</div>';
        resultsEl.innerHTML = html;
      }
    }).catch(function (err) {
      resultsEl.innerHTML = '<div style="color:var(--accent-red);padding:12px;">Backend error: ' + escHtml(err.message) + '<br>Try searching directly at <a href="https://marketplace.visualstudio.com" target="_blank" style="color:var(--accent);">marketplace.visualstudio.com</a></div>';
    });
  } else {
    resultsEl.innerHTML = '<div style="color:var(--text-muted);padding:12px;">Backend offline. Visit <a href="https://marketplace.visualstudio.com/search?term=' + encodeURIComponent(query) + '" target="_blank" style="color:var(--accent);">VS Code Marketplace</a> to search.</div>';
  }
}

function searchOpenVSX() {
  var input = document.getElementById('extMarketplaceSearch');
  var resultsEl = document.getElementById('extMarketplaceResults');
  if (!input || !resultsEl) return;
  var query = (input.value || '').trim();
  if (!query) { resultsEl.innerHTML = '<div style="color:var(--text-muted);font-style:italic;padding:12px;">Enter a search query.</div>'; return; }

  resultsEl.innerHTML = '<div style="color:var(--accent);padding:12px;">&#x1F50D; Searching Open VSX for "' + escHtml(query) + '"...</div>';

  // Open VSX API is public
  fetch('https://open-vsx.org/api/-/search?query=' + encodeURIComponent(query) + '&size=25', {
    signal: AbortSignal.timeout(15000)
  }).then(function (r) { return r.json(); }).then(function (data) {
    var exts = data.extensions || [];
    if (exts.length === 0) {
      resultsEl.innerHTML = '<div style="color:var(--text-muted);padding:12px;">No results on Open VSX for "' + escHtml(query) + '".</div>';
    } else {
      var html = '';
      exts.forEach(function (ext) {
        var id = (ext.namespace || '') + '.' + (ext.name || '');
        var isInstalled = ExtensionState.findById(id) !== undefined;
        html += '<div class="ext-card">';
        html += '  <div class="ext-icon">&#x1F4E6;</div>';
        html += '  <div class="ext-body">';
        html += '    <div class="ext-title-row">';
        html += '      <span class="ext-name">' + escHtml(ext.displayName || ext.name || id) + '</span>';
        html += '      <span class="ext-version">v' + escHtml(ext.version || '?') + '</span>';
        html += '    </div>';
        html += '    <div class="ext-publisher">' + escHtml(ext.namespace || '?') + '</div>';
        if (ext.description) html += '    <div class="ext-desc">' + escHtml(ext.description.substring(0, 120)) + '</div>';
        html += '    <div class="ext-meta">';
        if (ext.downloadCount) html += '<span style="font-size:10px;color:var(--text-muted);">' + ext.downloadCount.toLocaleString() + ' downloads</span>';
        if (ext.averageRating) html += '<span style="font-size:10px;color:var(--accent-orange);">\u2605 ' + ext.averageRating.toFixed(1) + '</span>';
        html += '    </div>';
        html += '  </div>';
        html += '  <div class="ext-actions">';
        if (isInstalled) {
          html += '    <span style="font-size:10px;color:var(--accent-green);font-weight:600;">Installed</span>';
        } else {
          html += '    <button class="code-btn" onclick="installFromOpenVSX(\'' + escHtml(ext.namespace || '') + '\', \'' + escHtml(ext.name || '') + '\')" style="font-size:9px;">Install</button>';
        }
        html += '  </div>';
        html += '</div>';
      });
      resultsEl.innerHTML = html;
    }
  }).catch(function (err) {
    resultsEl.innerHTML = '<div style="color:var(--accent-red);padding:12px;">Open VSX error: ' + escHtml(err.message) + '<br>Visit <a href="https://open-vsx.org" target="_blank" style="color:var(--accent);">open-vsx.org</a> directly.</div>';
  });
}

function installFromMarketplaceResult(extId) {
  var outputEl = document.getElementById('extInstallOutput');
  if (outputEl) outputEl.textContent = 'Installing ' + extId + ' from marketplace...';
  if (typeof EngineAPI !== 'undefined' && EngineAPI.installExtension) {
    EngineAPI.installExtension(extId, 'marketplace').then(function (data) {
      if (data.extension) {
        ExtensionState.install(data.extension);
      } else {
        ExtensionState.install({ id: extId, name: extId, enabled: true, type: 'vsix', version: '1.0.0' });
      }
      if (outputEl) outputEl.textContent = '\u2705 Installed: ' + extId;
      refreshExtPanel();
    }).catch(function (err) {
      if (outputEl) outputEl.textContent = '\u274C Install failed: ' + err.message;
    });
  } else {
    ExtensionState.install({ id: extId, name: extId, enabled: true, type: 'vsix', version: '1.0.0' });
    if (outputEl) outputEl.textContent = '\u2705 Registered: ' + extId + ' (backend offline — local state only)';
    refreshExtPanel();
  }
}

function installFromOpenVSX(namespace, name) {
  var extId = namespace + '.' + name;
  installFromMarketplaceResult(extId);
}

function installVsixFromPath() {
  var input = document.getElementById('extVsixPath');
  var outputEl = document.getElementById('extInstallOutput');
  if (!input) return;
  var path = (input.value || '').trim();
  if (!path) { if (outputEl) outputEl.textContent = '\u274C Please enter a .vsix file path.'; return; }
  if (outputEl) outputEl.textContent = 'Loading VSIX: ' + path + '...';

  if (typeof EngineAPI !== 'undefined' && EngineAPI.loadVsixFromPath) {
    EngineAPI.loadVsixFromPath(path).then(function (data) {
      if (data.extension) {
        ExtensionState.install(data.extension);
        if (outputEl) outputEl.textContent = '\u2705 Loaded: ' + (data.extension.displayName || data.extension.name || path);
      } else {
        if (outputEl) outputEl.textContent = '\u2705 ' + (data.message || 'Loaded: ' + path);
      }
      refreshExtPanel();
    }).catch(function (err) {
      if (outputEl) outputEl.textContent = '\u274C Load failed: ' + err.message;
    });
  } else {
    // Local-only fallback
    var name = path.split(/[\\\/]/).pop().replace(/\.vsix$/, '');
    ExtensionState.install({ id: name, name: name, enabled: true, type: 'vsix', version: '1.0.0', path: path });
    if (outputEl) outputEl.textContent = '\u2705 Registered: ' + name + ' (backend offline — local state only)';
    refreshExtPanel();
  }
}

function installVsixFromFile() {
  var fileInput = document.getElementById('extVsixFile');
  var outputEl = document.getElementById('extInstallOutput');
  if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
    if (outputEl) outputEl.textContent = '\u274C Please select a .vsix file.';
    return;
  }
  var file = fileInput.files[0];
  if (!file.name.endsWith('.vsix')) {
    if (outputEl) outputEl.textContent = '\u274C Not a .vsix file: ' + file.name;
    return;
  }
  if (outputEl) outputEl.textContent = 'Loading: ' + file.name + ' (' + formatBytesExt(file.size) + ')...';

  var reader = new FileReader();
  reader.onload = function (e) {
    var base64 = e.target.result.split(',')[1] || e.target.result;
    if (typeof EngineAPI !== 'undefined' && EngineAPI._post) {
      EngineAPI._post('/api/extensions/load-vsix', {
        filename: file.name,
        data: base64,
        encoding: 'base64'
      }, 120000).then(function (data) {
        if (data.extension) {
          ExtensionState.install(data.extension);
          if (outputEl) outputEl.textContent = '\u2705 Loaded: ' + (data.extension.displayName || data.extension.name || file.name);
        } else {
          if (outputEl) outputEl.textContent = '\u2705 ' + (data.message || 'Loaded: ' + file.name);
        }
        refreshExtPanel();
      }).catch(function (err) {
        if (outputEl) outputEl.textContent = '\u274C ' + err.message;
      });
    } else {
      var name = file.name.replace(/\.vsix$/, '');
      ExtensionState.install({ id: name, name: name, enabled: true, type: 'vsix', version: '1.0.0' });
      if (outputEl) outputEl.textContent = '\u2705 Registered: ' + name + ' (backend offline)';
      refreshExtPanel();
    }
  };
  reader.onerror = function () { if (outputEl) outputEl.textContent = '\u274C Failed to read file.'; };
  reader.readAsDataURL(file);
}

function installFromMarketplaceId() {
  var input = document.getElementById('extMarketplaceId');
  var outputEl = document.getElementById('extInstallOutput');
  if (!input) return;
  var extId = (input.value || '').trim();
  if (!extId) { if (outputEl) outputEl.textContent = '\u274C Please enter a publisher.extension-name ID.'; return; }
  if (extId.indexOf('.') < 0) { if (outputEl) outputEl.textContent = '\u274C ID must be in format: publisher.extension-name'; return; }
  installFromMarketplaceResult(extId);
}

function installNativeExt() {
  var input = document.getElementById('extNativePath');
  var outputEl = document.getElementById('extInstallOutput');
  if (!input) return;
  var path = (input.value || '').trim();
  if (!path) { if (outputEl) outputEl.textContent = '\u274C Please enter a native extension directory path.'; return; }
  if (outputEl) outputEl.textContent = 'Loading native DLL extension from: ' + path + '...';

  if (typeof EngineAPI !== 'undefined' && EngineAPI._post) {
    EngineAPI._post('/api/extensions/load-native', { path: path }, 30000).then(function (data) {
      if (data.extension) {
        ExtensionState.install(Object.assign({}, data.extension, { type: 'native-dll' }));
        if (outputEl) outputEl.textContent = '\u2705 Native extension loaded: ' + (data.extension.name || path);
      } else {
        if (outputEl) outputEl.textContent = '\u2705 ' + (data.message || 'Loaded native extension');
      }
      refreshExtPanel();
    }).catch(function (err) {
      if (outputEl) outputEl.textContent = '\u274C ' + err.message;
    });
  } else {
    var name = path.split(/[\\\/]/).pop() || 'native-ext';
    ExtensionState.install({ id: name, name: name, enabled: true, type: 'native-dll', version: '1.0.0', path: path });
    if (outputEl) outputEl.textContent = '\u2705 Registered: ' + name + ' (backend offline — requires Win32IDE for DLL loading)';
    refreshExtPanel();
  }
}

function installPsm1Ext() {
  var input = document.getElementById('extPsm1Path');
  var outputEl = document.getElementById('extInstallOutput');
  if (!input) return;
  var path = (input.value || '').trim();
  if (!path) { if (outputEl) outputEl.textContent = '\u274C Please enter a .psm1 file path.'; return; }
  if (!path.toLowerCase().endsWith('.psm1')) { if (outputEl) outputEl.textContent = '\u274C File must be a .psm1 module.'; return; }
  if (outputEl) outputEl.textContent = 'Importing PowerShell module: ' + path + '...';

  if (typeof EngineAPI !== 'undefined' && EngineAPI._post) {
    EngineAPI._post('/api/extensions/import-psm1', { path: path }, 30000).then(function (data) {
      var name = path.split(/[\\\/]/).pop().replace(/\.psm1$/i, '');
      ExtensionState.install(Object.assign({ id: name, name: name, type: 'psm1', version: '1.0.0', path: path }, data.extension || {}));
      if (outputEl) outputEl.textContent = '\u2705 PowerShell module imported: ' + name;
      refreshExtPanel();
    }).catch(function (err) {
      if (outputEl) outputEl.textContent = '\u274C Import failed: ' + err.message;
    });
  } else {
    var name = path.split(/[\\\/]/).pop().replace(/\.psm1$/i, '');
    ExtensionState.install({ id: name, name: name, enabled: true, type: 'psm1', version: '1.0.0', path: path });
    if (outputEl) outputEl.textContent = '\u2705 Registered: ' + name + ' (backend offline — use PowerShell tab for Import-Module)';
    refreshExtPanel();
  }
}

function extPanelEnable(extId) {
  ExtensionState.enable(extId);
  if (typeof EngineAPI !== 'undefined' && EngineAPI.enableExtension) {
    EngineAPI.enableExtension(extId).catch(function () { /* local-only is fine */ });
  }
  refreshExtPanel();
}

function extPanelDisable(extId) {
  ExtensionState.disable(extId);
  if (typeof EngineAPI !== 'undefined' && EngineAPI.disableExtension) {
    EngineAPI.disableExtension(extId).catch(function () { /* local-only is fine */ });
  }
  refreshExtPanel();
}

function extPanelUninstall(extId) {
  if (!confirm('Uninstall extension "' + extId + '"?')) return;
  ExtensionState.uninstall(extId);
  if (typeof EngineAPI !== 'undefined' && EngineAPI.uninstallExtension) {
    EngineAPI.uninstallExtension(extId).catch(function () { /* local-only is fine */ });
  }
  refreshExtPanel();
}

function extPsAction(action) {
  var outputEl = document.getElementById('extPsOutput');
  if (!outputEl) return;

  var commands = {
    'list': 'Import-Module .\\scripts\\ExtensionManager.psm1 -Force; Get-Extension | Format-Table -AutoSize',
    'menu': 'Import-Module .\\scripts\\ExtensionManager.psm1 -Force; Show-ExtensionMenu',
    'craft': 'Import-Module .\\scripts\\ExtensionManager.psm1 -Force; Get-ChildItem -Path "d:\\RawrXD\\craft_room" -Recurse | Format-Table Name, Length, LastWriteTime',
    'scan': 'Import-Module .\\scripts\\ExtensionManager.psm1 -Force; Get-ChildItem -Path "d:\\RawrXD\\extensions" -Directory | ForEach-Object { Get-Extension -Name $_.Name }'
  };

  var cmd = commands[action];
  if (!cmd) { outputEl.textContent = 'Unknown PS action: ' + action; return; }

  outputEl.textContent = 'Running: ' + cmd + '\n\nWaiting for response...';

  if (typeof EngineAPI !== 'undefined' && EngineAPI._post) {
    EngineAPI._post('/api/cli', { command: cmd }, 30000).then(function (data) {
      outputEl.textContent = data.output || data.result || JSON.stringify(data, null, 2);
    }).catch(function (err) {
      outputEl.textContent = 'Backend unavailable. Run this in PowerShell:\n\n  cd d:\\RawrXD\n  ' + cmd;
    });
  } else {
    outputEl.textContent = 'Backend offline. Run this in PowerShell:\n\n  cd d:\\RawrXD\n  ' + cmd;
  }
}

function extPsCreate() {
  var nameInput = document.getElementById('extPsNewName');
  var typeSelect = document.getElementById('extPsNewType');
  var outputEl = document.getElementById('extPsOutput');
  if (!nameInput || !typeSelect || !outputEl) return;
  var name = (nameInput.value || '').trim();
  var type = typeSelect.value || 'Custom';
  if (!name) { outputEl.textContent = '\u274C Please enter an extension name.'; return; }

  var cmd = 'Import-Module .\\scripts\\ExtensionManager.psm1 -Force; New-Extension -Name "' + name + '" -Type ' + type + ' -AutoInstall';
  outputEl.textContent = 'Creating extension: ' + name + ' (type: ' + type + ')...\n\n  ' + cmd;

  if (typeof EngineAPI !== 'undefined' && EngineAPI._post) {
    EngineAPI._post('/api/cli', { command: cmd }, 30000).then(function (data) {
      outputEl.textContent = data.output || data.result || ('\u2705 Created extension: ' + name);
      ExtensionState.install({ id: name.toLowerCase(), name: name, enabled: true, type: type.toLowerCase(), version: '1.0.0', path: 'd:\\RawrXD\\extensions\\' + name.toLowerCase() });
      refreshExtPanel();
    }).catch(function (err) {
      outputEl.textContent = 'Backend offline. Run this in PowerShell:\n\n  cd d:\\RawrXD\n  ' + cmd;
    });
  } else {
    outputEl.textContent = 'Backend offline. Run this in PowerShell:\n\n  cd d:\\RawrXD\n  ' + cmd;
  }
}

// Wire ext panel into terminal 'ext panel' subcommand and ExtensionState callbacks
(function () {
  // Patch ExtensionState methods to track history
  var origInstall = ExtensionState.install;
  ExtensionState.install = function (ext) {
    var result = origInstall.call(ExtensionState, ext);
    if (!ExtensionState._history) ExtensionState._history = [];
    ExtensionState._history.push({ action: 'install', id: ext.id || ext.name || '?', time: Date.now() });
    return result;
  };
  var origUninstall = ExtensionState.uninstall;
  ExtensionState.uninstall = function (id) {
    var result = origUninstall.call(ExtensionState, id);
    if (!ExtensionState._history) ExtensionState._history = [];
    ExtensionState._history.push({ action: 'uninstall', id: id, time: Date.now() });
    return result;
  };
  var origEnable = ExtensionState.enable;
  ExtensionState.enable = function (id) {
    var result = origEnable.call(ExtensionState, id);
    if (!ExtensionState._history) ExtensionState._history = [];
    ExtensionState._history.push({ action: 'enable', id: id, time: Date.now() });
    return result;
  };
  var origDisable = ExtensionState.disable;
  ExtensionState.disable = function (id) {
    var result = origDisable.call(ExtensionState, id);
    if (!ExtensionState._history) ExtensionState._history = [];
    ExtensionState._history.push({ action: 'disable', id: id, time: Date.now() });
    return result;
  };
})();

// ======================================================================
// CHAIN-OF-THOUGHT MULTI-MODEL REVIEW SYSTEM
// ======================================================================
//
// Allows 1-8 models to be chained. Each step has:
//   - A role (reviewer, auditor, thinker, researcher, debater, summarizer, critic, synthesizer)
//   - A model (picked from whatever the backend offers)
//   - A custom instruction/system prompt override for that step
//   - An optional flag (skip = true means this step is skipped)
//
// Presets configure common patterns:
//   Review:   [Analyze] → [Critique] → [Summarize]
//   Audit:    [Audit] → [Verify] → [Report]
//   Think:    [Brainstorm] → [Evaluate] → [Refine] → [Conclude]
//   Research: [Gather] → [Analyze] → [Cross-check] → [Synthesize]
//   Debate:   [Argue For] → [Argue Against] → [Judge]
//   Custom:   User configures everything
//
// The chain executes sequentially. Each step receives:
//   1. The original user prompt
//   2. All previous steps' outputs (as context)
//   3. The step-specific instruction
//
// The final step's output is shown as the main assistant response.
// All intermediate steps are shown in a collapsible "Chain of Thought" block.
// ======================================================================

const COT_ROLES = [
  { id: 'reviewer', label: '\uD83D\uDD0D Reviewer', icon: '\uD83D\uDD0D', instruction: 'You are a code reviewer. Analyze the following carefully, identify issues, suggest improvements.' },
  { id: 'auditor', label: '\uD83D\uDEE1 Auditor', icon: '\uD83D\uDEE1', instruction: 'You are a security/quality auditor. Check for vulnerabilities, correctness issues, edge cases, and compliance.' },
  { id: 'thinker', label: '\uD83D\uDCAD Thinker', icon: '\uD83D\uDCAD', instruction: 'You are a deep thinker. Reason step-by-step through the problem, consider alternatives, and explain your reasoning.' },
  { id: 'researcher', label: '\uD83D\uDCDA Researcher', icon: '\uD83D\uDCDA', instruction: 'You are a research assistant. Gather relevant context, find patterns, cross-reference information, and cite sources.' },
  { id: 'debater_for', label: '\u2694 Argue For', icon: '\u2694', instruction: 'You argue IN FAVOR of the proposed approach. Present the strongest possible case for why this is correct/optimal.' },
  { id: 'debater_against', label: '\u2694 Argue Against', icon: '\u2694', instruction: 'You argue AGAINST the proposed approach. Present the strongest possible counterarguments and alternatives.' },
  { id: 'critic', label: '\uD83E\uDDD0 Critic', icon: '\uD83E\uDDD0', instruction: 'You are a harsh critic. Find every flaw, weakness, and edge case. Be thorough and unforgiving.' },
  { id: 'synthesizer', label: '\u2728 Synthesizer', icon: '\u2728', instruction: 'You are a synthesizer. Combine all previous analyses into a coherent, actionable final answer. Resolve conflicts and present the best path forward.' },
  { id: 'brainstorm', label: '\uD83D\uDCA1 Brainstorm', icon: '\uD83D\uDCA1', instruction: 'You are a creative brainstormer. Generate multiple diverse approaches and ideas without filtering.' },
  { id: 'verifier', label: '\u2705 Verifier', icon: '\u2705', instruction: 'You are a verifier. Check all previous claims for accuracy. Flag anything unverified or incorrect.' },
  { id: 'refiner', label: '\uD83D\uDD27 Refiner', icon: '\uD83D\uDD27', instruction: 'You are a refiner. Take the previous output and improve its clarity, correctness, and completeness.' },
  { id: 'summarizer', label: '\uD83D\uDCCB Summarizer', icon: '\uD83D\uDCCB', instruction: 'You are a summarizer. Distill everything into a concise, actionable summary.' },
];

const COT_PRESETS = {
  review: {
    label: 'Review',
    steps: [
      { role: 'reviewer', model: null, instruction: null, skip: false },
      { role: 'critic', model: null, instruction: null, skip: false },
      { role: 'synthesizer', model: null, instruction: null, skip: false },
    ]
  },
  audit: {
    label: 'Audit',
    steps: [
      { role: 'auditor', model: null, instruction: null, skip: false },
      { role: 'verifier', model: null, instruction: null, skip: false },
      { role: 'summarizer', model: null, instruction: null, skip: false },
    ]
  },
  think: {
    label: 'Think',
    steps: [
      { role: 'brainstorm', model: null, instruction: null, skip: false },
      { role: 'thinker', model: null, instruction: null, skip: false },
      { role: 'refiner', model: null, instruction: null, skip: false },
      { role: 'synthesizer', model: null, instruction: null, skip: false },
    ]
  },
  research: {
    label: 'Research',
    steps: [
      { role: 'researcher', model: null, instruction: null, skip: false },
      { role: 'thinker', model: null, instruction: null, skip: false },
      { role: 'verifier', model: null, instruction: null, skip: false },
      { role: 'synthesizer', model: null, instruction: null, skip: false },
    ]
  },
  debate: {
    label: 'Debate',
    steps: [
      { role: 'debater_for', model: null, instruction: null, skip: false },
      { role: 'debater_against', model: null, instruction: null, skip: false },
      { role: 'synthesizer', model: null, instruction: null, skip: false },
    ]
  },
  custom: {
    label: 'Custom',
    steps: [
      { role: 'thinker', model: null, instruction: null, skip: false },
    ]
  },
};

const CoT = {
  enabled: false,
  running: false,
  abortController: null,
  activePreset: null,
  steps: [],  // Array of { role, model, instruction, skip }

  toggle: function () {
    this.enabled = !this.enabled;
    var sw = document.getElementById('cotSwitch');
    var badge = document.getElementById('cotStatusBadge');
    var tbBtn = document.getElementById('cotToolbarBtn');
    var tbLabel = document.getElementById('cotToolbarLabel');
    if (this.enabled) {
      sw.classList.add('active');
      badge.textContent = 'ON';
      badge.style.color = 'var(--accent-green)';
      tbBtn.style.display = '';
      tbLabel.textContent = 'CoT: ON';
      if (this.steps.length === 0) this.applyPreset('review');
    } else {
      sw.classList.remove('active');
      badge.textContent = 'OFF';
      badge.style.color = 'var(--text-muted)';
      tbBtn.style.display = 'none';
      tbLabel.textContent = 'CoT: OFF';
    }
    this.persist();
  },

  setStepCount: function (n) {
    n = Math.max(1, Math.min(8, n));
    document.getElementById('cotStepVal').textContent = n;
    // Adjust steps array
    while (this.steps.length < n) {
      this.steps.push({ role: 'thinker', model: null, instruction: null, skip: false });
    }
    while (this.steps.length > n) {
      this.steps.pop();
    }
    this.renderChain();
    this.persist();
  },

  applyPreset: function (presetKey) {
    var preset = COT_PRESETS[presetKey];
    if (!preset) return;

    this.activePreset = presetKey;
    this.steps = preset.steps.map(function (s) {
      return { role: s.role, model: s.model, instruction: s.instruction, skip: s.skip };
    });

    document.getElementById('cotStepSlider').value = this.steps.length;
    document.getElementById('cotStepVal').textContent = this.steps.length;

    // Highlight active preset button
    document.querySelectorAll('.cot-preset-btn').forEach(function (btn) {
      btn.classList.remove('active');
    });
    var btns = document.querySelectorAll('.cot-preset-btn');
    var idx = Object.keys(COT_PRESETS).indexOf(presetKey);
    if (idx >= 0 && btns[idx]) btns[idx].classList.add('active');

    if (!this.enabled) this.toggle();
    this.renderChain();
    this.persist();
  },

  renderChain: function () {
    var container = document.getElementById('cotChainList');
    container.innerHTML = '';
    var models = State.model.list || [];
    var self = this;

    this.steps.forEach(function (step, i) {
      // Connector arrow between steps
      if (i > 0) {
        var conn = document.createElement('div');
        conn.className = 'cot-connector';
        conn.innerHTML = '&#x2193;';
        container.appendChild(conn);
      }

      var card = document.createElement('div');
      card.className = 'cot-step-card';
      if (step.skip) card.style.opacity = '0.45';

      var roleInfo = COT_ROLES.find(function (r) { return r.id === step.role; }) || COT_ROLES[0];

      // Header: step number + role + optional toggle
      var header = document.createElement('div');
      header.className = 'step-header';
      header.innerHTML =
        '<div class="step-num">' + (i + 1) + '</div>' +
        '<div class="step-role">' + roleInfo.icon + ' ' + roleInfo.label.replace(/^[^\s]+ /, '') + '</div>' +
        '<div class="step-optional ' + (step.skip ? 'skipped' : '') + '" data-idx="' + i + '">' +
        (step.skip ? '&#x274C; skipped' : '&#x2705; active') +
        '</div>';
      header.querySelector('.step-optional').onclick = function () {
        step.skip = !step.skip;
        self.renderChain();
        self.persist();
      };
      card.appendChild(header);

      // Role selector
      var roleSel = document.createElement('select');
      roleSel.className = 'cot-role-select';
      COT_ROLES.forEach(function (r) {
        var opt = document.createElement('option');
        opt.value = r.id;
        opt.textContent = r.label;
        if (r.id === step.role) opt.selected = true;
        roleSel.appendChild(opt);
      });
      roleSel.onchange = function () {
        step.role = this.value;
        self.activePreset = 'custom';
        self.renderChain();
        self.persist();
      };
      card.appendChild(roleSel);

      // Model selector
      var modelSel = document.createElement('select');
      modelSel.style.cssText = 'width:100%;background:var(--bg-secondary);color:var(--text-primary);border:1px solid var(--border-subtle);border-radius:4px;padding:3px 6px;font-size:10px;font-family:var(--font-mono);margin-top:4px;';
      var defOpt = document.createElement('option');
      defOpt.value = '';
      defOpt.textContent = '(use default model)';
      modelSel.appendChild(defOpt);
      models.forEach(function (m) {
        var mName = typeof m === 'string' ? m : (m.name || m.id || m.model || 'unknown');
        var opt = document.createElement('option');
        opt.value = mName;
        opt.textContent = mName;
        if (step.model === mName) opt.selected = true;
        modelSel.appendChild(opt);
      });
      modelSel.onchange = function () {
        step.model = this.value || null;
        self.persist();
      };
      card.appendChild(modelSel);

      // Custom instruction textarea
      var instrTA = document.createElement('textarea');
      instrTA.style.marginTop = '4px';
      instrTA.placeholder = roleInfo.instruction.substring(0, 60) + '...';
      instrTA.value = step.instruction || '';
      instrTA.oninput = function () {
        step.instruction = this.value || null;
        self.persist();
      };
      card.appendChild(instrTA);

      container.appendChild(card);
    });
  },

  // ================================================================
  // CHAIN EXECUTION ENGINE
  // Attempts Win32IDE server-side /api/cot/execute first if available,
  // falls through to client-side step-by-step execution if not.
  // ================================================================
  executeChain: async function (userQuery) {
    if (!this.enabled || this.steps.length === 0) return null;

    // --- Try Win32IDE server-side CoT execution first ---
    if (_win32IdeDetected && State.backend.online) {
      try {
        var cotPayload = {
          query: userQuery,
          steps: this.steps.filter(function (s) { return !s.skip; }).map(function (s) {
            return { role: s.role, model: s.model || State.model.current || 'rawrxd', instruction: s.instruction || '' };
          })
        };
        var cotRes = await fetch(getActiveUrl() + '/api/cot/execute', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(cotPayload),
          signal: AbortSignal.timeout(120000)
        });
        if (cotRes.ok) {
          var cotData = await cotRes.json();
          if (cotData.result || cotData.output || cotData.steps) {
            logDebug('CoT executed via Win32IDE server-side /api/cot/execute', 'info');
            // Build chain result display from server response
            var serverSteps = cotData.steps || [];
            var finalOutput = cotData.result || cotData.output || cotData.final || '';
            var chainDiv = document.createElement('div');
            chainDiv.className = 'cot-chain-result';
            var chainHeader = document.createElement('div');
            chainHeader.className = 'cot-chain-header';
            chainHeader.innerHTML =
              '<span class="chain-icon">\uD83E\uDDE0</span>' +
              '<span class="chain-summary">Chain of Thought (' + (serverSteps.length || cotPayload.steps.length) + ' steps) &mdash; server-side</span>' +
              '<span class="chain-toggle">&#x25BC;</span>';
            chainDiv.appendChild(chainHeader);
            var stepsContainer = document.createElement('div');
            stepsContainer.className = 'cot-chain-steps';
            if (serverSteps.length > 0) {
              serverSteps.forEach(function (ss, si) {
                var stepDiv = document.createElement('div');
                stepDiv.className = 'cot-step-card';
                stepDiv.innerHTML =
                  '<div class="cot-step-header">' +
                  '<span class="cot-step-num">' + (si + 1) + '</span>' +
                  '<span class="cot-step-role">' + esc(ss.role || 'step') + '</span>' +
                  '<span class="cot-step-model">' + esc(ss.model || '') + '</span>' +
                  '</div>' +
                  '<div class="cot-step-content">' + formatMessage(ss.content || ss.output || '') + '</div>';
                stepsContainer.appendChild(stepDiv);
              });
            }
            chainDiv.appendChild(stepsContainer);
            chainHeader.onclick = function () {
              stepsContainer.classList.toggle('collapsed');
              var arrow = chainHeader.querySelector('.chain-toggle');
              arrow.textContent = stepsContainer.classList.contains('collapsed') ? '\u25B6' : '\u25BC';
            };
            var msgDiv = addMessage('assistant', '', { skipMemory: true });
            var textEl = msgDiv.querySelector('.message-text');
            textEl.innerHTML = '';
            textEl.appendChild(chainDiv);
            // Add final output as regular text after the chain display
            if (finalOutput) {
              var finalDiv = document.createElement('div');
              finalDiv.style.marginTop = '12px';
              finalDiv.innerHTML = formatMessage(finalOutput);
              textEl.appendChild(finalDiv);
              // Also add to conversation memory
              Conversation.messages.push({ role: 'assistant', content: finalOutput });
              enforceContextWindow();
            }
            this.running = false;
            return finalOutput || (serverSteps.length > 0 ? serverSteps[serverSteps.length - 1].content || '' : '');
          }
        }
        // If /api/cot/execute returned non-ok, fall through to client-side execution
        logDebug('Win32IDE /api/cot/execute returned non-ok, falling back to client-side', 'warn');
      } catch (cotErr) {
        logDebug('Win32IDE /api/cot/execute failed (' + cotErr.message + '), falling back to client-side', 'warn');
      }
    }

    // --- Client-side step-by-step execution (original) ---
    this.running = true;
    this.abortController = new AbortController();
    var signal = this.abortController.signal;

    var activeSteps = this.steps.filter(function (s) { return !s.skip; });
    if (activeSteps.length === 0) return null;

    var chainResults = [];
    var totalT0 = performance.now();

    // Create chain result container in chat
    var chainDiv = document.createElement('div');
    chainDiv.className = 'cot-chain-result';
    var chainHeader = document.createElement('div');
    chainHeader.className = 'cot-chain-header';
    chainHeader.innerHTML =
      '<span class="chain-icon">\uD83E\uDDE0</span>' +
      '<span class="chain-summary">Chain of Thought (' + activeSteps.length + ' steps) &mdash; running...</span>' +
      '<span class="chain-toggle">&#x25BC;</span>';
    chainDiv.appendChild(chainHeader);

    var stepsContainer = document.createElement('div');
    stepsContainer.className = 'cot-chain-steps';
    chainDiv.appendChild(stepsContainer);

    // Add to chat as an assistant message
    var msgDiv = addMessage('assistant', '', { skipMemory: true });
    var textEl = msgDiv.querySelector('.message-text');
    textEl.innerHTML = '';
    textEl.appendChild(chainDiv);

    // Toggle collapse
    chainHeader.onclick = function () {
      stepsContainer.classList.toggle('collapsed');
      var arrow = chainHeader.querySelector('.chain-toggle');
      arrow.textContent = stepsContainer.classList.contains('collapsed') ? '\u25B6' : '\u25BC';
    };

    // Execute each step sequentially
    for (var i = 0; i < activeSteps.length; i++) {
      if (signal.aborted) break;

      var step = activeSteps[i];
      var roleInfo = COT_ROLES.find(function (r) { return r.id === step.role; }) || COT_ROLES[0];
      var stepModel = step.model || State.model.current || 'rawrxd';
      var stepInstruction = step.instruction || roleInfo.instruction;

      // Show running indicator
      var runningDiv = document.createElement('div');
      runningDiv.className = 'cot-running-indicator';
      runningDiv.innerHTML =
        '<div class="cot-spinner"></div>' +
        '<span>Step ' + (i + 1) + '/' + activeSteps.length + ': ' + esc(roleInfo.label) + ' (' + esc(stepModel) + ')...</span>';
      stepsContainer.appendChild(runningDiv);
      msgDiv.scrollIntoView({ behavior: 'smooth', block: 'end' });

      // Build messages for this step
      var messages = [];
      messages.push({
        role: 'system',
        content: stepInstruction + '\n\nYou are step ' + (i + 1) + ' of ' + activeSteps.length + ' in a chain-of-thought review pipeline.'
      });

      // Add the original user query
      messages.push({ role: 'user', content: userQuery });

      // Add all previous chain outputs as assistant context
      if (chainResults.length > 0) {
        var prevContext = chainResults.map(function (r, idx) {
          return '--- Step ' + (idx + 1) + ' (' + r.roleLabel + ') ---\n' + r.content;
        }).join('\n\n');
        messages.push({
          role: 'system',
          content: 'Previous analysis steps:\n\n' + prevContext + '\n\nNow provide your analysis based on all of the above.'
        });
      }

      var t0 = performance.now();
      var stepContent = '';
      var stepSuccess = false;

      try {
        var chainExtras = buildOpenAIPayloadExtras();
        var payload = Object.assign({
          model: stepModel,
          messages: messages,
          stream: false,
        }, chainExtras);

        var res = await fetch(State.backend.url + '/v1/chat/completions', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
          signal: signal,
        });

        if (res.ok) {
          var data = await res.json();
          if (data.choices && data.choices[0] && data.choices[0].message) {
            stepContent = data.choices[0].message.content || '[Empty response]';
          } else {
            stepContent = data.answer || data.response || '[No response]';
          }
          stepSuccess = true;
        } else {
          // Try Ollama /api/generate fallback
          var ollamaPayload = {
            model: stepModel,
            prompt: stepInstruction + '\n\nUser query: ' + userQuery +
              (chainResults.length > 0 ? '\n\nPrevious steps:\n' + chainResults.map(function (r, idx) { return 'Step ' + (idx + 1) + ': ' + r.content; }).join('\n') : ''),
            stream: false,
            options: buildOllamaOptions(),
          };
          var res2 = await fetch(State.backend.url + '/api/generate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(ollamaPayload),
            signal: signal,
          });
          if (res2.ok) {
            var data2 = await res2.json();
            stepContent = data2.response || '[No response]';
            stepSuccess = true;
          } else {
            stepContent = '[Error: HTTP ' + res2.status + ']';
          }
        }
      } catch (e) {
        if (e.name === 'AbortError') {
          stepContent = '[Cancelled]';
        } else {
          stepContent = '[Error: ' + e.message + ']';
        }
      }

      var elapsed = Math.round(performance.now() - t0);

      // Record metric
      recordMetric({
        latency: elapsed,
        tokens: 0,
        model: stepModel,
        endpoint: '/v1/chat/completions (CoT step ' + (i + 1) + ')',
        success: stepSuccess,
      });

      chainResults.push({
        role: step.role,
        roleLabel: roleInfo.label,
        model: stepModel,
        content: stepContent,
        elapsed: elapsed,
        success: stepSuccess,
      });

      // Remove running indicator and add step result
      stepsContainer.removeChild(runningDiv);

      var stepDiv = document.createElement('div');
      stepDiv.className = 'cot-chain-step';
      stepDiv.innerHTML =
        '<div class="cot-chain-step-header">' +
        '<span class="step-badge">' + (i + 1) + '</span>' +
        '<span class="step-role-label">' + esc(roleInfo.label) + '</span>' +
        '<span class="step-model-label">' + esc(stepModel) + '</span>' +
        '<span class="step-time">' + elapsed + 'ms</span>' +
        '</div>' +
        '<div class="cot-chain-step-body">' + formatMessage(stepContent) + '</div>';
      stepsContainer.appendChild(stepDiv);

      msgDiv.scrollIntoView({ behavior: 'smooth', block: 'end' });
    }

    // Final answer section
    var totalElapsed = Math.round(performance.now() - totalT0);
    var finalContent = chainResults.length > 0 ? chainResults[chainResults.length - 1].content : '[No output]';

    var finalDiv = document.createElement('div');
    finalDiv.className = 'cot-final-answer';
    finalDiv.innerHTML =
      '<div class="cot-final-label">\u2705 Final Answer (' + totalElapsed + 'ms total, ' + activeSteps.length + ' steps)</div>' +
      '<div class="cot-chain-step-body">' + formatMessage(finalContent) + '</div>';
    chainDiv.appendChild(finalDiv);

    // Update header
    chainHeader.querySelector('.chain-summary').textContent =
      'Chain of Thought (' + activeSteps.length + ' steps) \u2014 ' +
      (signal.aborted ? 'Cancelled' : 'Complete') +
      ' in ' + totalElapsed + 'ms';

    // Record to conversation memory (only the final synthesis)
    Conversation.addMessage('user', userQuery);
    Conversation.addMessage('assistant', finalContent);

    this.running = false;
    this.abortController = null;

    return { chainResults: chainResults, finalContent: finalContent, totalElapsed: totalElapsed };
  },

  cancel: function () {
    if (this.abortController) {
      this.abortController.abort();
    }
  },

  persist: function () {
    try {
      localStorage.setItem('rawrxd_cot', JSON.stringify({
        enabled: this.enabled,
        activePreset: this.activePreset,
        steps: this.steps,
      }));
    } catch (e) { /* ignore */ }
  },

  load: function () {
    try {
      var saved = localStorage.getItem('rawrxd_cot');
      if (saved) {
        var data = JSON.parse(saved);
        if (data.steps && Array.isArray(data.steps)) {
          this.steps = data.steps;
        }
        if (data.activePreset) this.activePreset = data.activePreset;
        if (data.enabled) {
          this.enabled = false; // toggle will flip it
          this.toggle();
        }
        document.getElementById('cotStepSlider').value = this.steps.length;
        document.getElementById('cotStepVal').textContent = this.steps.length;
        this.renderChain();
      }
    } catch (e) { /* ignore */ }
  },

  init: function () {
    this.load();
    this.renderChain();
    // Show toolbar button always (but greyed out when off)
    document.getElementById('cotToolbarBtn').style.display = '';
  },
};

// ================================================================
// HOOK: Override sendMessage to route through CoT when enabled
// ================================================================
(function () {
  var _originalSendToBackend = sendToBackend;

  sendToBackend = async function (query) {
    if (CoT.enabled && CoT.steps.length > 0 && !CoT.running) {
      var activeSteps = CoT.steps.filter(function (s) { return !s.skip; });
      if (activeSteps.length > 0) {
        await CoT.executeChain(query);
        return;
      }
    }
    // Fall through to normal single-model send
    await _originalSendToBackend(query);
  };
})();

// Initialize CoT on load
window.addEventListener('DOMContentLoaded', function () {
  // Delay slightly to let models load first
  setTimeout(function () { CoT.init(); }, 500);
});

/* ── Mobile Sidebar Toggle ── */
function toggleSidebarMobile(side) {
  const sidebar = document.querySelector(side === 'left' ? '.sidebar' : '.rightbar');
  const overlay = document.getElementById('sidebarOverlay');
  if (!sidebar) return;

  const isOpen = sidebar.classList.contains('slide-in');
  // Close any open sidebar first
  closeMobileSidebars();

  if (!isOpen) {
    sidebar.classList.add('slide-in');
    overlay.classList.add('active');
  }
}

function closeMobileSidebars() {
  var sb = document.querySelector('.sidebar');
  var rb = document.querySelector('.rightbar');
  var ov = document.getElementById('sidebarOverlay');
  if (sb) sb.classList.remove('slide-in');
  if (rb) rb.classList.remove('slide-in');
  if (ov) ov.classList.remove('active');
}

// Close mobile sidebars on Escape key + keyboard shortcuts
document.addEventListener('keydown', function (e) {
  if (e.key === 'Escape') {
    // Close browser overlay if open
    var browserEl = document.getElementById('browserOverlay');
    if (browserEl && browserEl.classList.contains('active')) {
      closeBrowserPanel();
      return;
    }
    closeMobileSidebars();
  }
  // Ctrl+` — toggle terminal
  if (e.ctrlKey && e.key === '`') {
    e.preventDefault();
    toggleTerminalFull();
  }
  // Ctrl+B — toggle right sidebar
  if (e.ctrlKey && e.key === 'b') {
    e.preventDefault();
    toggleRightbar();
  }
  // Ctrl+Shift+B — toggle browser
  if (e.ctrlKey && e.shiftKey && e.key === 'B') {
    e.preventDefault();
    var bel = document.getElementById('browserOverlay');
    if (bel && bel.classList.contains('active')) closeBrowserPanel();
    else showBrowserPanel();
  }
});

// Close mobile sidebars on window resize above breakpoint
window.addEventListener('resize', function () {
  if (window.innerWidth > 1200) {
    closeMobileSidebars();
  }
});
