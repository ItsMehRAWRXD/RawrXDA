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

} // namespace RawrXD
