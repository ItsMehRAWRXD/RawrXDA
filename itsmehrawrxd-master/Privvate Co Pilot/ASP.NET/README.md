# Private AI Co Pilot - Browser Extension

A powerful browser extension that integrates Google Gemini AI for code explanation and assistance.

## Features

-  **AI Chat Interface**: Direct chat with Google Gemini AI
-  **Context Menu Integration**: Right-click on selected text to get AI explanations
-  **Beautiful UI**: Modern, responsive design with gradient backgrounds
-  **Secure**: API keys stored locally in browser storage
-  **Fast**: Optimized for quick responses and smooth interactions
-  **Smart Detection**: Automatically detects code-like content

## Installation

1. **Get a Google Gemini API Key**:
   - Go to [Google AI Studio](https://makersuite.google.com/app/apikey)
   - Create a new API key
   - Copy the key for later use

2. **Load the Extension**:
   - Open Chrome and go to `chrome://extensions/`
   - Enable "Developer mode" (toggle in top right)
   - Click "Load unpacked" and select this folder
   - The extension should appear in your extensions list

3. **Configure the Extension**:
   - Click the extension icon in your browser toolbar
   - Enter your Gemini API key in the settings section
   - Click "Save API Key"

## Usage

### Chat Interface
- Click the extension icon to open the chat interface
- Type your questions or requests
- Get AI responses directly in the popup

### Context Menu
- Select any text on any webpage
- Right-click and choose " Explain with AI"
- Get instant explanations in a beautiful modal

### Smart Features
- The extension automatically detects code-like content
- Shows helpful indicators when text is selected
- Provides contextual suggestions and explanations

## File Structure

```
 manifest.json          # Extension configuration
 popup.html            # Main chat interface
 popup.js              # Popup functionality
 background.js         # Background service worker
 content.js            # Content script for page interaction
 icons/                # Extension icons (create these)
    icon16.png        # 16x16 icon
    icon48.png        # 48x48 icon
    icon128.png       # 128x128 icon
 README.md             # This file
```

## Creating Icons

You need to create three icon files in the `icons/` directory:

### icon16.png (16x16 pixels)
- Small icon for the browser toolbar
- Simple, recognizable design
- High contrast for visibility

### icon48.png (48x48 pixels)
- Medium icon for extension management
- More detailed than 16x16
- Clear branding

### icon128.png (128x128 pixels)
- Large icon for Chrome Web Store
- Full detail and branding
- Professional appearance

### Icon Design Suggestions
- Use a robot or AI-themed design
- Include gradient colors (matching the UI: #667eea to #764ba2)
- Ensure icons are clear at all sizes
- Use PNG format with transparency

## API Configuration

The extension uses Google Gemini API with the following settings:
- **Model**: gemini-pro
- **Temperature**: 0.7 (balanced creativity)
- **Max Tokens**: 2048
- **Safety Settings**: Medium and above blocking

## Development

### Testing
1. Make changes to the code
2. Go to `chrome://extensions/`
3. Click the refresh icon on your extension
4. Test the functionality

### Debugging
- Use Chrome DevTools for popup debugging
- Check the background script in the Extensions page
- Use `console.log()` for debugging

## Security

- API keys are stored locally in browser storage
- No data is sent to external servers except Google Gemini
- All communications use HTTPS
- Content scripts run in isolated contexts

## Troubleshooting

### Extension Not Working
1. Check if the API key is correctly saved
2. Verify the extension has the necessary permissions
3. Check the browser console for errors

### API Errors
1. Verify your Gemini API key is valid
2. Check your API quota and billing
3. Ensure you have internet connectivity

### Context Menu Not Appearing
1. Refresh the webpage
2. Reload the extension
3. Check if the extension has proper permissions

## License

This extension is part of the RawrZ Security Platform suite.

## Support

For issues or questions, please refer to the main RawrZ documentation or contact support.
