# WebView2Container - Monaco Editor Integration

A complete C++ wrapper around Microsoft WebView2 for embedding Monaco Editor in Windows applications.

## Overview

This implementation provides a production-ready interface for embedding the Monaco Editor (VS Code's editor) in C++ Windows applications using Microsoft's WebView2 control. It replaces the previous stub implementation with full WebView2 functionality.

## Features

### 🎯 Core Functionality
- **WebView2 Environment Creation**: Automatic WebView2 runtime initialization
- **Monaco Editor Integration**: Pre-configured Monaco Editor with CDN loading
- **Asynchronous Communication**: Full bidirectional JavaScript ↔ C++ communication
- **Error Handling**: Comprehensive error reporting and callback mechanisms

### 🎨 Editor Features
- **Theme Support**: Light/dark theme switching
- **Language Support**: Syntax highlighting for multiple programming languages
- **Content Management**: Set/get editor content with real-time updates
- **Cursor Tracking**: Live cursor position monitoring
- **Read-only Mode**: Toggle between editable/read-only states
- **Text Insertion**: Programmatic text insertion at cursor position
- **Line Navigation**: Jump to specific lines with reveal functionality

### ⚙️ Configuration
- **Font Settings**: Customizable font size
- **Tab Configuration**: Adjustable tab size
- **Word Wrap**: Enable/disable word wrapping
- **Minimap**: Toggle code minimap visibility
- **Auto-layout**: Responsive sizing and layout management

## File Structure

```
src/core/
├── WebView2Container_stubs.cpp  # Full implementation (replaces stubs)
├── WebView2Container.h          # Header file with complete API
├── CMakeLists.txt               # CMake build configuration
├── example_usage.cpp            # Complete usage example
└── README.md                    # This documentation
```

## Implementation Details

### Architecture

The implementation uses the following design patterns:

1. **Global State Management**: Single `WebView2State` structure manages the entire WebView2 lifecycle
2. **COM Integration**: Proper COM initialization and cleanup
3. **Async Event Handling**: WebView2 events are handled asynchronously with C++ callbacks
4. **JSON Communication**: Simple JSON-based message passing between JavaScript and C++
5. **Error Propagation**: Comprehensive error handling with status codes and descriptive messages

### Key Components

#### WebView2State Structure
```cpp
struct WebView2State {
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    HWND parentHwnd;
    bool isInitialized;
    // ... callback management
};
```

#### Monaco Editor Integration
The implementation includes a complete Monaco Editor HTML template with:
- CDN-based Monaco loading from jsdelivr
- Pre-configured JavaScript API functions
- Event handling for content changes, cursor movement
- Theme and language switching capabilities

## API Reference

### Core Functions

#### Initialization
```cpp
void WebView2Container_Constructor(void);
WebView2Result WebView2Container_Initialize(void* hwnd, const char* initialURL);
WebView2Result WebView2Container_Destroy(void);
void WebView2Container_Destructor(void);
```

#### Window Management
```cpp
void WebView2Container_Resize(int x, int y, int width, int height);
void WebView2Container_Show(void);
void WebView2Container_Hide(void);
```

#### Content Operations
```cpp
WebView2Result WebView2Container_SetContent(const char* content, const char* baseURL);
WebView2Result WebView2Container_GetContent(void);
WebView2Result WebView2Container_ExecuteScript(const char* script);
WebView2Result WebView2Container_InsertText(const char* text);
```

#### Editor Configuration
```cpp
WebView2Result WebView2Container_SetTheme(const char* themeName);
WebView2Result WebView2Container_SetLanguage(const char* language);
WebView2Result WebView2Container_SetOptions(const MonacoEditorOptions* options);
WebView2Result WebView2Container_SetReadOnly(bool readOnly);
```

#### Callbacks
```cpp
typedef void (*WebView2ReadyCallback)(void* userData);
typedef void (*WebView2ContentCallback)(const char* content, unsigned int length, void* userData);
typedef void (*WebView2CursorCallback)(int line, int column, void* userData);
typedef void (*WebView2ErrorCallback)(const char* error, void* userData);

void WebView2Container_SetReadyCallback(WebView2ReadyCallback callback, void* userData);
void WebView2Container_SetContentCallback(WebView2ContentCallback callback, void* userData);
void WebView2Container_SetCursorCallback(WebView2CursorCallback callback, void* userData);
void WebView2Container_SetErrorCallback(WebView2ErrorCallback callback, void* userData);
```

### Return Values

All functions that return `WebView2Result` use this structure:
```cpp
struct WebView2Result {
    int status;         // 0 = success, negative = error
    char message[512];  // Status or error message
};
```

## Build Requirements

### Dependencies
1. **WebView2 SDK**: Microsoft.Web.WebView2 NuGet package or standalone SDK
2. **Windows SDK**: Windows 10 SDK version 10.0.17763.0 or later
3. **C++ Standard**: C++17 or later
4. **Runtime**: Microsoft Edge WebView2 Runtime

### Required Libraries
- `WebView2Loader.dll.lib` (or `WebView2LoaderStatic.lib` for static linking)
- `ole32.lib`
- `oleaut32.lib`
- `user32.lib`
- `version.lib`

### CMake Build
```bash
# Configure
cmake -B build -DWEBVIEW2_BUILD_EXAMPLE=ON

# Build
cmake --build build

# Install (optional)
cmake --build build --target install
```

### Visual Studio Build
1. Create new C++ Windows Desktop Application project
2. Install WebView2 NuGet package: `Microsoft.Web.WebView2`
3. Add library dependencies to Additional Dependencies
4. Include the source files in your project

## Usage Example

```cpp
#include "WebView2Container.h"

// Initialize
WebView2Container_Constructor();
WebView2Container_SetReadyCallback(OnEditorReady, nullptr);
WebView2Result result = WebView2Container_Initialize(hwnd, nullptr);

// Set content
WebView2Container_SetContent("console.log('Hello World!');", nullptr);
WebView2Container_SetLanguage("javascript");
WebView2Container_SetTheme("dark");

// Configure editor
MonacoEditorOptions options = {16, 4, true, true};
WebView2Container_SetOptions(&options);

// Execute custom script
WebView2Container_ExecuteScript("editor.focus();");

// Cleanup
WebView2Container_Destroy();
WebView2Container_Destructor();
```

## Error Handling

The implementation provides comprehensive error handling:

1. **Status Codes**: All operations return status codes (0 = success)
2. **Error Messages**: Descriptive error messages in `WebView2Result.message`
3. **Error Callbacks**: Asynchronous error reporting via callbacks
4. **Validation**: Input parameter validation with appropriate error responses

## Performance Considerations

- **Async Operations**: Most operations are asynchronous and non-blocking
- **Memory Management**: Proper COM object lifetime management
- **Event Handling**: Efficient event routing with minimal overhead
- **CDN Loading**: Monaco Editor loaded from CDN for optimal performance

## Troubleshooting

### Common Issues

1. **WebView2 Runtime Missing**
   - Install Microsoft Edge WebView2 Runtime
   - Download from: https://developer.microsoft.com/microsoft-edge/webview2/

2. **Initialization Failures**
   - Check that hwnd is valid
   - Ensure COM is properly initialized
   - Verify WebView2Loader.dll is accessible

3. **Content Not Loading**
   - Check internet connectivity (for CDN)
   - Verify JavaScript console for errors
   - Ensure proper HTML escaping in content

### Debug Output
Enable verbose error reporting by setting up error callbacks:
```cpp
WebView2Container_SetErrorCallback([](const char* error, void* userData) {
    OutputDebugStringA(error);
}, nullptr);
```

## License

This implementation is designed for integration into larger projects. Ensure compliance with:
- Microsoft WebView2 license terms
- Monaco Editor MIT license
- Your project's specific license requirements

## Contributing

When modifying the implementation:
1. Maintain the extern "C" interface for C compatibility
2. Follow the existing error handling patterns
3. Update documentation for API changes
4. Test with multiple WebView2 runtime versions
5. Ensure proper COM object lifetime management

## Version History

- **v1.0.0**: Initial complete implementation
  - Full WebView2 API integration
  - Monaco Editor embedding
  - Comprehensive callback system
  - Production-ready error handling