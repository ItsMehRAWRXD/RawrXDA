#include "react_server_generator.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace RawrXD {

static void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ReactServerGenerator] Failed to write " << path.string() << "\n";
        return;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static std::string package_json(const std::string& name, int port) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"name\": \"" << name << "\",\n";
    oss << "  \"version\": \"1.0.0\",\n";
    oss << "  \"type\": \"module\",\n";
    oss << "  \"scripts\": {\n";
    oss << "    \"dev\": \"node server.js\",\n";
    oss << "    \"start\": \"node server.js\"\n";
    oss << "  },\n";
    oss << "  \"engines\": {\n";
    oss << "    \"node\": \">=20\"\n";
    oss << "  },\n";
    oss << "  \"rawrxd\": {\n";
    oss << "    \"port\": " << port << "\n";
    oss << "  }\n";
    oss << "}\n";
    return oss.str();
}

static std::string index_html(const std::string& title) {
    std::ostringstream oss;
    oss << "<!doctype html>\n";
    oss << "<html lang=\"en\">\n";
    oss << "  <head>\n";
    oss << "    <meta charset=\"UTF-8\" />\n";
    oss << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n";
    oss << "    <title>" << title << "</title>\n";
    oss << "    <script crossorigin src=\"https://unpkg.com/react@18/umd/react.production.min.js\"></script>\n";
    oss << "    <script crossorigin src=\"https://unpkg.com/react-dom@18/umd/react-dom.production.min.js\"></script>\n";
    oss << "  </head>\n";
    oss << "  <body>\n";
    oss << "    <div id=\"root\"></div>\n";
    oss << "    <script type=\"module\" src=\"/app.js\"></script>\n";
    oss << "  </body>\n";
    oss << "</html>\n";
    return oss.str();
}

static std::string app_js(const std::string& title) {
    std::ostringstream oss;
    oss << "const root = ReactDOM.createRoot(document.getElementById('root'));\n";
    oss << "const App = () => React.createElement('div', { style: { fontFamily: 'sans-serif', padding: '24px' } }, [\n";
    oss << "  React.createElement('h1', { key: 'h1' }, '" << title << "'),\n";
    oss << "  React.createElement('p', { key: 'p' }, 'RawrXD React server scaffold running with zero deps.'),\n";
    oss << "  React.createElement('div', { key: 'info', style: { marginTop: '12px', opacity: 0.7 } }, 'Edit app.js to customize UI.')\n";
    oss << "]);\n";
    oss << "root.render(React.createElement(App));\n";
    return oss.str();
}

static std::string server_js(int port) {
    std::ostringstream oss;
    oss << "import http from 'http';\n";
    oss << "import { readFile, stat, watch } from 'fs/promises';\n";
    oss << "import { extname, join } from 'path';\n";
    oss << "import { WebSocketServer } from 'ws';\n\n";
    oss << "const port = process.env.PORT ? Number(process.env.PORT) : " << port << ";\n";
    oss << "const ollamaUrl = process.env.OLLAMA_URL || 'http://localhost:11434';\n";
    oss << "const root = process.cwd();\n\n";
    oss << "const mime = {\n";
    oss << "  '.html': 'text/html',\n";
    oss << "  '.js': 'text/javascript',\n";
    oss << "  '.mjs': 'text/javascript',\n";
    oss << "  '.css': 'text/css',\n";
    oss << "  '.json': 'application/json',\n";
    oss << "  '.svg': 'image/svg+xml',\n";
    oss << "  '.png': 'image/png',\n";
    oss << "  '.ico': 'image/x-icon',\n";
    oss << "  '.woff2': 'font/woff2',\n";
    oss << "};\n\n";
    oss << "// CORS headers for development\n";
    oss << "const corsHeaders = {\n";
    oss << "  'Access-Control-Allow-Origin': '*',\n";
    oss << "  'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',\n";
    oss << "  'Access-Control-Allow-Headers': 'Content-Type, Authorization',\n";
    oss << "};\n\n";
    oss << "// API proxy to Ollama\n";
    oss << "async function proxyToOllama(req, res) {\n";
    oss << "  try {\n";
    oss << "    const chunks = [];\n";
    oss << "    for await (const chunk of req) chunks.push(chunk);\n";
    oss << "    const body = Buffer.concat(chunks).toString();\n";
    oss << "    const targetUrl = ollamaUrl + req.url.replace('/api/ollama', '');\n";
    oss << "    const resp = await fetch(targetUrl, {\n";
    oss << "      method: req.method,\n";
    oss << "      headers: { 'Content-Type': 'application/json' },\n";
    oss << "      body: body || undefined,\n";
    oss << "    });\n";
    oss << "    res.writeHead(resp.status, { ...corsHeaders, 'Content-Type': 'application/json' });\n";
    oss << "    const data = await resp.text();\n";
    oss << "    res.end(data);\n";
    oss << "  } catch (err) {\n";
    oss << "    res.writeHead(502, { ...corsHeaders, 'Content-Type': 'application/json' });\n";
    oss << "    res.end(JSON.stringify({ error: 'Ollama proxy error', detail: err.message }));\n";
    oss << "  }\n";
    oss << "}\n\n";
    oss << "const server = http.createServer(async (req, res) => {\n";
    oss << "  // Handle CORS preflight\n";
    oss << "  if (req.method === 'OPTIONS') {\n";
    oss << "    res.writeHead(204, corsHeaders);\n";
    oss << "    return res.end();\n";
    oss << "  }\n\n";
    oss << "  // API proxy routes\n";
    oss << "  if (req.url.startsWith('/api/ollama')) {\n";
    oss << "    return proxyToOllama(req, res);\n";
    oss << "  }\n\n";
    oss << "  // Health check\n";
    oss << "  if (req.url === '/api/health') {\n";
    oss << "    res.writeHead(200, { ...corsHeaders, 'Content-Type': 'application/json' });\n";
    oss << "    return res.end(JSON.stringify({ status: 'ok', uptime: process.uptime() }));\n";
    oss << "  }\n\n";
    oss << "  // Static file serving\n";
    oss << "  try {\n";
    oss << "    const url = req.url.split('?')[0];\n";
    oss << "    const filePath = join(root, url === '/' ? '/index.html' : url);\n";
    oss << "    const data = await readFile(filePath);\n";
    oss << "    res.writeHead(200, {\n";
    oss << "      ...corsHeaders,\n";
    oss << "      'Content-Type': mime[extname(filePath)] || 'application/octet-stream',\n";
    oss << "      'Cache-Control': 'no-cache',\n";
    oss << "    });\n";
    oss << "    res.end(data);\n";
    oss << "  } catch (err) {\n";
    oss << "    // SPA fallback: serve index.html for any non-file route\n";
    oss << "    try {\n";
    oss << "      const data = await readFile(join(root, 'index.html'));\n";
    oss << "      res.writeHead(200, { ...corsHeaders, 'Content-Type': 'text/html' });\n";
    oss << "      res.end(data);\n";
    oss << "    } catch {\n";
    oss << "      res.writeHead(404, { ...corsHeaders, 'Content-Type': 'text/plain' });\n";
    oss << "      res.end('Not Found');\n";
    oss << "    }\n";
    oss << "  }\n";
    oss << "});\n\n";
    oss << "// WebSocket for hot reload\n";
    oss << "const wss = new WebSocketServer({ server });\n";
    oss << "const clients = new Set();\n";
    oss << "wss.on('connection', (ws) => {\n";
    oss << "  clients.add(ws);\n";
    oss << "  ws.on('close', () => clients.delete(ws));\n";
    oss << "  ws.on('message', (msg) => {\n";
    oss << "    // Echo for ping/pong\n";
    oss << "    try { ws.send(msg.toString()); } catch {}\n";
    oss << "  });\n";
    oss << "});\n\n";
    oss << "// File watcher for hot reload\n";
    oss << "const notifyReload = () => {\n";
    oss << "  for (const ws of clients) {\n";
    oss << "    try { ws.send(JSON.stringify({ type: 'reload' })); } catch {}\n";
    oss << "  }\n";
    oss << "};\n\n";
    oss << "(async () => {\n";
    oss << "  try {\n";
    oss << "    const watcher = watch(root, { recursive: true });\n";
    oss << "    for await (const event of watcher) {\n";
    oss << "      if (event.filename && /\\.(js|html|css|json)$/.test(event.filename)) {\n";
    oss << "        console.log(`[HMR] ${event.filename} changed, notifying clients`);\n";
    oss << "        notifyReload();\n";
    oss << "      }\n";
    oss << "    }\n";
    oss << "  } catch (err) {\n";
    oss << "    console.warn('[HMR] File watching not available:', err.message);\n";
    oss << "  }\n";
    oss << "})();\n\n";
    oss << "server.listen(port, () => {\n";
    oss << "  console.log(`RawrXD React server running on http://localhost:${port}`);\n";
    oss << "  console.log(`Ollama proxy: /api/ollama -> ${ollamaUrl}`);\n";
    oss << "  console.log(`Hot reload: WebSocket on ws://localhost:${port}`);\n";
    oss << "});\n";
    return oss.str();
}

std::string ReactServerGenerator::Generate(const std::string& project_name, const ReactServerConfig& config) {
    std::filesystem::path base = config.outputDir.empty()
        ? std::filesystem::current_path() / project_name
        : std::filesystem::path(config.outputDir);

    std::filesystem::create_directories(base);
    write_file(base / "package.json", package_json(project_name, config.port));
    write_file(base / "index.html", index_html(project_name));
    write_file(base / "app.js", app_js(project_name));
    write_file(base / "server.js", server_js(config.port));

    // Generate .env file for configuration
    std::ostringstream envContent;
    envContent << "PORT=" << config.port << "\n";
    envContent << "OLLAMA_URL=http://localhost:11434\n";
    envContent << "NODE_ENV=development\n";
    write_file(base / ".env", envContent.str());

    // Generate hot-reload client snippet
    std::ostringstream hmrClient;
    hmrClient << "// Hot Module Reload client\n";
    hmrClient << "(function() {\n";
    hmrClient << "  const ws = new WebSocket(`ws://${location.host}`);\n";
    hmrClient << "  ws.onmessage = (e) => {\n";
    hmrClient << "    try {\n";
    hmrClient << "      const msg = JSON.parse(e.data);\n";
    hmrClient << "      if (msg.type === 'reload') location.reload();\n";
    hmrClient << "    } catch {}\n";
    hmrClient << "  };\n";
    hmrClient << "  ws.onclose = () => setTimeout(() => location.reload(), 1000);\n";
    hmrClient << "})();\n";
    write_file(base / "hmr-client.js", hmrClient.str());

    // Generate README
    std::ostringstream readme;
    readme << "# " << project_name << "\n\n";
    readme << "Generated by RawrXD React Server Generator\n\n";
    readme << "## Quick Start\n\n";
    readme << "```bash\n";
    readme << "npm install ws  # WebSocket dependency\n";
    readme << "npm run dev     # Start development server\n";
    readme << "```\n\n";
    readme << "## Features\n\n";
    readme << "- Zero-config React development server\n";
    readme << "- CORS enabled for development\n";
    readme << "- Ollama API proxy at `/api/ollama`\n";
    readme << "- WebSocket-based hot reload\n";
    readme << "- SPA fallback routing\n";
    readme << "- Health check at `/api/health`\n\n";
    readme << "## Environment Variables\n\n";
    readme << "| Variable | Default | Description |\n";
    readme << "|----------|---------|-------------|\n";
    readme << "| PORT | " << config.port << " | Server port |\n";
    readme << "| OLLAMA_URL | http://localhost:11434 | Ollama API endpoint |\n";
    readme << "| NODE_ENV | development | Environment mode |\n";
    write_file(base / "README.md", readme.str());

    std::ostringstream summary;
    summary << "[ReactServerGenerator] Files created:\n";
    summary << "  - package.json\n";
    summary << "  - index.html\n";
    summary << "  - app.js\n";
    summary << "  - server.js (CORS + WebSocket + Ollama proxy + HMR)\n";
    summary << "  - hmr-client.js\n";
    summary << "  - .env\n";
    summary << "  - README.md\n";
    return summary.str();
}

} // namespace RawrXD
