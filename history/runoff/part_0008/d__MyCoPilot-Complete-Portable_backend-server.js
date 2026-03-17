const express = require('express');
const fs = require('fs').promises;
const path = require('path');
const { exec } = require('child_process');
const https = require('https');
const http = require('http');

const app = express();
const PORT = process.env.PORT || 8080;

// Serve static files and index.html from project root
const staticRoot = path.join(__dirname);
app.use(express.static(staticRoot));
app.get('/', (req, res) => {
  res.sendFile(path.join(staticRoot, 'index.html'));
});
app.use((req, res, next) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type');
    if (req.method === 'OPTIONS') return res.sendStatus(200);
    next();
});
app.use(express.json());

// Health endpoint for quick readiness checks
app.get('/health', (req, res) => {
  res.json({ status: 'ok', port: PORT, timestamp: new Date().toISOString() });
});

// Restart endpoint for auto-healing
app.post('/api/restart', (req, res) => {
  res.json({ success: true, message: 'Restart signal received' });
  setTimeout(() => {
    console.log('Restarting server...');
    process.exit(0);
  }, 1000);
});

app.get('/api/file/read', async (req, res) => {
  try {
    const content = await fs.readFile(req.query.path, 'utf8');
    // Return JSON for frontend consumers
    res.json({ content, path: req.query.path });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.get('/api/file/list', async (req, res) => {
    try {
        const files = await fs.readdir(req.query.path, { withFileTypes: true });
        res.json(files.map(f => ({ name: f.name, type: f.isDirectory() ? 'directory' : 'file' })));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Add the endpoints that the IDE expects
// Full D: drive explorer endpoint
app.get('/api/drive-explorer', async (req, res) => {
    try {
        const startPath = req.query.path || 'D:\\';
        const maxDepth = parseInt(req.query.depth) || 2;
        const includeHidden = req.query.hidden === 'true';
        
        async function exploreDirectory(dirPath, currentDepth = 0) {
            if (currentDepth > maxDepth) return null;
            
            try {
                const files = await fs.readdir(dirPath, { withFileTypes: true });
                const result = {
                    name: path.basename(dirPath) || dirPath,
                    path: dirPath,
                    type: 'directory',
                    children: []
                };
                
                for (const file of files) {
                    if (!includeHidden && file.name.startsWith('.')) continue;
                    
                    const fullPath = path.join(dirPath, file.name);
                    if (file.isDirectory()) {
                        const subDir = await exploreDirectory(fullPath, currentDepth + 1);
                        if (subDir) result.children.push(subDir);
                    } else {
                        result.children.push({
                            name: file.name,
                            path: fullPath,
                            type: 'file',
                            size: (await fs.stat(fullPath)).size
                        });
                    }
                }
                
                return result;
            } catch (e) {
                return {
                    name: path.basename(dirPath),
                    path: dirPath,
                    type: 'directory',
                    error: e.message,
                    children: []
                };
            }
        }
        
        const tree = await exploreDirectory(startPath);
        res.json(tree);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/files', async (req, res) => {
    try {
        const dirPath = req.query.path || 'D:\\';
        const readContent = req.query.readContent === 'true';
        const files = await fs.readdir(dirPath, { withFileTypes: true });
        const result = {
            currentPath: dirPath,
            parentPath: path.dirname(dirPath),
            files: files.map(f => ({ 
                name: f.name, 
                path: path.join(dirPath, f.name),
                type: f.isDirectory() ? 'directory' : 'file',
                isDirectory: f.isDirectory(),
                fullPath: path.resolve(path.join(dirPath, f.name))
            }))
        };
        
        if (readContent && files.length === 1 && !files[0].isDirectory()) {
            const content = await fs.readFile(path.join(dirPath, files[0].name), 'utf8');
            result.content = content;
        }
        
        res.json(result);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/file-content', async (req, res) => {
    try {
        const filePath = req.query.path;
        if (!filePath) return res.status(400).json({ error: 'path required' });
        const content = await fs.readFile(filePath, 'utf8');
        res.json({ content, path: filePath });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Directory navigation endpoints
app.get('/api/browse-directory', async (req, res) => {
    try {
        const dirPath = req.query.path || 'D:\\';
        const showHidden = req.query.hidden === 'true';
        
        const stats = await fs.stat(dirPath);
        if (!stats.isDirectory()) {
            return res.status(400).json({ error: 'Path is not a directory' });
        }
        
        const files = await fs.readdir(dirPath, { withFileTypes: true });
        const result = {
            currentDirectory: dirPath,
            parentDirectory: dirPath !== 'D:\\' ? path.dirname(dirPath) : null,
            contents: []
        };
        
        for (const file of files) {
            if (!showHidden && file.name.startsWith('.')) continue;
            
            const fullPath = path.join(dirPath, file.name);
            try {
                const fileStat = await fs.stat(fullPath);
                result.contents.push({
                    name: file.name,
                    path: fullPath,
                    type: file.isDirectory() ? 'directory' : 'file',
                    size: file.isDirectory() ? null : fileStat.size,
                    modified: fileStat.mtime,
                    isReadable: true,
                    extension: file.isDirectory() ? null : path.extname(file.name)
                });
            } catch (statError) {
                result.contents.push({
                    name: file.name,
                    path: fullPath,
                    type: file.isDirectory() ? 'directory' : 'file',
                    size: null,
                    modified: null,
                    isReadable: false,
                    error: statError.message
                });
            }
        }
        
        // Sort: directories first, then files
        result.contents.sort((a, b) => {
            if (a.type === 'directory' && b.type === 'file') return -1;
            if (a.type === 'file' && b.type === 'directory') return 1;
            return a.name.localeCompare(b.name);
        });
        
        res.json(result);
    } catch (e) {
        res.status(500).json({ error: e.message, path: req.query.path });
    }
});

// Search files across D: drive
app.get('/api/search-files', async (req, res) => {
    try {
        const searchTerm = req.query.q;
        const searchPath = req.query.path || 'D:\\';
        const maxResults = parseInt(req.query.limit) || 50;
        
        if (!searchTerm) {
            return res.status(400).json({ error: 'Search term required' });
        }
        
        const results = [];
        
        async function searchDirectory(dirPath, depth = 0) {
            if (depth > 5 || results.length >= maxResults) return; // Limit depth and results
            
            try {
                const files = await fs.readdir(dirPath, { withFileTypes: true });
                
                for (const file of files) {
                    if (results.length >= maxResults) break;
                    
                    const fullPath = path.join(dirPath, file.name);
                    
                    // Check if filename matches search term
                    if (file.name.toLowerCase().includes(searchTerm.toLowerCase())) {
                        results.push({
                            name: file.name,
                            path: fullPath,
                            type: file.isDirectory() ? 'directory' : 'file',
                            directory: dirPath
                        });
                    }
                    
                    // Recursively search subdirectories
                    if (file.isDirectory() && depth < 5) {
                        await searchDirectory(fullPath, depth + 1);
                    }
                }
            } catch (e) {
                // Skip directories we can't read
            }
        }
        
        await searchDirectory(searchPath);
        
        res.json({
            searchTerm,
            searchPath,
            results,
            totalFound: results.length
        });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/save-file', async (req, res) => {
    try {
        const { path: filePath, content } = req.body;
        if (!filePath) return res.status(400).json({ error: 'path required' });
        await fs.mkdir(path.dirname(filePath), { recursive: true });
        await fs.writeFile(filePath, content, 'utf8');
        res.json({ success: true, path: filePath });
    } catch (e) {
        res.status(500).json({ error: e.message, success: false });
    }
});

app.get('/api/compiler/native', async (req, res) => {
    try {
        const compilers = [
            { name: 'GCC', available: false, version: null },
            { name: 'G++', available: false, version: null },
            { name: 'Clang', available: false, version: null },
            { name: 'Python', available: true, version: '3.x' },
            { name: 'Node.js', available: true, version: '18.x' },
            { name: 'Java', available: false, version: null },
            { name: 'Rust', available: false, version: null },
            { name: 'Go', available: false, version: null }
        ];
        
        res.json({ compilers });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/compiler/scan', async (req, res) => {
    try {
        const { exec } = require('child_process');
        const util = require('util');
        const execPromise = util.promisify(exec);
        const results = [];
        
        const checks = [
            { name: 'Python', cmd: 'python --version' },
            { name: 'Node.js', cmd: 'node --version' },
            { name: 'GCC', cmd: 'gcc --version' },
            { name: 'G++', cmd: 'g++ --version' },
            { name: 'Java', cmd: 'java --version' },
            { name: 'Rust', cmd: 'rustc --version' }
        ];
        
        for (const check of checks) {
            try {
                const { stdout } = await execPromise(check.cmd);
                const version = stdout.split('\n')[0];
                results.push({ name: check.name, path: check.name.toLowerCase(), version, available: true });
            } catch (e) {
                results.push({ name: check.name, path: null, version: null, available: false });
            }
        }
        
        res.json({ compilers: results });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// GET variant for clients expecting GET /api/compiler/scan
app.get('/api/compiler/scan', async (req, res) => {
  try {
    const toolsRoots = [
      'D:\\portable-toolchains',
      'D:\\portable_toolchains',
      'D:\\fixed_portable_toolchains',
      'D:\\fixed_portable_toolchain_system',
      'D:\\portable-toolchains\\mingw64',
      'D:\\portable-toolchains\\llvm',
      'D:\\portable-toolchains\\gcc',
      'D:\\portable-toolchains\\clang',
      'D:\\portable-toolchains\\java',
      'D:\\portable-toolchains\\jdk',
      'D:\\portable-toolchains\\graalvm-portable',
      'D:\\portable-toolchains\\java-portable',
      'D:\\portable-toolchains\\sources',
      // Common Windows install locations
      'C:\\Program Files\\Java',
      'C:\\Program Files (x86)\\Java',
      'C:\\Program Files\\Eclipse Adoptium',
      'C:\\Program Files\\LLVM',
      'C:\\mingw64',
      'C:\\Program Files\\Git\\mingw64'
    ];

    const exists = async (p) => {
      try { await fs.access(p); return true; } catch { return false; }
    };

    const findFirstExecutable = async (names, roots, maxDepth = 5) => {
      const path = require('path');
      const queue = [];
      for (const root of roots) {
        if (await exists(root)) queue.push({ dir: root, depth: 0 });
      }
      while (queue.length) {
        const { dir, depth } = queue.shift();
        try {
          const entries = await fs.readdir(dir, { withFileTypes: true });
          for (const ent of entries) {
            const full = path.join(dir, ent.name);
            if (ent.isFile()) {
              if (names.some(n => ent.name.toLowerCase() === n.toLowerCase())) {
                return full;
              }
            } else if (ent.isDirectory() && depth < maxDepth) {
              queue.push({ dir: full, depth: depth + 1 });
            }
          }
        } catch {}
      }
      return null;
    };

    const detect = async () => {
      const res = {
        compilers: [
          { name: 'Python', path: 'python', available: true, version: '3.x' },
          { name: 'Node.js', path: 'node', available: true, version: '18.x' },
          { name: 'GCC', path: null, available: false, version: null },
          { name: 'G++', path: null, available: false, version: null },
          { name: 'Clang', path: null, available: false, version: null },
          { name: 'Java', path: null, available: false, version: null },
          { name: 'Rust', path: null, available: false, version: null },
          { name: 'Go', path: null, available: false, version: null }
        ]
      };

      const candidates = [
        { key: 'GCC', names: ['gcc', 'mingw64\\bin\\gcc.exe', 'gcc.exe'] },
        { key: 'G++', names: ['g++', 'mingw64\\bin\\g++.exe', 'g++.exe'] },
        { key: 'Clang', names: ['clang', 'clang.exe', 'bin\\clang.exe'] },
        { key: 'Java', names: ['java.exe', 'bin\\java.exe', 'jdk\\bin\\java.exe', 'jre\\bin\\java.exe', 'openjdk\\bin\\java.exe', 'bin\\javac.exe', 'javac.exe'] },
        { key: 'Rust', names: ['rustc', 'rustc.exe', 'bin\\rustc.exe'] },
        { key: 'Go', names: ['go', 'go.exe', 'bin\\go.exe'] }
      ];

      for (const c of candidates) {
        let foundPath = null;
        // PATH check first
        try {
          const which = process.platform === 'win32' ? 'where' : 'which';
          const { exec } = require('child_process');
          const util = require('util');
          const execp = util.promisify(exec);
          const { stdout } = await execp(`${which} ${c.names[0]}`);
          if (stdout) foundPath = stdout.split(/\r?\n/)[0].trim();
        } catch {}

        if (!foundPath) {
          foundPath = await findFirstExecutable(c.names, toolsRoots, 5);
        }

        // Explicit Java hints + JAVA_HOME
        if (!foundPath && c.key === 'Java') {
          const javaHints = [
            'D:\\portable-toolchains\\graalvm-portable\\graalvm-community-openjdk-21.0.1+12.1\\bin\\java.exe',
            'D:\\portable-toolchains\\java-portable\\bin\\java.exe',
            'D:\\portable-toolchains\\sources\\jdk-21.0.1\\bin\\java.exe'
          ];
          for (const hint of javaHints) {
            if (await exists(hint)) { foundPath = hint; break; }
          }
          if (!foundPath && process.env.JAVA_HOME) {
            const javaHomePath = path.join(process.env.JAVA_HOME, 'bin', 'java.exe');
            if (await exists(javaHomePath)) foundPath = javaHomePath;
          }
        }

        if (foundPath) {
          const item = res.compilers.find(x => x.name === c.key);
          if (item) {
            item.available = true;
            item.path = foundPath;
            // Try to get a real version when possible
            try {
              const { exec } = require('child_process');
              const util = require('util');
              const execp = util.promisify(exec);
              let versionCmd = `${foundPath} --version`;
              if (c.key === 'Java') versionCmd = `"${foundPath}" -version`;
              const { stdout, stderr } = await execp(versionCmd);
              const out = (stdout || stderr || '').split(/\r?\n/)[0];
              item.version = out || 'portable';
            } catch {
              item.version = 'portable';
            }
          }
        }
      }

      return res;
    };

    const results = await detect();
    res.json(results);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/compile', async (req, res) => {
    try {
        const { file, content } = req.body;
        const ext = path.extname(file);
        const base = path.basename(file, ext);
        const dir = path.dirname(file);
        
        let cmd = '';
        if (ext === '.c') cmd = `gcc "${file}" -o "${path.join(dir, base)}.exe"`;
        else if (ext === '.cpp') cmd = `g++ "${file}" -o "${path.join(dir, base)}.exe"`;
        else if (ext === '.java') cmd = `javac "${file}"`;
        else if (ext === '.rs') cmd = `rustc "${file}" -o "${path.join(dir, base)}.exe"`;
        else return res.json({ success: false, error: 'Unsupported file type' });
        
        exec(cmd, { cwd: dir || 'D:\\' }, (err, stdout, stderr) => {
            res.json({ success: !err, output: stdout || stderr, error: err?.message });
        });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/run', async (req, res) => {
    try {
        const { file, content } = req.body;
        const ext = path.extname(file);
        const base = path.basename(file, ext);
        const dir = path.dirname(file);
        
        let cmd = '';
        if (ext === '.exe') cmd = `"${file}"`;
        else if (ext === '.py') cmd = `python "${file}"`;
        else if (ext === '.js') cmd = `node "${file}"`;
        else if (ext === '.java') cmd = `java -cp "${dir}" ${base}`;
        else if (ext === '.c' || ext === '.cpp') cmd = `"${path.join(dir, base)}.exe"`;
        else return res.json({ success: false, error: 'Unsupported file type' });
        
        exec(cmd, { cwd: dir || 'D:\\' }, (err, stdout, stderr) => {
            res.json({ success: !err, output: stdout || stderr, error: err?.message });
        });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/execute', async (req, res) => {
    try {
        const { command, cwd } = req.body;
        exec(command, { cwd: cwd || 'D:\\' }, (err, stdout, stderr) => {
            res.json({ 
                success: !err,
                output: stdout || stderr,
                error: err?.message 
            });
        });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/file/save', async (req, res) => {
    try {
        await fs.mkdir(path.dirname(req.body.path), { recursive: true });
        await fs.writeFile(req.body.path, req.body.content);
        res.json({ success: true });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/file/write', async (req, res) => {
    try {
        await fs.mkdir(path.dirname(req.body.path), { recursive: true });
        await fs.writeFile(req.body.path, req.body.content);
        res.json({ success: true });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/command', async (req, res) => {
    exec(req.body.command, { cwd: req.body.cwd || 'D:\\' }, (err, stdout, stderr) => {
        res.json({ stdout, stderr, error: err?.message });
    });
});

app.get('/api/read-file', async (req, res) => {
    try {
        const content = await fs.readFile(req.query.path, 'utf8');
        res.json({ success: true, content });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/write-file', async (req, res) => {
    try {
        await fs.mkdir(path.dirname(req.body.path), { recursive: true });
        await fs.writeFile(req.body.path, req.body.content);
        res.json({ success: true });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Compatibility alias: some clients call /api/files/write
app.post('/api/files/write', async (req, res) => {
  try {
    const targetPath = req.body.path || req.body.filePath;
    if (!targetPath) return res.status(400).json({ error: 'path required' });
    await fs.mkdir(path.dirname(targetPath), { recursive: true });
    await fs.writeFile(targetPath, req.body.content);
    res.json({ success: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// Minimal AI stub to avoid 404s from legacy callers
app.post('/api/ai', (req, res) => {
  res.json({ success: true, message: 'AI endpoint stub. Use web-based providers or /api/ollama.' });
});

app.post('/api/terminal', (req, res) => {
    exec(req.body.command, { cwd: req.body.cwd || 'D:\\' }, (err, stdout, stderr) => {
        res.json({ output: stdout || stderr || err?.message });
    });
});

app.post('/api/run-command', (req, res) => {
    exec(req.body.cmd, { cwd: 'D:\\' }, (err, stdout, stderr) => {
        res.json({ output: stdout || stderr || err?.message });
    });
});

app.post('/api/ollama', (req, res) => {
    const data = JSON.stringify({
        model: req.body.model || 'codellama:7b',
        prompt: req.body.prompt,
        stream: false
    });
    
    const options = {
        hostname: 'localhost',
        port: 11434,
        path: '/api/generate',
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': data.length }
    };
    
    const proxyReq = http.request(options, (proxyRes) => {
        let body = '';
        proxyRes.on('data', chunk => body += chunk);
        proxyRes.on('end', () => res.json(JSON.parse(body)));
    });
    
    proxyReq.on('error', (e) => res.status(500).json({ error: e.message }));
    proxyReq.write(data);
    proxyReq.end();
});

app.get('/api/ollama/status', (req, res) => {
    const options = {
        hostname: 'localhost',
        port: 11434,
        path: '/api/tags',
        method: 'GET'
    };
    
    const proxyReq = http.request(options, (proxyRes) => {
        let body = '';
        proxyRes.on('data', chunk => body += chunk);
        proxyRes.on('end', () => {
            try {
                const data = JSON.parse(body);
                res.json({ online: true, models: data.models });
            } catch (e) {
                res.json({ online: false, error: e.message });
            }
        });
    });
    
    proxyReq.on('error', (e) => res.json({ online: false, error: e.message }));
    proxyReq.end();
});

// Optional chat proxy for Ollama chat API
app.post('/api/ollama/chat', (req, res) => {
  const data = JSON.stringify({
    model: req.body.model || 'codellama:7b',
    messages: req.body.messages || [],
    stream: false
  });

  const options = {
    hostname: 'localhost',
    port: 11434,
    path: '/api/chat',
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'Content-Length': data.length }
  };

  const proxyReq = http.request(options, (proxyRes) => {
    let body = '';
    proxyRes.on('data', chunk => body += chunk);
    proxyRes.on('end', () => {
      try { res.json(JSON.parse(body)); } catch (e) { res.status(502).json({ error: e.message, raw: body }); }
    });
  });

  proxyReq.on('error', (e) => res.status(500).json({ error: e.message }));
  proxyReq.write(data);
  proxyReq.end();
});

app.post('/api/check-file', async (req, res) => {
    try {
        await fs.access(req.body.path);
        res.json({ exists: true });
    } catch (e) {
        res.json({ exists: false });
    }
});

app.post('/api/extension/download', (req, res) => {
    const extName = path.basename(req.body.url, '.vsix');
    const extPath = path.join('D:\\MyCoPilot-Complete-Portable\\extensions', extName);
    
    fs.mkdir(extPath, { recursive: true }).then(() => {
        https.get(req.body.url, (response) => {
            const file = require('fs').createWriteStream(path.join(extPath, 'extension.vsix'));
            response.pipe(file);
            file.on('finish', () => {
                file.close();
                res.json({ success: true, name: extName });
            });
        }).on('error', (e) => {
            res.status(500).json({ error: e.message });
        });
    });
});

// Microsoft Copilot Bridge WebSocket Server
const WebSocket = require('ws');

// Create WebSocket server for Copilot bridge
const copilotWss = new WebSocket.Server({ port: 8081, path: '/copilot-bridge' });

copilotWss.on('connection', (ws) => {
  console.log('Copilot Bridge client connected');
  
  ws.on('message', (message) => {
    try {
      const data = JSON.parse(message);
      console.log('Copilot Bridge message:', data);
      
      // Handle different message types
      switch (data.type) {
        case 'COPILOT_QUERY':
          handleCopilotQuery(data, ws);
          break;
        case 'CODE_COMPLETION_REQUEST':
          handleCodeCompletionRequest(data, ws);
          break;
        case 'ERROR_FIX_REQUEST':
          handleErrorFixRequest(data, ws);
          break;
        case 'AUTO_FIX_REQUEST':
          handleAutoFixRequest(data, ws);
          break;
        case 'RUN_COMMAND':
          handleRunCommand(data, ws);
          break;
        case 'BROWSER_READY':
          handleBrowserReady(data, ws);
          break;
      }
    } catch (error) {
      console.error('Copilot Bridge message error:', error);
    }
  });
  
  ws.on('close', () => {
    console.log('Copilot Bridge client disconnected');
  });
});

async function handleCopilotQuery(data, ws) {
  // Forward to Microsoft Copilot
  try {
    const response = await fetch('https://copilot.microsoft.com/api/v1/chat', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
      },
      body: JSON.stringify({
        messages: [{ role: 'user', content: data.query }],
        stream: false
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      ws.send(JSON.stringify({
        type: 'COPILOT_RESPONSE',
        success: true,
        response: result.choices[0].message.content,
        originalQuery: data.query
      }));
    } else {
      throw new Error(`HTTP ${response.status}`);
    }
  } catch (error) {
    ws.send(JSON.stringify({
      type: 'COPILOT_RESPONSE',
      success: false,
      error: error.message,
      originalQuery: data.query
    }));
  }
}

async function handleCodeCompletionRequest(data, ws) {
  // Handle code completion requests
  const completionQuery = `Complete this code:\n\n${data.code}\n\nLanguage: ${data.language}`;
  
  try {
    const response = await fetch('https://copilot.microsoft.com/api/v1/chat', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
      },
      body: JSON.stringify({
        messages: [{ role: 'user', content: completionQuery }],
        stream: false
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      ws.send(JSON.stringify({
        type: 'CODE_COMPLETION_RESPONSE',
        success: true,
        completions: [{
          text: result.choices[0].message.content,
          label: 'Microsoft Copilot Suggestion'
        }]
      }));
    }
  } catch (error) {
    ws.send(JSON.stringify({
      type: 'CODE_COMPLETION_RESPONSE',
      success: false,
      error: error.message
    }));
  }
}

async function handleErrorFixRequest(data, ws) {
  // Handle error fix requests
  const fixQuery = `Fix this error:\n\nError: ${data.error}\nFile: ${data.file}\nLine: ${data.line}\n\nCode:\n${data.code}`;
  
  try {
    const response = await fetch('https://copilot.microsoft.com/api/v1/chat', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
      },
      body: JSON.stringify({
        messages: [{ role: 'user', content: fixQuery }],
        stream: false
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      ws.send(JSON.stringify({
        type: 'ERROR_FIX_RESPONSE',
        success: true,
        fix: result.choices[0].message.content,
        originalError: data
      }));
    }
  } catch (error) {
    ws.send(JSON.stringify({
      type: 'ERROR_FIX_RESPONSE',
      success: false,
      error: error.message,
      originalError: data
    }));
  }
}

function handleAutoFixRequest(data, ws) {
  console.log('Auto-fix request received:', data);
  
  data.issues.forEach(issue => {
    const fix = mapIssueToFix(issue);
    if (fix) {
      ws.send(JSON.stringify({
        type: 'APPLY_FIX',
        payload: fix
      }));
    }
  });
}

function mapIssueToFix(issue) {
  const fixes = {
    'duplicateSymbol': { kind: 'duplicateSymbol' },
    'backendDown': { kind: 'backendDown' },
    'missingScanEp': { kind: 'missingScanEp' },
    'ws404': { kind: 'backendDown' }
  };
  return fixes[issue.kind];
}

function handleRunCommand(data, ws) {
  const { spawn } = require('child_process');
  const process = spawn('cmd', ['/c', data.command], {
    cwd: 'D:\\',
    stdio: 'inherit'
  });
  
  process.on('close', (code) => {
    console.log(`Command '${data.command}' exited with code ${code}`);
    ws.send(JSON.stringify({
      type: 'COMMAND_RESULT',
      success: code === 0,
      exitCode: code
    }));
  });
}

function handleBrowserReady(data, ws) {
  console.log('Browser extension ready');
  ws.send(JSON.stringify({
    type: 'BRIDGE_READY',
    status: 'connected'
  }));
}

// (duplicate handleRunCommand and handleBrowserReady removed)

app.listen(PORT, () => {
  console.log(`[START] Backend server running on http://localhost:${PORT}`);
  console.log('[START] Microsoft Copilot Bridge WebSocket server started on ws://localhost:8081/copilot-bridge');
});

process.on('uncaughtException', (err) => {
  console.error('[FATAL] Uncaught exception:', err);
});

process.on('unhandledRejection', (reason) => {
  console.error('[FATAL] Unhandled rejection:', reason);
});
