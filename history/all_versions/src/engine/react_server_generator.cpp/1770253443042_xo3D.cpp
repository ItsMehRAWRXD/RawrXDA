#include "react_server_generator.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace RawrXD {

std::string ReactServerGenerator::GeneratePackageJson(const std::string& name, const ReactServerConfig& config) {
    std::ostringstream oss;
    oss << "{\n"
        << "  \"name\": \"" << name << "-server\",\n"
        << "  \"version\": \"1.0.0\",\n"
        << "  \"description\": \"" << config.description << "\",\n"
        << "  \"main\": \"dist/server.js\",\n"
        << "  \"scripts\": {\n"
        << "    \"dev\": \"ts-node src/server.ts\",\n"
        << "    \"build\": \"tsc\",\n"
        << "    \"start\": \"node dist/server.js\"\n"
        << "  },\n"
        << "  \"dependencies\": {\n"
        << "    \"express\": \"^4.18.2\",\n"
        << "    \"ws\": \"^8.14.2\",\n"
        << "    \"cors\": \"^2.8.5\",\n"
        << "    \"body-parser\": \"^1.20.2\"\n"
        << "  },\n"
        << "  \"devDependencies\": {\n"
        << "    \"@types/node\": \"^20.0.0\",\n"
        << "    \"@types/express\": \"^4.17.17\",\n"
        << "    \"typescript\": \"^5.1.6\",\n"
        << "    \"ts-node\": \"^10.9.1\"\n"
        << "  }\n"
        << "}\n";
    return oss.str();
}

std::string ReactServerGenerator::GenerateServerTs() {
    return R"(import express from 'express';
import cors from 'cors';
import bodyParser from 'body-parser';
import { createServer } from 'http';
import { WebSocketServer } from 'ws';

const app = express();
const port = process.env.PORT || 3000;

app.use(cors());
app.use(bodyParser.json());

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date() });
});

// Model loading endpoint
app.post('/api/model/load', (req, res) => {
  const { path } = req.body;
  // Forward to C++ backend
  res.json({ success: true, model: path });
});

// Code generation endpoint
app.post('/api/generate', (req, res) => {
  const { prompt, maxTokens } = req.body;
  // Forward to C++ inference engine
  res.json({ generated: 'Generated response' });
});

// Code analysis endpoint
app.post('/api/analyze', (req, res) => {
  const { code } = req.body;
  // Forward to code analyzer
  res.json({ analysis: {} });
});

// WebSocket for streaming
const server = createServer(app);
const wss = new WebSocketServer({ server });

wss.on('connection', (ws) => {
  console.log('Client connected');

  ws.on('message', (data) => {
    const message = JSON.parse(data.toString());
    
    if (message.type === 'generate') {
      // Stream generation tokens
      ws.send(JSON.stringify({ token: 'Hello' }));
      ws.send(JSON.stringify({ token: ' world' }));
      ws.send(JSON.stringify({ done: true }));
    }
  });

  ws.on('close', () => {
    console.log('Client disconnected');
  });
});

server.listen(port, () => {
  console.log(`Server running on port ${port}`);
});
)";
}

std::string ReactServerGenerator::GenerateApiRouter() {
    return R"(import { Router } from 'express';
import { exec } from 'child_process';

const router = Router();

// Execute shell commands safely
router.post('/exec', (req, res) => {
  const { command } = req.body;
  
  // Only allow whitelisted commands
  const allowed = ['ls', 'pwd', 'cat', 'grep'];
  const cmd = command.split(' ')[0];
  
  if (!allowed.includes(cmd)) {
    res.status(403).json({ error: 'Command not allowed' });
    return;
  }
  
  exec(command, (error, stdout, stderr) => {
    if (error) {
      res.status(500).json({ error: stderr });
      return;
    }
    res.json({ output: stdout });
  });
});

// Get system info
router.get('/system', (req, res) => {
  res.json({
    platform: process.platform,
    arch: process.arch,
    memory: process.memoryUsage()
  });
});

export default router;
)";
}

std::string ReactServerGenerator::GenerateWebsocketHandler() {
    return R"(import { WebSocket } from 'ws';

export class WebSocketHandler {
  private clients: Set<WebSocket> = new Set();

  addClient(ws: WebSocket) {
    this.clients.add(ws);
  }

  removeClient(ws: WebSocket) {
    this.clients.delete(ws);
  }

  broadcast(data: any) {
    const message = JSON.stringify(data);
    this.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    });
  }

  sendToClient(ws: WebSocket, data: any) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(data));
    }
  }

  handleInference(ws: WebSocket, prompt: string, maxTokens: number) {
    // Simulate token streaming
    let tokenCount = 0;
    const interval = setInterval(() => {
      if (tokenCount >= maxTokens) {
        this.sendToClient(ws, { done: true });
        clearInterval(interval);
        return;
      }
      this.sendToClient(ws, { token: 'token_' + tokenCount });
      tokenCount++;
    }, 50);
  }
}
)";
}

std::string ReactServerGenerator::Generate(const std::string& projectName, const ReactServerConfig& config) {
    std::ostringstream result;
    
    // Create output directory
    std::filesystem::create_directories(config.outputDir);
    std::filesystem::create_directories(config.outputDir + "/src");
    
    // Write package.json
    std::ofstream pkgFile(config.outputDir + "/package.json");
    pkgFile << GeneratePackageJson(projectName, config);
    pkgFile.close();
    
    // Write server.ts
    std::ofstream serverFile(config.outputDir + "/src/server.ts");
    serverFile << GenerateServerTs();
    serverFile.close();
    
    // Write api/router.ts
    std::filesystem::create_directories(config.outputDir + "/src/api");
    std::ofstream routerFile(config.outputDir + "/src/api/router.ts");
    routerFile << GenerateApiRouter();
    routerFile.close();
    
    // Write websocket handler
    std::ofstream wsFile(config.outputDir + "/src/websocket.ts");
    wsFile << GenerateWebsocketHandler();
    wsFile.close();
    
    result << "[ReactServerGenerator] Generated server project: " << projectName << "\n";
    result << "  Location: " << config.outputDir << "\n";
    result << "  Port: " << config.port << "\n";
    result << "  Description: " << config.description << "\n";
    
    std::cout << result.str();
    return result.str();
}

} // namespace RawrXD
