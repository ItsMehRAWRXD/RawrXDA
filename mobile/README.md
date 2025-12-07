# Mobile Copilot Integration (Native)

Real GitHub Copilot-style streaming for Android and iOS mobile IDEs.

## Components

### Android (`android/CopilotBridge.java`)
- **JavascriptInterface**: Exposes `requestCompletion(prompt, model, upstream)` to WebView JavaScript.
- **HTTP Streaming**: Connects to `http://localhost:3200/stream` (or custom proxy URL) and reads SSE line-by-line.
- **Callbacks**: Invokes `window.Copilot.onToken(token)`, `window.Copilot.onComplete()`, `window.Copilot.onError(error)` in the WebView for real-time UI updates.

### iOS (`ios/CopilotBridge.swift`)
- **WKScriptMessageHandler**: Handles messages from JavaScript via `window.webkit.messageHandlers.copilot.postMessage({...})`.
- **URLSession Streaming**: Fetches streaming responses from the proxy and parses NDJSON/SSE.
- **Callbacks**: Evaluates JavaScript `window.Copilot.onToken(...)` etc. to update the UI.

## Integration Steps

### Android
1. Add `CopilotBridge.java` to your Android project.
2. In your Activity, set up the WebView:
   ```java
   WebView webView = findViewById(R.id.webView);
   webView.getSettings().setJavaScriptEnabled(true);
   CopilotBridge bridge = new CopilotBridge(webView);
   webView.addJavascriptInterface(bridge, "CopilotNative");
   webView.loadUrl("file:///android_asset/editor.html");
   ```
3. In your HTML/JS (editor.html), call:
   ```javascript
   window.CopilotNative.requestCompletion(prompt, model, upstream);
   window.Copilot = {
     onToken: (token) => { /* append to ghost text */ },
     onComplete: () => { /* show accept/reject */ },
     onError: (err) => { /* display error */ }
   };
   ```

### iOS
1. Add `CopilotBridge.swift` to your iOS project.
2. In your ViewController, set up the WKWebView:
   ```swift
   let config = WKWebViewConfiguration()
   let bridge = CopilotBridge(webView: webView)
   config.userContentController.add(bridge, name: "copilot")
   let webView = WKWebView(frame: .zero, configuration: config)
   ```
3. In your HTML/JS, post messages:
   ```javascript
   window.webkit.messageHandlers.copilot.postMessage({
     action: 'requestCompletion',
     prompt: '...',
     model: 'llama3.1:8b-instruct',
     upstream: 'http://localhost:11434/api/generate'
   });
   window.Copilot = {
     onToken: (token) => { /* append to ghost text */ },
     onComplete: () => { /* show accept/reject */ },
     onError: (err) => { /* display error */ }
   };
   ```

## Proxy Setup
- The mobile bridges default to `http://localhost:3200/stream`.
- For testing on a real device, set the proxy to your dev machine's IP (e.g., `http://192.168.1.100:3200/stream`).
- Use `bridge.setProxyUrl(...)` (Android) or post a `setProxyUrl` message (iOS).

## Notes
- This is **real streaming**, not mocked. The native code opens an HTTP connection to your copilot-proxy service and reads SSE events in real time.
- Works with Ollama, OpenAI-compatible endpoints, or any streaming LLM backend your proxy supports.
- For production, secure the proxy with HTTPS and proper authentication (use the OAuth token from `copilot-auth`).
