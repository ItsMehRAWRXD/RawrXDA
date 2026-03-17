from typing import Dict, Any, List
import asyncio
import aiohttp
from datetime import datetime
import logging
import json

class ChatMessage:
    def __init__(self, role: str, content: str):
        self.role = role
        self.content = content
        self.timestamp = datetime.utcnow().isoformat()
        self.id = f"{role}-{self.timestamp}"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "role": self.role,
            "content": self.content,
            "timestamp": self.timestamp,
            "id": self.id
        }

class ChatEngine:
    def __init__(self, api_base: str):
        self.api_base = api_base
        self.logger = logging.getLogger("ChatEngine")
        self.message_history: List[ChatMessage] = []
        self.is_connected = False
        self._ws = None

    async def initialize(self) -> bool:
        """Initialize chat engine and establish WebSocket connection"""
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(f"{self.api_base}/api/chat/health") as response:
                    if response.status != 200:
                        raise RuntimeError("Chat service unavailable")

            # Establish WebSocket connection for real-time messaging
            self._ws = await aiohttp.ClientSession().ws_connect(
                f"{self.api_base.replace('http', 'ws')}/chat/ws"
            )
            self.is_connected = True
            
            # Start background message handler
            asyncio.create_task(self._handle_incoming_messages())
            return True

        except Exception as e:
            self.logger.error(f"Chat engine initialization failed: {str(e)}")
            return False

    async def send_message(self, role: str, content: str) -> Dict[str, Any]:
        """Send a message and get the response"""
        if not self.is_connected:
            raise RuntimeError("Chat engine not connected")

        message = ChatMessage(role, content)
        self.message_history.append(message)

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    f"{self.api_base}/api/chat/message",
                    json=message.to_dict()
                ) as response:
                    if response.status != 200:
                        raise RuntimeError(f"Message sending failed: {response.status}")
                    
                    result = await response.json()
                    response_msg = ChatMessage("assistant", result["response"])
                    self.message_history.append(response_msg)
                    return result

        except Exception as e:
            self.logger.error(f"Failed to send message: {str(e)}")
            raise

    async def _handle_incoming_messages(self):
        """Handle incoming WebSocket messages"""
        try:
            async for msg in self._ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    data = json.loads(msg.data)
                    if data["type"] == "message":
                        message = ChatMessage(data["role"], data["content"])
                        self.message_history.append(message)
                        # Handle message received callback here
                elif msg.type == aiohttp.WSMsgType.ERROR:
                    self.logger.error(f"WebSocket error: {msg.data}")
                    break
        except Exception as e:
            self.logger.error(f"WebSocket handler error: {str(e)}")
            self.is_connected = False

    def get_message_history(self) -> List[Dict[str, Any]]:
        """Get chat message history"""
        return [msg.to_dict() for msg in self.message_history]

    async def clear_history(self) -> bool:
        """Clear chat history"""
        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(f"{self.api_base}/api/chat/clear") as response:
                    if response.status == 200:
                        self.message_history.clear()
                        return True
                    return False
        except Exception as e:
            self.logger.error(f"Failed to clear history: {str(e)}")
            return False