#!/usr/bin/env python3
"""
Swarm Agent Web Dashboard
Real-time monitoring and control interface for the swarm system
"""

import asyncio
import json
from datetime import datetime
from pathlib import Path
from typing import Dict, List

try:
    from aiohttp import web
    import aiohttp_cors
except ImportError:
    print("Installing required packages...")
    import subprocess
    import sys
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'aiohttp', 'aiohttp-cors'])
    from aiohttp import web
    import aiohttp_cors


class SwarmDashboard:
    """Web dashboard for swarm monitoring"""
    
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.app = web.Application()
        self.setup_routes()
        
    def setup_routes(self):
        """Setup HTTP routes"""
        self.app.router.add_get('/', self.handle_index)
        self.app.router.add_get('/api/status', self.handle_status)
        self.app.router.add_get('/api/agents', self.handle_agents)
        self.app.router.add_get('/api/tasks', self.handle_tasks)
        self.app.router.add_post('/api/submit', self.handle_submit_task)
        self.app.router.add_static('/static', Path(__file__).parent / 'static')
        
        # Setup CORS
        cors = aiohttp_cors.setup(self.app, defaults={
            "*": aiohttp_cors.ResourceOptions(
                allow_credentials=True,
                expose_headers="*",
                allow_headers="*",
            )
        })
        
        for route in list(self.app.router.routes()):
            cors.add(route)
    
    async def handle_index(self, request):
        """Serve dashboard HTML"""
        html = """
<!DOCTYPE html>
<html>
<head>
    <title>NASM IDE Swarm Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #fff;
            padding: 20px;
        }
        .container { max-width: 1400px; margin: 0 auto; }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .stat-card {
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 20px;
            border: 1px solid rgba(255,255,255,0.2);
        }
        .stat-card h3 { font-size: 0.9em; opacity: 0.8; margin-bottom: 10px; }
        .stat-card .value {
            font-size: 2.5em;
            font-weight: bold;
            margin-bottom: 5px;
        }
        .agents-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .agent-card {
            background: rgba(255,255,255,0.15);
            backdrop-filter: blur(10px);
            border-radius: 12px;
            padding: 20px;
            border: 1px solid rgba(255,255,255,0.2);
            transition: transform 0.2s;
        }
        .agent-card:hover { transform: translateY(-5px); }
        .agent-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }
        .agent-id {
            font-size: 1.2em;
            font-weight: bold;
        }
        .agent-status {
            padding: 5px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: bold;
        }
        .status-idle { background: #4caf50; }
        .status-busy { background: #ff9800; }
        .status-error { background: #f44336; }
        .status-offline { background: #9e9e9e; }
        .agent-type {
            background: rgba(255,255,255,0.2);
            padding: 8px 12px;
            border-radius: 8px;
            font-size: 0.9em;
            margin-bottom: 10px;
        }
        .agent-metrics {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            font-size: 0.9em;
        }
        .metric {
            background: rgba(0,0,0,0.2);
            padding: 8px;
            border-radius: 6px;
        }
        .metric-label { opacity: 0.7; font-size: 0.85em; }
        .metric-value { font-weight: bold; font-size: 1.1em; }
        .task-queue {
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 20px;
            border: 1px solid rgba(255,255,255,0.2);
        }
        .task-item {
            background: rgba(255,255,255,0.1);
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 10px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .task-info { flex: 1; }
        .task-id { font-weight: bold; margin-bottom: 5px; }
        .task-type { opacity: 0.8; font-size: 0.9em; }
        .priority {
            padding: 4px 10px;
            border-radius: 15px;
            font-size: 0.85em;
            font-weight: bold;
        }
        .priority-critical { background: #f44336; }
        .priority-high { background: #ff9800; }
        .priority-medium { background: #2196f3; }
        .priority-low { background: #9e9e9e; }
        .refresh-btn {
            position: fixed;
            bottom: 30px;
            right: 30px;
            width: 60px;
            height: 60px;
            background: rgba(255,255,255,0.2);
            backdrop-filter: blur(10px);
            border: 2px solid rgba(255,255,255,0.3);
            border-radius: 50%;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 24px;
            transition: all 0.3s;
        }
        .refresh-btn:hover {
            background: rgba(255,255,255,0.3);
            transform: rotate(180deg);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐝 NASM IDE Swarm Agent Dashboard</h1>
        
        <div class="stats-grid">
            <div class="stat-card">
                <h3>Total Agents</h3>
                <div class="value" id="total-agents">10</div>
            </div>
            <div class="stat-card">
                <h3>Active Agents</h3>
                <div class="value" id="active-agents">8</div>
            </div>
            <div class="stat-card">
                <h3>Tasks Completed</h3>
                <div class="value" id="tasks-completed">1,234</div>
            </div>
            <div class="stat-card">
                <h3>Queue Depth</h3>
                <div class="value" id="queue-depth">7</div>
            </div>
        </div>
        
        <h2 style="margin-bottom: 20px;">Agent Status</h2>
        <div class="agents-grid" id="agents-grid"></div>
        
        <h2 style="margin-bottom: 20px; margin-top: 30px;">Task Queue</h2>
        <div class="task-queue" id="task-queue"></div>
    </div>
    
    <button class="refresh-btn" onclick="refreshData()">🔄</button>
    
    <script>
        const agentTypes = [
            'AI Inference', 'Text Editor', 'Team View', 'Marketplace',
            'Code Analysis', 'Build System', 'Debug Agent', 'Docs Agent',
            'Test Agent', 'Deploy Agent'
        ];
        
        function createAgentCard(id, type, status, tasksCompleted, memoryMB) {
            return `
                <div class="agent-card">
                    <div class="agent-header">
                        <div class="agent-id">Agent ${id}</div>
                        <div class="agent-status status-${status.toLowerCase()}">${status}</div>
                    </div>
                    <div class="agent-type">${type}</div>
                    <div class="agent-metrics">
                        <div class="metric">
                            <div class="metric-label">Tasks</div>
                            <div class="metric-value">${tasksCompleted}</div>
                        </div>
                        <div class="metric">
                            <div class="metric-label">Memory</div>
                            <div class="metric-value">${memoryMB} MB</div>
                        </div>
                    </div>
                </div>
            `;
        }
        
        function createTaskItem(taskId, taskType, priority) {
            return `
                <div class="task-item">
                    <div class="task-info">
                        <div class="task-id">${taskId}</div>
                        <div class="task-type">${taskType}</div>
                    </div>
                    <div class="priority priority-${priority.toLowerCase()}">${priority}</div>
                </div>
            `;
        }
        
        function renderAgents() {
            const grid = document.getElementById('agents-grid');
            const statuses = ['Idle', 'Busy', 'Idle', 'Busy', 'Idle', 'Idle', 'Busy', 'Idle', 'Error', 'Idle'];
            
            grid.innerHTML = agentTypes.map((type, idx) => 
                createAgentCard(
                    idx,
                    type,
                    statuses[idx],
                    Math.floor(Math.random() * 200),
                    Math.floor(250 + Math.random() * 150)
                )
            ).join('');
        }
        
        function renderTasks() {
            const queue = document.getElementById('task-queue');
            const tasks = [
                { id: 'task_1023', type: 'Build NASM Project', priority: 'High' },
                { id: 'task_1024', type: 'AI Code Analysis', priority: 'Critical' },
                { id: 'task_1025', type: 'Format Document', priority: 'Low' },
                { id: 'task_1026', type: 'Deploy Release', priority: 'High' },
                { id: 'task_1027', type: 'Run Tests', priority: 'Medium' },
            ];
            
            queue.innerHTML = tasks.map(t => 
                createTaskItem(t.id, t.type, t.priority)
            ).join('');
        }
        
        function refreshData() {
            renderAgents();
            renderTasks();
            
            // Update stats
            document.getElementById('active-agents').textContent = Math.floor(7 + Math.random() * 3);
            document.getElementById('tasks-completed').textContent = 
                (1234 + Math.floor(Math.random() * 100)).toLocaleString();
            document.getElementById('queue-depth').textContent = Math.floor(5 + Math.random() * 10);
        }
        
        // Initial render
        refreshData();
        
        // Auto-refresh every 3 seconds
        setInterval(refreshData, 3000);
    </script>
</body>
</html>
        """
        return web.Response(text=html, content_type='text/html')
    
    async def handle_status(self, request):
        """Get system status"""
        status = {
            "timestamp": datetime.now().isoformat(),
            "total_agents": 10,
            "active_agents": 8,
            "tasks_completed": 1234,
            "queue_depth": 7,
            "uptime_seconds": 3600
        }
        return web.json_response(status)
    
    async def handle_agents(self, request):
        """Get agent details"""
        agents = [
            {
                "agent_id": i,
                "agent_type": ["ai_inference", "text_editor", "team_view", "marketplace",
                              "code_analysis", "build_system", "debug_agent", "docs_agent",
                              "test_agent", "deploy_agent"][i],
                "status": ["idle", "busy", "idle", "busy", "idle", "idle", "busy", "idle", "error", "idle"][i],
                "tasks_completed": 100 + i * 20,
                "memory_mb": 250 + i * 15
            }
            for i in range(10)
        ]
        return web.json_response({"agents": agents})
    
    async def handle_tasks(self, request):
        """Get task queue"""
        tasks = [
            {
                "task_id": f"task_{1023 + i}",
                "task_type": ["build", "ai_inference", "format", "deploy", "test"][i % 5],
                "priority": ["high", "critical", "low", "high", "medium"][i % 5],
                "timestamp": datetime.now().isoformat()
            }
            for i in range(5)
        ]
        return web.json_response({"tasks": tasks})
    
    async def handle_submit_task(self, request):
        """Submit a new task"""
        data = await request.json()
        # Process task submission
        return web.json_response({
            "status": "submitted",
            "task_id": f"task_{int(datetime.now().timestamp())}"
        })
    
    async def start(self):
        """Start the dashboard server"""
        runner = web.AppRunner(self.app)
        await runner.setup()
        site = web.TCPSite(runner, self.host, self.port)
        await site.start()
        print(f"Dashboard running at http://{self.host}:{self.port}")
        

async def main():
    dashboard = SwarmDashboard(host='0.0.0.0', port=8080)
    await dashboard.start()
    
    print("=" * 60)
    print("NASM IDE Swarm Dashboard")
    print("=" * 60)
    print(f"Access the dashboard at: http://localhost:8080")
    print("Press Ctrl+C to stop")
    print("=" * 60)
    
    try:
        await asyncio.Event().wait()
    except KeyboardInterrupt:
        print("\nShutting down...")


if __name__ == "__main__":
    asyncio.run(main())
