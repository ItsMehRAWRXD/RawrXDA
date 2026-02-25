/**
 * AI IDE Generator - Uses AI to generate IDE components based on project analysis
 * Integrates with cloud models for intelligent IDE component generation
 */

const { CloudModelManager } = require('../cloud-models/cloud-model-manager.js');

class AIDEGenerator {
  constructor(cloudManager = null) {
    this.cloudManager = cloudManager || new CloudModelManager();
    this.generatedComponents = [];
    this.templates = this.loadTemplates();
  }

  loadTemplates() {
    return {
      explorer: {
        name: 'File Explorer',
        description: 'Project file browser with tree view',
        features: ['file-tree', 'search', 'filter', 'context-menu']
      },
      api: {
        name: 'API Tester',
        description: 'REST API testing interface',
        features: ['endpoint-list', 'request-builder', 'response-viewer', 'history']
      },
      components: {
        name: 'Component Browser',
        description: 'Browse and manage UI components',
        features: ['component-list', 'preview', 'props-editor', 'usage-stats']
      },
      functions: {
        name: 'Function Browser',
        description: 'Browse and search functions',
        features: ['function-list', 'search', 'signature-view', 'usage-tracking']
      },
      classes: {
        name: 'Class Browser',
        description: 'Browse and manage classes',
        features: ['class-list', 'inheritance-tree', 'methods-view', 'properties']
      },
      config: {
        name: 'Configuration Manager',
        description: 'Manage configuration files',
        features: ['config-list', 'editor', 'validation', 'backup']
      },
      assets: {
        name: 'Asset Manager',
        description: 'Manage project assets',
        features: ['asset-grid', 'preview', 'optimization', 'metadata']
      },
      ai: {
        name: 'AI Assistant',
        description: 'AI-powered code assistance',
        features: ['chat', 'code-generation', 'refactoring', 'documentation']
      }
    };
  }

  async generateIDEPanel(panelType, projectContext) {
    console.log(`🤖 Generating ${panelType} panel...`);
    
    try {
      // Check if we have cloud models available
      if (!this.cloudManager.apiKeys || Object.keys(this.cloudManager.apiKeys).length === 0) {
        console.log('⚠️ No cloud models available, using template-based generation');
        return this.generateFromTemplate(panelType, projectContext);
      }

      const prompt = this.buildPrompt(panelType, projectContext);
      
      // Try to use cloud models for generation
      const response = await this.cloudManager.generateCode(prompt, 'javascript', 'auto');
      
      return this.extractCode(response.code);
      
    } catch (error) {
      console.warn(`⚠️ AI generation failed for ${panelType}, falling back to template:`, error.message);
      return this.generateFromTemplate(panelType, projectContext);
    }
  }

  buildPrompt(panelType, projectContext) {
    const template = this.templates[panelType] || this.templates.explorer;
    
    return `
Generate a ${template.name} panel for a web-based IDE with the following project context:

Project Structure:
${JSON.stringify(projectContext.projectStructure, null, 2)}

Components Found:
${JSON.stringify(projectContext.components, null, 2)}

Functions Found:
${JSON.stringify(projectContext.functions, null, 2)}

API Endpoints:
${JSON.stringify(projectContext.endpoints, null, 2)}

Classes Found:
${JSON.stringify(projectContext.classes, null, 2)}

Configuration Files:
${JSON.stringify(projectContext.configs, null, 2)}

Assets:
${JSON.stringify(projectContext.assets, null, 2)}

Create a functional ${template.name} panel with:
- Modern, dark theme styling (similar to VS Code)
- Responsive design that works in a sidebar
- Interactive elements for ${template.features.join(', ')}
- Real-time updates via WebSocket
- Integration hooks for the existing BigDaddyG system
- Proper error handling and loading states
- Accessibility features

Requirements:
1. Use vanilla JavaScript (no external frameworks)
2. Include CSS for dark theme styling
3. Add WebSocket integration for real-time updates
4. Include proper event handling
5. Add search and filter functionality where applicable
6. Include context menus for actions
7. Add keyboard shortcuts support
8. Include proper error boundaries

Return the code in this exact format:
\`\`\`html
<!-- HTML structure -->
\`\`\`

\`\`\`css
/* CSS styling */
\`\`\`

\`\`\`javascript
// JavaScript functionality
\`\`\`
`;
  }

  extractCode(content) {
    // Extract code from markdown code blocks
    const htmlMatch = content.match(/```html\n([\s\S]*?)\n```/);
    const cssMatch = content.match(/```css\n([\s\S]*?)\n```/);
    const jsMatch = content.match(/```javascript\n([\s\S]*?)\n```/);

    return {
      html: htmlMatch ? htmlMatch[1].trim() : '',
      css: cssMatch ? cssMatch[1].trim() : '',
      js: jsMatch ? jsMatch[1].trim() : ''
    };
  }

  generateFromTemplate(panelType, projectContext) {
    console.log(`📝 Generating ${panelType} panel from template...`);
    
    const template = this.templates[panelType] || this.templates.explorer;
    
    switch (panelType) {
      case 'explorer':
        return this.generateFileExplorer(projectContext);
      case 'api':
        return this.generateAPITester(projectContext);
      case 'components':
        return this.generateComponentBrowser(projectContext);
      case 'functions':
        return this.generateFunctionBrowser(projectContext);
      case 'classes':
        return this.generateClassBrowser(projectContext);
      case 'config':
        return this.generateConfigManager(projectContext);
      case 'assets':
        return this.generateAssetManager(projectContext);
      case 'ai':
        return this.generateAIAssistant(projectContext);
      default:
        return this.generateGenericPanel(panelType, projectContext);
    }
  }

  generateFileExplorer(projectContext) {
    return {
      html: `
<div class="ide-panel file-explorer">
  <div class="panel-header">
    <h3>📁 Project Files</h3>
    <div class="panel-actions">
      <button class="btn-icon" title="Refresh" onclick="refreshFileExplorer()">🔄</button>
      <button class="btn-icon" title="Search" onclick="toggleFileSearch()">🔍</button>
    </div>
  </div>
  
  <div class="panel-content">
    <div class="file-search" id="file-search" style="display: none;">
      <input type="text" placeholder="Search files..." onkeyup="searchFiles(this.value)">
    </div>
    
    <div class="file-tree" id="file-tree">
      ${this.generateFileTreeHTML(projectContext.projectStructure)}
    </div>
  </div>
</div>
`,
      css: `
.file-explorer {
  height: 100%;
  display: flex;
  flex-direction: column;
  background: #1e1e1e;
  color: #cccccc;
  font-family: 'Consolas', 'Monaco', monospace;
  font-size: 13px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: #2d2d30;
  border-bottom: 1px solid #3e3e42;
}

.panel-header h3 {
  margin: 0;
  font-size: 13px;
  font-weight: 600;
  color: #cccccc;
}

.panel-actions {
  display: flex;
  gap: 4px;
}

.btn-icon {
  background: none;
  border: none;
  color: #cccccc;
  cursor: pointer;
  padding: 4px;
  border-radius: 3px;
  font-size: 12px;
}

.btn-icon:hover {
  background: #3e3e42;
}

.panel-content {
  flex: 1;
  overflow: auto;
  padding: 8px;
}

.file-search {
  margin-bottom: 8px;
}

.file-search input {
  width: 100%;
  padding: 6px 8px;
  background: #3c3c3c;
  border: 1px solid #3e3e42;
  border-radius: 3px;
  color: #cccccc;
  font-size: 12px;
}

.file-tree {
  user-select: none;
}

.file-item, .folder-item {
  display: flex;
  align-items: center;
  padding: 2px 4px;
  cursor: pointer;
  border-radius: 3px;
  margin: 1px 0;
}

.file-item:hover, .folder-item:hover {
  background: #2a2d2e;
}

.file-item.selected, .folder-item.selected {
  background: #094771;
}

.file-icon, .folder-icon {
  margin-right: 6px;
  font-size: 14px;
}

.file-name, .folder-name {
  flex: 1;
  font-size: 12px;
}

.folder-children {
  margin-left: 16px;
  border-left: 1px solid #3e3e42;
  padding-left: 8px;
}

.folder-toggle {
  margin-right: 4px;
  cursor: pointer;
  font-size: 10px;
  width: 12px;
  text-align: center;
}

.folder-collapsed .folder-children {
  display: none;
}
`,
      js: `
let fileExplorerState = {
  selectedFile: null,
  expandedFolders: new Set(),
  searchQuery: ''
};

function refreshFileExplorer() {
  // Refresh file tree from server
  if (window.ws && window.ws.readyState === WebSocket.OPEN) {
    window.ws.send(JSON.stringify({
      command: 'refresh_file_explorer'
    }));
  }
}

function toggleFileSearch() {
  const search = document.getElementById('file-search');
  search.style.display = search.style.display === 'none' ? 'block' : 'none';
  if (search.style.display === 'block') {
    search.querySelector('input').focus();
  }
}

function searchFiles(query) {
  fileExplorerState.searchQuery = query.toLowerCase();
  filterFileTree();
}

function filterFileTree() {
  const tree = document.getElementById('file-tree');
  const items = tree.querySelectorAll('.file-item, .folder-item');
  
  items.forEach(item => {
    const name = item.querySelector('.file-name, .folder-name').textContent.toLowerCase();
    const matches = !fileExplorerState.searchQuery || name.includes(fileExplorerState.searchQuery);
    
    item.style.display = matches ? 'flex' : 'none';
  });
}

function toggleFolder(folderElement) {
  const isCollapsed = folderElement.classList.contains('folder-collapsed');
  
  if (isCollapsed) {
    folderElement.classList.remove('folder-collapsed');
  } else {
    folderElement.classList.add('folder-collapsed');
  }
}

function selectFile(fileElement) {
  // Remove previous selection
  document.querySelectorAll('.file-item.selected, .folder-item.selected').forEach(el => {
    el.classList.remove('selected');
  });
  
  // Add selection to current element
  fileElement.classList.add('selected');
  fileExplorerState.selectedFile = fileElement.dataset.path;
  
  // Emit file selection event
  if (window.ws && window.ws.readyState === WebSocket.OPEN) {
    window.ws.send(JSON.stringify({
      command: 'file_selected',
      path: fileElement.dataset.path
    }));
  }
}

// Initialize file explorer
document.addEventListener('DOMContentLoaded', function() {
  // Add event listeners for file tree interactions
  document.addEventListener('click', function(e) {
    if (e.target.closest('.folder-toggle')) {
      e.preventDefault();
      toggleFolder(e.target.closest('.folder-item'));
    } else if (e.target.closest('.file-item, .folder-item')) {
      e.preventDefault();
      selectFile(e.target.closest('.file-item, .folder-item'));
    }
  });
});
`
    };
  }

  generateFileTreeHTML(structure, level = 0) {
    let html = '';
    
    Object.entries(structure).forEach(([name, item]) => {
      if (item.type === 'directory') {
        html += `
          <div class="folder-item" data-path="${name}" onclick="selectFile(this)">
            <span class="folder-toggle" onclick="event.stopPropagation(); toggleFolder(this.parentElement)">▶</span>
            <span class="folder-icon">📁</span>
            <span class="folder-name">${name}</span>
          </div>
          <div class="folder-children">
            ${this.generateFileTreeHTML(item.children || {}, level + 1)}
          </div>
        `;
      } else {
        const icon = this.getFileIcon(name);
        html += `
          <div class="file-item" data-path="${name}" onclick="selectFile(this)">
            <span class="file-icon">${icon}</span>
            <span class="file-name">${name}</span>
          </div>
        `;
      }
    });
    
    return html;
  }

  getFileIcon(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const iconMap = {
      'js': '📄',
      'ts': '📘',
      'jsx': '⚛️',
      'tsx': '⚛️',
      'html': '🌐',
      'css': '🎨',
      'scss': '🎨',
      'json': '📋',
      'md': '📝',
      'py': '🐍',
      'java': '☕',
      'cpp': '⚙️',
      'c': '⚙️',
      'php': '🐘',
      'rb': '💎',
      'go': '🐹',
      'rs': '🦀',
      'sql': '🗄️',
      'xml': '📄',
      'yml': '⚙️',
      'yaml': '⚙️',
      'png': '🖼️',
      'jpg': '🖼️',
      'jpeg': '🖼️',
      'gif': '🖼️',
      'svg': '🖼️',
      'ico': '🖼️'
    };
    
    return iconMap[ext] || '📄';
  }

  generateAPITester(projectContext) {
    return {
      html: `
<div class="ide-panel api-tester">
  <div class="panel-header">
    <h3>🌐 API Tester</h3>
    <div class="panel-actions">
      <button class="btn-icon" title="Add Request" onclick="addAPIRequest()">➕</button>
      <button class="btn-icon" title="Import" onclick="importAPIRequests()">📥</button>
    </div>
  </div>
  
  <div class="panel-content">
    <div class="api-endpoints">
      <h4>Available Endpoints</h4>
      <div class="endpoint-list" id="endpoint-list">
        ${this.generateEndpointListHTML(projectContext.endpoints)}
      </div>
    </div>
    
    <div class="api-request-builder">
      <h4>Request Builder</h4>
      <div class="request-form">
        <div class="form-group">
          <label>Method:</label>
          <select id="request-method">
            <option value="GET">GET</option>
            <option value="POST">POST</option>
            <option value="PUT">PUT</option>
            <option value="DELETE">DELETE</option>
            <option value="PATCH">PATCH</option>
          </select>
        </div>
        
        <div class="form-group">
          <label>URL:</label>
          <input type="text" id="request-url" placeholder="http://localhost:3000/api/endpoint">
        </div>
        
        <div class="form-group">
          <label>Headers:</label>
          <textarea id="request-headers" placeholder='{"Content-Type": "application/json"}'></textarea>
        </div>
        
        <div class="form-group">
          <label>Body:</label>
          <textarea id="request-body" placeholder='{"key": "value"}'></textarea>
        </div>
        
        <button class="btn-primary" onclick="sendAPIRequest()">Send Request</button>
      </div>
    </div>
    
    <div class="api-response">
      <h4>Response</h4>
      <div class="response-content" id="response-content">
        <div class="response-placeholder">No response yet</div>
      </div>
    </div>
  </div>
</div>
`,
      css: `
.api-tester {
  height: 100%;
  display: flex;
  flex-direction: column;
  background: #1e1e1e;
  color: #cccccc;
  font-family: 'Consolas', 'Monaco', monospace;
  font-size: 13px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: #2d2d30;
  border-bottom: 1px solid #3e3e42;
}

.panel-header h3 {
  margin: 0;
  font-size: 13px;
  font-weight: 600;
  color: #cccccc;
}

.panel-actions {
  display: flex;
  gap: 4px;
}

.btn-icon {
  background: none;
  border: none;
  color: #cccccc;
  cursor: pointer;
  padding: 4px;
  border-radius: 3px;
  font-size: 12px;
}

.btn-icon:hover {
  background: #3e3e42;
}

.panel-content {
  flex: 1;
  overflow: auto;
  padding: 12px;
}

.api-endpoints, .api-request-builder, .api-response {
  margin-bottom: 20px;
}

.api-endpoints h4, .api-request-builder h4, .api-response h4 {
  margin: 0 0 8px 0;
  font-size: 12px;
  font-weight: 600;
  color: #cccccc;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.endpoint-list {
  max-height: 150px;
  overflow-y: auto;
  border: 1px solid #3e3e42;
  border-radius: 3px;
}

.endpoint-item {
  display: flex;
  align-items: center;
  padding: 6px 8px;
  border-bottom: 1px solid #3e3e42;
  cursor: pointer;
}

.endpoint-item:hover {
  background: #2a2d2e;
}

.endpoint-item:last-child {
  border-bottom: none;
}

.endpoint-method {
  padding: 2px 6px;
  border-radius: 3px;
  font-size: 10px;
  font-weight: 600;
  margin-right: 8px;
  min-width: 40px;
  text-align: center;
}

.method-get { background: #61afef; color: #1e1e1e; }
.method-post { background: #98c379; color: #1e1e1e; }
.method-put { background: #e06c75; color: #1e1e1e; }
.method-delete { background: #e06c75; color: #1e1e1e; }
.method-patch { background: #d19a66; color: #1e1e1e; }

.endpoint-path {
  flex: 1;
  font-size: 12px;
  font-family: 'Consolas', 'Monaco', monospace;
}

.endpoint-file {
  font-size: 10px;
  color: #888;
  margin-left: 8px;
}

.form-group {
  margin-bottom: 12px;
}

.form-group label {
  display: block;
  margin-bottom: 4px;
  font-size: 11px;
  font-weight: 600;
  color: #cccccc;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.form-group select,
.form-group input,
.form-group textarea {
  width: 100%;
  padding: 6px 8px;
  background: #3c3c3c;
  border: 1px solid #3e3e42;
  border-radius: 3px;
  color: #cccccc;
  font-size: 12px;
  font-family: 'Consolas', 'Monaco', monospace;
}

.form-group textarea {
  height: 60px;
  resize: vertical;
}

.btn-primary {
  background: #007acc;
  border: none;
  color: white;
  padding: 8px 16px;
  border-radius: 3px;
  cursor: pointer;
  font-size: 12px;
  font-weight: 600;
}

.btn-primary:hover {
  background: #005a9e;
}

.response-content {
  background: #2d2d30;
  border: 1px solid #3e3e42;
  border-radius: 3px;
  padding: 12px;
  min-height: 100px;
  font-family: 'Consolas', 'Monaco', monospace;
  font-size: 12px;
  white-space: pre-wrap;
  overflow: auto;
}

.response-placeholder {
  color: #888;
  font-style: italic;
}

.response-success {
  color: #98c379;
}

.response-error {
  color: #e06c75;
}
`,
      js: `
let apiTesterState = {
  requests: [],
  currentRequest: null
};

function addAPIRequest() {
  const newRequest = {
    id: Date.now(),
    method: 'GET',
    url: '',
    headers: '{}',
    body: ''
  };
  
  apiTesterState.requests.push(newRequest);
  apiTesterState.currentRequest = newRequest;
  
  // Update form with new request
  document.getElementById('request-method').value = newRequest.method;
  document.getElementById('request-url').value = newRequest.url;
  document.getElementById('request-headers').value = newRequest.headers;
  document.getElementById('request-body').value = newRequest.body;
}

function importAPIRequests() {
  // Import requests from project endpoints
  const endpoints = ${JSON.stringify(projectContext.endpoints)};
  
  endpoints.forEach(endpoint => {
    const request = {
      id: Date.now() + Math.random(),
      method: endpoint.method,
      url: \`http://localhost:3000\${endpoint.path}\`,
      headers: '{"Content-Type": "application/json"}',
      body: ''
    };
    
    apiTesterState.requests.push(request);
  });
  
  // Refresh endpoint list
  refreshEndpointList();
}

function refreshEndpointList() {
  const endpointList = document.getElementById('endpoint-list');
  endpointList.innerHTML = \`\${this.generateEndpointListHTML(projectContext.endpoints)}\`;
}

function selectEndpoint(endpoint) {
  // Fill form with endpoint data
  document.getElementById('request-method').value = endpoint.method;
  document.getElementById('request-url').value = \`http://localhost:3000\${endpoint.path}\`;
  document.getElementById('request-headers').value = '{"Content-Type": "application/json"}';
  document.getElementById('request-body').value = '';
}

async function sendAPIRequest() {
  const method = document.getElementById('request-method').value;
  const url = document.getElementById('request-url').value;
  const headers = document.getElementById('request-headers').value;
  const body = document.getElementById('request-body').value;
  
  const responseContent = document.getElementById('response-content');
  responseContent.innerHTML = '<div class="response-placeholder">Sending request...</div>';
  
  try {
    const requestOptions = {
      method: method,
      headers: JSON.parse(headers || '{}')
    };
    
    if (body && method !== 'GET') {
      requestOptions.body = body;
    }
    
    const response = await fetch(url, requestOptions);
    const responseText = await response.text();
    
    let responseHtml = \`<div class="response-success">\${method} \${url} - \${response.status} \${response.statusText}</div>\\n\\n\`;
    
    try {
      const jsonResponse = JSON.parse(responseText);
      responseHtml += JSON.stringify(jsonResponse, null, 2);
    } catch {
      responseHtml += responseText;
    }
    
    responseContent.innerHTML = responseHtml;
    
  } catch (error) {
    responseContent.innerHTML = \`<div class="response-error">Error: \${error.message}</div>\`;
  }
}

// Initialize API tester
document.addEventListener('DOMContentLoaded', function() {
  // Add event listeners for endpoint selection
  document.addEventListener('click', function(e) {
    if (e.target.closest('.endpoint-item')) {
      const endpointElement = e.target.closest('.endpoint-item');
      const method = endpointElement.dataset.method;
      const path = endpointElement.dataset.path;
      
      selectEndpoint({ method, path });
    }
  });
  
  // Initialize with first endpoint if available
  const endpoints = ${JSON.stringify(projectContext.endpoints)};
  if (endpoints.length > 0) {
    selectEndpoint(endpoints[0]);
  }
});
`
    };
  }

  generateEndpointListHTML(endpoints) {
    if (!endpoints || endpoints.length === 0) {
      return '<div class="endpoint-item"><span class="endpoint-path">No endpoints found</span></div>';
    }
    
    return endpoints.map(endpoint => `
      <div class="endpoint-item" data-method="${endpoint.method}" data-path="${endpoint.path}" onclick="selectEndpoint(this)">
        <span class="endpoint-method method-${endpoint.method.toLowerCase()}">${endpoint.method}</span>
        <span class="endpoint-path">${endpoint.path}</span>
        <span class="endpoint-file">${endpoint.file}</span>
      </div>
    `).join('');
  }

  generateComponentBrowser(projectContext) {
    // Similar structure but for components
    return {
      html: `
<div class="ide-panel component-browser">
  <div class="panel-header">
    <h3>🧩 Components</h3>
    <div class="panel-actions">
      <button class="btn-icon" title="Search" onclick="toggleComponentSearch()">🔍</button>
      <button class="btn-icon" title="Filter" onclick="toggleComponentFilter()">🔧</button>
    </div>
  </div>
  
  <div class="panel-content">
    <div class="component-search" id="component-search" style="display: none;">
      <input type="text" placeholder="Search components..." onkeyup="searchComponents(this.value)">
    </div>
    
    <div class="component-list" id="component-list">
      ${this.generateComponentListHTML(projectContext.components)}
    </div>
  </div>
</div>
`,
      css: `
.component-browser {
  height: 100%;
  display: flex;
  flex-direction: column;
  background: #1e1e1e;
  color: #cccccc;
  font-family: 'Consolas', 'Monaco', monospace;
  font-size: 13px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: #2d2d30;
  border-bottom: 1px solid #3e3e42;
}

.panel-header h3 {
  margin: 0;
  font-size: 13px;
  font-weight: 600;
  color: #cccccc;
}

.panel-actions {
  display: flex;
  gap: 4px;
}

.btn-icon {
  background: none;
  border: none;
  color: #cccccc;
  cursor: pointer;
  padding: 4px;
  border-radius: 3px;
  font-size: 12px;
}

.btn-icon:hover {
  background: #3e3e42;
}

.panel-content {
  flex: 1;
  overflow: auto;
  padding: 12px;
}

.component-search {
  margin-bottom: 12px;
}

.component-search input {
  width: 100%;
  padding: 6px 8px;
  background: #3c3c3c;
  border: 1px solid #3e3e42;
  border-radius: 3px;
  color: #cccccc;
  font-size: 12px;
}

.component-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.component-item {
  background: #2d2d30;
  border: 1px solid #3e3e42;
  border-radius: 3px;
  padding: 12px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.component-item:hover {
  background: #3e3e42;
  border-color: #007acc;
}

.component-item.selected {
  background: #094771;
  border-color: #007acc;
}

.component-header {
  display: flex;
  align-items: center;
  margin-bottom: 8px;
}

.component-icon {
  margin-right: 8px;
  font-size: 16px;
}

.component-name {
  font-weight: 600;
  font-size: 14px;
  color: #cccccc;
}

.component-type {
  margin-left: auto;
  padding: 2px 6px;
  background: #3e3e42;
  border-radius: 3px;
  font-size: 10px;
  text-transform: uppercase;
  color: #888;
}

.component-details {
  font-size: 11px;
  color: #888;
}

.component-file {
  margin-bottom: 4px;
}

.component-category {
  text-transform: uppercase;
  letter-spacing: 0.5px;
}
`,
      js: `
let componentBrowserState = {
  selectedComponent: null,
  searchQuery: '',
  filterType: 'all'
};

function toggleComponentSearch() {
  const search = document.getElementById('component-search');
  search.style.display = search.style.display === 'none' ? 'block' : 'none';
  if (search.style.display === 'block') {
    search.querySelector('input').focus();
  }
}

function toggleComponentFilter() {
  // Toggle filter options
  console.log('Toggle component filter');
}

function searchComponents(query) {
  componentBrowserState.searchQuery = query.toLowerCase();
  filterComponents();
}

function filterComponents() {
  const list = document.getElementById('component-list');
  const items = list.querySelectorAll('.component-item');
  
  items.forEach(item => {
    const name = item.querySelector('.component-name').textContent.toLowerCase();
    const type = item.querySelector('.component-type').textContent.toLowerCase();
    
    const matchesSearch = !componentBrowserState.searchQuery || 
                         name.includes(componentBrowserState.searchQuery);
    
    const matchesFilter = componentBrowserState.filterType === 'all' || 
                         type.includes(componentBrowserState.filterType);
    
    item.style.display = (matchesSearch && matchesFilter) ? 'block' : 'none';
  });
}

function selectComponent(componentElement) {
  // Remove previous selection
  document.querySelectorAll('.component-item.selected').forEach(el => {
    el.classList.remove('selected');
  });
  
  // Add selection to current element
  componentElement.classList.add('selected');
  componentBrowserState.selectedComponent = componentElement.dataset.component;
  
  // Emit component selection event
  if (window.ws && window.ws.readyState === WebSocket.OPEN) {
    window.ws.send(JSON.stringify({
      command: 'component_selected',
      component: componentElement.dataset.component
    }));
  }
}

// Initialize component browser
document.addEventListener('DOMContentLoaded', function() {
  // Add event listeners for component selection
  document.addEventListener('click', function(e) {
    if (e.target.closest('.component-item')) {
      e.preventDefault();
      selectComponent(e.target.closest('.component-item'));
    }
  });
});
`
    };
  }

  generateComponentListHTML(components) {
    if (!components || components.length === 0) {
      return '<div class="component-item"><div class="component-name">No components found</div></div>';
    }
    
    return components.map(component => `
      <div class="component-item" data-component="${component.name}" onclick="selectComponent(this)">
        <div class="component-header">
          <span class="component-icon">${this.getComponentIcon(component.type)}</span>
          <span class="component-name">${component.name}</span>
          <span class="component-type">${component.type}</span>
        </div>
        <div class="component-details">
          <div class="component-file">📄 ${component.file}</div>
          <div class="component-category">${component.category}</div>
        </div>
      </div>
    `).join('');
  }

  getComponentIcon(type) {
    const iconMap = {
      'react_component': '⚛️',
      'vue_component': '💚',
      'angular_component': '🅰️',
      'css_class': '🎨',
      'css_id': '🎨',
      'documentation': '📝',
      'sql_script': '🗄️'
    };
    
    return iconMap[type] || '🧩';
  }

  // Additional panel generators would follow similar patterns...
  generateFunctionBrowser(projectContext) {
    // Similar to component browser but for functions
    return this.generateGenericPanel('functions', projectContext);
  }

  generateClassBrowser(projectContext) {
    // Similar to component browser but for classes
    return this.generateGenericPanel('classes', projectContext);
  }

  generateConfigManager(projectContext) {
    // Similar to component browser but for configs
    return this.generateGenericPanel('config', projectContext);
  }

  generateAssetManager(projectContext) {
    // Similar to component browser but for assets
    return this.generateGenericPanel('assets', projectContext);
  }

  generateAIAssistant(projectContext) {
    // AI chat interface
    return this.generateGenericPanel('ai', projectContext);
  }

  generateGenericPanel(panelType, projectContext) {
    return {
      html: `
<div class="ide-panel ${panelType}-panel">
  <div class="panel-header">
    <h3>${this.templates[panelType]?.name || panelType}</h3>
  </div>
  <div class="panel-content">
    <div class="panel-placeholder">
      <p>${this.templates[panelType]?.description || 'Panel content will be generated here'}</p>
    </div>
  </div>
</div>
`,
      css: `
.${panelType}-panel {
  height: 100%;
  display: flex;
  flex-direction: column;
  background: #1e1e1e;
  color: #cccccc;
  font-family: 'Consolas', 'Monaco', monospace;
  font-size: 13px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: #2d2d30;
  border-bottom: 1px solid #3e3e42;
}

.panel-header h3 {
  margin: 0;
  font-size: 13px;
  font-weight: 600;
  color: #cccccc;
}

.panel-content {
  flex: 1;
  overflow: auto;
  padding: 12px;
}

.panel-placeholder {
  text-align: center;
  color: #888;
  font-style: italic;
  margin-top: 50px;
}
`,
      js: `
// Generic panel JavaScript
console.log('${panelType} panel loaded');
`
    };
  }
}

module.exports = AIDEGenerator;
