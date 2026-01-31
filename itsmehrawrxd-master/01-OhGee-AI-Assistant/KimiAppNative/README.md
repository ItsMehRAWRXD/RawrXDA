# OhGee - AI Assistant Hub

A native Windows desktop application that provides quick access to multiple AI assistants through a modern, unified interface. Like Ollama but without Ollama - direct cloud AI integration!

## Features

- **Multi-AI Support**: Access Kimi AI, Cursor, and ChatGPT from a single application
- **Global Hotkeys**: 
  - `Ctrl+Shift+G` - Open Kimi AI
  - `Ctrl+Shift+C` - Open Cursor
  - `Ctrl+Shift+H` - Open ChatGPT
  - `Ctrl+Shift+A` - Open Native Chat Assistant
- **System Tray Integration**: Runs in the background with system tray icon
- **Modern UI**: Clean, modern interface with tabbed navigation
- **Native Performance**: Built with C# and WPF for optimal Windows performance

## Requirements

- Windows 10/11
- .NET 8.0 Runtime
- WebView2 Runtime (usually pre-installed on Windows 10/11)

## Installation

1. Download the latest release from the releases page
2. Extract the files to your desired location
3. Run `AIAssistantHub.exe`

## Building from Source

1. Install .NET 8.0 SDK
2. Clone this repository
3. Navigate to the project directory
4. Run the following commands:

```bash
dotnet restore
dotnet build --configuration Release
dotnet publish --configuration Release --runtime win-x64 --self-contained true
```

## Usage

### First Launch
- The application will start minimized to the system tray
- Right-click the tray icon to access the context menu
- Use the global hotkeys to quickly open any AI assistant

### Global Hotkeys
- **Ctrl+Shift+G**: Opens Kimi AI
- **Ctrl+Shift+C**: Opens Cursor  
- **Ctrl+Shift+H**: Opens ChatGPT

### System Tray
- **Left-click**: Toggle window visibility
- **Double-click**: Open Kimi AI
- **Right-click**: Access context menu with all options

### Window Controls
- **Minimize**: Hides window to system tray
- **Close**: Hides window (application continues running in tray)
- **Drag**: Click and drag the title bar to move the window

## Configuration

The application uses the following URLs by default:
- Kimi AI: https://kimi.moonshot.cn/
- Cursor: https://www.cursor.com/
- ChatGPT: https://chat.openai.com/

## Troubleshooting

### WebView2 Issues
If you encounter WebView2-related errors:
1. Ensure WebView2 Runtime is installed
2. Download from: https://developer.microsoft.com/en-us/microsoft-edge/webview2/

### Hotkey Conflicts
If hotkeys don't work:
1. Check if other applications are using the same hotkeys
2. Run the application as administrator if needed
3. Restart the application

### Performance Issues
- Close unused browser tabs in the WebView
- Restart the application periodically
- Ensure sufficient system resources

## Development

### Project Structure
```
OhGee/
 App.xaml                 # Application definition
 App.xaml.cs             # Application logic and hotkey handling
 MainWindow.xaml         # Main window UI
 MainWindow.xaml.cs      # Main window logic
 ChatWindow.xaml         # Native chat interface
 ChatWindow.xaml.cs      # Chat interface logic
 SystemTrayManager.cs    # System tray functionality
 KimiAppNative.csproj    # Project file
 README.md              # This file
```

### Key Components
- **App.xaml.cs**: Handles global hotkeys and application lifecycle
- **MainWindow.xaml**: Modern UI with tabbed interface
- **SystemTrayManager.cs**: System tray icon and context menu
- **WebView2**: Embedded browser for AI assistant interfaces

## License

This project is open source. Please check the license file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For support, please open an issue on the GitHub repository.
