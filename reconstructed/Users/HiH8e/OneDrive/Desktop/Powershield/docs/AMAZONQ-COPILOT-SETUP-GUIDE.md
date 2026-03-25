# Amazon Q & GitHub Copilot Setup Guide

This guide helps you fix Amazon Q connection issues and set up GitHub Copilot in your workspace.

## 🔧 Fixing Amazon Q Connection Error

### Problem
If you're seeing the error: `connect ECONNREFUSED 127.0.0.1:443`, this is caused by a proxy setting in your Cursor configuration.

### Solution

1. **Run the fix script:**
   ```powershell
   .\Fix-AmazonQ-Connection.ps1 -FixProxy
   ```

2. **Or manually fix in Cursor settings:**
   - Open Cursor Settings (Ctrl+,)
   - Search for `aiSuite.simple.provider`
   - Remove or change the value from `"proxy"` to something else (or delete the setting)
   - Save and restart Cursor

3. **Verify the fix:**
   ```powershell
   .\Fix-AmazonQ-Connection.ps1 -ShowStatus
   ```

### What the script does:
- Removes the problematic `"aiSuite.simple.provider": "proxy"` setting
- Ensures Amazon Q settings are properly configured
- Updates your Cursor user settings file

## 🚀 GitHub Copilot Setup

### Automatic Setup

1. **Run the setup script:**
   ```powershell
   .\Setup-GitHubCopilot.ps1
   ```

2. **Install the extension:**
   - VS Code/Cursor will prompt you to install GitHub Copilot
   - Or manually install: `GitHub.copilot` from the extensions marketplace

3. **Sign in:**
   - When prompted, sign in with your GitHub account
   - Activate your GitHub Copilot subscription if needed

### Manual Setup

1. **Add to workspace extensions:**
   - The extension is already added to `.vscode/extensions.json`
   - VS Code/Cursor will prompt to install it

2. **Configure settings:**
   - Settings are already configured in `.vscode/settings.json`
   - Key settings:
     - `github.copilot.enable`: Enabled for all languages
     - `github.copilot.editor.enableAutoCompletions`: Auto-completions enabled
     - `github.copilot.chat.enabled`: Chat feature enabled

## 📋 Workspace Configuration

### Extensions Added
- ✅ `GitHub.copilot` - Official GitHub Copilot extension
- ✅ `amazonwebservices.amazon-q-vscode` - Amazon Q Developer extension

### Settings Configured

**GitHub Copilot:**
```json
{
  "github.copilot.enable": {
    "*": true
  },
  "github.copilot.editor.enableAutoCompletions": true,
  "github.copilot.chat.enabled": true,
  "github.copilot.editor.enableCodeActions": true
}
```

**Amazon Q:**
```json
{
  "amazonQ.telemetry": true,
  "amazonQ.workspaceIndex": true,
  "amazonQ.workspaceIndexMaxSize": 1000000,
  "amazonQ.workspaceIndexMaxFileSize": 200
}
```

## 🔍 Troubleshooting

### Amazon Q Still Not Working?

1. **Check Cursor settings:**
   ```powershell
   .\Fix-AmazonQ-Connection.ps1 -ShowStatus
   ```

2. **Verify extension is installed:**
   - Open Extensions view (Ctrl+Shift+X)
   - Search for "Amazon Q"
   - Ensure it's installed and enabled

3. **Check authentication:**
   - Amazon Q requires AWS authentication
   - Use AWS Builder ID or IAM Identity Center
   - Command Palette (Ctrl+Shift+P) → "Amazon Q: Sign In"

### GitHub Copilot Not Working?

1. **Verify extension installation:**
   - Extensions view → Search "GitHub Copilot"
   - Ensure it's installed and enabled

2. **Check authentication:**
   - Command Palette → "GitHub Copilot: Sign In"
   - Sign in with your GitHub account

3. **Verify subscription:**
   - GitHub Copilot requires an active subscription
   - Check at: https://github.com/settings/copilot

## 📝 Next Steps

1. ✅ Run `.\Fix-AmazonQ-Connection.ps1 -FixProxy` to fix Amazon Q
2. ✅ Restart Cursor completely
3. ✅ Sign in to Amazon Q (if needed)
4. ✅ Install GitHub Copilot extension (if prompted)
5. ✅ Sign in to GitHub Copilot
6. ✅ Start coding with AI assistance!

## 🆘 Need Help?

- **Amazon Q Issues:** Check AWS Toolkit documentation
- **GitHub Copilot Issues:** Check GitHub Copilot documentation
- **Script Issues:** Check the script output for error messages

