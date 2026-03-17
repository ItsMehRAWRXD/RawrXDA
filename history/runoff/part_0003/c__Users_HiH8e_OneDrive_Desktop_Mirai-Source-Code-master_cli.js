#!/usr/bin/env node
/*
  ide-cli: Simple agentic CLI for IDE manifesting and checks.
  Usage: node cli.js <command>
*/

const { Command } = require('commander');
const http = require('http');
const express = require('express');
const path = require('path');
const fs = require('fs');
const os = require('os');
const child_process = require('child_process');

const program = new Command();
program.name('ide-cli').description('Agentic CLI for IDE management and checks').version('0.1.0');

// Default repo root (cwd)
const ROOT = process.cwd();
const TODOS_FILE = path.join(ROOT, '.ide-cli-todos.json');

// ----------------------------
// Helper utilities
// ----------------------------
function spawnGit(args, cwd = ROOT) {
  try {
    const res = child_process.spawnSync('git', args, { cwd, stdio: 'inherit' });
    if (res.error) throw res.error;
    return res.status === 0;
  } catch (e) {
    console.error('git command failed:', e.message || e);
    return false;
  }
}

async function walkDir(dir) {
  const results = [];
  const list = await fs.promises.readdir(dir, { withFileTypes: true });
  for (const ent of list) {
    const full = path.join(dir, ent.name);
    if (ent.isDirectory()) {
      results.push(...await walkDir(full));
    } else {
      const st = await fs.promises.stat(full);
      results.push({ path: path.relative(ROOT, full), size: st.size, mtime: st.mtimeMs });
    }
  }
  return results;
}

function ensureTodosFile() {
  if (!fs.existsSync(TODOS_FILE)) fs.writeFileSync(TODOS_FILE, JSON.stringify({ todos: [] }, null, 2));
}

function loadTodos() {
  ensureTodosFile();
  return JSON.parse(fs.readFileSync(TODOS_FILE, 'utf8'));
}

function saveTodos(obj) {
  fs.writeFileSync(TODOS_FILE, JSON.stringify(obj, null, 2));
}

async function startStaticServer(dir = ROOT, port = 8000) {
  const app = express();
  app.use(express.static(dir));
  return new Promise((resolve, reject) => {
    const srv = app.listen(port, () => resolve(srv));
    srv.on('error', reject);
  });
}

// ----------------------------
// Commands
// ----------------------------
program.command('serve')
  .description('Serve the current directory via HTTP (static server)')
  .option('-p, --port <port>', 'port', '8000')
  .action(async (opts) => {
    const port = parseInt(opts.port || '8000', 10);
    console.log(`Serving ${ROOT} on http://localhost:${port}`);
    const srv = await startStaticServer(ROOT, port);
    console.log('Press Ctrl+C to stop');
  });

program.command('manifest')
  .description('Create a JSON manifest of files under the repo')
  .option('-o, --out <file>', 'output file', 'ide-manifest.json')
  .action(async (opts) => {
    console.log('Building manifest...');
    const files = await walkDir(ROOT);
    const out = path.join(ROOT, opts.out || 'ide-manifest.json');
    fs.writeFileSync(out, JSON.stringify({ root: ROOT, generatedAt: Date.now(), files }, null, 2));
    console.log('Manifest written to', out);
  });

program.command('ui-check')
  .description('Run UI checks using Puppeteer to measure bounding boxes and find overlaps')
  .option('-u, --url <url>', 'URL of the IDE (default: http://localhost:8000/IDEre2.html)', 'http://localhost:8000/IDEre2.html')
  .option('-p, --port <port>', 'if server should be started automatically, provide port to start static server (optional)')
  .action(async (opts) => {
    const puppeteer = (() => {
      try { return require('puppeteer'); } catch (e) { return null; }
    })();
    if (!puppeteer) {
      console.error('Puppeteer not installed. Run: npm install puppeteer');
      process.exit(1);
    }

    let server;
    if (opts.port) {
      server = await startStaticServer(ROOT, parseInt(opts.port, 10));
      console.log('Started static server on port', opts.port);
    }

    console.log('Launching headless browser...');
    const browser = await puppeteer.launch({ headless: true, args: ['--no-sandbox'] });
    const page = await browser.newPage();
    page.setViewport({ width: 1400, height: 900 });

    try {
      await page.goto(opts.url, { waitUntil: 'networkidle2', timeout: 60000 });
      console.log('Page loaded:', opts.url);

      // Evaluate DOM and collect elements of interest
      const result = await page.evaluate(() => {
        // Collect all visible elements with bounding boxes > 0
        function visible(el) {
          const style = window.getComputedStyle(el);
          return style && style.display !== 'none' && style.visibility !== 'hidden' && style.opacity !== '0';
        }
        const all = Array.from(document.querySelectorAll('body *'));
        const boxes = [];
        for (const el of all) {
          if (!visible(el)) continue;
          const r = el.getBoundingClientRect();
          if (r.width < 1 || r.height < 1) continue;
          boxes.push({ tag: el.tagName.toLowerCase(), id: el.id || null, classes: el.className || null, rect: { x: r.x, y: r.y, w: r.width, h: r.height }, z: parseInt(window.getComputedStyle(el).zIndex) || 0 });
        }

        // Simple overlap detection O(n^2) for now
        const overlaps = [];
        for (let i = 0; i < boxes.length; i++) {
          for (let j = i + 1; j < boxes.length; j++) {
            const a = boxes[i].rect, b = boxes[j].rect;
            if (!(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y)) {
              overlaps.push({ a: boxes[i], b: boxes[j] });
            }
          }
        }

        // Return a lightweight summary
        return { width: window.innerWidth, height: window.innerHeight, elements: boxes.length, overlaps: overlaps.slice(0, 50) };
      });

      console.log('Viewport:', result.width, 'x', result.height);
      console.log('Elements measured:', result.elements);
      console.log('Overlaps detected (sample up to 50):', result.overlaps.length);
      if (result.overlaps.length > 0) console.log('First overlap sample:', JSON.stringify(result.overlaps[0], null, 2));

      await page.screenshot({ path: path.join(ROOT, 'ui-check-screenshot.png'), fullPage: true });
      console.log('Screenshot saved to ui-check-screenshot.png');

    } catch (e) {
      console.error('ui-check failed:', e && e.message ? e.message : e);
    } finally {
      await browser.close();
      if (server) server.close();
    }
  });

program.command('git-init')
  .description('Initialize a git repository in the current directory')
  .action(() => {
    if (fs.existsSync(path.join(ROOT, '.git'))) {
      console.log('Already a git repository');
      return;
    }
    const ok = spawnGit(['init']);
    if (ok) console.log('Git repo initialized');
  });

program.command('git-add-commit')
  .description('Add all and commit with message')
  .option('-m, --message <msg>', 'commit message', 'chore: update via ide-cli')
  .action((opts) => {
    spawnGit(['add', '.']);
    spawnGit(['commit', '-m', opts.message]);
  });

program.command('git-status')
  .description('Show git status')
  .action(() => {
    spawnGit(['status']);
  });

program.command('todo-import')
  .description('Import a todo list JSON (array of tasks)')
  .argument('<file>', 'file to import')
  .action((file) => {
    const full = path.resolve(file);
    if (!fs.existsSync(full)) return console.error('File not found:', full);
    const data = JSON.parse(fs.readFileSync(full, 'utf8'));
    const current = loadTodos();
    current.todos = current.todos.concat(Array.isArray(data) ? data : (data.todos || []));
    saveTodos(current);
    console.log('Imported', (Array.isArray(data) ? data.length : (data.todos || []).length), 'items');
  });

program.command('todo-list')
  .description('List stored todos')
  .action(() => {
    const current = loadTodos();
    console.log('Todos:', JSON.stringify(current.todos, null, 2));
  });

program.command('set-model-perms')
  .description('Recursively set permission 0o755 on models directory (best-effort; Windows may ignore)')
  .option('-d, --dir <dir>', 'models directory', 'models')
  .action(async (opts) => {
    const dir = path.resolve(opts.dir);
    if (!fs.existsSync(dir)) return console.error('Directory not found:', dir);
    const walk = async (p) => {
      const st = await fs.promises.stat(p);
      if (st.isDirectory()) {
        const children = await fs.promises.readdir(p);
        for (const c of children) await walk(path.join(p, c));
        try { await fs.promises.chmod(p, 0o755); } catch (e) { /* ignore */ }
      } else {
        try { await fs.promises.chmod(p, 0o755); } catch (e) { /* ignore */ }
      }
    };
    await walk(dir);
    console.log('Permissions set (best-effort) on', dir);
  });

program.command('help').action(() => program.help());

program.parse(process.argv);

if (!process.argv.slice(2).length) {
  program.outputHelp();
}
