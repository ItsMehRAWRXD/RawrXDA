#!/usr/bin/env python3
"""
Simple HTTP server for Beast Swarm demo
"""

import http.server
import socketserver
import webbrowser
import threading
import time
import os

def start_server(port=8080):
    """Start HTTP server on specified port"""
    try:
        # Change to the directory containing our files
        os.chdir(r"C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master")
        
        Handler = http.server.SimpleHTTPRequestHandler
        with socketserver.TCPServer(("", port), Handler) as httpd:
            print(f"🔥 Beast Swarm Demo Server started at http://localhost:{port}")
            print(f"📁 Serving files from: {os.getcwd()}")
            print(f"🌐 Demo URL: http://localhost:{port}/beast-swarm-demo.html")
            print("Press Ctrl+C to stop the server")
            
            # Open browser after short delay
            def open_browser():
                time.sleep(2)
                webbrowser.open(f'http://localhost:{port}/beast-swarm-demo.html')
            
            threading.Thread(target=open_browser, daemon=True).start()
            
            httpd.serve_forever()
            
    except KeyboardInterrupt:
        print("\n🛑 Server stopped by user")
    except Exception as e:
        print(f"❌ Server error: {e}")

if __name__ == "__main__":
    start_server()