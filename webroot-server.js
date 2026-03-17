const express = require('express');
const path = require('path');
const fs = require('fs');
const cors = require('cors');

class RawrXDWebroot {
  constructor(options = {}) {
    this.port = options.port || 8080;
    this.host = options.host || 'localhost';
    this.app = express();
    this.server = null;

    // Setup middleware
    this.app.use(cors());
    this.app.use(express.json());
    this.app.use(express.static('./', {
      dotfiles: 'allow',
      index: false
    }));

    // Setup routes
    this.setupRoutes();
  }

  setupRoutes() {
    // Page discovery endpoint
    this.app.get('/api/pages', (req, res) => {
      const pages = this.discoverPages();
      res.json({
        total: pages.length,
        pages: pages.map(page => ({
          path: page,
          url: `http://${this.host}:${this.port}/${page.replace(/\\/g, '/')}`
        }))
      });
    });

    // Health check
    this.app.get('/api/health', (req, res) => {
      res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        webroot: process.cwd()
      });
    });

    // Directory listing
    this.app.get('/api/dir/*', (req, res) => {
      const reqPath = req.params[0] || '';
      const fullPath = path.resolve(reqPath);

      if (!fs.existsSync(fullPath)) {
        return res.status(404).json({ error: 'Directory not found' });
      }

      const stat = fs.statSync(fullPath);
      if (!stat.isDirectory()) {
        return res.status(400).json({ error: 'Not a directory' });
      }

      const items = fs.readdirSync(fullPath).map(item => {
        const itemPath = path.join(fullPath, item);
        const itemStat = fs.statSync(itemPath);
        return {
          name: item,
          path: path.relative('.', itemPath),
          type: itemStat.isDirectory() ? 'directory' : 'file',
          size: itemStat.size,
          modified: itemStat.mtime
        };
      });

      res.json({ path: reqPath, items });
    });

    // Catch-all for serving HTML files
    this.app.get('*', (req, res) => {
      let filePath = req.path.substring(1); // Remove leading slash

      // If no extension, try adding .html
      if (!path.extname(filePath)) {
        filePath += '.html';
      }

      const fullPath = path.resolve(filePath);

      if (fs.existsSync(fullPath) && fs.statSync(fullPath).isFile()) {
        // Inject testing helpers into HTML files
        if (path.extname(fullPath) === '.html') {
          let content = fs.readFileSync(fullPath, 'utf8');
          content = this.injectTestingHelpers(content);
          res.setHeader('Content-Type', 'text/html');
          res.send(content);
        } else {
          res.sendFile(fullPath);
        }
      } else {
        res.status(404).send(`
          <html>
            <head><title>404 - Page Not Found</title></head>
            <body style="font-family: Arial, sans-serif; text-align: center; padding: 50px;">
              <h1>404 - Page Not Found</h1>
              <p>File not found: ${filePath}</p>
              <a href="/api/pages">Available Pages</a>
            </body>
          </html>
        `);
      }
    });
  }

  // Inject testing helpers into HTML content
  injectTestingHelpers(content) {
    const helpers = `
    <!-- RawrXD Testing Helpers -->
    <script>
      // Test harness integration
      window.rawrxdTesting = {
        errors: [],
        warnings: [],
        features: {},
        
        logError: function(error) {
          this.errors.push(error);
          console.error('Test Error:', error);
        },
        
        logWarning: function(warning) {
          this.warnings.push(warning);
          console.warn('Test Warning:', warning);
        },
        
        reportFeature: function(name, status, details) {
          this.features[name] = { status, details, timestamp: Date.now() };
          console.log('Feature Report:', name, status);
        },
        
        getReport: function() {
          return {
            errors: this.errors,
            warnings: this.warnings,
            features: this.features,
            url: window.location.href,
            timestamp: Date.now()
          };
        }
      };
      
      // Override console methods to capture errors
      const originalError = console.error;
      console.error = function(...args) {
        window.rawrxdTesting.logError(args.join(' '));
        originalError.apply(console, args);
      };
      
      // Capture unhandled errors
      window.addEventListener('error', function(e) {
        window.rawrxdTesting.logError('Unhandled error: ' + e.message + ' at ' + e.filename + ':' + e.lineno);
      });
      
      // Report page load
      window.addEventListener('DOMContentLoaded', function() {
        window.rawrxdTesting.reportFeature('dom', 'loaded', { loadTime: performance.now() });
      });
    </script>
    `;

    // Insert before closing head tag, or at beginning if no head
    if (content.includes('</head>')) {
      content = content.replace('</head>', helpers + '</head>');
    } else {
      content = helpers + content;
    }

    return content;
  }

  // Discover all HTML pages
  discoverPages() {
    const pages = [];
    const searchDirs = [
      'gui',
      'reconstructed',
      'RawrZ-Security',
      'bigdaddyg-ide'
    ];

    const walkDir = (dir, fileList = []) => {
      if (!fs.existsSync(dir)) return fileList;

      const files = fs.readdirSync(dir);
      files.forEach(file => {
        const filePath = path.join(dir, file);
        const stat = fs.statSync(filePath);

        if (stat.isDirectory() && !file.startsWith('.') && !file.includes('node_modules')) {
          walkDir(filePath, fileList);
        } else if (file.endsWith('.html')) {
          fileList.push(filePath.replace(/\\/g, '/'));
        }
      });
      return fileList;
    };

    for (const dir of searchDirs) {
      if (fs.existsSync(dir)) {
        walkDir(dir, pages);
      }
    }

    return pages;
  }

  // Start the server
  async start() {
    return new Promise((resolve, reject) => {
      this.server = this.app.listen(this.port, this.host, (err) => {
        if (err) {
          reject(err);
        } else {
          console.log(`🌐 RawrXD Webroot Server running at http://${this.host}:${this.port}`);
          console.log(`📁 Serving from: ${process.cwd()}`);
          console.log(`🔍 Pages API: http://${this.host}:${this.port}/api/pages`);
          resolve();
        }
      });
    });
  }

  // Stop the server
  async stop() {
    return new Promise((resolve) => {
      if (this.server) {
        this.server.close(() => {
          console.log('🛑 Webroot server stopped');
          resolve();
        });
      } else {
        resolve();
      }
    });
  }

  // Get server info
  getInfo() {
    return {
      url: `http://${this.host}:${this.port}`,
      host: this.host,
      port: this.port,
      webroot: process.cwd(),
      running: !!this.server
    };
  }
}

// CLI support
if (require.main === module) {
  const args = process.argv.slice(2);
  const options = {};

  for (let i = 0; i < args.length; i += 2) {
    const flag = args[i];
    const value = args[i + 1];

    switch (flag) {
      case '--port':
        options.port = parseInt(value);
        break;
      case '--host':
        options.host = value;
        break;
    }
  }

  const server = new RawrXDWebroot(options);

  server.start().then(() => {
    console.log('✅ Webroot server started successfully!');

    // Graceful shutdown
    process.on('SIGINT', async () => {
      console.log('\n📝 Shutting down...');
      await server.stop();
      process.exit(0);
    });

  }).catch(error => {
    console.error('💥 Failed to start server:', error);
    process.exit(1);
  });
}

module.exports = RawrXDWebroot;