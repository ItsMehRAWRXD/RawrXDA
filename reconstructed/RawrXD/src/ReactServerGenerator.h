#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace RawrXD {

class ReactServerGenerator {
public:
    static bool Generate(const std::string& projectDir, const std::string& name) {
        try {
            std::filesystem::create_directories(projectDir);
            std::filesystem::create_directories(projectDir + "/public");
            std::filesystem::create_directories(projectDir + "/src");
            std::filesystem::create_directories(projectDir + "/src/components");
            std::filesystem::create_directories(projectDir + "/api/routes");
            
            // 1. package.json
            std::ofstream pkg(projectDir + "/package.json");
            pkg << R"({
  "name": ")" << name << R"(",
  "version": "1.0.0",
  "scripts": {
    "start": "node server.js",
    "dev": "nodemon server.js",
    "build": "webpack --mode production"
  },
  "dependencies": {
    "express": "^4.18.2",
    "socket.io": "^4.7.2",
    "cors": "^2.8.5",
    "dotenv": "^16.3.1"
  }
})";

            // 2. server.js
            std::ofstream srv(projectDir + "/server.js");
            srv << R"(const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
const cors = require('cors');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(cors());
app.use(express.static('public'));
app.use(express.json());

app.get('/api/health', (req, res) => res.json({ status: 'ok', uptime: process.uptime() }));

io.on('connection', (socket) => {
    console.log('Client connected:', socket.id);
    socket.emit('message', { user: 'System', text: 'Welcome to RawrXD React App' });
    socket.on('disconnect', () => console.log('Client disconnected'));
});

app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => console.log(`Server running on port ${PORT}`));
)";

            // 3. index.html
            std::ofstream idx(projectDir + "/public/index.html");
            idx << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RawrXD App</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #1a1a1a; color: #fff; margin: 0; display: flex; align-items: center; justify-content: center; height: 100vh; flex-direction: column; }
        .container { text-align: center; border: 1px solid #333; padding: 2rem; border-radius: 12px; background: #222; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        h1 { color: #61dafb; }
        .status { margin-top: 20px; color: #888; font-size: 0.9em; }
    </style>
</head>
<body>
    <div class="container">
        <h1>RawrXD React Server</h1>
        <p>Agentic Generation Complete</p>
        <div class="status">WebSocket Status: <span id="ws-status">Connecting...</span></div>
    </div>
    <script src="/socket.io/socket.io.js"></script>
    <script>
        const socket = io();
        const statusEl = document.getElementById('ws-status');
        socket.on('connect', () => { statusEl.innerText = 'Active'; statusEl.style.color = '#4caf50'; });
        socket.on('disconnect', () => { statusEl.innerText = 'Disconnected'; statusEl.style.color = '#f44336'; });
    </script>
</body>
</html>)";
            
            return true;
        } catch (...) { return false; }
    }
};

} // namespace RawrXD
