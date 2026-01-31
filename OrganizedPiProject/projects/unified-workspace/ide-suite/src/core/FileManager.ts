import { EventEmitter } from 'events';
import * as fs from 'fs/promises';
import * as path from 'path';
import { SecurityManager } from './SecurityManager.js';

export interface FileInfo {
  path: string;
  name: string;
  size: number;
  isDirectory: boolean;
  isFile: boolean;
  lastModified: Date;
  permissions: string;
}

export interface FileChangeEvent {
  type: 'created' | 'modified' | 'deleted' | 'renamed';
  path: string;
  oldPath?: string;
  timestamp: Date;
}

export class FileManager extends EventEmitter {
  private workspace: string;
  private securityManager: SecurityManager;
  private watchers: Map<string, any> = new Map();
  private isInitialized: boolean = false;

  constructor(workspace: string, securityManager: SecurityManager) {
    super();
    this.workspace = path.resolve(workspace);
    this.securityManager = securityManager;
  }

  async initialize(): Promise<void> {
    console.log('Initializing File Manager...');
    
    try {
      // Ensure workspace exists
      await this.ensureWorkspace();
      
      // Set up file watching
      await this.setupFileWatching();
      
      this.isInitialized = true;
      console.log('File Manager initialized');
    } catch (error) {
      console.error('Failed to initialize File Manager:', error);
      throw error;
    }
  }

  private async ensureWorkspace(): Promise<void> {
    try {
      await fs.access(this.workspace);
    } catch {
      await fs.mkdir(this.workspace, { recursive: true });
    }
  }

  private async setupFileWatching(): Promise<void> {
    // Watch the workspace directory for changes
    try {
      const { watch } = await import('chokidar');
      const watcher = watch(this.workspace, {
        ignored: /(^|[\/\\])\../, // ignore dotfiles
        persistent: true,
        ignoreInitial: true,
      });

      watcher.on('add', (filePath) => this.handleFileChange('created', filePath));
      watcher.on('change', (filePath) => this.handleFileChange('modified', filePath));
      watcher.on('unlink', (filePath) => this.handleFileChange('deleted', filePath));
      watcher.on('move', (oldPath, newPath) => this.handleFileChange('renamed', newPath, oldPath));

      this.watchers.set('workspace', watcher);
    } catch (error) {
      console.error('Failed to setup file watching:', error);
    }
  }

  private handleFileChange(type: string, path: string, oldPath?: string): void {
    const event: FileChangeEvent = {
      type: type as any,
      path,
      oldPath,
      timestamp: new Date(),
    };
    
    this.emit('file-changed', event);
  }

  // File operations
  async readFile(filePath: string): Promise<string> {
    const fullPath = this.resolvePath(filePath);
    
    // Security check
    const hasAccess = await this.securityManager.validateFileAccess(fullPath, 'read');
    if (!hasAccess) {
      throw new Error('Access denied to read file');
    }

    try {
      const content = await fs.readFile(fullPath, 'utf8');
      return content;
    } catch (error) {
      console.error(`Error reading file ${filePath}:`, error);
      throw error;
    }
  }

  async writeFile(filePath: string, content: string): Promise<void> {
    const fullPath = this.resolvePath(filePath);
    
    // Security check
    const hasAccess = await this.securityManager.validateFileAccess(fullPath, 'write');
    if (!hasAccess) {
      throw new Error('Access denied to write file');
    }

    try {
      // Ensure directory exists
      const dir = path.dirname(fullPath);
      await fs.mkdir(dir, { recursive: true });
      
      await fs.writeFile(fullPath, content, 'utf8');
      
      this.emit('file-written', { path: fullPath, content });
    } catch (error) {
      console.error(`Error writing file ${filePath}:`, error);
      throw error;
    }
  }

  async deleteFile(filePath: string): Promise<void> {
    const fullPath = this.resolvePath(filePath);
    
    // Security check
    const hasAccess = await this.securityManager.validateFileAccess(fullPath, 'delete');
    if (!hasAccess) {
      throw new Error('Access denied to delete file');
    }

    try {
      await fs.unlink(fullPath);
      this.emit('file-deleted', { path: fullPath });
    } catch (error) {
      console.error(`Error deleting file ${filePath}:`, error);
      throw error;
    }
  }

  async createDirectory(dirPath: string): Promise<void> {
    const fullPath = this.resolvePath(dirPath);
    
    try {
      await fs.mkdir(fullPath, { recursive: true });
      this.emit('directory-created', { path: fullPath });
    } catch (error) {
      console.error(`Error creating directory ${dirPath}:`, error);
      throw error;
    }
  }

  async deleteDirectory(dirPath: string): Promise<void> {
    const fullPath = this.resolvePath(dirPath);
    
    try {
      await fs.rmdir(fullPath, { recursive: true });
      this.emit('directory-deleted', { path: fullPath });
    } catch (error) {
      console.error(`Error deleting directory ${dirPath}:`, error);
      throw error;
    }
  }

  async renameFile(oldPath: string, newPath: string): Promise<void> {
    const fullOldPath = this.resolvePath(oldPath);
    const fullNewPath = this.resolvePath(newPath);
    
    // Security checks
    const hasReadAccess = await this.securityManager.validateFileAccess(fullOldPath, 'read');
    const hasWriteAccess = await this.securityManager.validateFileAccess(fullNewPath, 'write');
    
    if (!hasReadAccess || !hasWriteAccess) {
      throw new Error('Access denied to rename file');
    }

    try {
      await fs.rename(fullOldPath, fullNewPath);
      this.emit('file-renamed', { oldPath: fullOldPath, newPath: fullNewPath });
    } catch (error) {
      console.error(`Error renaming file ${oldPath} to ${newPath}:`, error);
      throw error;
    }
  }

  // Directory operations
  async listDirectory(dirPath: string = ''): Promise<FileInfo[]> {
    const fullPath = this.resolvePath(dirPath);
    
    try {
      const entries = await fs.readdir(fullPath, { withFileTypes: true });
      const fileInfos: FileInfo[] = [];
      
      for (const entry of entries) {
        const entryPath = path.join(fullPath, entry.name);
        const stats = await fs.stat(entryPath);
        
        fileInfos.push({
          path: entryPath,
          name: entry.name,
          size: stats.size,
          isDirectory: entry.isDirectory(),
          isFile: entry.isFile(),
          lastModified: stats.mtime,
          permissions: stats.mode.toString(8),
        });
      }
      
      return fileInfos.sort((a, b) => {
        // Directories first, then files
        if (a.isDirectory && !b.isDirectory) return -1;
        if (!a.isDirectory && b.isDirectory) return 1;
        return a.name.localeCompare(b.name);
      });
    } catch (error) {
      console.error(`Error listing directory ${dirPath}:`, error);
      throw error;
    }
  }

  async getFileInfo(filePath: string): Promise<FileInfo> {
    const fullPath = this.resolvePath(filePath);
    
    try {
      const stats = await fs.stat(fullPath);
      
      return {
        path: fullPath,
        name: path.basename(fullPath),
        size: stats.size,
        isDirectory: stats.isDirectory(),
        isFile: stats.isFile(),
        lastModified: stats.mtime,
        permissions: stats.mode.toString(8),
      };
    } catch (error) {
      console.error(`Error getting file info for ${filePath}:`, error);
      throw error;
    }
  }

  // Search operations
  async searchFiles(query: string, options: {
    includePattern?: string;
    excludePattern?: string;
    caseSensitive?: boolean;
    maxResults?: number;
  } = {}): Promise<FileInfo[]> {
    const {
      includePattern = '**/*',
      excludePattern,
      caseSensitive = false,
      maxResults = 100,
    } = options;

    const results: FileInfo[] = [];
    const searchRegex = new RegExp(
      caseSensitive ? query : query.toLowerCase(),
      caseSensitive ? 'g' : 'gi'
    );

    try {
      await this.searchDirectory(this.workspace, searchRegex, results, {
        includePattern,
        excludePattern,
        maxResults,
      });
    } catch (error) {
      console.error('Error searching files:', error);
    }

    return results;
  }

  private async searchDirectory(
    dirPath: string,
    regex: RegExp,
    results: FileInfo[],
    options: {
      includePattern: string;
      excludePattern?: string;
      maxResults: number;
    }
  ): Promise<void> {
    if (results.length >= options.maxResults) return;

    try {
      const entries = await fs.readdir(dirPath, { withFileTypes: true });
      
      for (const entry of entries) {
        if (results.length >= options.maxResults) break;
        
        const entryPath = path.join(dirPath, entry.name);
        
        // Check exclude pattern
        if (options.excludePattern && entryPath.match(options.excludePattern)) {
          continue;
        }
        
        if (entry.isDirectory()) {
          await this.searchDirectory(entryPath, regex, results, options);
        } else if (entry.isFile()) {
          // Check if file matches include pattern
          if (entryPath.match(options.includePattern)) {
            const content = await fs.readFile(entryPath, 'utf8');
            if (regex.test(content)) {
              const stats = await fs.stat(entryPath);
              results.push({
                path: entryPath,
                name: entry.name,
                size: stats.size,
                isDirectory: false,
                isFile: true,
                lastModified: stats.mtime,
                permissions: stats.mode.toString(8),
              });
            }
          }
        }
      }
    } catch (error) {
      // Skip directories we can't access
      console.warn(`Cannot search directory ${dirPath}:`, error);
    }
  }

  // Utility methods
  private resolvePath(filePath: string): string {
    if (path.isAbsolute(filePath)) {
      return filePath;
    }
    return path.resolve(this.workspace, filePath);
  }

  getWorkspace(): string {
    return this.workspace;
  }

  setWorkspace(newWorkspace: string): void {
    this.workspace = path.resolve(newWorkspace);
    this.emit('workspace-changed', this.workspace);
  }

  async cleanup(): Promise<void> {
    console.log('Cleaning up File Manager...');
    
    // Close all watchers
    for (const [name, watcher] of this.watchers) {
      try {
        await watcher.close();
      } catch (error) {
        console.error(`Error closing watcher ${name}:`, error);
      }
    }
    
    this.watchers.clear();
    this.isInitialized = false;
  }

  getStatus(): { isInitialized: boolean; workspace: string; watchers: string[] } {
    return {
      isInitialized: this.isInitialized,
      workspace: this.workspace,
      watchers: Array.from(this.watchers.keys()),
    };
  }
}
