import { EventEmitter } from 'events';
import * as crypto from 'crypto';
import * as fs from 'fs/promises';
import * as path from 'path';
import { createHash } from 'crypto';

export interface SecurityConfig {
  enableSandbox: boolean;
  allowNetworkAccess: boolean;
  maxFileSize: number;
  allowedFileTypes: string[];
}

export interface SecurityViolation {
  type: 'file-access' | 'network-access' | 'extension-execution' | 'resource-usage';
  message: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  timestamp: Date;
  details?: any;
}

export class SecurityManager extends EventEmitter {
  private config: SecurityConfig;
  private allowedPaths: Set<string> = new Set();
  private blockedPaths: Set<string> = new Set();
  private fileHashes: Map<string, string> = new Map();
  private resourceUsage: Map<string, number> = new Map();
  private isInitialized: boolean = false;

  constructor(config: SecurityConfig) {
    super();
    this.config = config;
  }

  async initialize(): Promise<void> {
    console.log('Initializing Security Manager...');
    
    // Create security audit log
    await this.createAuditLog();
    
    // Initialize file integrity monitoring
    await this.initializeFileIntegrity();
    
    // Set up resource monitoring
    this.setupResourceMonitoring();
    
    this.isInitialized = true;
    console.log('Security Manager initialized');
  }

  private async createAuditLog(): Promise<void> {
    const auditDir = path.join(process.cwd(), 'security', 'audit');
    await fs.mkdir(auditDir, { recursive: true });
    
    const auditFile = path.join(auditDir, `audit-${Date.now()}.log`);
    await fs.writeFile(auditFile, `Security Audit Log Started: ${new Date().toISOString()}\n`);
  }

  private async initializeFileIntegrity(): Promise<void> {
    // Calculate hashes for critical system files
    const criticalFiles = [
      'package.json',
      'src/core/SecurityManager.ts',
      'src/core/SecureIDE.ts',
    ];
    
    for (const file of criticalFiles) {
      try {
        const filePath = path.resolve(file);
        const content = await fs.readFile(filePath, 'utf8');
        const hash = createHash('sha256').update(content).digest('hex');
        this.fileHashes.set(filePath, hash);
      } catch (error) {
        console.warn(`Could not hash file ${file}:`, error);
      }
    }
  }

  private setupResourceMonitoring(): void {
    // Monitor memory usage
    setInterval(() => {
      const memUsage = process.memoryUsage();
      this.resourceUsage.set('memory', memUsage.heapUsed);
      
      if (memUsage.heapUsed > 1024 * 1024 * 1024) { // 1GB
        this.emitViolation({
          type: 'resource-usage',
          message: 'High memory usage detected',
          severity: 'medium',
          timestamp: new Date(),
          details: { memoryUsage: memUsage.heapUsed },
        });
      }
    }, 30000); // Check every 30 seconds
  }

  // File access security
  async validateFileAccess(filePath: string, operation: 'read' | 'write' | 'delete'): Promise<boolean> {
    const resolvedPath = path.resolve(filePath);
    
    // Check if path is blocked
    if (this.blockedPaths.has(resolvedPath)) {
      this.emitViolation({
        type: 'file-access',
        message: `Blocked file access attempt: ${operation} on ${resolvedPath}`,
        severity: 'high',
        timestamp: new Date(),
        details: { filePath: resolvedPath, operation },
      });
      return false;
    }
    
    // Check file type restrictions
    const ext = path.extname(resolvedPath).toLowerCase();
    if (this.config.allowedFileTypes.length > 0 && !this.config.allowedFileTypes.includes(ext)) {
      this.emitViolation({
        type: 'file-access',
        message: `Disallowed file type access: ${ext}`,
        severity: 'medium',
        timestamp: new Date(),
        details: { filePath: resolvedPath, fileType: ext },
      });
      return false;
    }
    
    // Check file size for write operations
    if (operation === 'write') {
      try {
        const stats = await fs.stat(resolvedPath);
        if (stats.size > this.config.maxFileSize) {
          this.emitViolation({
            type: 'file-access',
            message: `File size exceeds limit: ${stats.size} bytes`,
            severity: 'medium',
            timestamp: new Date(),
            details: { filePath: resolvedPath, fileSize: stats.size },
          });
          return false;
        }
      } catch (error) {
        // File doesn't exist yet, that's okay for write operations
      }
    }
    
    return true;
  }

  // Network access security
  validateNetworkAccess(url: string): boolean {
    if (!this.config.allowNetworkAccess) {
      this.emitViolation({
        type: 'network-access',
        message: `Network access blocked: ${url}`,
        severity: 'high',
        timestamp: new Date(),
        details: { url },
      });
      return false;
    }
    
    // Additional URL validation can be added here
    return true;
  }

  // Extension execution security
  validateExtensionExecution(extensionPath: string, permissions: string[]): boolean {
    // Check if extension is in trusted list
    if (!this.config.allowedFileTypes.includes(path.extname(extensionPath))) {
      this.emitViolation({
        type: 'extension-execution',
        message: `Untrusted extension execution: ${extensionPath}`,
        severity: 'critical',
        timestamp: new Date(),
        details: { extensionPath, permissions },
      });
      return false;
    }
    
    return true;
  }

  // File integrity checking
  async checkFileIntegrity(filePath: string): Promise<boolean> {
    const resolvedPath = path.resolve(filePath);
    const storedHash = this.fileHashes.get(resolvedPath);
    
    if (!storedHash) {
      return true; // File not in integrity check list
    }
    
    try {
      const content = await fs.readFile(resolvedPath, 'utf8');
      const currentHash = createHash('sha256').update(content).digest('hex');
      
      if (currentHash !== storedHash) {
        this.emitViolation({
          type: 'file-access',
          message: `File integrity violation: ${resolvedPath}`,
          severity: 'critical',
          timestamp: new Date(),
          details: { filePath: resolvedPath, expectedHash: storedHash, actualHash: currentHash },
        });
        return false;
      }
      
      return true;
    } catch (error) {
      console.error(`Error checking file integrity for ${resolvedPath}:`, error);
      return false;
    }
  }

  // Add allowed path
  addAllowedPath(path: string): void {
    this.allowedPaths.add(path.resolve(path));
  }

  // Add blocked path
  addBlockedPath(path: string): void {
    this.blockedPaths.add(path.resolve(path));
  }

  // Generate secure token
  generateSecureToken(): string {
    return crypto.randomBytes(32).toString('hex');
  }

  // Hash sensitive data
  hashSensitiveData(data: string): string {
    return createHash('sha256').update(data).digest('hex');
  }

  // Emit security violation
  private emitViolation(violation: SecurityViolation): void {
    console.error('Security violation:', violation);
    this.emit('security-violation', violation);
    
    // Log to audit file
    this.logSecurityViolation(violation);
  }

  private async logSecurityViolation(violation: SecurityViolation): Promise<void> {
    try {
      const auditDir = path.join(process.cwd(), 'security', 'audit');
      const auditFile = path.join(auditDir, `violations-${new Date().toISOString().split('T')[0]}.log`);
      
      const logEntry = `${new Date().toISOString()} - ${violation.type.toUpperCase()} - ${violation.severity.toUpperCase()} - ${violation.message}\n`;
      await fs.appendFile(auditFile, logEntry);
    } catch (error) {
      console.error('Failed to log security violation:', error);
    }
  }

  async cleanup(): Promise<void> {
    console.log('Cleaning up Security Manager...');
    this.isInitialized = false;
  }

  getSecurityStatus(): {
    isInitialized: boolean;
    allowedPaths: string[];
    blockedPaths: string[];
    resourceUsage: Record<string, number>;
  } {
    return {
      isInitialized: this.isInitialized,
      allowedPaths: Array.from(this.allowedPaths),
      blockedPaths: Array.from(this.blockedPaths),
      resourceUsage: Object.fromEntries(this.resourceUsage),
    };
  }
}
