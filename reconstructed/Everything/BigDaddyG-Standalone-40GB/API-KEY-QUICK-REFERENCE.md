# API Key Management - Quick Reference

## 🔐 Access the API Key Manager

```javascript
// Open the settings panel
window.apiKeySettingsPanel?.open();

// Close the settings panel
window.apiKeySettingsPanel?.close();

// Toggle visibility
window.apiKeySettingsPanel?.toggle();
```

## 💾 Store and Retrieve API Keys

```javascript
// Store a key
window.apiKeyManager.setKey('openai', 'sk-...');

// Store a key with additional config
window.apiKeyManager.setKey('aws', 'AKIA...', {
  region: 'us-east-1',
  accessKeyId: 'AKIA...',
  secretAccessKey: 'wJalr...'
});

// Get a key
const key = window.apiKeyManager.getKey('openai');

// Check if configured
if (window.apiKeyManager.isConfigured('openai')) {
  // Use OpenAI
}

// Get all configured providers
const providers = window.apiKeyManager.getConfiguredProviders();
// Returns: ['openai', 'anthropic']

// Remove a key
window.apiKeyManager.removeKey('openai');
```

## 🧪 Test API Connections

```javascript
// Test a connection
const result = await window.apiKeySettingsPanel.testConnection(
  'openai',
  'sk-...',
  {} // additional config
);

if (result.success) {
  console.log('✅', result.message);
} else {
  console.error('❌', result.message);
}
```

## 📋 Supported Providers

| Provider | Key Format | Required Config | Test Endpoint |
|----------|-----------|-----------------|---------------|
| OpenAI | `sk-*` | None | `/v1/models` |
| Claude | `sk-ant-*` | None | `/v1/models` |
| AWS Bedrock | AWS Access Key | region, accessKeyId, secretAccessKey | (local) |
| GitHub Copilot | `gho_*` | None | `/user` |
| Cursor Pro | Bearer token | None | `/v1/user/profile` |
| Moonshot (Kimi) | `sk-*` | None | `/v1/models` |
| Google Gemini | API Key | keyType (optional) | `/v1/models` |
| Cohere | `co_*` | None | `/v1/models` |
| Together AI | Bearer token | None | `/models/list` |
| HuggingFace | `hf_*` | None | (hub.huggingface.co) |
| Replicate | Token | None | `/v1/models` |
| Ollama | (not needed) | None | `localhost:11434/api/tags` |

## 🔗 Integration with Models

```javascript
// When user selects a model with provider
const provider = 'openai';
const model = 'gpt-4-turbo';

// Get the API key
const apiKey = window.apiKeyManager.getKey(provider);

// Use it in your API call
if (apiKey) {
  const response = await fetch('https://api.openai.com/v1/chat/completions', {
    headers: {
      'Authorization': `Bearer ${apiKey}`,
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      model: model,
      messages: [/* ... */]
    })
  });
}
```

## 🚨 Error Handling

```javascript
try {
  const key = window.apiKeyManager.getKey('openai');
  if (!key) {
    console.log('No OpenAI key stored');
    window.apiKeySettingsPanel?.open(); // Open settings
    return;
  }

  // Use key...
} catch (error) {
  console.error('API key error:', error.message);
}
```

## ⚙️ Check Server Status

```javascript
// Health check
fetch('http://localhost:3000/health')
  .then(r => r.json())
  .then(d => {
    console.log('Server status:', d.status);
    console.log('Active connections:', d.active_connections);
  });

// Beaconism status
fetch('http://localhost:3000/beacon-status')
  .then(r => r.json())
  .then(d => console.log('Beaconism enabled:', d.beaconism_enabled));
```

## 🎨 UI Elements

The 🔐 Keys button is located in the right sidebar header, orange colored:

```html
<!-- In your component, the button is automatically added to index.html -->
<!-- Click it to open the API key management modal -->
```

## 🔐 Security Notes

✅ **Safe:** Keys stored locally in localStorage  
✅ **Safe:** Keys encrypted (base64 obfuscation for now)  
✅ **Safe:** Keys never transmitted to BigDaddyG servers  
⚠️ **Note:** For production, upgrade to TweetNaCl.js encryption  
⚠️ **Note:** Master password protection recommended for shared devices  

---

**Files Involved:**
- `api-key-manager.js` - Storage & retrieval
- `api-key-settings-panel.js` - UI modal
- `Micro-Model-Server.js` - CORS proxy & test endpoint
- `index.html` - 🔐 Keys button
- `preload.js` - IPC bridge
- `main.js` - Server lifecycle
