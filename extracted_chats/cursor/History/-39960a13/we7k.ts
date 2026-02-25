// BigDaddyGEngine/extensions/HotReloadEngine.ts
// Hot-Reload Engine for Extensions

import { ExtensionManifest } from './manager';

export interface HotReloadConfig {
  enabled: boolean;
  watchInterval: number;
  autoReload: boolean;
}

export class HotReloadEngine {
  private watchers: Map<string, FileSystemWatcher> = new Map();
  private config: HotReloadConfig = {
    enabled: true,
    watchInterval: 1000,
    autoReload: true
  };

  /**
   * Start watching an extension for changes
   */
  watchExtension(extensionId: string, extensionPath: string): void {
    if (!this.config.enabled) return;

    console.log(`🔥 Starting hot reload watcher for: ${extensionId}`);

    const watcher = new FileSystemWatcher(
      extensionPath,
      this.config.watchInterval,
      (changedFiles: string[]) => {
        console.log(`🔄 Files changed in ${extensionId}:`, changedFiles);
        
        if (this.config.autoReload) {
          this.reloadExtension(extensionId);
        }
      }
    );

    this.watchers.set(extensionId, watcher);
    watcher.start();
  }

  /**
   * Stop watching an extension
   */
  unwatchExtension(extensionId: string): void {
    const watcher = this.watchers.get(extensionId);
    if (watcher) {
      watcher.stop();
      this.watchers.delete(extensionId);
      console.log(`🔥 Stopped watching: ${extensionId}`);
    }
  }

  /**
   * Reload an extension
   */
  async reloadExtension(extensionId: string): Promise<void> {
    console.log(`♻️ Reloading extension: ${extensionId}`);

    try {
      // Import the reloadable extension
      const { ExtensionManager } = await import('./manager');
      const manager = new ExtensionManager();

      // Reload the extension module
      const extensionPath = `./extensions/${extensionId}`;
      
      // Cache-bust to force reload
      const timestamp = Date.now();
      const module = await import(`${extensionPath}?reload=${timestamp}`);

      console.log(`✅ Extension ${extensionId} reloaded successfully`);
      
      // Emit reload event for UI updates
      window.dispatchEvent(new CustomEvent('extension:reloaded', { 
        detail: { extensionId } 
      }));

    } catch (error) {
      console.error(`❌ Failed to reload extension ${extensionId}:`, error);
    }
  }

  /**
   * Reload all watched extensions
   */
  async reloadAll(): Promise<void> {
    const extensionIds = Array.from(this.watchers.keys());
    
    for (const id of extensionIds) {
      await this.reloadExtension(id);
    }
  }

  /**
   * Update hot reload configuration
   */
  updateConfig(config: Partial<HotReloadConfig>): void {
    this.config = { ...this.config, ...config };
    
    if (!config.enabled) {
      this.stopAll();
    }
  }

  /**
   * Stop all watchers
   */
  stopAll(): void {
    this.watchers.forEach((watcher, id) => {
      watcher.stop();
      console.log(`🛑 Stopped watching: ${id}`);
    });
    this.watchers.clear();
  }

  /**
   * Get status of hot reload system
   */
  getStatus(): {
    enabled: boolean;
    watching: string[];
    watchersCount: number;
  } {
    return {
      enabled: this.config.enabled,
      watching: Array.from(this.watchers.keys()),
      watchersCount: this.watchers.size
    };
  }
}

/**
 * File system watcher implementation
 */
class FileSystemWatcher {
  private path: string;
  private interval: number;
  private callback: (files: string[]) => void;
  private intervalId: NodeJS.Timeout | null = null;
  private lastModified: Map<string, number> = new Map();

  constructor(path: string, interval: number, callback: (files: string[]) => void) {
    this.path = path;
    this.interval = interval;
    this.callback = callback;
  }

  start(): void {
    if (this.intervalId) return;

    // Initial scan
    this.scan();

    // Periodic scans
    this.intervalId = setInterval(() => {
      this.scan();
    }, this.interval);
  }

  stop(): void {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }

  private async scan(): Promise<void> {
    try {
      // In a real implementation, this would check file timestamps
      // For now, we simulate with random file changes
      const changedFiles: string[] = [];

      // Simulate file change detection
      if (Math.random() > 0.95) {
        const files = ['main.ts', 'extension.ts', 'package.json'];
        const changed = files[Math.floor(Math.random() * files.length)];
        changedFiles.push(changed);
      }

      if (changedFiles.length > 0) {
        this.callback(changedFiles);
      }
    } catch (error) {
      console.error('File system scan error:', error);
    }
  }
}

// Global hot reload engine instance
let globalHotReloadEngine: HotReloadEngine | null = null;

export function getGlobalHotReloadEngine(): HotReloadEngine {
  if (!globalHotReloadEngine) {
    globalHotReloadEngine = new HotReloadEngine();
  }
  return globalHotReloadEngine;
}

/**
 * Initialize hot reload for an extension
 */
export async function enableHotReload(extensionId: string, extensionPath: string): Promise<void> {
  const engine = getGlobalHotReloadEngine();
  engine.watchExtension(extensionId, extensionPath);
}

/**
 * Disable hot reload for an extension
 */
export function disableHotReload(extensionId: string): void {
  const engine = getGlobalHotReloadEngine();
  engine.unwatchExtension(extensionId);
}
