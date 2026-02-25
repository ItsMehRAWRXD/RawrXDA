/**
 * BigDaddyG IDE - System Optimizer
 * Real hardware detection and auto-optimization for AMD Ryzen 7 7800X3D
 * NO SIMULATIONS - 100% REAL FUNCTIONALITY
 */

// Browser/Node compatibility
const os_module = typeof require !== 'undefined' ? require('os') : null;
const execSync = typeof require !== 'undefined' ? require('child_process').execSync : null;
const fs_sync = typeof require !== 'undefined' ? require('fs') : null;
const path_sys = typeof require !== 'undefined' ? require('path') : null;

class SystemOptimizer {
  constructor() {
    this.configPath = path.join(os.homedir(), '.bigdaddyg', 'optimization-config.json');
    this.systemInfo = null;
    this.currentSettings = null;
    this.optimalSettings = null;
    this.isScanning = false;
  }
  
  async scanSystem() {
    console.log('[Optimizer] Starting full system scan...');
    this.isScanning = true;
    
    const startTime = Date.now();
    
    try {
      // Detect all hardware
      const cpu = await this.detectCPU();
      const memory = await this.detectMemory();
      const gpu = await this.detectGPU();
      const storage = await this.detectStorage();
      const network = await this.detectNetwork();
      
      this.systemInfo = {
        cpu,
        memory,
        gpu,
        storage,
        network,
        os: {
          platform: os.platform(),
          release: os.release(),
          arch: os.arch(),
          hostname: os.hostname()
        },
        scanTime: Date.now() - startTime
      };
      
      console.log('[Optimizer] System scan complete:', this.systemInfo);
      
      // Calculate optimal settings
      this.optimalSettings = this.calculateOptimalSettings(this.systemInfo);
      
      // Load current settings or use optimal
      this.currentSettings = this.loadSettings() || this.optimalSettings;
      
      this.isScanning = false;
      
      return this.systemInfo;
      
    } catch (error) {
      console.error('[Optimizer] Scan error:', error);
      this.isScanning = false;
      throw error;
    }
  }
  
  async detectCPU() {
    const cpus = os.cpus();
    const platform = os.platform();
    
    let cpuInfo = {
      model: cpus[0].model,
      cores: cpus.length,
      physicalCores: cpus.length / 2, // Assumes SMT
      threads: cpus.length,
      speed: cpus[0].speed,
      vendor: 'unknown',
      cache: {},
      features: []
    };
    
    // Platform-specific detection
    if (platform === 'win32') {
      try {
        // Get detailed CPU info from WMIC
        const wmicCPU = execSync('wmic cpu get Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed,L2CacheSize,L3CacheSize /format:csv', { encoding: 'utf8' });
        const lines = wmicCPU.split('\n').filter(l => l.trim() && !l.startsWith('Node'));
        
        if (lines.length > 0) {
          const parts = lines[0].split(',');
          cpuInfo.physicalCores = parseInt(parts[3]) || cpuInfo.physicalCores;
          cpuInfo.threads = parseInt(parts[4]) || cpuInfo.threads;
          cpuInfo.speed = parseInt(parts[2]) || cpuInfo.speed;
          cpuInfo.cache.L2 = parseInt(parts[1]) || 0;
          cpuInfo.cache.L3 = parseInt(parts[0]) || 0;
        }
        
        // Detect if AMD Ryzen 7800X3D
        if (cpuInfo.model.includes('7800X3D')) {
          cpuInfo.cache.L3 = 96 * 1024; // 96MB for X3D
          cpuInfo.features.push('3D V-Cache');
        }
        
        // Detect AMD vs Intel
        if (cpuInfo.model.includes('AMD') || cpuInfo.model.includes('Ryzen')) {
          cpuInfo.vendor = 'AMD';
        } else if (cpuInfo.model.includes('Intel')) {
          cpuInfo.vendor = 'Intel';
        }
        
      } catch (error) {
        console.error('[Optimizer] WMIC error:', error);
      }
    } else if (platform === 'linux') {
      try {
        const cpuinfo = fs.readFileSync('/proc/cpuinfo', 'utf8');
        
        // Parse cache sizes
        const l2Match = cpuinfo.match(/cache size\s*:\s*(\d+)\s*KB/);
        const l3Match = cpuinfo.match(/L3 cache\s*:\s*(\d+)\s*KB/);
        
        if (l2Match) cpuInfo.cache.L2 = parseInt(l2Match[1]);
        if (l3Match) cpuInfo.cache.L3 = parseInt(l3Match[1]);
        
        // Detect vendor
        if (cpuinfo.includes('AMD')) cpuInfo.vendor = 'AMD';
        if (cpuinfo.includes('Intel')) cpuInfo.vendor = 'Intel';
        
      } catch (error) {
        console.error('[Optimizer] /proc/cpuinfo error:', error);
      }
    }
    
    return cpuInfo;
  }
  
  async detectMemory() {
    const totalMem = os.totalmem();
    const freeMem = os.freemem();
    const usedMem = totalMem - freeMem;
    
    let memInfo = {
      total: totalMem,
      free: freeMem,
      used: usedMem,
      totalGB: Math.round(totalMem / (1024 ** 3)),
      freeGB: Math.round(freeMem / (1024 ** 3)),
      usedGB: Math.round(usedMem / (1024 ** 3)),
      usagePercent: Math.round((usedMem / totalMem) * 100),
      speed: 0,
      type: 'Unknown'
    };
    
    const platform = os.platform();
    
    if (platform === 'win32') {
      try {
        // Get RAM speed from WMIC
        const wmicRAM = execSync('wmic memorychip get Speed,SMBIOSMemoryType /format:csv', { encoding: 'utf8' });
        const lines = wmicRAM.split('\n').filter(l => l.trim() && !l.startsWith('Node'));
        
        if (lines.length > 0) {
          const parts = lines[0].split(',');
          memInfo.speed = parseInt(parts[0]) || 0;
          
          // Decode memory type
          const typeCode = parseInt(parts[1]) || 0;
          const types = { 0: 'Unknown', 20: 'DDR', 21: 'DDR2', 24: 'DDR3', 26: 'DDR4', 34: 'DDR5' };
          memInfo.type = types[typeCode] || 'Unknown';
        }
      } catch (error) {
        console.error('[Optimizer] RAM detection error:', error);
      }
    }
    
    return memInfo;
  }
  
  async detectGPU() {
    const platform = os.platform();
    let gpuInfo = {
      detected: false,
      vendor: 'Unknown',
      model: 'Unknown',
      vram: 0,
      driver: 'Unknown'
    };
    
    if (platform === 'win32') {
      try {
        // Get GPU info from WMIC
        const wmicGPU = execSync('wmic path win32_VideoController get Name,AdapterRAM,DriverVersion /format:csv', { encoding: 'utf8' });
        const lines = wmicGPU.split('\n').filter(l => l.trim() && !l.startsWith('Node'));
        
        if (lines.length > 0) {
          const parts = lines[0].split(',');
          gpuInfo.detected = true;
          gpuInfo.vram = parseInt(parts[0]) || 0;
          gpuInfo.driver = parts[1] || 'Unknown';
          gpuInfo.model = parts[2] || 'Unknown';
          
          // Detect vendor
          if (gpuInfo.model.includes('NVIDIA') || gpuInfo.model.includes('GeForce') || gpuInfo.model.includes('RTX')) {
            gpuInfo.vendor = 'NVIDIA';
          } else if (gpuInfo.model.includes('AMD') || gpuInfo.model.includes('Radeon')) {
            gpuInfo.vendor = 'AMD';
          } else if (gpuInfo.model.includes('Intel')) {
            gpuInfo.vendor = 'Intel';
          }
        }
      } catch (error) {
        console.error('[Optimizer] GPU detection error:', error);
      }
    } else if (platform === 'linux') {
      try {
        const lspci = execSync('lspci | grep -i vga', { encoding: 'utf8' });
        gpuInfo.model = lspci.trim();
        gpuInfo.detected = true;
        
        if (lspci.includes('NVIDIA')) gpuInfo.vendor = 'NVIDIA';
        if (lspci.includes('AMD')) gpuInfo.vendor = 'AMD';
        if (lspci.includes('Intel')) gpuInfo.vendor = 'Intel';
      } catch (error) {
        console.error('[Optimizer] lspci error:', error);
      }
    }
    
    return gpuInfo;
  }
  
  async detectStorage() {
    const platform = os.platform();
    let storageInfo = {
      drives: [],
      totalSpace: 0,
      freeSpace: 0
    };
    
    if (platform === 'win32') {
      try {
        // Get disk info from WMIC
        const wmicDisk = execSync('wmic logicaldisk get DeviceID,Size,FreeSpace,MediaType /format:csv', { encoding: 'utf8' });
        const lines = wmicDisk.split('\n').filter(l => l.trim() && !l.startsWith('Node'));
        
        lines.forEach(line => {
          const parts = line.split(',');
          if (parts.length >= 4) {
            const drive = {
              letter: parts[0],
              size: parseInt(parts[2]) || 0,
              free: parseInt(parts[1]) || 0,
              type: parseInt(parts[3]) === 12 ? 'SSD/NVMe' : 'HDD'
            };
            
            storageInfo.drives.push(drive);
            storageInfo.totalSpace += drive.size;
            storageInfo.freeSpace += drive.free;
          }
        });
      } catch (error) {
        console.error('[Optimizer] Storage detection error:', error);
      }
    } else if (platform === 'linux') {
      try {
        const df = execSync('df -B1', { encoding: 'utf8' });
        const lines = df.split('\n').slice(1);
        
        lines.forEach(line => {
          const parts = line.trim().split(/\s+/);
          if (parts.length >= 6) {
            const drive = {
              device: parts[0],
              size: parseInt(parts[1]) || 0,
              free: parseInt(parts[3]) || 0,
              mount: parts[5]
            };
            
            storageInfo.drives.push(drive);
            storageInfo.totalSpace += drive.size;
            storageInfo.freeSpace += drive.free;
          }
        });
      } catch (error) {
        console.error('[Optimizer] df error:', error);
      }
    }
    
    return storageInfo;
  }
  
  async detectNetwork() {
    const interfaces = os.networkInterfaces();
    const networkInfo = {
      interfaces: [],
      activeConnections: 0
    };
    
    Object.keys(interfaces).forEach(name => {
      const iface = interfaces[name].find(i => i.family === 'IPv4' && !i.internal);
      if (iface) {
        networkInfo.interfaces.push({
          name,
          address: iface.address,
          mac: iface.mac,
          netmask: iface.netmask
        });
        networkInfo.activeConnections++;
      }
    });
    
    return networkInfo;
  }
  
  calculateOptimalSettings(systemInfo) {
    const settings = {
      // Worker threads based on CPU
      workerThreads: systemInfo.cpu.threads,
      maxWorkers: Math.max(4, Math.floor(systemInfo.cpu.threads * 0.75)), // 75% of threads
      
      // Memory allocation
      maxMemoryPerAgent: Math.floor((systemInfo.memory.totalGB * 0.8) / 200), // 80% RAM for 200 agents
      nodeMaxOldSpace: Math.floor(systemInfo.memory.totalGB * 1024 * 0.6), // 60% RAM for Node.js
      
      // Swarm settings
      swarmSize: 200,
      parallelBatchSize: systemInfo.cpu.threads,
      
      // Cache optimization (7800X3D has 96MB L3)
      cacheOptimized: systemInfo.cpu.cache.L3 > 32 * 1024, // 32MB threshold
      prefetchDistance: systemInfo.cpu.cache.L3 > 64 * 1024 ? 64 : 32,
      
      // GPU acceleration
      gpuAcceleration: systemInfo.gpu.detected && systemInfo.gpu.vram > 2 * 1024 * 1024 * 1024,
      
      // Storage settings
      tempDir: systemInfo.storage.drives.find(d => d.type === 'SSD/NVMe') ? 
                systemInfo.storage.drives[0].letter + ':\\temp' : 
                os.tmpdir(),
      
      // Network settings
      maxConnections: 100,
      connectionPoolSize: Math.min(50, systemInfo.network.activeConnections * 10),
      
      // Performance settings
      frameRateLimit: 240,
      vsync: false,
      hardwareAcceleration: true,
      
      // Monaco Editor
      monacoWorkers: 4,
      monacoMemory: 512,
      
      // AI Model settings
      modelConcurrency: Math.floor(systemInfo.cpu.threads / 2),
      contextWindowSize: 128000,
      
      // Auto-detected optimizations
      optimizations: {
        amdRyzenOptimized: systemInfo.cpu.vendor === 'AMD' && systemInfo.cpu.model.includes('Ryzen'),
        x3dCacheOptimized: systemInfo.cpu.cache.L3 > 64 * 1024,
        ddr5Optimized: systemInfo.memory.type === 'DDR5',
        nvmeOptimized: systemInfo.storage.drives.some(d => d.type === 'SSD/NVMe')
      }
    };
    
    console.log('[Optimizer] Optimal settings calculated:', settings);
    return settings;
  }
  
  applySettings(settings = this.optimalSettings) {
    if (!settings) {
      console.error('[Optimizer] No settings to apply');
      return false;
    }
    
    console.log('[Optimizer] Applying settings...');
    
    try {
      // Apply Node.js memory limit
      if (global.gc) {
        process.env.NODE_OPTIONS = `--max-old-space-size=${settings.nodeMaxOldSpace}`;
      }
      
      // Apply to Electron
      if (process.type === 'browser') {
        const { app } = require('electron');
        
        // GPU settings
        if (settings.hardwareAcceleration) {
          app.commandLine.appendSwitch('enable-gpu-rasterization');
          app.commandLine.appendSwitch('enable-zero-copy');
        }
        
        // Frame rate
        if (settings.frameRateLimit) {
          app.commandLine.appendSwitch('disable-frame-rate-limit');
        }
        
        // Memory
        app.commandLine.appendSwitch('js-flags', `--max-old-space-size=${settings.nodeMaxOldSpace}`);
      }
      
      // Save as current settings
      this.currentSettings = settings;
      this.saveSettings(settings);
      
      console.log('[Optimizer] ✅ Settings applied successfully');
      return true;
      
    } catch (error) {
      console.error('[Optimizer] Error applying settings:', error);
      return false;
    }
  }
  
  resetToOptimal() {
    console.log('[Optimizer] Resetting to optimal settings...');
    
    if (!this.optimalSettings) {
      console.error('[Optimizer] No optimal settings calculated');
      return false;
    }
    
    return this.applySettings(this.optimalSettings);
  }
  
  resetToDefaults() {
    console.log('[Optimizer] Resetting to default settings...');
    
    const defaults = {
      workerThreads: 16,
      maxWorkers: 12,
      maxMemoryPerAgent: 200,
      nodeMaxOldSpace: 4096,
      swarmSize: 200,
      parallelBatchSize: 16,
      cacheOptimized: false,
      prefetchDistance: 32,
      gpuAcceleration: false,
      maxConnections: 100,
      connectionPoolSize: 50,
      frameRateLimit: 60,
      vsync: true,
      hardwareAcceleration: true,
      monacoWorkers: 4,
      monacoMemory: 512,
      modelConcurrency: 4,
      contextWindowSize: 128000
    };
    
    return this.applySettings(defaults);
  }
  
  tweakSetting(key, value) {
    if (!this.currentSettings) {
      console.error('[Optimizer] No current settings loaded');
      return false;
    }
    
    console.log(`[Optimizer] Tweaking ${key} = ${value}`);
    
    this.currentSettings[key] = value;
    this.saveSettings(this.currentSettings);
    
    return true;
  }
  
  loadSettings() {
    try {
      if (fs.existsSync(this.configPath)) {
        const data = fs.readFileSync(this.configPath, 'utf8');
        const settings = JSON.parse(data);
        console.log('[Optimizer] Loaded saved settings');
        return settings;
      }
    } catch (error) {
      console.error('[Optimizer] Error loading settings:', error);
    }
    return null;
  }
  
  saveSettings(settings) {
    try {
      const dir = path.dirname(this.configPath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      
      fs.writeFileSync(this.configPath, JSON.stringify(settings, null, 2));
      console.log('[Optimizer] Settings saved to:', this.configPath);
      return true;
    } catch (error) {
      console.error('[Optimizer] Error saving settings:', error);
      return false;
    }
  }
  
  getSystemInfo() {
    return this.systemInfo;
  }
  
  getCurrentSettings() {
    return this.currentSettings;
  }
  
  getOptimalSettings() {
    return this.optimalSettings;
  }
  
  compareSettings() {
    if (!this.currentSettings || !this.optimalSettings) {
      return null;
    }
    
    const comparison = {};
    Object.keys(this.optimalSettings).forEach(key => {
      if (this.optimalSettings[key] !== this.currentSettings[key]) {
        comparison[key] = {
          current: this.currentSettings[key],
          optimal: this.optimalSettings[key],
          difference: this.optimalSettings[key] - this.currentSettings[key]
        };
      }
    });
    
    return comparison;
  }
  
  getPerformanceScore() {
    if (!this.currentSettings || !this.optimalSettings) {
      return 0;
    }
    
    let score = 100;
    const comparison = this.compareSettings();
    
    if (comparison) {
      const diffCount = Object.keys(comparison).length;
      const totalSettings = Object.keys(this.optimalSettings).length;
      score = Math.max(0, 100 - (diffCount / totalSettings) * 100);
    }
    
    return Math.round(score);
  }
  
  generateReport() {
    return {
      systemInfo: this.systemInfo,
      currentSettings: this.currentSettings,
      optimalSettings: this.optimalSettings,
      comparison: this.compareSettings(),
      performanceScore: this.getPerformanceScore(),
      timestamp: Date.now()
    };
  }
}

module.exports = SystemOptimizer;

