"""
NASM IDE Swarm Client
Python client library for interfacing with the swarm from the NASM IDE
"""

import asyncio
import aiohttp
from typing import Optional, Dict, Any, List

class SwarmClient:
    def __init__(self, coordinator_url: str = "http://localhost:8888"):
        self.coordinator_url = coordinator_url
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()
    
    async def dispatch(self, task_type: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        """Dispatch a task to the swarm"""
        async with self.session.post(
            f"{self.coordinator_url}/dispatch",
            json={'type': task_type, 'payload': payload}
        ) as resp:
            return await resp.json()
    
    async def get_status(self) -> Dict[str, Any]:
        """Get swarm status"""
        async with self.session.get(f"{self.coordinator_url}/status") as resp:
            return await resp.json()
    
    async def send_agent_command(self, agent_id: int, command: Dict[str, Any]) -> Dict[str, Any]:
        """Send command to specific agent"""
        async with self.session.post(
            f"{self.coordinator_url}/agent/{agent_id}/command",
            json=command
        ) as resp:
            return await resp.json()
    
    # High-level API methods
    
    async def code_complete(self, code: str, cursor: int) -> List[Dict[str, Any]]:
        """Get code completions"""
        result = await self.dispatch('code_complete', {
            'task': 'code_complete',
            'code': code,
            'cursor': cursor
        })
        return result.get('result', {}).get('completions', [])
    
    async def refactor_code(self, code: str) -> Dict[str, Any]:
        """Refactor code"""
        result = await self.dispatch('refactor', {
            'task': 'refactor',
            'code': code
        })
        return result.get('result', {})
    
    async def open_file(self, path: str) -> str:
        """Open file through editor agent"""
        result = await self.dispatch('file_open', {
            'task': 'file_open',
            'path': path
        })
        return result.get('result', {}).get('content', '')
    
    async def save_file(self, path: str, content: str) -> bool:
        """Save file through editor agent"""
        result = await self.dispatch('file_save', {
            'task': 'file_save',
            'path': path,
            'content': content
        })
        return result.get('success', False)
    
    async def syntax_highlight(self, code: str) -> List[Dict[str, Any]]:
        """Get syntax highlighting tokens"""
        result = await self.dispatch('syntax_highlight', {
            'task': 'syntax_highlight',
            'code': code
        })
        return result.get('result', {}).get('tokens', [])
    
    async def sync_changes(self, changes: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Sync collaborative changes"""
        result = await self.dispatch('collab_sync', {
            'task': 'sync',
            'changes': changes
        })
        return result.get('result', {})
    
    async def send_chat(self, message: str) -> Dict[str, Any]:
        """Send chat message"""
        result = await self.dispatch('chat', {
            'task': 'chat',
            'message': message
        })
        return result.get('result', {})
    
    async def search_extensions(self, query: str) -> List[Dict[str, Any]]:
        """Search marketplace for extensions"""
        result = await self.dispatch('plugin_install', {
            'task': 'search',
            'query': query
        })
        return result.get('result', {}).get('extensions', [])
    
    async def install_extension(self, extension: str) -> bool:
        """Install extension"""
        result = await self.dispatch('plugin_install', {
            'task': 'install',
            'extension': extension
        })
        return result.get('result', {}).get('installed', False)
    
    async def build_debug(self, source: str) -> Dict[str, Any]:
        """Build in debug mode"""
        result = await self.dispatch('build_debug', {
            'task': 'build_debug',
            'source': source
        })
        return result.get('result', {})
    
    async def build_release(self, source: str) -> Dict[str, Any]:
        """Build in release mode"""
        result = await self.dispatch('build_release', {
            'task': 'build_release',
            'source': source
        })
        return result.get('result', {})
    
    async def detect_toolchains(self) -> Dict[str, Any]:
        """Detect available toolchains"""
        result = await self.dispatch('detect_toolchain', {
            'task': 'detect'
        })
        return result.get('result', {}).get('toolchains', {})
    
    async def update_status(self, message: str, status_type: str = 'info') -> bool:
        """Update status bar"""
        result = await self.dispatch('status_update', {
            'task': 'status_update',
            'message': message,
            'type': status_type
        })
        return result.get('success', False)
    
    async def handle_error(self, error: str) -> Dict[str, Any]:
        """Handle and recover from error"""
        result = await self.dispatch('error_handle', {
            'task': 'error_handle',
            'error': error
        })
        return result.get('result', {})
    
    async def detect_platform(self) -> Dict[str, Any]:
        """Detect platform information"""
        result = await self.dispatch('platform_detect', {
            'task': 'platform_detect'
        })
        return result.get('result', {})
    
    async def parallel_build(self, sources: List[str]) -> Dict[str, Any]:
        """Execute parallel build"""
        result = await self.dispatch('parallel_build', {
            'task': 'parallel_build',
            'sources': sources
        })
        return result.get('result', {})


# Example usage
async def example_usage():
    async with SwarmClient() as client:
        # Check swarm status
        status = await client.get_status()
        print(f"Swarm status: {status}")
        
        # Get code completions
        completions = await client.code_complete("mov ", 4)
        print(f"Completions: {completions}")
        
        # Build debug
        build_result = await client.build_debug("hello.asm")
        print(f"Build result: {build_result}")
        
        # Detect toolchains
        toolchains = await client.detect_toolchains()
        print(f"Toolchains: {toolchains}")

if __name__ == '__main__':
    asyncio.run(example_usage())
