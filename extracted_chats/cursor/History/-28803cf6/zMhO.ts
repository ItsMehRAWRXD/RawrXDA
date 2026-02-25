// BigDaddyGEngine/extensions/ExtensionAPI.ts
// Extension API for Custom Panels

export interface ExtensionContext {
  window: WindowAPI;
  workspace: WorkspaceAPI;
  commands: CommandsAPI;
  events: EventsAPI;
  storage: StorageAPI;
}

export interface WindowAPI {
  createPanel(id: string, title: string, component: React.ComponentType): void;
  showPanel(id: string): void;
  hidePanel(id: string): void;
  closePanel(id: string): void;
  focusPanel(id: string): void;
}

export interface WorkspaceAPI {
  getWorkspaceFolder(): string;
  getWorkspaceFolders(): string[];
  openFile(uri: string): Promise<void>;
  saveFile(uri: string, content: string): Promise<void>;
}

export interface CommandsAPI {
  registerCommand(command: string, handler: (...args: any[]) => any): void;
  executeCommand(command: string, ...args: any[]): Promise<any>;
  getCommands(): string[];
}

export interface EventsAPI {
  on(event: string, handler: (...args: any[]) => void): void;
  off(event: string, handler: (...args: any[]) => void): void;
  emit(event: string, ...args: any[]): void;
}

export interface StorageAPI {
  get<T>(key: string): T | null;
  set<T>(key: string, value: T): void;
  remove(key: string): void;
  clear(): void;
}

export class ExtensionAPI implements ExtensionContext {
  window: WindowAPI;
  workspace: WorkspaceAPI;
  commands: CommandsAPI;
  events: EventsAPI;
  storage: StorageAPI;

  constructor() {
    this.window = new WindowAPIImpl();
    this.workspace = new WorkspaceAPIImpl();
    this.commands = new CommandsAPIImpl();
    this.events = new EventsAPIImpl();
    this.storage = new StorageAPIImpl();
  }
}

class WindowAPIImpl implements WindowAPI {
  private panels: Map<string, any> = new Map();

  createPanel(id: string, title: string, component: React.ComponentType): void {
    console.log(`Creating panel: ${id} - ${title}`);
    this.panels.set(id, { title, component, visible: false });
  }

  showPanel(id: string): void {
    const panel = this.panels.get(id);
    if (panel) {
      panel.visible = true;
      console.log(`Showing panel: ${id}`);
    }
  }

  hidePanel(id: string): void {
    const panel = this.panels.get(id);
    if (panel) {
      panel.visible = false;
      console.log(`Hiding panel: ${id}`);
    }
  }

  closePanel(id: string): void {
    this.panels.delete(id);
    console.log(`Closing panel: ${id}`);
  }

  focusPanel(id: string): void {
    console.log(`Focusing panel: ${id}`);
  }
}

class WorkspaceAPIImpl implements WorkspaceAPI {
  getWorkspaceFolder(): string {
    return '/';
  }

  getWorkspaceFolders(): string[] {
    return ['/'];
  }

  async openFile(uri: string): Promise<void> {
    console.log(`Opening file: ${uri}`);
  }

  async saveFile(uri: string, content: string): Promise<void> {
    console.log(`Saving file: ${uri}`);
  }
}

class CommandsAPIImpl implements CommandsAPI {
  private commands: Map<string, (...args: any[]) => any> = new Map();

  registerCommand(command: string, handler: (...args: any[]) => any): void {
    this.commands.set(command, handler);
    console.log(`Registered command: ${command}`);
  }

  async executeCommand(command: string, ...args: any[]): Promise<any> {
    const handler = this.commands.get(command);
    if (handler) {
      return await handler(...args);
    }
    throw new Error(`Command not found: ${command}`);
  }

  getCommands(): string[] {
    return Array.from(this.commands.keys());
  }
}

class EventsAPIImpl implements EventsAPI {
  private listeners: Map<string, Set<(...args: any[]) => void>> = new Map();

  on(event: string, handler: (...args: any[]) => void): void {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)!.add(handler);
  }

  off(event: string, handler: (...args: any[]) => void): void {
    const handlers = this.listeners.get(event);
    if (handlers) {
      handlers.delete(handler);
    }
  }

  emit(event: string, ...args: any[]): void {
    const handlers = this.listeners.get(event);
    if (handlers) {
      handlers.forEach(handler => handler(...args));
    }
  }
}

class StorageAPIImpl implements StorageAPI {
  private storage: Map<string, any> = new Map();

  get<T>(key: string): T | null {
    const value = this.storage.get(key);
    return value || null;
  }

  set<T>(key: string, value: T): void {
    this.storage.set(key, value);
  }

  remove(key: string): void {
    this.storage.delete(key);
  }

  clear(): void {
    this.storage.clear();
  }
}

export function getExtensionAPI(): ExtensionAPI {
  return globalExtensionAPI;
}

const globalExtensionAPI = new ExtensionAPI();

// Example extension using the API
export function createSampleExtension(context: ExtensionContext) {
  // Register a command
  context.commands.registerCommand('sample.hello', () => {
    console.log('Hello from extension!');
  });

  // Listen to events
  context.events.on('file:opened', (uri: string) => {
    console.log(`File opened: ${uri}`);
  });

  // Store data
  context.storage.set('myKey', 'myValue');
  
  return {
    activate: () => {
      console.log('Extension activated');
    },
    deactivate: () => {
      console.log('Extension deactivated');
    }
  };
}
