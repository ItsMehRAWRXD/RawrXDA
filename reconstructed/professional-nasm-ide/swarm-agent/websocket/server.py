#!/usr/bin/env python3
"""
WebSocket Server for NASM IDE AI Panel
Provides real-time communication between frontend and swarm agents
"""

import asyncio
import json
import logging
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Installing websockets package...")
    import subprocess
    subprocess.check_call(['pip', 'install', 'websockets'])
    import websockets

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class NASMIDEWebSocketServer:
    """WebSocket server for IDE communication"""
    
    def __init__(self, host='localhost', port=8765):
        self.host = host
        self.port = port
        self.clients = set()
        
    async def handler(self, websocket):
        """Handle WebSocket connections"""
        logger.info(f"New client connected from {websocket.remote_address}")
        self.clients.add(websocket)
        
        try:
            async for message in websocket:
                await self.process_message(websocket, message)
        except websockets.exceptions.ConnectionClosed:
            logger.info("Client disconnected")
        finally:
            self.clients.remove(websocket)
    
    async def process_message(self, websocket, message):
        """Process incoming messages from clients"""
        try:
            data = json.loads(message)
            msg_type = data.get('type', 'unknown')
            
            logger.info(f"Received: {msg_type}")
            
            if msg_type == 'ai_request':
                await self.handle_ai_request(websocket, data)
            elif msg_type == 'status_request':
                await self.handle_status_request(websocket)
            elif msg_type == 'text_editor':
                await self.handle_text_editor(websocket, data)
            elif msg_type == 'build_request':
                await self.handle_build_request(websocket, data)
            elif msg_type == 'command':
                await self.handle_command(websocket, data)
            else:
                logger.warning(f"Unknown message type: {msg_type}")
                
        except json.JSONDecodeError:
            logger.error("Invalid JSON received")
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    async def handle_ai_request(self, websocket, data):
        """Handle AI chat requests"""
        action = data.get('action', '')
        message = data.get('message', '')
        context = data.get('context', {})
        
        # Simulate AI response
        response = {
            'type': 'ai_response',
            'agent': 'Code Analysis',
            'response': f"Processing your request: '{message}'. Analysis in progress...",
            'timestamp': datetime.now().isoformat()
        }
        
        await websocket.send(json.dumps(response))
        
        # Simulate longer processing
        await asyncio.sleep(1)
        
        # Send actual response
        if 'optimize' in message.lower():
            detailed_response = {
                'type': 'ai_response',
                'agent': 'Optimization',
                'response': 'I recommend using SIMD instructions (SSE/AVX) for better performance. Consider loop unrolling and register allocation optimization.',
                'timestamp': datetime.now().isoformat()
            }
        elif 'debug' in message.lower() or 'error' in message.lower():
            detailed_response = {
                'type': 'ai_response',
                'agent': 'Debug',
                'response': 'No errors detected in current code. Use breakpoints to trace execution flow.',
                'timestamp': datetime.now().isoformat()
            }
        else:
            detailed_response = {
                'type': 'ai_response',
                'agent': 'AI Assistant',
                'response': f"I understand you want help with: {message}. I can assist with NASM assembly code completion, optimization, and debugging.",
                'timestamp': datetime.now().isoformat()
            }
        
        await websocket.send(json.dumps(detailed_response))
    
    async def handle_status_request(self, websocket):
        """Handle swarm status requests"""
        status = {
            'type': 'swarm_status',
            'agents': {
                'agent_0': {'status': 'ready', 'type': 'Code Completion'},
                'agent_1': {'status': 'ready', 'type': 'Text Generation'},
                'agent_2': {'status': 'busy', 'type': 'Syntax Analysis'},
                'agent_3': {'status': 'ready', 'type': 'Error Detection'},
                'agent_4': {'status': 'ready', 'type': 'Documentation'},
                'agent_5': {'status': 'busy', 'type': 'Collaboration'},
                'agent_6': {'status': 'ready', 'type': 'Marketplace'},
                'agent_7': {'status': 'ready', 'type': 'Optimization'},
                'agent_8': {'status': 'ready', 'type': 'Debugging'},
                'agent_9': {'status': 'error', 'type': 'Integration'},
            },
            'timestamp': datetime.now().isoformat()
        }
        
        await websocket.send(json.dumps(status))
    
    async def handle_text_editor(self, websocket, data):
        """Handle text editor actions"""
        action = data.get('action', '')
        content = data.get('content', '')
        
        if action == 'syntax_check':
            response = {
                'type': 'syntax_check',
                'status': 'ok',
                'errors': [],
                'warnings': [],
                'timestamp': datetime.now().isoformat()
            }
            await websocket.send(json.dumps(response))
    
    async def handle_build_request(self, websocket, data):
        """Handle build requests"""
        command = data.get('command', '')
        files = data.get('files', [])
        
        # Simulate build process
        build_response = {
            'type': 'build_status',
            'status': 'building',
            'message': f"Building {len(files)} file(s)...",
            'timestamp': datetime.now().isoformat()
        }
        await websocket.send(json.dumps(build_response))
        
        await asyncio.sleep(2)
        
        # Build complete
        complete_response = {
            'type': 'build_status',
            'status': 'success',
            'message': 'Build completed successfully',
            'timestamp': datetime.now().isoformat()
        }
        await websocket.send(json.dumps(complete_response))
    
    async def handle_command(self, websocket, data):
        """Handle IDE commands"""
        action = data.get('action', '')
        
        response = {
            'type': 'command_result',
            'action': action,
            'status': 'executed',
            'timestamp': datetime.now().isoformat()
        }
        await websocket.send(json.dumps(response))
    
    async def start(self):
        """Start the WebSocket server"""
        logger.info(f"Starting WebSocket server on {self.host}:{self.port}")
        
        async with websockets.serve(self.handler, self.host, self.port):
            logger.info(f"WebSocket server running at ws://{self.host}:{self.port}")
            logger.info("Waiting for IDE connections...")
            await asyncio.Future()  # Run forever


def main():
    """Main entry point"""
    server = NASMIDEWebSocketServer(host='localhost', port=8766)
    
    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Server error: {e}")


if __name__ == "__main__":
    main()
