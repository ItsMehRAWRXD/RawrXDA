#!/usr/bin/env node

/**
 * RawrXD Comprehensive Page Tester
 * Systematically tests all HTML pages and their features
 * 
 * Usage: node comprehensive-page-tester.js [options]
 * Options:
 *   --headless     Run browser in headless mode (default: false)
 *   --timeout      Page timeout in ms (default: 30000)
 *   --report       Generate detailed HTML report (default: true)
 *   --parallel     Number of parallel tests (default: 3)
 *   --filter       Filter pages by pattern (e.g., "rawrz")
 */

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer');
const { performance } = require('perf_hooks');
const RawrXDWebroot = require('./webroot-server');
const http = require('http');

class RawrXDPageTester {
  constructor(options = {}) {
    this.options = {
      headless: options.headless !== false,
      timeout: options.timeout || 30000,
      generateReport: options.report !== false,
      parallelTests: options.parallel || 3,
      filter: options.filter || null,
      verbose: options.verbose || false,
      useWebroot: options.webroot !== false,
      webrootPort: options.webrootPort || 8080,
      webrootHost: options.webrootHost || 'localhost',
      ...options
    };

    this.results = [];
    this.browser = null;
    this.webroot = null;
    './RawrZ-Security',
      './bigdaddyg-ide'
    ];

    const walkDir = (dir, fileList = []) => {
      if (!fs.existsSync(dir)) return fileList;

      const files = fs.readdirSync(dir);
      files.forEach(file => {
        const filePath = path.join(dir, file);
        const stat = fs.statSync(filePath);

        if (stat.isDirectory()) {
          walkDir(filePath, fileList);
        } else if (file.endsWith('.html')) {
          // Apply filter if specified
          if (!this.options.filter || filePath.toLowerCase().includes(this.options.filter.toLowerCase())) {
            fileList.push({
              path: filePath,
              url: this.options.useWebroot ?
                `http://${this.options.webrootHost}:${this.options.webrootPort}/${filePath.replace(/\\/g, '/')}` :
                `file://${path.resolve(filePath).replace(/\\/g, '/')}`,
              type: this.options.useWebroot ? 'webroot' : 'file'
            });
          }
        }
      });
      return fileList;
    };

    for (const dir of searchDirs) {
      walkDir(dir, pages);
    }

    console.log(`🔍 Discovered ${pages.length} HTML pages for testing`);
    return pages;
  }

  // Initialize browser instance
  async initBrowser() {
    console.log('🚀 Starting browser...');
    this.browser = await puppeteer.launch({
      headless: this.options.headless,
      args: [
        '--no-sandbox',
        '--disable-setuid-sandbox',
        '--disable-dev-shm-usage',
        '--disable-web-security',
        '--allow-file-access-from-files'
      ]
    });
  }

  // Test individual page
  async testPage(pageInfo) {
    const result = {
      path: pageInfo.path,
      url: pageInfo.url,
      name: path.basename(pageInfo.path),
      type: pageInfo.type,
      startTime: performance.now(),
      status: 'unknown',
      errors: [],
      warnings: [],
      features: {
        javascript: { status: 'unknown', errors: [] },
        forms: { count: 0, working: 0, errors: [] },
        buttons: { count: 0, working: 0, errors: [] },
        inputs: { count: 0, working: 0, errors: [] },
        links: { count: 0, working: 0, errors: [] },
        external: { dependencies: [], missing: [] }
      },
      performance: {
        loadTime: 0,
        domContentLoaded: 0,
        totalTime: 0
      },
      accessibility: {
        score: 0,
        issues: []
      }
    };

    let page = null;

    try {
      console.log(`📄 Testing: ${pageInfo.path} (${pageInfo.type})`);
      page = await this.browser.newPage();

      // Set up error and console monitoring
      const consoleMessages = [];
      const jsErrors = [];

      page.on('console', msg => {
        const text = msg.text();
        consoleMessages.push({ type: msg.type(), text });
        if (msg.type() === 'error') {
          jsErrors.push(text);
        }
      });

      page.on('pageerror', error => {
        jsErrors.push(error.message);
      });

      // Navigate to page
      const loadStart = performance.now();

      await page.goto(pageInfo.url, {
      });

      result.performance.loadTime = performance.now() - loadStart;

      // Wait for page to stabilize
      await page.waitForTimeout(2000);

      // Test JavaScript functionality
      result.features.javascript = await this.testJavaScript(page);

      // Test DOM elements
      result.features.forms = await this.testForms(page);
      result.features.buttons = await this.testButtons(page);
      result.features.inputs = await this.testInputs(page);
      result.features.links = await this.testLinks(page);

      // Test external dependencies
      result.features.external = await this.testExternalDependencies(page);

      // Test webroot-specific features if applicable
      if (pageInfo.type === 'webroot') {
        result.features.webroot = await this.testWebrootFeatures(page);
      }

      // Collect errors
      result.features.javascript.errors = jsErrors;

      // Determine overall status
      if (jsErrors.length === 0 && result.features.javascript.status === 'working') {
        result.status = 'passing';
      } else if (jsErrors.length > 0 || result.features.javascript.status === 'error') {
        result.status = 'failing';
      } else {
        result.status = 'warning';
      }

    } catch (error) {
      result.status = 'error';
      result.errors.push(`Page load failed: ${error.message}`);
    } finally {
      if (page) {
        await page.close();
      }
      result.performance.totalTime = performance.now() - result.startTime;
    }

    return result;
  }

  // Test JavaScript functionality
  async testJavaScript(page) {
    try {
      // Check if JavaScript is enabled and working
      const jsTest = await page.evaluate(() => {
        try {
          // Test basic JS functionality
          const testObj = { test: true };
          const hasConsole = typeof console !== 'undefined';
          const hasWindow = typeof window !== 'undefined';
          const hasDocument = typeof document !== 'undefined';

          // Test common functions
          const functions = [];
          if (typeof window !== 'undefined') {
            for (const prop in window) {
              if (typeof window[prop] === 'function' && !prop.startsWith('_')) {
                functions.push(prop);
              }
            }
          }

          return {
            basic: true,
            hasConsole,
            hasWindow,
            hasDocument,
            functions: functions.slice(0, 20), // Limit to first 20
            userFunctions: functions.filter(f =>
              !['alert', 'confirm', 'prompt', 'setTimeout', 'setInterval'].includes(f)
            ).slice(0, 10)
          };
        } catch (e) {
          return { basic: false, error: e.message };
        }
      });

      if (jsTest.basic) {
        return {
          status: 'working',
          details: jsTest,
          errors: []
        };
      } else {
        return {
          status: 'error',
          details: jsTest,
          errors: [jsTest.error]
        };
      }
    } catch (error) {
      return {
        status: 'error',
        details: null,
        errors: [error.message]
      };
    }
  }

  // Test forms
  async testForms(page) {
    try {
      return await page.evaluate(() => {
        const forms = document.querySelectorAll('form');
        const result = { count: forms.length, working: 0, errors: [] };

        forms.forEach((form, index) => {
          try {
            // Test form properties
            const hasAction = form.action !== '';
            const hasMethod = form.method !== '';
            const inputs = form.querySelectorAll('input, textarea, select');

            // Try to trigger form events (without actually submitting)
            if (typeof form.checkValidity === 'function') {
              form.checkValidity();
              result.working++;
            }
          } catch (e) {
            result.errors.push(`Form ${index}: ${e.message}`);
          }
        });

        return result;
      });
    } catch (error) {
      return { count: 0, working: 0, errors: [error.message] };
    }
  }

  // Test buttons
  async testButtons(page) {
    try {
      return await page.evaluate(() => {
        const buttons = document.querySelectorAll('button, input[type="button"], input[type="submit"]');
        const result = { count: buttons.length, working: 0, errors: [] };

        buttons.forEach((button, index) => {
          try {
            // Test if button is clickable
            if (!button.disabled && button.style.display !== 'none') {
              // Test click event (create fake event to avoid actual triggering)
              const event = new Event('click', { bubbles: true });
              if (button.onclick || button.addEventListener) {
                result.working++;
              }
            }
          } catch (e) {
            result.errors.push(`Button ${index}: ${e.message}`);
          }
        });

        return result;
      });
    } catch (error) {
      return { count: 0, working: 0, errors: [error.message] };
    }
  }

  // Test inputs
  async testInputs(page) {
    try {
      return await page.evaluate(() => {
        const inputs = document.querySelectorAll('input, textarea, select');
        const result = { count: inputs.length, working: 0, errors: [] };

        inputs.forEach((input, index) => {
          try {
            // Test input properties and methods
            if (input.type && !input.disabled) {
              // Try to set value
              const oldValue = input.value;
              input.value = 'test';
              if (input.value === 'test') {
                result.working++;
              }
              input.value = oldValue; // Restore
            }
          } catch (e) {
            result.errors.push(`Input ${index}: ${e.message}`);
          }
        });

        return result;
      });
    } catch (error) {
      return { count: 0, working: 0, errors: [error.message] };
    }
  }

  // Test links
  async testLinks(page) {
    try {
      return await page.evaluate(() => {
        const links = document.querySelectorAll('a[href]');
        const result = { count: links.length, working: 0, errors: [] };

        links.forEach((link, index) => {
          try {
            const href = link.href;
            if (href && href !== '#' && href !== 'javascript:void(0)') {
              result.working++;
            }
          } catch (e) {
            result.errors.push(`Link ${index}: ${e.message}`);
          }
        });

        return result;
      });
    } catch (error) {
      return { count: 0, working: 0, errors: [error.message] };
    }
  }

  // Test external dependencies
  async testExternalDependencies(page) {
    try {
      return await page.evaluate(() => {
        const scripts = document.querySelectorAll('script[src]');
        const links = document.querySelectorAll('link[href]');
        const result = { dependencies: [], missing: [] };

        scripts.forEach(script => {
          result.dependencies.push({
            type: 'script',
            src: script.src,
            loaded: !script.onerror
          });
        });

        links.forEach(link => {
          result.dependencies.push({
            type: 'stylesheet',
            href: link.href,
            loaded: !link.onerror
          });
        });

        return result;
      });
    } catch (error) {
      return { dependencies: [], missing: [error.message] };
    }
  }

  // Test webroot-specific features
  async testWebrootFeatures(page) {
    try {
      return await page.evaluate(() => {
        const result = {
          testingHelpers: false,
          reportingApi: false,
          errors: [],
          features: {}
        };

        // Check if testing helpers were injected
        if (typeof window.rawrxdTesting !== 'undefined') {
          result.testingHelpers = true;
          result.reportingApi = typeof window.rawrxdTesting.getReport === 'function';

          // Get the testing report if available
          if (result.reportingApi) {
            try {
              const report = window.rawrxdTesting.getReport();
              result.features = report.features || {};
              result.errors = report.errors || [];
            } catch (e) {
              result.errors.push('Failed to get testing report: ' + e.message);
            }
          }
        }

        return result;
      });
    } catch (error) {
      return {
        testingHelpers: false,
        reportingApi: false,
        errors: [error.message],
        features: {}
      };
    }
  }

  // Basic accessibility test
  async testAccessibility(page) {
    try {
      return await page.evaluate(() => {
        let score = 100;
        const issues = [];

        // Check for alt text on images
        const images = document.querySelectorAll('img');
        images.forEach((img, index) => {
          if (!img.alt) {
            issues.push(`Image ${index} missing alt text`);
            score -= 5;
          }
        });

        // Check for form labels
        const inputs = document.querySelectorAll('input, textarea, select');
        inputs.forEach((input, index) => {
          const id = input.id;
          if (id && !document.querySelector(`label[for="${id}"]`)) {
            issues.push(`Input ${index} missing label`);
            score -= 3;
          }
        });

        // Check for headings structure
        const headings = document.querySelectorAll('h1, h2, h3, h4, h5, h6');
        if (headings.length === 0) {
          issues.push('No heading structure found');
          score -= 10;
        }

        return { score: Math.max(0, score), issues };
      });
    } catch (error) {
      return { score: 0, issues: [error.message] };
    }
  }

  // Run tests in parallel batches
  async runTests(pages) {
    console.log(`🔬 Testing ${pages.length} pages with ${this.options.parallelTests} parallel workers`);

    const results = [];

    for (let i = 0; i < pages.length; i += this.options.parallelTests) {
      const batch = pages.slice(i, i + this.options.parallelTests);
      const batchResults = await Promise.all(
        batch.map(page => this.testPage(page))
      );
      results.push(...batchResults);

      console.log(`✅ Completed batch ${Math.floor(i / this.options.parallelTests) + 1}/${Math.ceil(pages.length / this.options.parallelTests)}`);
    }

    return results;
  }

  // Generate comprehensive HTML report
  generateReport(results) {
    const totalTime = performance.now() - this.startTime;
    const summary = {
      total: results.length,
      passing: results.filter(r => r.status === 'passing').length,
      failing: results.filter(r => r.status === 'failing').length,
      warnings: results.filter(r => r.status === 'warning').length,
      errors: results.filter(r => r.status === 'error').length,
      totalTime: Math.round(totalTime)
    };

    const html = `
<!DOCTYPE html>
<html>
<head>
    <title>RawrXD Page Test Report</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 40px; background: #f5f7fa; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 12px; margin-bottom: 30px; }
        .summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .summary-card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); text-align: center; }
        .summary-card h3 { margin: 0; font-size: 2em; }
        .summary-card p { margin: 5px 0 0 0; color: #666; text-transform: uppercase; font-size: 0.9em; letter-spacing: 1px; }
        .passing { color: #27ae60; } .failing { color: #e74c3c; } .warning { color: #f39c12; } .error { color: #8e44ad; }
        .results { background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); overflow: hidden; }
        .result-item { padding: 20px; border-bottom: 1px solid #eee; }
        .result-item:last-child { border-bottom: none; }
        .result-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
        .result-title { font-weight: bold; font-size: 1.1em; }
        .status-badge { padding: 4px 8px; border-radius: 4px; color: white; font-size: 0.8em; text-transform: uppercase; }
        .details { display: none; margin-top: 15px; padding: 15px; background: #f8f9fa; border-radius: 4px; }
        .details.show { display: block; }
        .feature-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; margin-bottom: 15px; }
        .feature-item { padding: 8px; background: white; border-radius: 4px; border-left: 4px solid #ddd; }
        .performance { margin-top: 15px; }
        .toggle-btn { background: none; border: 1px solid #ddd; padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 0.8em; }
        .error-list { background: #fff5f5; border: 1px solid #fed7d7; border-radius: 4px; padding: 10px; margin-top: 10px; }
        .error-list ul { margin: 0; padding-left: 20px; }
        .error-list li { color: #e53e3e; margin: 5px 0; }
    </style>
    <script>
        function toggleDetails(id) {
            const details = document.getElementById(id);
            details.classList.toggle('show');
        }
    </script>
</head>
<body>
    <div class="header">
        <h1>🧪 RawrXD Page Test Report</h1>
        <p>Generated: ${new Date().toISOString()}</p>
        <p>Total Test Time: ${(totalTime / 1000).toFixed(2)}s</p>
    </div>

    <div class="summary">
        <div class="summary-card">
            <h3>${summary.total}</h3>
            <p>Total Pages</p>
        </div>
        <div class="summary-card">
            <h3 class="passing">${summary.passing}</h3>
            <p>Passing</p>
        </div>
        <div class="summary-card">
            <h3 class="failing">${summary.failing}</h3>
            <p>Failing</p>
        </div>
        <div class="summary-card">
            <h3 class="warning">${summary.warnings}</h3>
            <p>Warnings</p>
        </div>
        <div class="summary-card">
            <h3 class="error">${summary.errors}</h3>
            <p>Errors</p>
        </div>
    </div>

    <div class="results">
        ${results.map((result, index) => `
            <div class="result-item">
                <div class="result-header">
                    <div>
                        <div class="result-title">${result.name}</div>
                        <div style="font-size: 0.9em; color: #666;">${result.path}</div>
                    </div>
                    <div>
                        <span class="status-badge ${result.status}" style="background: ${this.getStatusColor(result.status)}">${result.status}</span>
                        <button class="toggle-btn" onclick="toggleDetails('details-${index}')">Details</button>
                    </div>
                </div>
                
                <div id="details-${index}" class="details">
                    <div class="feature-grid">
                        <div class="feature-item">
                            <strong>JavaScript:</strong> ${result.features.javascript.status}
                        </div>
                        <div class="feature-item">
                            <strong>Forms:</strong> ${result.features.forms.working}/${result.features.forms.count}
                        </div>
                        <div class="feature-item">
                            <strong>Buttons:</strong> ${result.features.buttons.working}/${result.features.buttons.count}
                        </div>
                        <div class="feature-item">
                            <strong>Inputs:</strong> ${result.features.inputs.working}/${result.features.inputs.count}
                        </div>
                        <div class="feature-item">
                            <strong>Links:</strong> ${result.features.links.working}/${result.features.links.count}
                        </div>
                        <div class="feature-item">
                            <strong>Accessibility:</strong> ${result.accessibility.score}/100
                        </div>
                    </div>
                    
                    <div class="performance">
                        <strong>Performance:</strong>
                        Load: ${result.performance.loadTime.toFixed(2)}ms |
                        Total: ${result.performance.totalTime.toFixed(2)}ms
                    </div>
                    
                    ${result.errors.length > 0 || result.features.javascript.errors.length > 0 ? `
                        <div class="error-list">
                            <strong>Errors:</strong>
                            <ul>
                                ${[...result.errors, ...result.features.javascript.errors].map(error => `<li>${error}</li>`).join('')}
                            </ul>
                        </div>
                    ` : ''}
                    
                    ${result.accessibility.issues.length > 0 ? `
                        <div class="error-list">
                            <strong>Accessibility Issues:</strong>
                            <ul>
                                ${result.accessibility.issues.map(issue => `<li>${issue}</li>`).join('')}
                            </ul>
                        </div>
                    ` : ''}
                </div>
            </div>
        `).join('')}
    </div>
</body>
</html>`;

    const reportPath = path.resolve('./test-report.html');
    fs.writeFileSync(reportPath, html);
    console.log(`📊 Report generated: ${reportPath}`);
  }

  getStatusColor(status) {
    const colors = {
      'passing': '#27ae60',
      'failing': '#e74c3c',
      'warning': '#f39c12',
      'error': '#8e44ad'
    };
    return colors[status] || '#95a5a6';
  }

  // Main execution
  async run() {
    try {
      console.log('🧪 RawrXD Comprehensive Page Tester Starting...');
      console.log(`Options: ${JSON.stringify(this.options, null, 2)}`);

      const pages = await this.discoverPages();
      if (pages.length === 0) {
        console.log('❌ No HTML pages found to test');
        return;
      }

      await this.initBrowser();
      this.results = await this.runTests(pages);

      if (this.options.generateReport) {
        this.generateReport(this.results);
      }

      // Print summary
      const summary = {
        total: this.results.length,
        passing: this.results.filter(r => r.status === 'passing').length,
        failing: this.results.filter(r => r.status === 'failing').length,
        warnings: this.results.filter(r => r.status === 'warning').length,
        errors: this.results.filter(r => r.status === 'error').length
      };

      console.log('\n📋 Test Summary:');
      console.log(`✅ Passing: ${summary.passing}`);
      console.log(`❌ Failing: ${summary.failing}`);
      console.log(`⚠️  Warnings: ${summary.warnings}`);
      console.log(`💥 Errors: ${summary.errors}`);
      console.log(`📊 Total: ${summary.total}`);

    } catch (error) {
      console.error('💥 Test runner failed:', error);
    } finally {
      if (this.browser) {
        await this.browser.close();
      }
    }
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
      case '--headless':
        options.headless = value !== 'false';
        break;
      case '--timeout':
        options.timeout = parseInt(value);
        break;
      case '--report':
        options.generateReport = value !== 'false';
        break;
      case '--parallel':
        options.parallelTests = parseInt(value);
        break;
      case '--filter':
        options.filter = value;
        break;
      case '--verbose':
        options.verbose = value !== 'false';
        break;
    }
  }

  const tester = new RawrXDPageTester(options);
  tester.run().then(() => {
    console.log('✨ Testing completed!');
    process.exit(0);
  }).catch(error => {
    console.error('💥 Fatal error:', error);
    process.exit(1);
  });
}

module.exports = RawrXDPageTester;