package com.copilot.mobile;

import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import android.os.Handler;
import android.os.Looper;

/**
 * Native Android bridge for Copilot streaming completions.
 * Provides real HTTP streaming to the WebView-based editor.
 */
public class CopilotBridge {
    private WebView webView;
    private Handler mainHandler;
    private String proxyUrl = "http://localhost:3200/stream";

    public CopilotBridge(WebView webView) {
        this.webView = webView;
        this.mainHandler = new Handler(Looper.getMainLooper());
    }

    @JavascriptInterface
    public void requestCompletion(String prompt, String model, String upstream) {
        new Thread(() -> {
            try {
                URL url = new URL(proxyUrl);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json");
                conn.setDoOutput(true);

                // Build request body
                JSONObject body = new JSONObject();
                body.put("prompt", prompt);
                body.put("model", model != null ? model : "llama3.1:8b-instruct");
                body.put("upstream", upstream != null ? upstream : "http://localhost:11434/api/generate");
                body.put("provider", "ollama");
                body.put("max_tokens", 256);

                OutputStream os = conn.getOutputStream();
                os.write(body.toString().getBytes("utf-8"));
                os.close();

                int responseCode = conn.getResponseCode();
                if (responseCode != 200) {
                    notifyError("HTTP " + responseCode);
                    return;
                }

                // Stream response line-by-line
                BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), "utf-8"));
                String line;
                while ((line = br.readLine()) != null) {
                    if (line.startsWith("data:")) {
                        String payload = line.substring(5).trim();
                        try {
                            JSONObject json = new JSONObject(payload);
                            if (json.has("response")) {
                                String token = json.getString("response");
                                notifyToken(token);
                            }
                            if (json.optBoolean("done", false)) {
                                notifyComplete();
                                break;
                            }
                        } catch (Exception e) {
                            // Ignore malformed JSON
                        }
                    }
                }
                br.close();
            } catch (Exception e) {
                notifyError(e.getMessage());
            }
        }).start();
    }

    private void notifyToken(String token) {
        mainHandler.post(() -> {
            webView.evaluateJavascript("window.Copilot.onToken(" + JSONObject.quote(token) + ")", null);
        });
    }

    private void notifyComplete() {
        mainHandler.post(() -> {
            webView.evaluateJavascript("window.Copilot.onComplete()", null);
        });
    }

    private void notifyError(String error) {
        mainHandler.post(() -> {
            webView.evaluateJavascript("window.Copilot.onError(" + JSONObject.quote(error) + ")", null);
        });
    }

    @JavascriptInterface
    public void setProxyUrl(String url) {
        this.proxyUrl = url;
    }
}
