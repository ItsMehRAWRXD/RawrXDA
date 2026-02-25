// BigDaddyGEngine/ui/theme/ThemeManager.ts
// Theme Management System

import { useState, useEffect } from 'react';

export interface Theme {
  id: string;
  name: string;
  colors: {
    background: string;
    foreground: string;
    accent: string;
    border: string;
    text: string;
    textSecondary: string;
  };
}

export class ThemeManager {
  private currentTheme: Theme;
  private listeners: Set<(theme: Theme) => void> = new Set();

  constructor() {
    this.currentTheme = this.getDefaultTheme();
    this.loadSavedTheme();
  }

  private getDefaultTheme(): Theme {
    return {
      id: 'dark',
      name: 'Dark',
      colors: {
        background: '#1e1e1e',
        foreground: '#252526',
        accent: '#007acc',
        border: '#3e3e42',
        text: '#cccccc',
        textSecondary: '#858585'
      }
    };
  }

  getThemes(): Theme[] {
    return [
      {
        id: 'dark',
        name: 'Dark',
        colors: {
          background: '#1e1e1e',
          foreground: '#252526',
          accent: '#007acc',
          border: '#3e3e42',
          text: '#cccccc',
          textSecondary: '#858585'
        }
      },
      {
        id: 'light',
        name: 'Light',
        colors: {
          background: '#ffffff',
          foreground: '#f3f3f3',
          accent: '#0066bf',
          border: '#e1e1e1',
          text: '#1e1e1e',
          textSecondary: '#6e6e6e'
        }
      },
      {
        id: 'high-contrast',
        name: 'High Contrast',
        colors: {
          background: '#000000',
          foreground: '#1a1a1a',
          accent: '#00ff00',
          border: '#ffffff',
          text: '#ffffff',
          textSecondary: '#ffff00'
        }
      },
      {
        id: 'monokai',
        name: 'Monokai',
        colors: {
          background: '#272822',
          foreground: '#383830',
          accent: '#f92672',
          border: '#49483e',
          text: '#f8f8f2',
          textSecondary: '#75715e'
        }
      }
    ];
  }

  getCurrentTheme(): Theme {
    return this.currentTheme;
  }

  setTheme(themeId: string): void {
    const theme = this.getThemes().find(t => t.id === themeId);
    if (theme) {
      this.currentTheme = theme;
      this.applyTheme(theme);
      this.saveTheme(themeId);
      this.notifyListeners();
    }
  }

  private applyTheme(theme: Theme): void {
    const root = document.documentElement;
    
    // Apply CSS custom properties
    root.style.setProperty('--bg-color', theme.colors.background);
    root.style.setProperty('--fg-color', theme.colors.foreground);
    root.style.setProperty('--accent-color', theme.colors.accent);
    root.style.setProperty('--border-color', theme.colors.border);
    root.style.setProperty('--text-color', theme.colors.text);
    root.style.setProperty('--text-secondary-color', theme.colors.textSecondary);

    // Apply theme class
    document.body.className = `theme-${theme.id}`;
  }

  subscribe(listener: (theme: Theme) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private notifyListeners(): void {
    this.listeners.forEach(listener => listener(this.currentTheme));
  }

  private saveTheme(themeId: string): void {
    localStorage.setItem('theme', themeId);
  }

  private loadSavedTheme(): void {
    const saved = localStorage.getItem('theme');
    if (saved) {
      this.setTheme(saved);
    }
  }
}

export function ThemeSelector({ onToggle }: { onToggle?: () => void }) {
  const [currentTheme, setCurrentTheme] = useState('dark');
  
  const manager = getGlobalThemeManager();
  const themes = manager.getThemes();

  useEffect(() => {
    setCurrentTheme(manager.getCurrentTheme().id);
    
    const unsubscribe = manager.subscribe((theme) => {
      setCurrentTheme(theme.id);
    });

    return unsubscribe;
  }, []);

  const handleThemeChange = (themeId: string) => {
    manager.setTheme(themeId);
  };

  return (
    <div className="theme-selector">
      <div className="theme-header">
        <span>🎨 Theme</span>
        {onToggle && (
          <button onClick={onToggle} title="Close">×</button>
        )}
      </div>

      <div className="theme-options">
        {themes.map(theme => (
          <div
            key={theme.id}
            className={`theme-option ${currentTheme === theme.id ? 'active' : ''}`}
            onClick={() => handleThemeChange(theme.id)}
          >
            <div 
              className="theme-preview"
              style={{ 
                backgroundColor: theme.colors.background,
                borderColor: theme.colors.border 
              }}
            >
              <div style={{ color: theme.colors.text }}>A</div>
              <div style={{ color: theme.colors.accent }}>●</div>
            </div>
            <span>{theme.name}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

let globalThemeManager: ThemeManager | null = null;

export function getGlobalThemeManager(): ThemeManager {
  if (!globalThemeManager) {
    globalThemeManager = new ThemeManager();
    // Apply theme on initialization
    globalThemeManager.getCurrentTheme();
  }
  return globalThemeManager;
}
