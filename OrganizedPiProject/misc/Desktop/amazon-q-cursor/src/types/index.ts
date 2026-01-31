export interface AmazonQConfig {
  region: string;
  accessKeyId?: string;
  secretAccessKey?: string;
  profile?: string;
  modelId: string;
  temperature: number;
  maxTokens: number;
}

export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: Date;
  metadata?: {
    tokens?: number;
    model?: string;
    processingTime?: number;
  };
}

export interface CodeAnalysis {
  suggestions: CodeSuggestion[];
  issues: CodeIssue[];
  summary: string;
  confidence: number;
}

export interface CodeSuggestion {
  id: string;
  type: 'improvement' | 'optimization' | 'security' | 'best-practice';
  title: string;
  description: string;
  code: string;
  lineStart: number;
  lineEnd: number;
  severity: 'low' | 'medium' | 'high';
}

export interface CodeIssue {
  id: string;
  type: 'error' | 'warning' | 'info';
  message: string;
  line: number;
  column?: number;
  suggestion?: string;
}

export interface FileContext {
  path: string;
  content: string;
  language: string;
  cursorPosition?: {
    line: number;
    column: number;
  };
}

export interface AmazonQRequest {
  type: 'chat' | 'analyze' | 'suggest' | 'explain' | 'refactor';
  content: string;
  context?: FileContext;
  conversationId?: string;
}

export interface AmazonQResponse {
  success: boolean;
  data?: any;
  error?: string;
  conversationId?: string;
}

export interface WebSocketMessage {
  type: 'message' | 'error' | 'status' | 'typing';
  data: any;
  timestamp: Date;
}

export interface ExtensionConfig {
  enabled: boolean;
  autoSuggest: boolean;
  inlineChat: boolean;
  codeAnalysis: boolean;
  theme: 'light' | 'dark' | 'auto';
}
