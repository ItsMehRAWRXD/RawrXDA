#!/usr/bin/env python3
"""
Simple Web Dashboard Test
"""

import asyncio
from aiohttp import web
import aiohttp_cors

async def index(request):
    return web.Response(text="""
<!DOCTYPE html>
<html>
<head>
    <title>NASM IDE Swarm Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1e1e1e; color: #fff; }
        .header { background: #007acc; padding: 20px; border-radius: 5px; margin-bottom: 20px; }
        .status { background: #2d2d2d; padding: 15px; border-radius: 5px; margin-bottom: 10px; }
        .online { color: #4caf50; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 NASM IDE Swarm Dashboard</h1>
        <p>Professional AI Agent System</p>
    </div>
    
    <div class="status">
        <h2>System Status</h2>
        <p class="online">✅ Dashboard Online</p>
        <p class="online">✅ Python 3.12 Active</p>
        <p class="online">✅ Web Server Running</p>
    </div>
    
    <div class="status">
        <h2>AI Agents</h2>
        <p>🤖 AI Agent: Active</p>
        <p>📝 Editor Agent: Active</p>
        <p>👥 TeamView Agent: Active</p>
        <p>🛒 Marketplace Agent: Active</p>
        <p>🔍 CodeAnalysis Agent: Active</p>
    </div>
</body>
</html>
    """, content_type='text/html')

async def api_status(request):
    return web.json_response({
        "status": "online",
        "agents": 10,
        "python_version": "3.12",
        "timestamp": "2025-11-21"
    })

async def init_app():
    app = web.Application()
    
    # Add CORS
    cors = aiohttp_cors.setup(app, defaults={
        "*": aiohttp_cors.ResourceOptions(
            allow_credentials=True,
            expose_headers="*",
            allow_headers="*",
        )
    })
    
    # Routes
    app.router.add_get('/', index)
    app.router.add_get('/api/status', api_status)
    
    # Add CORS to routes
    for route in list(app.router.routes()):
        cors.add(route)
    
    return app

async def main():
    print("🚀 Starting NASM IDE Swarm Dashboard...")
    print("🌐 Server: http://localhost:8080")
    print("⚡ Press Ctrl+C to stop")
    
    app = await init_app()
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', 8080)
    await site.start()
    
    print("✅ Dashboard is now running!")
    
    # Keep running
    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("\n🛑 Shutting down...")
        await runner.cleanup()

if __name__ == "__main__":
    asyncio.run(main())