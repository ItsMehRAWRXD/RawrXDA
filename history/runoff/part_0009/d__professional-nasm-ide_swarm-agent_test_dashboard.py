#!/usr/bin/env python3
"""
Simple test to verify dashboard starts correctly
"""

import sys
print(f"Python: {sys.version}")

try:
    import aiohttp
    print("[OK] aiohttp available")
except ImportError as e:
    print(f"[ERROR] aiohttp missing: {e}")
    sys.exit(1)

try:
    import aiohttp_cors
    print("[OK] aiohttp_cors available")
except ImportError as e:
    print(f"[ERROR] aiohttp_cors missing: {e}")

print("\nStarting minimal test server...")

from aiohttp import web

async def handle(request):
    return web.Response(text="NASM IDE Swarm Dashboard - TEST OK!")

app = web.Application()
app.router.add_get('/', handle)

print("Starting server on http://0.0.0.0:8080")
print("Open your browser to: http://localhost:8080")
print("Press Ctrl+C to stop")

web.run_app(app, host='0.0.0.0', port=8080)
