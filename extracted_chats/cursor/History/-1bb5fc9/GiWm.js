#!/usr/bin/env node
/*
 * === BigDaddyG Copilot-style CLI IDE ===
 *  – inline ghost-text while you type
 *  – TAB to accept, ESC to hide
 *  – integrated with BigDaddyG AI backend
 *  – supports all previous commands
 */

import fs from 'fs';
import path from 'path';
import os from 'os';
import { EventEmitter } from 'events';
import fetch from 'node-fetch';

// ---------- low-level terminal raw mode ----------
import { stdin, stdout } from 'process';
stdin.setRawMode(true);
stdin.resume();
stdin.setEncoding('utf8');

const CSI = '\x1b[';
const clearRight = () => stdout.write(CSI + '0K');
const moveToCol = n => stdout.write(CSI + n + 'G');
const grey   = s => `\x1b[90m${s}\x1b[0m`;
const yellow = s => `\x1b[33m${s}\x1b[0m`;
const cyan   = s => `\x1b[36m${s}\x1b[0m`;
const green  = s => `\x1b[32m${s}\x1b[0m`;

// ---------- config / AI integration ----------
const CONFIG_DIR = path.join(os.homedir(), '.bigdaddyg_cli');
const CONFIG_FILE = path.join(CONFIG_DIR, 'config.json');
if (!fs.existsSync(CONFIG_DIR)) fs.mkdirSync(CONFIG_DIR, { recursive: true });

const config = fs.existsSync(CONFIG_FILE)
  ? JSON.parse(fs.readFileSync(CONFIG_FILE, 'utf8'))
  : { 
      model: 'bigdaddyg:40gb',
      ollamaUrl: 'http://localhost:11434',
      temperature: 0.4,
      debounceMs: 250,
      maxTokens: 100
    };

// Save config if new
if (!fs.existsSync(CONFIG_FILE)) {
  fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));
}

// ---------- BigDaddyG AI Engine (Real Integration!) ----------
class BigDaddyGAI {
  constructor() {
    this.ollamaUrl = config.ollamaUrl;
    this.model = config.model;
    this.embeddedUrl = 'http://localhost:11435'; // Embedded model engine
  }

  async complete(prefix, suffix = '') {
    // Try embedded model first, fall back to Ollama
    try {
      return await this.queryModel(this.embeddedUrl, prefix, suffix);
    } catch (err) {
      try {
        return await this.queryModel(this.ollamaUrl, prefix, suffix);
      } catch (err2) {
        // Fallback to local heuristics
        return this.localComplete(prefix);
      }
    }
  }

  async queryModel(baseUrl, prefix, suffix) {
    const response = await fetch(`${baseUrl}/api/generate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: this.model,
        prompt: `Complete this code:\n${prefix}`,
        stream: false,
        options: {
          temperature: config.temperature,
          num_predict: config.maxTokens,
          stop: ['\n\n', '```']
        }
      })
    });

    if (!response.ok) throw new Error('Model not available');
    
    const data = await response.json();
    return data.response.trim().split('\n')[0]; // First line only for inline
  }

  localComplete(prefix) {
    // Simple heuristic completions when model unavailable
    const patterns = {
      'console.': 'log()',
      'const ': 'x = ',
      'function ': 'myFunction() {}',
      'import ': '{ } from ',
      'if (': ') {}',
      'for (': 'let i = 0; i < 10; i++) {}',
      '.map(': 'x => x)',
      '.filter(': 'x => x)',
    };

    for (const [pattern, completion] of Object.entries(patterns)) {
      if (prefix.endsWith(pattern)) {
        return completion;
      }
    }

    return ''; // No suggestion
  }

  async explain(code) {
    try {
      const response = await fetch(`${this.ollamaUrl}/api/generate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: this.model,
          prompt: `Explain this code:\n\n${code}`,
          stream: false
        })
      });
      const data = await response.json();
      return data.response;
    } catch {
      return 'Model not available. Install Ollama or start embedded model.';
    }
  }

  async chat(text) {
    try {
      const response = await fetch(`${this.ollamaUrl}/api/generate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: this.model,
          prompt: text,
          stream: false
        })
      });
      const data = await response.json();
      return data.response;
    } catch {
      return 'Model not available. Install Ollama or start embedded model.';
    }
  }
}

// ---------- ghost-text machinery ----------
class StreamCompleter extends EventEmitter {
  constructor(ai) {
    super();
    this.ai = ai;
    this.timer = null;
    this.ghost = '';
    this.accepted = false;
  }

  update(prefix) {
    clearTimeout(this.timer);
    if (!prefix.trim()) return this.hide();
    this.timer = setTimeout(async () => {
      try {
        this.ghost = await this.ai.complete(prefix);
        this.show();
      } catch (err) {
        // Silent fail - no ghost text if model unavailable
      }
    }, config.debounceMs || 250);
  }

  show() {
    if (!this.ghost) return;
    stdout.write(grey(this.ghost));
    moveToCol(this.cursorCol());
  }

  hide() {
    if (!this.ghost) return;
    const cols = this.ghost.length;
    stdout.write(CSI + cols + 'D' + CSI + '0K');
    this.ghost = '';
  }

  cursorCol() {
    // Current column = prompt length + typed length
    return 'bigdaddyg> '.length + 1 + global.currentLine.length;
  }

  accept() {
    if (!this.ghost) return false;
    this.hide();
    this.emit('accept', this.ghost);
    this.ghost = '';
    return true;
  }
}

// ---------- file operations ----------
const fileOps = {
  read: f => fs.readFileSync(f, 'utf8'),
  write: (f, c) => fs.writeFileSync(f, c),
  list: () => fs.readdirSync('.'),
  exists: f => fs.existsSync(f)
};

// ---------- commands ----------
const commands = {
  help: () => {
    console.log(cyan('\n╔═══════════════════════════════════════════════════════════════════╗'));
    console.log(cyan('║              BIGDADDYG CLI - COPILOT MODE                         ║'));
    console.log(cyan('╚═══════════════════════════════════════════════════════════════════╝\n'));
    console.log(yellow('Commands:'));
    console.log('  suggest <prompt>  - Get AI code suggestion');
    console.log('  explain <file>    - Explain code in file');
    console.log('  chat <text>       - Chat with AI');
    console.log('  ls                - List files');
    console.log('  open <file>       - Read file');
    console.log('  write <file> <txt> - Write to file');
    console.log('  config            - Show configuration');
    console.log(yellow('\nShortcuts:'));
    console.log('  TAB       - Accept ghost text suggestion');
    console.log('  ESC       - Toggle ghost text on/off');
    console.log('  Ctrl-C    - Exit');
    console.log(yellow('\nGhost Text:'));
    console.log('  Start typing... AI suggestions appear in ' + grey('grey'));
    console.log('  Press TAB to accept, keep typing to ignore\n');
  },

  ls: () => {
    const files = fileOps.list();
    console.log(cyan('\n📂 Current Directory:\n'));
    files.forEach(f => {
      const stat = fs.statSync(f);
      const icon = stat.isDirectory() ? '📁' : '📄';
      console.log(`${icon} ${f}`);
    });
    console.log();
  },

  open: args => {
    if (!args[0]) return console.log('Usage: open <filename>');
    if (!fileOps.exists(args[0])) return console.log(`File not found: ${args[0]}`);
    console.log(cyan(`\n📄 ${args[0]}:\n`));
    console.log(fileOps.read(args[0]));
    console.log();
  },

  write: args => {
    if (args.length < 2) return console.log('Usage: write <filename> <content>');
    const [file, ...content] = args;
    fileOps.write(file, content.join(' '));
    console.log(green(`✅ Saved: ${file}\n`));
  },

  suggest: async args => {
    if (!args.length) return console.log('Usage: suggest <prompt>');
    console.log(cyan('\n🤖 AI Suggestion:\n'));
    const result = await ai.complete(args.join(' '));
    console.log(result);
    console.log();
  },

  explain: async args => {
    if (!args[0]) return console.log('Usage: explain <filename>');
    if (!fileOps.exists(args[0])) return console.log(`File not found: ${args[0]}`);
    console.log(cyan('\n🤖 AI Explanation:\n'));
    const code = fileOps.read(args[0]);
    const explanation = await ai.explain(code);
    console.log(explanation);
    console.log();
  },

  chat: async args => {
    if (!args.length) return console.log('Usage: chat <message>');
    console.log(cyan('\n🤖 BigDaddyG AI:\n'));
    const response = await ai.chat(args.join(' '));
    console.log(response);
    console.log();
  },

  config: () => {
    console.log(cyan('\n⚙️  BigDaddyG CLI Configuration:\n'));
    console.log(JSON.stringify(config, null, 2));
    console.log(grey(`\nConfig file: ${CONFIG_FILE}\n`));
  }
};

// ---------- input loop ----------
const completer = new StreamCompleter(new BigDaddyGAI());
let currentLine = '';
let suggestionMode = true;

global.currentLine = currentLine; // For cursor position calculation

completer.on('accept', txt => {
  currentLine += txt;
  global.currentLine = currentLine;
  stdout.write(txt);
});

// Welcome message
console.log(cyan('╔═══════════════════════════════════════════════════════════════════╗'));
console.log(cyan('║              BIGDADDYG CLI - COPILOT MODE ENABLED                 ║'));
console.log(cyan('╚═══════════════════════════════════════════════════════════════════╝'));
console.log(yellow('\n🤖 AI-powered CLI with inline ghost-text suggestions'));
console.log(grey('Type ') + cyan('help') + grey(' for commands, TAB to accept suggestions, ESC to toggle, Ctrl-C to exit\n'));

stdout.write('bigdaddyg> ');

stdin.on('data', async c => {
  const ch = c.toString();
  
  // Ctrl-C: Exit
  if (ch === '\u0003') {
    completer.hide();
    console.log(cyan('\n\n👋 Goodbye!\n'));
    process.exit(0);
  }

  // ESC: Toggle suggestions
  if (ch === '\x1b') {
    completer.hide();
    suggestionMode = !suggestionMode;
    const status = suggestionMode ? green('ON') : grey('OFF');
    stdout.write(grey(` [Ghost text: ${status}] `));
    setTimeout(() => {
      clearRight();
      moveToCol('bigdaddyg> '.length + 1 + currentLine.length);
    }, 800);
    return;
  }

  // TAB: Accept ghost text
  if (ch === '\t') {
    if (completer.accept()) {
      global.currentLine = currentLine;
    }
    return;
  }

  // ENTER: Execute command
  if (ch === '\r') {
    completer.hide();
    stdout.write('\n');
    const [cmd, ...args] = currentLine.trim().split(/\s+/);
    
    if (commands[cmd]) {
      await commands[cmd](args);
    } else if (currentLine.trim()) {
      console.log(grey(`Unknown command: ${cmd}`));
      console.log(grey('Type "help" for available commands\n'));
    }
    
    currentLine = '';
    global.currentLine = currentLine;
    stdout.write('bigdaddyg> ');
    return;
  }

  // BACKSPACE: Delete character
  if (ch === '\x7f') {
    if (currentLine.length) {
      currentLine = currentLine.slice(0, -1);
      global.currentLine = currentLine;
      completer.hide();
      moveToCol('bigdaddyg> '.length + 1 + currentLine.length);
      stdout.write('\b \b');
    }
    return;
  }

  // Regular character: Add to line
  currentLine += ch;
  global.currentLine = currentLine;
  stdout.write(ch);
  
  // Update ghost text
  if (suggestionMode) {
    completer.update(currentLine);
  }
});

// Handle process exit gracefully
process.on('SIGINT', () => {
  completer.hide();
  console.log(cyan('\n\n👋 Goodbye!\n'));
  process.exit(0);
});

