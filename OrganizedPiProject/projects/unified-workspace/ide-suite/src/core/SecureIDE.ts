import { EventEmitter } from 'events';
import { SecurityManager } from './SecurityManager.js';
import { FileManager } from './FileManager.js';
import { AIEngine } from './AIEngine.js';
import { ExtensionManager } from './ExtensionManager.js';
import { EditorManager } from './EditorManager.js';
import { TerminalManager } from './TerminalManager.js';
import { WebSocketServer } from '../server/WebSocketServer.js';
import { HttpServer } from '../server/HttpServer.js';

export interface IDEConfig {
  port: number;
  host: string;
  workspace: string;
  security: {
    enableSandbox: boolean;
    allowNetworkAccess: boolean;
    maxFileSize: number;
    allowedFileTypes: string[];
  };
  ai: {
    enableLocalAI: boolean;
    modelPath?: string;
    maxTokens: number;
  };
  extensions: {
    enableExtensions: boolean;
    trustedExtensions: string[];
  };
}

export class SecureIDE extends EventEmitter {
  private config: IDEConfig;
  private securityManager: SecurityManager;
  private fileManager: FileManager;
  private aiEngine: AIEngine;
  private extensionManager: ExtensionManager;
  private editorManager: EditorManager;
  private terminalManager: TerminalManager;
  private httpServer: HttpServer;
  private wsServer: WebSocketServer;
  private isRunning: boolean = false;

  constructor(config: IDEConfig) {
    super();
    this.config = config;
    this.initializeComponents();
  }

  private initializeComponents(): void {
    // Initialize security manager first
    this.securityManager = new SecurityManager(this.config.security);
    
    // Initialize core components
    this.fileManager = new FileManager(this.config.workspace, this.securityManager);
    this.aiEngine = new AIEngine(this.config.ai);
    this.extensionManager = new ExtensionManager(this.config.extensions, this.securityManager);
    this.editorManager = new EditorManager(this.fileManager, this.aiEngine);
    this.terminalManager = new TerminalManager(this.securityManager);
    
    // Initialize servers
    this.httpServer = new HttpServer(this.config.port, this.config.host, {
      fileManager: this.fileManager,
      editorManager: this.editorManager,
      aiEngine: this.aiEngine,
      extensionManager: this.extensionManager,
      terminalManager: this.terminalManager,
    });
    
    this.wsServer = new WebSocketServer(this.config.port + 1, {
      fileManager: this.fileManager,
      editorManager: this.editorManager,
      aiEngine: this.aiEngine,
      extensionManager: this.extensionManager,
      terminalManager: this.terminalManager,
    });

    this.setupEventHandlers();
  }

  private setupEventHandlers(): void {
    // Security events
    this.securityManager.on('security-violation', (violation) => {
      console.error('Security violation detected:', violation);
      this.emit('security-violation', violation);
    });

    // File system events
    this.fileManager.on('file-changed', (file) => {
      this.editorManager.handleFileChange(file);
      this.emit('file-changed', file);
    });

    // AI events
    this.aiEngine.on('ai-response', (response) => {
      this.emit('ai-response', response);
    });

    // Extension events
    this.extensionManager.on('extension-loaded', (extension) => {
      console.log('Extension loaded:', extension.name);
      this.emit('extension-loaded', extension);
    });

    this.extensionManager.on('extension-error', (error) => {
      console.error('Extension error:', error);
      this.emit('extension-error', error);
    });
  }

  async start(): Promise<void> {
    if (this.isRunning) {
      throw new Error('IDE is already running');
    }

    try {
      console.log('Starting Secure IDE...');
      
      // Initialize security
      await this.securityManager.initialize();
      
      // Initialize file manager
      await this.fileManager.initialize();
      
      // Initialize AI engine
      if (this.config.ai.enableLocalAI) {
        await this.aiEngine.initialize();
      }
      
      // Initialize extensions
      if (this.config.extensions.enableExtensions) {
        await this.extensionManager.initialize();
      }
      
      // Start servers
      await this.httpServer.start();
      await this.wsServer.start();
      
      this.isRunning = true;
      console.log(`Secure IDE started on http://${this.config.host}:${this.config.port}`);
      this.emit('started');
      
    } catch (error) {
      console.error('Failed to start Secure IDE:', error);
      this.emit('error', error);
      throw error;
    }
  }

  async stop(): Promise<void> {
    if (!this.isRunning) {
      return;
    }

    try {
      console.log('Stopping Secure IDE...');
      
      // Stop servers
      await this.httpServer.stop();
      await this.wsServer.stop();
      
      // Cleanup components
      await this.extensionManager.cleanup();
      await this.aiEngine.cleanup();
      await this.fileManager.cleanup();
      await this.securityManager.cleanup();
      
      this.isRunning = false;
      console.log('Secure IDE stopped');
      this.emit('stopped');
      
    } catch (error) {
      console.error('Error stopping Secure IDE:', error);
      this.emit('error', error);
    }
  }

  // Public API methods
  getFileManager(): FileManager {
    return this.fileManager;
  }

  getAIEngine(): AIEngine {
    return this.aiEngine;
  }

  getExtensionManager(): ExtensionManager {
    return this.extensionManager;
  }

  getEditorManager(): EditorManager {
    return this.editorManager;
  }

  getTerminalManager(): TerminalManager {
    return this.terminalManager;
  }

  getSecurityManager(): SecurityManager {
    return this.securityManager;
  }

  isRunning(): boolean {
    return this.isRunning;
  }

  getConfig(): IDEConfig {
    return { ...this.config };
  }

  updateConfig(newConfig: Partial<IDEConfig>): void {
    this.config = { ...this.config, ...newConfig };
    this.emit('config-updated', this.config);
  }
}
