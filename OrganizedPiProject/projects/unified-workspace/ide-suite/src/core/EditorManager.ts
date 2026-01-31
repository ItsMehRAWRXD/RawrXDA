import { EventEmitter } from 'events';
import { FileManager } from './FileManager.js';
import { AIEngine } from './AIEngine.js';
import { SecurityManager } from './SecurityManager.js';

export interface EditorConfig {
  theme: 'light' | 'dark' | 'auto';
  fontSize: number;
  tabSize: number;
  wordWrap: boolean;
  minimap: boolean;
  autoSave: boolean;
  autoSaveDelay: number;
}

export interface EditorState {
  filePath: string;
  content: string;
  cursorPosition: { line: number; column: number };
  selection: { start: number; end: number } | null;
  language: string;
  isDirty: boolean;
  lastSaved: Date | null;
}

export interface EditorSession {
  id: string;
  filePath: string;
  state: EditorState;
  aiSuggestions: AISuggestion[];
  isActive: boolean;
  createdAt: Date;
}

export interface AISuggestion {
  id: string;
  type: 'completion' | 'fix' | 'improvement' | 'explanation';
  text: string;
  range: { start: number; end: number };
  confidence: number;
  isApplied: boolean;
}

export class EditorManager extends EventEmitter {
  private fileManager: FileManager;
  private aiEngine: AIEngine;
  private securityManager: SecurityManager;
  private config: EditorConfig;
  private sessions: Map<string, EditorSession> = new Map();
  private activeSessionId: string | null = null;
  private autoSaveTimer: NodeJS.Timeout | null = null;

  constructor(
    fileManager: FileManager,
    aiEngine: AIEngine,
    config: EditorConfig = {
      theme: 'auto',
      fontSize: 14,
      tabSize: 2,
      wordWrap: true,
      minimap: true,
      autoSave: true,
      autoSaveDelay: 5000,
    }
  ) {
    super();
    this.fileManager = fileManager;
    this.aiEngine = aiEngine;
    this.config = config;
    this.setupEventHandlers();
  }

  private setupEventHandlers(): void {
    // Handle file changes from file manager
    this.fileManager.on('file-changed', (file) => {
      this.handleFileChange(file);
    });

    // Handle AI responses
    this.aiEngine.on('ai-response', (response) => {
      this.handleAIResponse(response);
    });
  }

  // Session management
  async openFile(filePath: string): Promise<EditorSession> {
    // Security check
    const hasAccess = await this.securityManager.validateFileAccess(filePath, 'read');
    if (!hasAccess) {
      throw new Error('Access denied to file');
    }

    // Check if file is already open
    const existingSession = Array.from(this.sessions.values()).find(
      session => session.filePath === filePath
    );
    
    if (existingSession) {
      this.setActiveSession(existingSession.id);
      return existingSession;
    }

    // Read file content
    const content = await this.fileManager.readFile(filePath);
    const language = this.detectLanguage(filePath);
    
    // Create new session
    const session: EditorSession = {
      id: this.generateSessionId(),
      filePath,
      state: {
        filePath,
        content,
        cursorPosition: { line: 0, column: 0 },
        selection: null,
        language,
        isDirty: false,
        lastSaved: null,
      },
      aiSuggestions: [],
      isActive: false,
      createdAt: new Date(),
    };

    this.sessions.set(session.id, session);
    this.setActiveSession(session.id);
    
    this.emit('session-opened', session);
    return session;
  }

  closeSession(sessionId: string): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    // Save if dirty
    if (session.state.isDirty) {
      this.saveSession(sessionId);
    }

    this.sessions.delete(sessionId);
    
    if (this.activeSessionId === sessionId) {
      this.activeSessionId = null;
    }
    
    this.emit('session-closed', session);
  }

  setActiveSession(sessionId: string): void {
    // Deactivate current session
    if (this.activeSessionId) {
      const currentSession = this.sessions.get(this.activeSessionId);
      if (currentSession) {
        currentSession.isActive = false;
      }
    }

    // Activate new session
    const session = this.sessions.get(sessionId);
    if (session) {
      session.isActive = true;
      this.activeSessionId = sessionId;
      this.emit('session-activated', session);
    }
  }

  // Content management
  updateContent(sessionId: string, content: string): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    session.state.content = content;
    session.state.isDirty = true;
    
    // Trigger auto-save
    if (this.config.autoSave) {
      this.scheduleAutoSave(sessionId);
    }
    
    // Request AI suggestions
    this.requestAISuggestions(sessionId);
    
    this.emit('content-updated', { sessionId, content });
  }

  updateCursorPosition(sessionId: string, position: { line: number; column: number }): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    session.state.cursorPosition = position;
    this.emit('cursor-moved', { sessionId, position });
  }

  updateSelection(sessionId: string, selection: { start: number; end: number } | null): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    session.state.selection = selection;
    this.emit('selection-changed', { sessionId, selection });
  }

  // File operations
  async saveSession(sessionId: string): Promise<void> {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    // Security check
    const hasAccess = await this.securityManager.validateFileAccess(session.filePath, 'write');
    if (!hasAccess) {
      throw new Error('Access denied to save file');
    }

    await this.fileManager.writeFile(session.filePath, session.state.content);
    session.state.isDirty = false;
    session.state.lastSaved = new Date();
    
    this.emit('session-saved', session);
  }

  async saveAll(): Promise<void> {
    const savePromises = Array.from(this.sessions.keys()).map(sessionId => 
      this.saveSession(sessionId)
    );
    await Promise.all(savePromises);
  }

  // AI integration
  private async requestAISuggestions(sessionId: string): Promise<void> {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    try {
      const request = {
        type: 'code-completion' as const,
        content: session.state.content,
        context: {
          filePath: session.filePath,
          language: session.state.language,
          cursorPosition: session.state.cursorPosition,
        },
        conversationId: sessionId,
      };

      const response = await this.aiEngine.processRequest(request);
      
      if (response.success && response.suggestions) {
        session.aiSuggestions = response.suggestions.map(suggestion => ({
          id: this.generateSuggestionId(),
          type: suggestion.type,
          text: suggestion.text,
          range: suggestion.range,
          confidence: suggestion.confidence,
          isApplied: false,
        }));
        
        this.emit('ai-suggestions', { sessionId, suggestions: session.aiSuggestions });
      }
    } catch (error) {
      console.error('Error requesting AI suggestions:', error);
    }
  }

  applySuggestion(sessionId: string, suggestionId: string): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    const suggestion = session.aiSuggestions.find(s => s.id === suggestionId);
    if (!suggestion) return;

    // Apply suggestion to content
    const { start, end } = suggestion.range;
    const before = session.state.content.substring(0, start);
    const after = session.state.content.substring(end);
    session.state.content = before + suggestion.text + after;
    session.state.isDirty = true;
    
    suggestion.isApplied = true;
    
    this.emit('suggestion-applied', { sessionId, suggestionId });
  }

  // Utility methods
  private detectLanguage(filePath: string): string {
    const ext = filePath.split('.').pop()?.toLowerCase();
    const languageMap: Record<string, string> = {
      'js': 'javascript',
      'ts': 'typescript',
      'jsx': 'javascript',
      'tsx': 'typescript',
      'py': 'python',
      'java': 'java',
      'cpp': 'cpp',
      'c': 'c',
      'cs': 'csharp',
      'php': 'php',
      'rb': 'ruby',
      'go': 'go',
      'rs': 'rust',
      'html': 'html',
      'css': 'css',
      'scss': 'scss',
      'json': 'json',
      'xml': 'xml',
      'yaml': 'yaml',
      'yml': 'yaml',
      'md': 'markdown',
    };
    
    return languageMap[ext || ''] || 'plaintext';
  }

  private scheduleAutoSave(sessionId: string): void {
    if (this.autoSaveTimer) {
      clearTimeout(this.autoSaveTimer);
    }
    
    this.autoSaveTimer = setTimeout(() => {
      this.saveSession(sessionId);
    }, this.config.autoSaveDelay);
  }

  private handleFileChange(file: any): void {
    // Handle external file changes
    const session = Array.from(this.sessions.values()).find(
      s => s.filePath === file.path
    );
    
    if (session) {
      session.state.content = file.content;
      session.state.isDirty = false;
      this.emit('file-changed-externally', session);
    }
  }

  private handleAIResponse(response: any): void {
    // Handle AI responses
    this.emit('ai-response-received', response);
  }

  private generateSessionId(): string {
    return `session_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private generateSuggestionId(): string {
    return `suggestion_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  // Public API
  getSession(sessionId: string): EditorSession | undefined {
    return this.sessions.get(sessionId);
  }

  getActiveSession(): EditorSession | null {
    return this.activeSessionId ? this.sessions.get(this.activeSessionId) || null : null;
  }

  getAllSessions(): EditorSession[] {
    return Array.from(this.sessions.values());
  }

  getDirtySessions(): EditorSession[] {
    return Array.from(this.sessions.values()).filter(session => session.state.isDirty);
  }

  updateConfig(newConfig: Partial<EditorConfig>): void {
    this.config = { ...this.config, ...newConfig };
    this.emit('config-updated', this.config);
  }

  getConfig(): EditorConfig {
    return { ...this.config };
  }
}
