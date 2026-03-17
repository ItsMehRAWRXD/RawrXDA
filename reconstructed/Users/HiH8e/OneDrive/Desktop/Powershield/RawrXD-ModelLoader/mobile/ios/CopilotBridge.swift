import Foundation
import WebKit

/// Native iOS bridge for Copilot streaming completions.
/// Provides real HTTP streaming to the WKWebView-based editor.
@objc class CopilotBridge: NSObject, WKScriptMessageHandler {
    weak var webView: WKWebView?
    var proxyUrl = "http://localhost:3200/stream"
    
    init(webView: WKWebView) {
        self.webView = webView
        super.init()
    }
    
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.name == "copilot" else { return }
        guard let body = message.body as? [String: Any] else { return }
        guard let action = body["action"] as? String else { return }
        
        switch action {
        case "requestCompletion":
            let prompt = body["prompt"] as? String ?? ""
            let model = body["model"] as? String ?? "llama3.1:8b-instruct"
            let upstream = body["upstream"] as? String ?? "http://localhost:11434/api/generate"
            requestCompletion(prompt: prompt, model: model, upstream: upstream)
        case "setProxyUrl":
            if let url = body["url"] as? String {
                self.proxyUrl = url
            }
        default:
            break
        }
    }
    
    func requestCompletion(prompt: String, model: String, upstream: String) {
        guard let url = URL(string: proxyUrl) else {
            notifyError("Invalid proxy URL")
            return
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        let body: [String: Any] = [
            "prompt": prompt,
            "model": model,
            "upstream": upstream,
            "provider": "ollama",
            "max_tokens": 256
        ]
        
        guard let jsonData = try? JSONSerialization.data(withJSONObject: body) else {
            notifyError("Failed to serialize request")
            return
        }
        
        request.httpBody = jsonData
        
        let task = URLSession.shared.dataTask(with: request) { [weak self] data, response, error in
            guard let self = self else { return }
            
            if let error = error {
                self.notifyError(error.localizedDescription)
                return
            }
            
            guard let httpResponse = response as? HTTPURLResponse, httpResponse.statusCode == 200 else {
                self.notifyError("HTTP error")
                return
            }
            
            guard let data = data else {
                self.notifyError("No data received")
                return
            }
            
            // Parse SSE stream line-by-line
            let lines = String(data: data, encoding: .utf8)?.components(separatedBy: "\n\n") ?? []
            for line in lines {
                if line.hasPrefix("data:") {
                    let payload = line.replacingOccurrences(of: "data:", with: "").trimmingCharacters(in: .whitespaces)
                    if let jsonData = payload.data(using: .utf8),
                       let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] {
                        if let token = json["response"] as? String {
                            self.notifyToken(token)
                        }
                        if let done = json["done"] as? Bool, done {
                            self.notifyComplete()
                            break
                        }
                    }
                }
            }
        }
        
        task.resume()
    }
    
    private func notifyToken(_ token: String) {
        DispatchQueue.main.async { [weak self] in
            let escapedToken = token.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
            self?.webView?.evaluateJavaScript("window.Copilot.onToken(\"\(escapedToken)\")")
        }
    }
    
    private func notifyComplete() {
        DispatchQueue.main.async { [weak self] in
            self?.webView?.evaluateJavaScript("window.Copilot.onComplete()")
        }
    }
    
    private func notifyError(_ error: String) {
        DispatchQueue.main.async { [weak self] in
            let escapedError = error.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
            self?.webView?.evaluateJavaScript("window.Copilot.onError(\"\(escapedError)\")")
        }
    }
}
