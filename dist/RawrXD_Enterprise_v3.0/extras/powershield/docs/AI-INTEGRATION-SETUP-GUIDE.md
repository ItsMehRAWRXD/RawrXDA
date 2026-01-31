# 🤖 AI Integration Setup Guide

**RawrXD IDE - Amazon Q, GitHub Copilot, and Open VSX Registry Integration**

---

## 📋 Overview

This guide covers setting up and using the AI integration modules in RawrXD IDE:
- **Amazon Q Developer** - AWS-powered AI assistant
- **GitHub Copilot** - AI pair programmer
- **Open VSX Registry** - Open-source extension marketplace
- **Unified AI Router** - Orchestrates all AI services
- **Backend Server** - Secure API proxy (bypasses CORS)

---

## 🚀 Quick Start

### 1. Load AI Modules

```powershell
# Import all AI modules
Import-Module .\Modules\RawrXD-AmazonQ.psm1
Import-Module .\Modules\RawrXD.GitHubCopilot.psm1
Import-Module .\Modules\RawrXD-AIAgentRouter.psm1
```

### 2. Initialize AI Router

```powershell
# Initialize with all services
Initialize-AIAgentRouter -LoadAmazonQ -LoadGitHubCopilot -LoadOllama -DefaultService "Ollama"
```

### 3. Start Backend Server (Optional)

```powershell
# Start API proxy server (bypasses CORS)
.\AI-Backend-Server.ps1 -Port 8888
```

---

## 🔐 Amazon Q Developer Setup

### Prerequisites
- AWS CLI installed: https://aws.amazon.com/cli/
- AWS account with Amazon Q Developer access
- AWS credentials configured

### Setup Methods

#### Method 1: AWS Profile (Recommended)
```powershell
# Configure AWS CLI
aws configure --profile default

# Initialize Amazon Q
Initialize-AmazonQ -Profile "default" -Region "us-east-1"
```

#### Method 2: Direct Credentials
```powershell
Initialize-AmazonQ -AccessKey "AKIA..." -SecretKey "..." -Region "us-east-1"
```

### Usage

```powershell
# Chat with Amazon Q
Invoke-AmazonQChat -Message "Explain this PowerShell code" -Context @{code = $code}

# Get code suggestions
Get-AmazonQCodeSuggestions -Code $code -Language "powershell"

# Analyze code
Invoke-AmazonQCodeAnalysis -Code $code -Language "powershell"
```

---

## 🐙 GitHub Copilot Setup

### Prerequisites
- GitHub account with Copilot subscription
- GitHub Personal Access Token with `copilot` scope

### Setup Methods

#### Method 1: OAuth Flow (Recommended)
```powershell
# Start OAuth browser flow
Connect-GitHubCopilot -UseOAuth
```

#### Method 2: Manual Token
```powershell
# Enter token manually
Connect-GitHubCopilot -GitHubToken "ghp_xxxxxxxxxxxx"

# Or interactive prompt
Connect-GitHubCopilot
```

### Getting a GitHub Token

1. Go to: https://github.com/settings/tokens
2. Click "Generate new token (classic)"
3. Select scopes:
   - `read:user`
   - `user:email`
   - `copilot`
4. Copy the token

### Usage

```powershell
# Chat with Copilot
Invoke-CopilotChat -Prompt "Write a PowerShell function to parse JSON"

# Get code suggestions
Get-CopilotCodeSuggestions -FileContent $code -Language "powershell" -CursorPosition @{Line=10;Column=5}

# Code review
Invoke-CopilotCodeReview -FileContent $code -Language "powershell"
```

---

## 📦 Open VSX Registry Integration

### Overview
Open VSX Registry is automatically integrated into the marketplace. No setup required!

### Features
- Full API support for extension search
- Extension metadata and statistics
- Download links for VSIX packages
- No authentication required

### Usage

The marketplace automatically fetches from Open VSX Registry when enabled in configuration:

```powershell
# Initialize marketplace (includes Open VSX)
Initialize-RawrXDMarketplace -MarketplacePath ".\marketplace"
```

### Configuration

Edit `marketplace\marketplace-config.json`:

```json
{
  "realMarketplaceSources": {
    "openVSX": {
      "enabled": true,
      "apiUrl": "https://open-vsx.org/api",
      "maxResults": 100,
      "rateLimitMs": 200
    }
  }
}
```

---

## 🎯 Unified AI Agent Router

### Overview
The AI Router automatically selects the best AI service for each request, with intelligent fallback.

### Initialization

```powershell
# Load all services
Initialize-AIAgentRouter `
    -LoadAmazonQ `
    -LoadGitHubCopilot `
    -LoadOllama `
    -DefaultService "Ollama"
```

### Usage

```powershell
# Chat (auto-routes to best available service)
$response = Invoke-AIChat -Message "Help me write PowerShell code"

# Get code suggestions (tries all services)
$suggestions = Get-AICodeSuggestions -Code $code -Language "powershell"

# Check router status
Get-AIRouterStatus
```

### Service Priority

1. **Amazon Q** (Priority 1) - Best for AWS-related code
2. **GitHub Copilot** (Priority 2) - Best for general coding
3. **Ollama** (Priority 3) - Local fallback

### Fallback Chain

If primary service fails, router automatically tries:
1. GitHub Copilot
2. Amazon Q
3. Ollama

---

## 🔧 Backend Server (CORS Bypass)

### Purpose
The backend server acts as a proxy to bypass browser CORS restrictions for API calls.

### Start Server

```powershell
# Basic (no auth)
.\AI-Backend-Server.ps1 -Port 8888

# With API key authentication
.\AI-Backend-Server.ps1 -Port 8888 -EnableAuth -ApiKey "your-secret-key"
```

### API Endpoints

#### Amazon Q Proxy
```
POST http://localhost:8888/api/amazonq/chat
Headers: X-API-Key: your-key (if enabled)
Body: {
  "message": "Your question",
  "conversationId": "...",
  "context": {}
}
```

#### GitHub Copilot Proxy
```
POST http://localhost:8888/api/copilot/chat
Headers: 
  X-API-Key: your-key (if enabled)
  Authorization: Bearer ghp_xxxxx
Body: {
  "messages": [{"role": "user", "content": "Your prompt"}]
}
```

#### Open VSX Registry Proxy
```
GET http://localhost:8888/api/openvsx/-/search?query=powershell
Headers: X-API-Key: your-key (if enabled)
```

#### Server Status
```
GET http://localhost:8888/api/status
```

### Frontend Usage

```javascript
// Example: Call backend server from JavaScript
fetch('http://localhost:8888/api/copilot/chat', {
    method: 'POST',
    headers: {
        'Content-Type': 'application/json',
        'X-API-Key': 'your-key'
    },
    body: JSON.stringify({
        token: 'ghp_xxxxx',
        messages: [{role: 'user', content: 'Hello'}]
    })
})
.then(r => r.json())
.then(data => console.log(data));
```

---

## 🌐 Browser Login Enhancements

### PS51-Browser-Host.ps1

Already enhanced with:
- ✅ Quick login buttons (Gmail, GitHub, ChatGPT, Kimi, DeepSeek, Cursor)
- ✅ Cookie and authentication settings enabled
- ✅ JavaScript and popups enabled
- ✅ OAuth-friendly configuration

### WebView2 Browser

Enhanced with:
- ✅ Login-friendly settings (cookies, autofill, JavaScript)
- ✅ Quick login buttons injected via JavaScript
- ✅ Third-party cookie support for OAuth

### Quick Login Buttons

Available services:
- 📧 **Gmail** - https://mail.google.com
- 🐙 **GitHub** - https://github.com/login
- 🤖 **ChatGPT** - https://chat.openai.com
- ✨ **Kimi** - https://kimi.moonshot.cn
- 🧠 **DeepSeek** - https://chat.deepseek.com
- 📝 **Cursor** - https://cursor.sh

---

## 📝 Configuration Files

### Amazon Q Config
Location: `$env:APPDATA\RawrXD\amazonq-config.json`

### GitHub Copilot Config
Location: `$env:APPDATA\RawrXD\copilot-config.json`

### Marketplace Config
Location: `.\marketplace\marketplace-config.json`

Example:
```json
{
  "realMarketplaceSources": {
    "vscode": {
      "enabled": true,
      "apiUrl": "https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery",
      "maxResults": 50
    },
    "openVSX": {
      "enabled": true,
      "apiUrl": "https://open-vsx.org/api",
      "maxResults": 100,
      "rateLimitMs": 200
    },
    "powershellGallery": {
      "enabled": true
    },
    "github": {
      "enabled": true
    }
  }
}
```

---

## 🔒 Security Notes

### API Keys
- **Amazon Q**: Uses AWS credentials (stored securely via AWS CLI)
- **GitHub Copilot**: Personal Access Token (stored in config file - consider encryption)
- **Backend Server**: API key for authentication (optional)

### Recommendations
1. **Encrypt tokens** in config files (use DPAPI or SecureString)
2. **Use API key** for backend server in production
3. **Rotate tokens** regularly
4. **Don't commit** config files with tokens to git

---

## 🧪 Testing

### Test Amazon Q
```powershell
Initialize-AmazonQ -Profile "default"
$result = Invoke-AmazonQChat -Message "Hello"
```

### Test GitHub Copilot
```powershell
Connect-GitHubCopilot
$status = Get-CopilotStatus
$result = Invoke-CopilotChat -Prompt "Hello"
```

### Test AI Router
```powershell
Initialize-AIAgentRouter -LoadAmazonQ -LoadGitHubCopilot -LoadOllama
$status = Get-AIRouterStatus
$result = Invoke-AIChat -Message "Hello"
```

### Test Backend Server
```powershell
# Start server
.\AI-Backend-Server.ps1 -Port 8888

# In another terminal
Invoke-RestMethod -Uri "http://localhost:8888/api/status"
```

---

## 🐛 Troubleshooting

### Amazon Q Issues

**Problem**: "AWS CLI not found"
- **Solution**: Install AWS CLI from https://aws.amazon.com/cli/

**Problem**: "Authentication failed"
- **Solution**: Run `aws configure` and verify credentials

### GitHub Copilot Issues

**Problem**: "Subscription not active"
- **Solution**: Ensure you have an active GitHub Copilot subscription

**Problem**: "Rate limit exceeded"
- **Solution**: Wait for rate limit reset or upgrade plan

### Backend Server Issues

**Problem**: "Port already in use"
- **Solution**: Use different port: `.\AI-Backend-Server.ps1 -Port 8889`

**Problem**: "CORS errors"
- **Solution**: Ensure `-EnableCORS` flag is set (default: enabled)

### Browser Login Issues

**Problem**: "Cookies not working"
- **Solution**: Check browser settings in PS51-Browser-Host.ps1 or WebView2 settings

**Problem**: "OAuth flow fails"
- **Solution**: Ensure third-party cookies are enabled

---

## 📚 API Reference

### Amazon Q Functions
- `Initialize-AmazonQ` - Initialize Amazon Q integration
- `Connect-AmazonQWithProfile` - Connect using AWS profile
- `Connect-AmazonQWithCredentials` - Connect using access keys
- `Invoke-AmazonQChat` - Chat with Amazon Q
- `Get-AmazonQCodeSuggestions` - Get code suggestions
- `Invoke-AmazonQCodeAnalysis` - Analyze code

### GitHub Copilot Functions
- `Initialize-GitHubCopilot` - Initialize Copilot integration
- `Connect-GitHubCopilot` - Connect with OAuth or token
- `Test-GitHubCopilotAuth` - Test authentication
- `Invoke-CopilotChat` - Chat with Copilot
- `Get-CopilotCodeSuggestions` - Get code suggestions
- `Invoke-CopilotCodeReview` - Code review
- `Get-CopilotStatus` - Get status

### AI Router Functions
- `Initialize-AIAgentRouter` - Initialize router
- `Invoke-AIChat` - Route chat request
- `Get-AICodeSuggestions` - Get suggestions from all services
- `Get-AIRouterStatus` - Get router status

### Marketplace Functions
- `Initialize-RawrXDMarketplace` - Initialize marketplace
- `Get-OpenVSXRegistryExtensions` - Fetch from Open VSX
- `Update-MarketplaceData` - Refresh marketplace data

---

## 🎯 Best Practices

1. **Start with Ollama** - It's free and local, good for testing
2. **Add GitHub Copilot** - Best for general coding assistance
3. **Add Amazon Q** - If you work with AWS
4. **Use Backend Server** - For web-based frontends to avoid CORS
5. **Cache credentials** - Save tokens securely for convenience
6. **Monitor rate limits** - Check `Get-CopilotStatus` and router status

---

## 📞 Support

For issues or questions:
1. Check module logs: `Write-DevConsole` output
2. Check router status: `Get-AIRouterStatus`
3. Check service status: `Get-CopilotStatus`
4. Review error messages in console

---

**Last Updated**: November 28, 2025  
**Version**: 1.0.0

