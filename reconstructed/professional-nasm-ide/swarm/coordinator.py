#!/usr/bin/env python3
"""
NASM IDE Swarm Coordinator
Manages 10 AI agents for distributed IDE features
"""

import asyncio
import json
import logging
import time
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass
from enum import Enum
import aiohttp
from aiohttp import web

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("SwarmCoordinator")

class AgentStatus(Enum):
    STARTING = "starting"
    RUNNING = "running"
    BUSY = "busy"
    ERROR = "error"
    STOPPED = "stopped"

@dataclass
class AgentInfo:
    id: int
    name: str
    model: str
    role: str
    port: int
    status: AgentStatus
    last_heartbeat: float
    process: Optional[asyncio.subprocess.Process] = None

class SwarmCoordinator:
    def __init__(self, config_path: str = "swarm_config.json"):
        self.config = self._load_config(config_path)
        self.agents: Dict[int, AgentInfo] = {}
        self.tasks: Dict[str, asyncio.Task] = {}
        self.session: Optional[aiohttp.ClientSession] = None
        
    def _load_config(self, path: str) -> dict:
        with open(path, 'r') as f:
            return json.load(f)
    
    async def start(self):
        """Start the swarm coordinator"""
        logger.info("🚀 Starting NASM IDE Swarm Coordinator")
        self.session = aiohttp.ClientSession()
        
        # Initialize agents
        for agent_config in self.config['agents']:
            agent = AgentInfo(
                id=agent_config['id'],
                name=agent_config['name'],
                model=agent_config['model'],
                role=agent_config['role'],
                port=agent_config['port'],
                status=AgentStatus.STOPPED,
                last_heartbeat=0
            )
            self.agents[agent.id] = agent
        
        # Start all agents
        await self._start_all_agents()
        
        # Start heartbeat monitor
        self.tasks['heartbeat'] = asyncio.create_task(self._heartbeat_monitor())
        
        # Start HTTP server
        app = web.Application()
        app.router.add_post('/dispatch', self.handle_dispatch)
        app.router.add_get('/status', self.handle_status)
        app.router.add_post('/agent/{agent_id}/command', self.handle_agent_command)
        
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, 'localhost', self.config['swarm']['coordinator_port'])
        await site.start()
        
        logger.info(f"✅ Coordinator running on port {self.config['swarm']['coordinator_port']}")
        
    async def _start_all_agents(self):
        """Start all configured agents"""
        tasks = []
        for agent_id, agent in self.agents.items():
            tasks.append(self._start_agent(agent))
        await asyncio.gather(*tasks)
    
    async def _start_agent(self, agent: AgentInfo):
        """Start a single agent process"""
        try:
            logger.info(f"Starting agent: {agent.name} (ID: {agent.id}) on port {agent.port}")
            
            # Start agent process
            cmd = [
                'python', 'agent_runner.py',
                '--id', str(agent.id),
                '--port', str(agent.port),
                '--model', agent.model,
                '--role', agent.role
            ]
            
            agent.process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            
            agent.status = AgentStatus.STARTING
            
            # Wait for agent to be ready
            await self._wait_for_agent_ready(agent)
            
            logger.info(f"✅ Agent {agent.name} ready")
            
        except Exception as e:
            logger.error(f"❌ Failed to start agent {agent.name}: {e}")
            agent.status = AgentStatus.ERROR
    
    async def _wait_for_agent_ready(self, agent: AgentInfo, timeout: int = 30):
        """Wait for agent to respond to health checks"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                async with self.session.get(f'http://localhost:{agent.port}/health') as resp:
                    if resp.status == 200:
                        agent.status = AgentStatus.RUNNING
                        agent.last_heartbeat = time.time()
                        return
            except:
                pass
            await asyncio.sleep(0.5)
        
        raise TimeoutError(f"Agent {agent.name} did not start in time")
    
    async def _heartbeat_monitor(self):
        """Monitor agent health with heartbeats"""
        while True:
            try:
                for agent_id, agent in self.agents.items():
                    if agent.status in [AgentStatus.RUNNING, AgentStatus.BUSY]:
                        try:
                            async with self.session.get(f'http://localhost:{agent.port}/health', timeout=2) as resp:
                                if resp.status == 200:
                                    agent.last_heartbeat = time.time()
                                else:
                                    logger.warning(f"⚠️ Agent {agent.name} health check failed")
                        except Exception as e:
                            logger.error(f"❌ Agent {agent.name} unreachable: {e}")
                            if time.time() - agent.last_heartbeat > 10:
                                agent.status = AgentStatus.ERROR
                                await self._restart_agent(agent)
                
                await asyncio.sleep(self.config['ipc_config']['heartbeat_interval_ms'] / 1000)
            except Exception as e:
                logger.error(f"Heartbeat monitor error: {e}")
    
    async def _restart_agent(self, agent: AgentInfo):
        """Restart a failed agent"""
        logger.info(f"🔄 Restarting agent: {agent.name}")
        
        # Kill existing process
        if agent.process:
            try:
                agent.process.kill()
                await agent.process.wait()
            except:
                pass
        
        # Restart
        await self._start_agent(agent)
    
    async def handle_dispatch(self, request: web.Request) -> web.Response:
        """Dispatch task to appropriate agent"""
        try:
            data = await request.json()
            task_type = data.get('type')
            payload = data.get('payload')
            
            # Route to appropriate agent based on task type
            agent = self._route_task(task_type)
            if not agent:
                return web.json_response({'error': 'No agent available for task'}, status=503)
            
            # Forward to agent
            async with self.session.post(
                f'http://localhost:{agent.port}/execute',
                json=payload,
                timeout=self.config['ipc_config']['timeout_ms'] / 1000
            ) as resp:
                result = await resp.json()
                return web.json_response(result)
                
        except Exception as e:
            logger.error(f"Dispatch error: {e}")
            return web.json_response({'error': str(e)}, status=500)
    
    def _route_task(self, task_type: str) -> Optional[AgentInfo]:
        """Route task to appropriate agent based on type"""
        routing = {
            'code_complete': 'code',
            'refactor': 'code',
            'file_open': 'editor',
            'file_save': 'editor',
            'syntax_highlight': 'editor',
            'collab_sync': 'collab',
            'chat': 'collab',
            'plugin_install': 'market',
            'build_debug': 'build',
            'build_release': 'build',
            'detect_toolchain': 'detect',
            'status_update': 'status',
            'error_handle': 'error',
            'platform_detect': 'platform',
            'parallel_build': 'advanced'
        }
        
        role = routing.get(task_type)
        if not role:
            return None
        
        # Find available agent with matching role
        for agent in self.agents.values():
            if agent.role == role and agent.status == AgentStatus.RUNNING:
                return agent
        
        return None
    
    async def handle_status(self, request: web.Request) -> web.Response:
        """Return swarm status"""
        status = {
            'coordinator': 'running',
            'agents': {}
        }
        
        for agent_id, agent in self.agents.items():
            status['agents'][agent_id] = {
                'name': agent.name,
                'role': agent.role,
                'status': agent.status.value,
                'port': agent.port,
                'last_heartbeat': agent.last_heartbeat
            }
        
        return web.json_response(status)
    
    async def handle_agent_command(self, request: web.Request) -> web.Response:
        """Send command to specific agent"""
        agent_id = int(request.match_info['agent_id'])
        agent = self.agents.get(agent_id)
        
        if not agent:
            return web.json_response({'error': 'Agent not found'}, status=404)
        
        data = await request.json()
        
        try:
            async with self.session.post(
                f'http://localhost:{agent.port}/command',
                json=data,
                timeout=5
            ) as resp:
                result = await resp.json()
                return web.json_response(result)
        except Exception as e:
            return web.json_response({'error': str(e)}, status=500)
    
    async def shutdown(self):
        """Gracefully shutdown all agents"""
        logger.info("🛑 Shutting down swarm coordinator")
        
        # Cancel tasks
        for task in self.tasks.values():
            task.cancel()
        
        # Stop all agents
        for agent in self.agents.values():
            if agent.process:
                try:
                    agent.process.terminate()
                    await agent.process.wait()
                except:
                    pass
        
        if self.session:
            await self.session.close()
        
        logger.info("✅ Shutdown complete")

async def main():
    coordinator = SwarmCoordinator("swarm_config.json")
    
    try:
        await coordinator.start()
        # Keep running
        await asyncio.Event().wait()
    except KeyboardInterrupt:
        logger.info("Received shutdown signal")
    finally:
        await coordinator.shutdown()

if __name__ == '__main__':
    asyncio.run(main())
