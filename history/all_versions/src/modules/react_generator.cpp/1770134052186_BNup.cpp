#include "react_generator.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace RawrXD {

bool ReactServerGenerator::Generate(const std::string& project_dir, const ReactServerConfig& config) {
    try {
        std::filesystem::path dir(project_dir);
        std::filesystem::create_directories(dir);
        std::filesystem::create_directories(dir / "public");
        std::filesystem::create_directories(dir / "src");
        std::filesystem::create_directories(dir / "src" / "components");
        
        if (config.include_tests) {
            std::filesystem::create_directories(dir / "tests");
        }
        
        // Generate all files
        if (!GeneratePackageJson(dir, config)) return false;
        if (!GenerateServerJs(dir, config)) return false;
        if (!GenerateIndexHtml(dir, config)) return false;
        if (!GenerateAppJs(dir, config)) return false;
        if (!GenerateEnvFile(dir, config)) return false;
        if (!GenerateReadme(dir, config)) return false;
        if (!GenerateGitignore(dir, config)) return false;
        if (config.include_docker) {
            if (!GenerateDockerfile(dir, config)) return false;
        }
        if (config.include_tests) {
            if (!GenerateTestFiles(dir, config)) return false;
        }
        
        // Generate IDE components if enabled
        if (config.include_ide_features) {
            if (!GenerateIDEComponents(dir, config)) return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Generation error: " << e.what() << std::endl;
        return false;
    }
}

bool ReactServerGenerator::GeneratePackageJson(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "package.json");
    file << GetPackageJsonContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateServerJs(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "server.js");
    file << GetServerJsContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateIndexHtml(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "public" / "index.html");
    file << GetIndexHtmlContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateAppJs(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "App.js");
    file << GetAppJsContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateEnvFile(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / ".env");
    file << GetEnvContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateReadme(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "README.md");
    file << GetReadmeContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateGitignore(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / ".gitignore");
    file << GetGitignoreContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateDockerfile(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "Dockerfile");
    file << GetDockerfileContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateTestFiles(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "tests" / "app.test.js");
    file << GetTestContent(config);
    return file.good();
}

// IDE Component Generation Implementation

bool ReactServerGenerator::GenerateIDEComponents(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::filesystem::create_directories(dir / "src" / "components" / "ide");
    
    if (config.include_monaco_editor && !GenerateMonacoEditor(dir, config)) return false;
    if (config.include_agent_modes && !GenerateAgentModePanel(dir, config)) return false;
    if (config.include_engine_management && !GenerateEngineManager(dir, config)) return false;
    if (config.include_memory_viewer && !GenerateMemoryViewer(dir, config)) return false;
    if (config.include_tool_output && !GenerateToolOutputPanel(dir, config)) return false;
    if (config.include_hotpatch_controls && !GenerateHotpatchControls(dir, config)) return false;
    if (config.include_re_tools && !GenerateREToolsPanel(dir, config)) return false;
    if (!GenerateMainIDEApp(dir, config)) return false;
    
    return true;
}

bool ReactServerGenerator::GenerateMonacoEditor(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "CodeEditor.js");
    file << GetMonacoEditorContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateAgentModePanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "AgentModes.js");
    file << GetAgentModePanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateEngineManager(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "EngineManager.js");
    file << GetEngineManagerContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateMemoryViewer(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "MemoryViewer.js");
    file << GetMemoryViewerContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateToolOutputPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "ToolOutput.js");
    file << GetToolOutputPanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateHotpatchControls(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "Hotpatch.js");
    file << GetHotpatchControlsContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateREToolsPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "components" / "ide" / "RETools.js");
    file << GetREToolsPanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateMainIDEApp(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "src" / "MainIDE.js");
    file << GetMainIDEAppContent(config);
    return file.good();
}

bool ReactServerGenerator::RegenerateMonacoEditor(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateMonacoEditor(dir, config);
}
bool ReactServerGenerator::RegenerateAgentModePanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateAgentModePanel(dir, config);
}
bool ReactServerGenerator::RegenerateEngineManager(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateEngineManager(dir, config);
}
bool ReactServerGenerator::RegenerateMemoryViewer(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateMemoryViewer(dir, config);
}
bool ReactServerGenerator::RegenerateToolOutputPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateToolOutputPanel(dir, config);
}
bool ReactServerGenerator::RegenerateHotpatchControls(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateHotpatchControls(dir, config);
}
bool ReactServerGenerator::RegenerateREToolsPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateREToolsPanel(dir, config);
}
bool ReactServerGenerator::RegenerateMainIDEApp(const std::filesystem::path& dir, const ReactServerConfig& config) {
    return GenerateMainIDEApp(dir, config);
}

std::string ReactServerGenerator::GetPackageJsonContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << R"({
  "name": ")" << config.name << R"(",
  "version": "1.0.0",
  "description": ")" << config.description << R"(",
  "main": "server.js",
  "scripts": {
    "start": "node server.js",
    "dev": "nodemon server.js",
    "build": "npm run build:client",
    "build:client": "cd src && npm run build")";
    
    if (config.include_typescript) {
        ss << R"(,
    "build:ts": "tsc")";
    }
    
    if (config.include_tests) {
        ss << R"(,
    "test": "jest")";
    }
    
    ss << R"(
  },
  "dependencies": {
    "express": "^4.18.2",
    "cors": "^2.8.5",
    "dotenv": "^16.0.3")";
    
    if (config.include_auth) {
        ss << R"(,
    "jsonwebtoken": "^9.0.0",
    "bcryptjs": "^2.4.3")";
    }
    
    if (config.include_database) {
        if (config.database_type == "sqlite") {
            ss << R"(,
    "sqlite3": "^5.1.6")";
        } else if (config.database_type == "mongodb") {
            ss << R"(,
    "mongoose": "^7.0.0")";
        } else if (config.database_type == "postgresql") {
            ss << R"(,
    "pg": "^8.10.0")";
        }
    }
    
    if (config.include_tailwind) {
        ss << R"(,
    "tailwindcss": "^3.3.0")";
    }

    // IDE Dependencies
    if (config.include_ide_features) {
        ss << R"(,
    "lucide-react": "^0.263.1",
    "framer-motion": "^10.12.16",
    "axios": "^1.4.0",
    "clsx": "^1.2.1",
    "tailwind-merge": "^1.13.0")";
    }

    if (config.include_monaco_editor) {
        ss << R"(,
    "@monaco-editor/react": "^4.5.1")";
    }
    
    ss << R"(
  },
  "devDependencies": {
    "nodemon": "^3.0.0")";
    
    if (config.include_typescript) {
        ss << R"(,
    "typescript": "^5.0.0",
    "@types/node": "^18.0.0")";
    }
    
    if (config.include_tests) {
        ss << R"(,
    "jest": "^29.0.0",
    "supertest": "^6.3.0")";
    }
    
    ss << R"(
  },
  "keywords": ["rawrxd", "generated", "react", "express")";
    
    if (config.include_auth) ss << R"(, "auth")";
    if (config.include_database) ss << R"(, "database")";
    if (config.include_typescript) ss << R"(, "typescript")";
    if (config.include_tailwind) ss << R"(, "tailwind")";
    if (config.include_docker) ss << R"(, "docker")";
    if (config.include_tests) ss << R"(, "testing")";
    
    ss << R"(],
  "author": ")" << config.author << R"(",
  "license": "MIT"
})";
    return ss.str();
}

std::string ReactServerGenerator::GetServerJsContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << R"(const express = require('express');
const cors = require('cors');
const path = require('path');
const http = require('http');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || )" << config.port << R"();

// C++ Backend Configuration
const CPP_BACKEND_HOST = process.env.CPP_BACKEND_HOST || ')" << config.cpp_backend_host << R"(';
const CPP_BACKEND_PORT = process.env.CPP_BACKEND_PORT || ')" << config.cpp_backend_port << R"(';

// Middleware
app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.static('public'));

// Proxy function to C++ backend
function proxyToCppBackend(endpoint, req, res) {
    const options = {
        hostname: CPP_BACKEND_HOST,
        port: CPP_BACKEND_PORT,
        path: endpoint,
        method: req.method,
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': req.body ? Buffer.byteLength(JSON.stringify(req.body)) : 0
        }
    };

    const proxyReq = http.request(options, (proxyRes) => {
        let data = '';
        proxyRes.on('data', (chunk) => { data += chunk; });
        proxyRes.on('end', () => {
            try {
                res.status(proxyRes.statusCode).json(JSON.parse(data));
            } catch {
                res.status(proxyRes.statusCode).send(data);
            }
        });
    });

    proxyReq.on('error', (err) => {
        console.error(`Backend error: ${err.message}`);
        res.status(502).json({ error: 'C++ backend unavailable', details: err.message });
    });

    if (req.body) {
        proxyReq.write(JSON.stringify(req.body));
    }
    proxyReq.end();
}

// API Routes
app.get('/api/health', (req, res) => {
    res.json({ 
        status: 'ok', 
        agent: 'RawrXD', 
        timestamp: new Date(),
        version: '1.0.0'
    });
});

app.get('/api/config', (req, res) => {
    res.json({
        name: process.env.APP_NAME || ')" << config.name << R"(',
        description: process.env.APP_DESCRIPTION || ')" << config.description << R"(',
        version: '1.0.0',
        features: {
            auth: )" << (config.include_auth ? "true" : "false") << R"(,
            database: )" << (config.include_database ? "true" : "false") << R"(,
            ide: )" << (config.include_ide_features ? "true" : "false") << R"(
        }
    });
});

// IDE Proxy Routes
app.all('/api/inference', (req, res) => proxyToCppBackend('/inference', req, res));
app.all('/api/engines', (req, res) => proxyToCppBackend('/engines', req, res));
app.all('/api/engine/load', (req, res) => proxyToCppBackend('/engine/load', req, res));
app.all('/api/mode*', (req, res) => proxyToCppBackend(req.path.replace('/api', ''), req, res));
app.all('/api/tools*', (req, res) => proxyToCppBackend(req.path.replace('/api', ''), req, res));
app.all('/api/memory*', (req, res) => proxyToCppBackend(req.path.replace('/api', ''), req, res));

// Error handling
app.use((err, req, res, next) => {
    console.error(err.stack);
    res.status(500).json({ error: 'Something went wrong!' });
});

// Serve React app
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
    console.log(`RawrXD React Server running on http://localhost:${PORT}`);
    console.log(`C++ Backend expected at ${CPP_BACKEND_HOST}:${CPP_BACKEND_PORT}`);
});

module.exports = app;
)";
    return ss.str();
}

std::string ReactServerGenerator::GetIndexHtmlContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="description" content=")" << config.description << R"(">
    <title>)" << config.name << R"(</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
            max-width: 800px;
            width: 100%;
            text-align: center;
        }
        
        h1 {
            color: #333;
            margin-bottom: 20px;
            font-size: 2.5em;
            background: linear-gradient(135deg, #667eea, #764ba2);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        
        .badge-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 10px;
            margin: 20px 0;
        }
        
        .badge {
            display: inline-block;
            background: #667eea;
            color: white;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 0.9em;
            font-weight: 500;
        }
        
        .feature-list {
            text-align: left;
            margin: 20px 0;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 10px;
        }
        
        .feature-list h3 {
            margin-bottom: 10px;
            color: #333;
        }
        
        .feature-list ul {
            list-style: none;
            padding: 0;
        }
        
        .feature-list li {
            padding: 5px 0;
            color: #666;
        }
        
        .feature-list li:before {
            content: "✓ ";
            color: #667eea;
            font-weight: bold;
        }
        
        .button-container {
            display: flex;
            gap: 10px;
            justify-content: center;
            margin-top: 30px;
            flex-wrap: wrap;
        }
        
        button {
            background: #667eea;
            color: white;
            border: none;
            padding: 12px 30px;
            border-radius: 8px;
            font-size: 1em;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.3s ease;
            min-width: 150px;
        }
        
        button:hover {
            background: #764ba2;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
        }
        
        button:active {
            transform: translateY(0);
        }
        
        .status {
            margin-top: 30px;
            padding: 20px;
            background: #f0f4ff;
            border-radius: 10px;
            border-left: 4px solid #667eea;
            text-align: left;
        }
        
        .status h3 {
            margin-bottom: 10px;
            color: #667eea;
        }
        
        .status pre {
            background: white;
            padding: 10px;
            border-radius: 5px;
            overflow-x: auto;
            font-size: 0.9em;
        }
        
        .code-block {
            background: #f8f9fa;
            border: 1px solid #e0e0e0;
            border-radius: 5px;
            padding: 15px;
            margin: 10px 0;
            text-align: left;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            overflow-x: auto;
        }
        
        .info-box {
            background: #e8f4fd;
            border: 1px solid #b3d9ff;
            border-radius: 5px;
            padding: 15px;
            margin: 20px 0;
            text-align: left;
        }
        
        .info-box h4 {
            color: #0066cc;
            margin-bottom: 10px;
        }
        
        .info-box p {
            color: #333;
            line-height: 1.5;
        }
        
        @media (max-width: 768px) {
            .container {
                padding: 20px;
                margin: 10px;
            }
            
            h1 {
                font-size: 2em;
            }
            
            .button-container {
                flex-direction: column;
                align-items: center;
            }
            
            button {
                width: 100%;
                max-width: 250px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>)" << config.name << R"(</h1>
        <p>)" << config.description << R"(</p>
        
        <div class="badge-container">
            <span class="badge">Express.js</span>
            <span class="badge">React</span>
            <span class="badge">RESTful API</span>
            <span class="badge">CORS</span>
            <span class="badge">Environment Config</span>)";
    
    if (config.include_auth) {
        ss << R"(
            <span class="badge">Authentication</span>)";
    }
    if (config.include_database) {
        ss << R"(
            <span class="badge">Database</span>)";
    }
    if (config.include_typescript) {
        ss << R"(
            <span class="badge">TypeScript</span>)";
    }
    if (config.include_tailwind) {
        ss << R"(
            <span class="badge">Tailwind CSS</span>)";
    }
    if (config.include_docker) {
        ss << R"(
            <span class="badge">Docker</span>)";
    }
    if (config.include_tests) {
        ss << R"(
            <span class="badge">Testing</span>)";
    }
    
    ss << R"(
        </div>
        
        <div class="info-box">
            <h4>🚀 Quick Start</h4>
            <p>This React + Express server was generated by RawrXD AI. It includes a modern frontend with a robust backend API.</p>
        </div>
        
        <div class="feature-list">
            <h3>✨ Features</h3>
            <ul>
                <li>Express.js backend with RESTful API</li>
                <li>React frontend with modern styling</li>
                <li>CORS enabled for cross-origin requests</li>
                <li>Environment configuration with dotenv</li>
                <li>Health check and config endpoints</li>
                <li>Error handling middleware</li>
                <li>Static file serving for React app</li>)";
    
    if (config.include_auth) {
        ss << R"(
                <li>JWT authentication system</li>
                <li>User login and registration</li>
                <li>Protected routes</li>)";
    }
    
    if (config.include_database) {
        ss << R"(
                <li>)" << config.database_type << R"( database integration</li>
                <li>Data persistence layer</li>)";
    }
    
    if (config.include_typescript) {
        ss << R"(
                <li>TypeScript support for type safety</li>)";
    }
    
    if (config.include_tailwind) {
        ss << R"(
                <li>Tailwind CSS for rapid styling</li>)";
    }
    
    if (config.include_docker) {
        ss << R"(
                <li>Docker support for containerization</li>)";
    }
    
    if (config.include_tests) {
        ss << R"(
                <li>Jest testing framework</li>
                <li>Supertest for API testing</li>)";
    }
    
    ss << R"(
            </ul>
        </div>
        
        <div class="code-block">
# Install dependencies
npm install

# Start development server
npm run dev

# Start production server
npm start

# Run tests
npm test
        </div>
        
        <div class="button-container">
            <button onclick="checkHealth()">🔍 Check Health</button>
            <button onclick="showConfig()">⚙️ Show Config</button>
            <button onclick="alert('Interactive!')">🎮 Interactive Demo</button>
            <button onclick="window.open('https://github.com/ItsMehRAWRXD/RawrXD', '_blank')">📚 Documentation</button>
        </div>
        
        <div class="status" id="status">
            <h3>📊 System Status</h3>
            <p>Ready to connect...</p>
        </div>
        
        <div class="info-box">
            <h4>💡 Pro Tips</h4>
            <p>• Use <code>npm run dev</code> for development with auto-restart<br>
               • Check the API endpoints at <code>/api/health</code> and <code>/api/config</code><br>
               • Customize the environment variables in <code>.env</code><br>
               • Extend the API by adding new routes in <code>server.js</code><br>
               • Build for production with <code>npm run build</code></p>
        </div>
    </div>
    
    <script>
        async function checkHealth() {
            try {
                const response = await fetch('/api/health');
                const data = await response.json();
                document.getElementById('status').innerHTML = 
                    '<h3>📊 System Status</h3><pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                document.getElementById('status').innerHTML = 
                    '<h3>📊 System Status</h3><p style="color: red;">Error: ' + error.message + '</p>';
            }
        }
        
        async function showConfig() {
            try {
                const response = await fetch('/api/config');
                const data = await response.json();
                document.getElementById('status').innerHTML = 
                    '<h3>📊 System Status</h3><pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                document.getElementById('status').innerHTML = 
                    '<h3>📊 System Status</h3><p style="color: red;">Error: ' + error.message + '</p>';
            }
        }
        
        // Auto-check health on load
        window.onload = checkHealth;
    </script>
</body>
</html>
)";
    return ss.str();
}

std::string ReactServerGenerator::GetAppJsContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << R"(import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
    const [health, setHealth] = useState(null);
    const [config, setConfig] = useState(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        checkHealth();
        showConfig();
    }, []);

    const checkHealth = async () => {
        try {
            const response = await fetch('/api/health');
            const data = await response.json();
            setHealth(data);
        } catch (error) {
            setHealth({ error: error.message });
        }
    };

    const showConfig = async () => {
        try {
            const response = await fetch('/api/config');
            const data = await response.json();
            setConfig(data);
            setLoading(false);
        } catch (error) {
            setConfig({ error: error.message });
            setLoading(false);
        }
    };

    return (
        <div className="App">
            <header className="App-header">
                <h1>)" << config.name << R"(</h1>
                <p className="App-description">)" << config.description << R"(</p>
                
                {loading && <div className="loading">Loading...</div>}
                
                {health && !health.error && (
                    <div className="health-status">
                        <h2>System Health</h2>
                        <p><strong>Status:</strong> {health.status}</p>
                        <p><strong>Agent:</strong> {health.agent}</p>
                        <p><strong>Version:</strong> {health.version}</p>
                        <p><strong>Time:</strong> {health.timestamp}</p>
                    </div>
                )}
                
                {config && !config.error && (
                    <div className="config">
                        <h2>Configuration</h2>
                        <pre>{JSON.stringify(config, null, 2)}</pre>
                    </div>
                )}
                
                <div className="button-group">
                    <button onClick={checkHealth}>Refresh Health</button>
                    <button onClick={showConfig}>Refresh Config</button>
                    <button onClick={() => alert('Interactive!')}>Interactive Demo</button>
                </div>
            </header>
        </div>
    );
}

export default App;
)";

    if (config.include_typescript) {
        ss << R"(
// TypeScript definitions
interface HealthStatus {
    status: string;
    agent: string;
    version: string;
    timestamp: string;
}

interface AppConfig {
    name: string;
    description: string;
    version: string;
    features: {
        auth: boolean;
        database: boolean;
        typescript: boolean;
        tailwind: boolean;
    };
}
)";
    }
    
    return ss.str();
}

std::string ReactServerGenerator::GetEnvContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << "# " << config.name << " Environment Configuration\n";
    ss << "APP_NAME=" << config.name << "\n";
    ss << "APP_DESCRIPTION=" << config.description << "\n";
    ss << "PORT=" << config.port << "\n";
    ss << "NODE_ENV=development\n\n";
    
    if (config.include_auth) {
        ss << "# JWT Authentication\n";
        ss << "JWT_SECRET=your-super-secret-jwt-key-change-this-in-production\n";
        ss << "JWT_EXPIRES_IN=7d\n\n";
    }
    
    if (config.include_database) {
        ss << "# Database Configuration\n";
        if (config.database_type == "sqlite") {
            ss << "DATABASE_URL=./database.sqlite\n";
        } else if (config.database_type == "mongodb") {
            ss << "DATABASE_URL=mongodb://localhost:27017/" << config.name << "\n";
        } else if (config.database_type == "postgresql") {
            ss << "DATABASE_URL=postgresql://user:password@localhost:5432/" << config.name << "\n";
        }
        ss << "\n";
    }
    
    ss << "# Additional Configuration\n";
    ss << "# Add your custom environment variables here\n";
    return ss.str();
}

std::string ReactServerGenerator::GetReadmeContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << "# " << config.name << "\n\n";
    ss << config.description << "\n\n";
    
    ss << "## ✨ Features\n\n";
    ss << "- ✅ Express.js backend with RESTful API\n";
    ss << "- ✅ React frontend with modern styling\n";
    ss << "- ✅ CORS enabled for cross-origin requests\n";
    ss << "- ✅ Environment configuration with dotenv\n";
    ss << "- ✅ Health check and config endpoints\n";
    ss << "- ✅ Error handling middleware\n";
    ss << "- ✅ Static file serving for React app\n";
    
    if (config.include_auth) {
        ss << "- ✅ JWT authentication system\n";
        ss << "- ✅ User login and registration\n";
        ss << "- ✅ Protected routes\n";
    }
    
    if (config.include_database) {
        ss << "- ✅ " << config.database_type << " database integration\n";
        ss << "- ✅ Data persistence layer\n";
    }
    
    if (config.include_typescript) {
        ss << "- ✅ TypeScript support for type safety\n";
    }
    
    if (config.include_tailwind) {
        ss << "- ✅ Tailwind CSS for rapid styling\n";
    }
    
    if (config.include_docker) {
        ss << "- ✅ Docker support for containerization\n";
    }
    
    if (config.include_tests) {
        ss << "- ✅ Jest testing framework\n";
        ss << "- ✅ Supertest for API testing\n";
    }
    
    ss << "\n## 🚀 Quick Start\n\n";
    ss << "```bash\n";
    ss << "# Install dependencies\n";
    ss << "npm install\n\n";
    ss << "# Start development server\n";
    ss << "npm run dev\n\n";
    ss << "# Start production server\n";
    ss << "npm start\n";
    ss << "```\n\n";
    
    ss << "The server will start on http://localhost:" << config.port << "\n\n";
    
    ss << "## 📡 API Endpoints\n\n";
    ss << "### System\n";
    ss << "- `GET /api/health` - Health check\n";
    ss << "- `GET /api/config` - Application configuration\n\n";
    
    if (config.include_auth) {
        ss << "### Authentication\n";
        ss << "- `POST /api/auth/login` - User login\n";
        ss << "- `POST /api/auth/register` - User registration\n";
        ss << "- `GET /api/auth/me` - Get current user info\n\n";
    }
    
    if (config.include_database) {
        ss << "### Data\n";
        ss << "- `GET /api/data` - Retrieve data\n";
        ss << "- `POST /api/data` - Create data\n\n";
    }
    
    ss << "### Frontend\n";
    ss << "- `GET *` - Serve React app (all other routes)\n\n";
    
    ss << "## 🛠 Development\n\n";
    ss << "Run with nodemon for auto-restart:\n";
    ss << "```bash\nnpm run dev\n```\n\n";
    
    if (config.include_typescript) {
        ss << "Build TypeScript:\n";
        ss << "```bash\nnpm run build:ts\n```\n\n";
    }
    
    if (config.include_tests) {
        ss << "Run tests:\n";
        ss << "```bash\nnpm test\n```\n\n";
    }
    
    ss << "## 📦 Project Structure\n\n";
    ss << "```\n";
    ss << config.name << "/\n";
    ss << "├── server.js          # Express server\n";
    ss << "├── .env               # Environment variables\n";
    ss << "├── package.json       # Dependencies and scripts\n";
    ss << "├── public/            # Static files\n";
    ss << "│   └── index.html     # React entry point\n";
    ss << "├── src/               # React source code\n";
    ss << "│   ├── App.js         # Main React component\n";
    ss << "│   └── components/    # React components\n";
    if (config.include_tests) {
        ss << "├── tests/             # Test files\n";
        ss << "│   └── app.test.js    # App tests\n";
    }
    ss << "└── README.md          # This file\n";
    ss << "```\n\n";
    
    ss << "## 🔧 Configuration\n\n";
    ss << "Edit the `.env` file to configure:\n";
    ss << "- Port number\n";
    if (config.include_database) {
        ss << "- Database connection\n";
    }
    if (config.include_auth) {
        ss << "- JWT secret\n";
    }
    ss << "- App name and description\n\n";
    
    if (config.include_docker) {
        ss << "## 🐳 Docker Support\n\n";
        ss << "Build and run with Docker:\n";
        ss << "```bash\n";
        ss << "docker build -t " << config.name << " .\n";
        ss << "docker run -p " << config.port << ":" << config.port << " " << config.name << "\n";
        ss << "```\n\n";
    }
    
    ss << "## 🤝 Contributing\n\n";
    ss << "This project was generated by RawrXD AI. Feel free to extend and customize it for your needs.\n\n";
    
    ss << "## 📄 License\n\n";
    ss << "MIT License - see LICENSE file for details\n\n";
    
    ss << "---\n";
    ss << "Generated by RawrXD AI v6.0\n";
    ss << "For more information: https://github.com/ItsMehRAWRXD/RawrXD\n";
    
    return ss.str();
}

std::string ReactServerGenerator::GetGitignoreContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << "# Dependencies\nnode_modules/\nnpm-debug.log*\nyarn-debug.log*\nyarn-error.log*\n\n";
    ss << "# Environment variables\n.env\n.env.local\n.env.development.local\n.env.test.local\n.env.production.local\n\n";
    ss << "# Build outputs\ndist/\nbuild/\n*.tsbuildinfo\n\n";
    ss << "# Database files\n*.sqlite\n*.db\n\n";
    ss << "# Logs\nlogs\n*.log\n\n";
    ss << "# IDE files\n.vscode/\n.idea/\n*.swp\n*.swo\n*~\n\n";
    ss << "# OS files\n.DS_Store\nThumbs.db\n\n";
    ss << "# Temporary files\ntmp/\ntemp/\n*.tmp\n*.temp\n\n";
    ss << "# Coverage reports\ncoverage/\n*.lcov\n\n";
    ss << "# Debug files\nnpm-debug.log*\nyarn-debug.log*\nyarn-error.log*\n\n";
    ss << "# Optional npm cache directory\n.npm\n\n";
    ss << "# Optional eslint cache\n.eslintcache\n\n";
    ss << "# Optional REPL history\n.node_repl_history\n\n";
    ss << "# Output of 'npm pack'\n*.tgz\n\n";
    ss << "# Yarn Integrity file\n.yarn-integrity\n\n";
    ss << "# parcel-bundler cache (https://parceljs.org/)\n.cache\n.parcel-cache\n\n";
    ss << "# next.js build output\n.next\n\n";
    ss << "# nuxt.js build output\n.nuxt\n\n";
    ss << "# vuepress build output\n.vuepress/dist\n\n";
    ss << "# Serverless directories\n.serverless\n\n";
    ss << "# FuseBox cache\n.fusebox/\n\n";
    ss << "# DynamoDB Local files\n.dynamodb/\n\n";
    ss << "# TernJS port file\n.tern-port\n\n";
    ss << "# Stores VSCode versions used for testing VSCode extensions\n.vscode-test\n\n";
    ss << "# yarn v2\n.yarn/cache\n.yarn/unplugged\n.yarn/build-state.yml\n.yarn/install-state.gz\n.pnp.*\n";
    
    return ss.str();
}

std::string ReactServerGenerator::GetDockerfileContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << "# Use official Node.js runtime as base image\nFROM node:18-alpine\n\n";
    ss << "# Set working directory\nWORKDIR /app\n\n";
    ss << "# Copy package files\nCOPY package*.json ./\n\n";
    ss << "# Install dependencies\nRUN npm ci --only=production\n\n";
    ss << "# Copy application code\nCOPY . .\n\n";
    ss << "# Create non-root user\nRUN addgroup -g 1001 -S nodejs\nRUN adduser -S nodeuser -u 1001\n\n";
    ss << "# Change ownership of the app directory\nRUN chown -R nodeuser:nodejs /app\nUSER nodeuser\n\n";
    ss << "# Expose port\nEXPOSE " << config.port << "\n\n";
    ss << "# Start the application\nCMD [\"npm\", \"start\"]\n\n";
    ss << "# Health check\nHEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \\\n  CMD curl -f http://localhost:" << config.port << "/api/health || exit 1\n";
    
    return ss.str();
}

std::string ReactServerGenerator::GetTestContent(const ReactServerConfig& config) {
    std::stringstream ss;
    ss << R"(const request = require('supertest');
const app = require('../server.js');

describe('API Tests', () => {
    it('should return health status', async () => {
        const res = await request(app).get('/api/health');
        expect(res.statusCode).toEqual(200);
        expect(res.body.status).toEqual('ok');
    });

    it('should return config', async () => {
        const res = await request(app).get('/api/config');
        expect(res.statusCode).toEqual(200);
        expect(res.body.name).toBeDefined();
        expect(res.body.features).toBeDefined();
    });
});

describe('React App', () => {
    it('should serve the React app', async () => {
        const res = await request(app).get('/');
        expect(res.statusCode).toEqual(200);
        expect(res.text).toContain(')" << config.name << R"(');
    });
});
)";
    return ss.str();
}

std::string ReactServerGenerator::GetMonacoEditorContent(const ReactServerConfig& config) {
    return R"(import React from 'react';
import Editor from '@monaco-editor/react';

export const CodeEditor = ({ code, onChange, language = 'cpp' }) => {
  return (
    <div className="h-full w-full border border-gray-700 rounded overflow-hidden">
      <Editor
        height="100%"
        defaultLanguage={language}
        defaultValue="// RawrXD Code Editor"
        value={code}
        onChange={onChange}
        theme="vs-dark"
        options={{
          minimap: { enabled: true },
          fontSize: 14,
          scrollBeyondLastLine: false,
          automaticLayout: true,
        }}
      />
    </div>
  );
};
)";
}

std::string ReactServerGenerator::GetAgentModePanelContent(const ReactServerConfig& config) {
    return R"(import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { Settings, Zap, Brain, Shield, Search } from 'lucide-react';

export const AgentModes = () => {
  const [modes, setModes] = useState({
    maxMode: false,
    deepThinking: false,
    deepResearch: false,
    noRefusal: false,
    autoCorrect: false
  });

  const [loading, setLoading] = useState(false);

  useEffect(() => {
    fetchStatus();
  }, []);

  const fetchStatus = async () => {
    try {
        const res = await axios.get('/api/status');
        setModes(res.data.modes || {});
    } catch (e) {
        console.error("Failed to fetch modes", e);
    }
  };

  const toggleMode = async (mode, value) => {
    setLoading(true);
    try {
      // Simulate command like "/deepthinking on"
      const cmd = `/${mode.toLowerCase()} ${value ? 'on' : 'off'}`;
      await axios.post('/api/command', { command: cmd });
      setModes(prev => ({ ...prev, [mode]: value }));
    } catch (e) {
      console.error(`Failed to toggle ${mode}`, e);
    } finally {
      setLoading(false);
    }
  };

  const ModeToggle = ({ label, icon: Icon, modeKey, color }) => (
    <div className="flex items-center justify-between p-3 bg-gray-800 rounded mb-2 hover:bg-gray-750 transition-colors">
      <div className="flex items-center gap-2">
        <Icon className={`w-5 h-5 ${modes[modeKey] ? color : 'text-gray-500'}`} />
        <span className="text-sm font-medium text-gray-200">{label}</span>
      </div>
      <label className="relative inline-flex items-center cursor-pointer">
        <input 
            type="checkbox" 
            className="sr-only peer"
            checked={modes[modeKey] || false}
            onChange={(e) => toggleMode(modeKey, e.target.checked)}
            disabled={loading}
        />
        <div className="w-11 h-6 bg-gray-700 peer-focus:outline-none peer-focus:ring-2 peer-focus:ring-blue-800 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
      </label>
    </div>
  );

  return (
    <div className="bg-gray-900 p-4 rounded-lg border border-gray-700 w-full">
      <h3 className="text-lg font-bold text-gray-100 mb-4 flex items-center gap-2">
        <Settings className="w-5 h-5" /> Agentic Modes
      </h3>
      
      <ModeToggle label="Max Context (32K+)" icon={Zap} modeKey="maxMode" color="text-yellow-400" />
      <ModeToggle label="Deep Thinking (CoT)" icon={Brain} modeKey="deepThinking" color="text-purple-400" />
      <ModeToggle label="Deep Research" icon={Search} modeKey="deepResearch" color="text-blue-400" />
      <ModeToggle label="No Refusal" icon={Shield} modeKey="noRefusal" color="text-red-500" />
      <ModeToggle label="Auto-Correct" icon={Zap} modeKey="autoCorrect" color="text-green-400" />
    </div>
  );
};
)";
}

std::string ReactServerGenerator::GetEngineManagerContent(const ReactServerConfig& config) {
    return R"(import React, { useState } from 'react';
import axios from 'axios';
import { HardDrive, Cpu, Activity, Server } from 'lucide-react';

export const EngineManager = () => {
  const [engineStatus, setEngineStatus] = useState("Idle");
  const [drives, setDrives] = useState([
    { path: "C:\\models", size: "500GB", active: false },
    { path: "D:\\models", size: "1TB", active: false },
    { path: "E:\\models", size: "2TB", active: false },
    { path: "F:\\models", size: "1.5TB", active: false },
    { path: "G:\\models", size: "800GB", active: false },
  ]);

  const load800B = async () => {
    setEngineStatus("Initializing 5-Drive Array...");
    try {
      await axios.post('/api/command', { command: '!engine load800b' });
      // Simulate progressive loading
      setTimeout(() => setDrives(d => d.map(dr => ({...dr, active: true}))), 1000);
      setEngineStatus("800B Model Distributed & Loaded");
    } catch (e) {
      setEngineStatus("Failed to load 800B Model");
    }
  };

  const setupDrives = async () => {
      await axios.post('/api/command', { command: '!engine setup5drive' });
      setEngineStatus("Drive Array Configured");
  };

  return (
    <div className="bg-gray-900 p-4 rounded-lg border border-gray-700 mt-4">
      <h3 className="text-lg font-bold text-gray-100 mb-4 flex items-center gap-2">
        <Server className="w-5 h-5" /> Engine System (800B Spec)
      </h3>

      <div className="flex gap-2 mb-4">
        <button 
            onClick={load800B}
            className="flex-1 bg-blue-600 hover:bg-blue-700 text-white py-2 px-4 rounded flex items-center justify-center gap-2 font-semibold transition-colors"
        >
            <Brain className="w-4 h-4" /> Load 800B Model
        </button>
        <button 
            onClick={setupDrives}
            className="bg-gray-700 hover:bg-gray-600 text-white py-2 px-4 rounded flex items-center gap-2 transition-colors"
        >
            <HardDrive className="w-4 h-4" /> Mount Drives
        </button>
      </div>

      <div className="bg-black p-3 rounded font-mono text-sm text-green-400 mb-4 border border-gray-800">
        Status: {engineStatus}
      </div>

      <div className="space-y-2">
        {drives.map((drive, i) => (
            <div key={i} className={`flex items-center justify-between p-2 rounded border ${drive.active ? 'bg-green-900/20 border-green-800' : 'bg-gray-800/50 border-gray-800'}`}>
                <div className="flex items-center gap-2">
                    <HardDrive className={`w-4 h-4 ${drive.active ? 'text-green-500' : 'text-gray-500'}`} />
                    <span className="text-gray-300 font-mono">{drive.path}</span>
                </div>
                <div className="flex items-center gap-4">
                    <span className="text-xs text-gray-500">{drive.size}</span>
                    <div className={`w-2 h-2 rounded-full ${drive.active ? 'bg-green-500 animate-pulse' : 'bg-red-900'}`}></div>
                </div>
            </div>
        ))}
      </div>
    </div>
  );
};
)";
}

std::string ReactServerGenerator::GetMemoryViewerContent(const ReactServerConfig& config) {
    return R"(import React, { useEffect, useState } from 'react';
import axios from 'axios';

export const MemoryViewer = () => {
    const [stats, setStats] = useState({ used: 0, total: 4096, chunks: [] });

    useEffect(() => {
        const interval = setInterval(async () => {
            try {
                const res = await axios.get('/api/memory/status');
                if (res.data) setStats(res.data);
            } catch {}
        }, 2000);
        return () => clearInterval(interval);
    }, []);

    const percentage = Math.round((stats.used / stats.total) * 100);

    return (
        <div className="bg-gray-900 border-t border-gray-700 p-2">
            <div className="flex justify-between items-center mb-1 text-xs text-gray-400">
                <span>Context Usage</span>
                <span>{stats.used} / {stats.total} tokens ({percentage}%)</span>
            </div>
            <div className="w-full bg-gray-800 rounded-full h-2.5 dark:bg-gray-700 overflow-hidden">
                <div 
                    className={`h-2.5 rounded-full ${percentage > 90 ? 'bg-red-600' : 'bg-blue-600'}`} 
                    style={{ width: `${percentage}%` }}
                ></div>
            </div>
        </div>
    );
};
)";
}

std::string ReactServerGenerator::GetToolOutputPanelContent(const ReactServerConfig& config) {
    return R"(import React, { useEffect, useRef } from 'react';

export const ToolOutput = ({ logs = [] }) => {
    const bottomRef = useRef(null);

    useEffect(() => {
        bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
    }, [logs]);

    return (
        <div className="h-full bg-black font-mono text-sm p-2 overflow-y-auto custom-scrollbar">
            {logs.length === 0 && <div className="text-gray-600 italic">No output...</div>}
            {logs.map((log, i) => (
                <div key={i} className="mb-1 border-b border-gray-900 pb-1">
                    <span className="text-gray-500 text-xs">[{log.time}]</span>
                    <span className={`ml-2 ${log.type === 'error' ? 'text-red-400' : log.type === 'success' ? 'text-green-400' : 'text-gray-300'}`}>
                        {log.message}
                    </span>
                </div>
            ))}
            <div ref={bottomRef} />
        </div>
    );
};
)";
}

std::string ReactServerGenerator::GetHotpatchControlsContent(const ReactServerConfig& config) {
    return R"(import React, { useState } from 'react';
import axios from 'axios';
import { Flame } from 'lucide-react';

export const HotpatchControls = () => {
    const [file, setFile] = useState('');
    const [oldCode, setOldCode] = useState('');
    const [newCode, setNewCode] = useState('');
    const [status, setStatus] = useState('');

    const applyHotpatch = async () => {
        try {
            setStatus('Applying patch...');
            await axios.post('/api/command', { 
                command: `/hotpatch ${file} ${oldCode} ${newCode}` 
            });
            setStatus('Patch Applied Successfully');
            setTimeout(() => setStatus(''), 3000);
        } catch (e) {
            setStatus('Patch Failed');
        }
    };

    return (
        <div className="p-4 bg-gray-900 border border-red-900/30 rounded mt-4">
            <h3 className="text-red-400 font-bold flex items-center gap-2 mb-3">
                <Flame className="w-4 h-4" /> Live Hotpatch
            </h3>
            <input 
                className="w-full bg-gray-800 text-gray-200 p-1 mb-2 text-sm border border-gray-700 rounded" 
                placeholder="File path..."
                value={file} onChange={e => setFile(e.target.value)}
            />
            <div className="grid grid-cols-2 gap-2 mb-2">
                <textarea 
                    className="bg-gray-800 text-gray-200 p-1 text-xs font-mono border border-gray-700 rounded h-20"
                    placeholder="Old Code (Match)"
                    value={oldCode} onChange={e => setOldCode(e.target.value)}
                />
                <textarea 
                    className="bg-gray-800 text-gray-200 p-1 text-xs font-mono border border-gray-700 rounded h-20"
                    placeholder="New Code (Replace)"
                    value={newCode} onChange={e => setNewCode(e.target.value)}
                />
            </div>
            <div className="flex justify-between items-center">
                <button 
                    onClick={applyHotpatch}
                    className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded text-sm font-bold w-full"
                >
                    APPLY HOTPATCH
                </button>
            </div>
            {status && <div className="text-center text-xs mt-2 text-yellow-400">{status}</div>}
        </div>
    );
};
)";
}
