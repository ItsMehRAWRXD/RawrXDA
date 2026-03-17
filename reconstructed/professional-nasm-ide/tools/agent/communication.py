#!/usr/bin/env python3
"""
Agent Communication Layer - ZeroMQ Message Broker
High-performance inter-agent communication system
"""

import zmq
import json
import time
import threading
import asyncio
import logging
from typing import Dict, List, Any, Callable, Optional
from dataclasses import dataclass, asdict
from enum import Enum
import uuid

logger = logging.getLogger("AgentCommunication")

class MessageType(Enum):
    TASK_REQUEST = "task_request"
    TASK_RESPONSE = "task_response"
    HEARTBEAT = "heartbeat"
    STATUS_UPDATE = "status_update"
    BROADCAST = "broadcast"
    DIRECT_MESSAGE = "direct_message"
    SHUTDOWN = "shutdown"

@dataclass
class Message:
    """Standard message format for inter-agent communication"""
    message_id: str
    sender_id: str
    receiver_id: str
    message_type: MessageType
    timestamp: float
    payload: Dict[str, Any]
    priority: int = 1
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "message_id": self.message_id,
            "sender_id": self.sender_id,
            "receiver_id": self.receiver_id,
            "message_type": self.message_type.value,
            "timestamp": self.timestamp,
            "payload": self.payload,
            "priority": self.priority
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Message':
        return cls(
            message_id=data["message_id"],
            sender_id=data["sender_id"],
            receiver_id=data["receiver_id"],
            message_type=MessageType(data["message_type"]),
            timestamp=data["timestamp"],
            payload=data["payload"],
            priority=data.get("priority", 1)
        )

class MessageBroker:
    """High-performance ZeroMQ message broker"""
    
    def __init__(self, broker_port: int = 5555):
        self.broker_port = broker_port
        self.context = zmq.Context()
        self.running = False
        
        # Broker sockets
        self.frontend = None  # Receives messages from agents
        self.backend = None   # Sends messages to agents
        
        # Agent registry
        self.agents: Dict[str, Dict[str, Any]] = {}
        self.message_handlers: Dict[str, Callable] = {}
        
        # Message queues
        self.message_queue: List[Message] = []
        self.priority_queue: List[Message] = []
        
        # Statistics
        self.messages_processed = 0
        self.start_time = time.time()
        
    def start_broker(self):
        """Start the message broker"""
        try:
            # Setup broker sockets
            self.frontend = self.context.socket(zmq.ROUTER)
            self.frontend.bind(f"tcp://*:{self.broker_port}")
            
            self.backend = self.context.socket(zmq.DEALER)
            self.backend.bind(f"tcp://*:{self.broker_port + 1}")
            
            self.running = True
            logger.info(f"Message broker started on ports {self.broker_port}-{self.broker_port + 1}")
            
            # Start broker loop in separate thread
            broker_thread = threading.Thread(target=self._broker_loop, daemon=True)
            broker_thread.start()
            
            # Start message processing loop
            process_thread = threading.Thread(target=self._process_messages, daemon=True)
            process_thread.start()
            
        except Exception as e:
            logger.error(f"Failed to start broker: {e}")
            self.running = False
    
    def _broker_loop(self):
        """Main broker loop - routes messages between agents"""
        poller = zmq.Poller()
        poller.register(self.frontend, zmq.POLLIN)
        poller.register(self.backend, zmq.POLLIN)
        
        while self.running:
            try:
                socks = dict(poller.poll(timeout=100))
                
                # Forward messages from frontend to backend
                if socks.get(self.frontend) == zmq.POLLIN:
                    message = self.frontend.recv_multipart()
                    self.backend.send_multipart(message)
                
                # Forward messages from backend to frontend
                if socks.get(self.backend) == zmq.POLLIN:
                    message = self.backend.recv_multipart()
                    self.frontend.send_multipart(message)
                    
            except zmq.Error as e:
                if e.errno == zmq.ETERM:
                    break
                logger.error(f"Broker error: {e}")
            except Exception as e:
                logger.error(f"Unexpected broker error: {e}")
    
    def _process_messages(self):
        """Process queued messages"""
        while self.running:
            try:
                # Process priority messages first
                if self.priority_queue:
                    message = self.priority_queue.pop(0)
                    self._route_message(message)
                elif self.message_queue:
                    message = self.message_queue.pop(0)
                    self._route_message(message)
                else:
                    time.sleep(0.01)  # Brief sleep when no messages
                    
            except Exception as e:
                logger.error(f"Message processing error: {e}")
    
    def _route_message(self, message: Message):
        """Route message to appropriate handler"""
        try:
            # Check if receiver is registered
            if message.receiver_id not in self.agents:
                logger.warning(f"Unknown receiver: {message.receiver_id}")
                return
            
            # Handle different message types
            if message.message_type == MessageType.HEARTBEAT:
                self._handle_heartbeat(message)
            elif message.message_type == MessageType.STATUS_UPDATE:
                self._handle_status_update(message)
            elif message.message_type == MessageType.BROADCAST:
                self._handle_broadcast(message)
            else:
                # Forward to agent
                self._forward_to_agent(message)
            
            self.messages_processed += 1
            
        except Exception as e:
            logger.error(f"Message routing error: {e}")
    
    def _handle_heartbeat(self, message: Message):
        """Handle agent heartbeat"""
        agent_id = message.sender_id
        if agent_id in self.agents:
            self.agents[agent_id]["last_heartbeat"] = time.time()
            self.agents[agent_id]["status"] = "active"
    
    def _handle_status_update(self, message: Message):
        """Handle agent status update"""
        agent_id = message.sender_id
        if agent_id in self.agents:
            self.agents[agent_id].update(message.payload)
    
    def _handle_broadcast(self, message: Message):
        """Handle broadcast message to all agents"""
        for agent_id in self.agents:
            if agent_id != message.sender_id:
                broadcast_msg = Message(
                    message_id=str(uuid.uuid4()),
                    sender_id=message.sender_id,
                    receiver_id=agent_id,
                    message_type=MessageType.DIRECT_MESSAGE,
                    timestamp=time.time(),
                    payload=message.payload
                )
                self._forward_to_agent(broadcast_msg)
    
    def _forward_to_agent(self, message: Message):
        """Forward message to specific agent"""
        try:
            agent_info = self.agents.get(message.receiver_id)
            if agent_info and "socket" in agent_info:
                socket = agent_info["socket"]
                msg_data = json.dumps(message.to_dict()).encode()
                socket.send(msg_data, zmq.NOBLOCK)
                
        except zmq.Again:
            # Queue is full, add to priority queue for retry
            self.priority_queue.append(message)
        except Exception as e:
            logger.error(f"Failed to forward message to {message.receiver_id}: {e}")
    
    def register_agent(self, agent_id: str, agent_info: Dict[str, Any]) -> bool:
        """Register agent with broker"""
        try:
            self.agents[agent_id] = {
                "id": agent_id,
                "type": agent_info.get("type", "unknown"),
                "registered_at": time.time(),
                "last_heartbeat": time.time(),
                "status": "registered",
                **agent_info
            }
            
            logger.info(f"Registered agent: {agent_id}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to register agent {agent_id}: {e}")
            return False
    
    def unregister_agent(self, agent_id: str):
        """Unregister agent from broker"""
        if agent_id in self.agents:
            agent_info = self.agents.pop(agent_id)
            if "socket" in agent_info:
                agent_info["socket"].close()
            logger.info(f"Unregistered agent: {agent_id}")
    
    def send_message(self, message: Message) -> bool:
        """Send message through broker"""
        try:
            if message.priority > 5:
                self.priority_queue.append(message)
            else:
                self.message_queue.append(message)
            return True
            
        except Exception as e:
            logger.error(f"Failed to queue message: {e}")
            return False
    
    def get_broker_stats(self) -> Dict[str, Any]:
        """Get broker performance statistics"""
        uptime = time.time() - self.start_time
        return {
            "uptime": uptime,
            "messages_processed": self.messages_processed,
            "messages_per_second": self.messages_processed / uptime if uptime > 0 else 0,
            "active_agents": len([a for a in self.agents.values() if a["status"] == "active"]),
            "total_agents": len(self.agents),
            "queue_size": len(self.message_queue),
            "priority_queue_size": len(self.priority_queue)
        }
    
    def stop_broker(self):
        """Stop the message broker"""
        self.running = False
        
        # Send shutdown message to all agents
        for agent_id in self.agents:
            shutdown_msg = Message(
                message_id=str(uuid.uuid4()),
                sender_id="broker",
                receiver_id=agent_id,
                message_type=MessageType.SHUTDOWN,
                timestamp=time.time(),
                payload={"reason": "broker_shutdown"}
            )
            self._forward_to_agent(shutdown_msg)
        
        # Cleanup
        if self.frontend:
            self.frontend.close()
        if self.backend:
            self.backend.close()
        self.context.term()
        
        logger.info("Message broker stopped")

class AgentCommunicator:
    """Agent-side communication interface"""
    
    def __init__(self, agent_id: str, broker_host: str = "localhost", broker_port: int = 5555):
        self.agent_id = agent_id
        self.broker_host = broker_host
        self.broker_port = broker_port
        
        self.context = zmq.Context()
        self.socket = None
        self.connected = False
        
        # Message handling
        self.message_handlers: Dict[MessageType, Callable] = {}
        self.running = False
        
    def connect_to_broker(self) -> bool:
        """Connect to message broker"""
        try:
            self.socket = self.context.socket(zmq.REQ)
            self.socket.connect(f"tcp://{self.broker_host}:{self.broker_port}")
            
            # Register with broker
            registration_msg = Message(
                message_id=str(uuid.uuid4()),
                sender_id=self.agent_id,
                receiver_id="broker",
                message_type=MessageType.STATUS_UPDATE,
                timestamp=time.time(),
                payload={"action": "register", "agent_type": "generic"}
            )
            
            self.socket.send(json.dumps(registration_msg.to_dict()).encode())
            response = self.socket.recv()
            
            self.connected = True
            logger.info(f"Agent {self.agent_id} connected to broker")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to broker: {e}")
            return False
    
    def send_message(self, receiver_id: str, message_type: MessageType, payload: Dict[str, Any], priority: int = 1) -> bool:
        """Send message to another agent"""
        if not self.connected:
            logger.warning("Not connected to broker")
            return False
        
        try:
            message = Message(
                message_id=str(uuid.uuid4()),
                sender_id=self.agent_id,
                receiver_id=receiver_id,
                message_type=message_type,
                timestamp=time.time(),
                payload=payload,
                priority=priority
            )
            
            self.socket.send(json.dumps(message.to_dict()).encode())
            return True
            
        except Exception as e:
            logger.error(f"Failed to send message: {e}")
            return False
    
    def broadcast_message(self, message_type: MessageType, payload: Dict[str, Any]) -> bool:
        """Broadcast message to all agents"""
        return self.send_message("*", MessageType.BROADCAST, payload)
    
    def send_heartbeat(self) -> bool:
        """Send heartbeat to broker"""
        return self.send_message(
            "broker",
            MessageType.HEARTBEAT,
            {"agent_id": self.agent_id, "status": "active"}
        )
    
    def register_handler(self, message_type: MessageType, handler: Callable[[Message], None]):
        """Register message handler for specific message type"""
        self.message_handlers[message_type] = handler
    
    def start_listening(self):
        """Start listening for messages"""
        self.running = True
        listen_thread = threading.Thread(target=self._listen_loop, daemon=True)
        listen_thread.start()
    
    def _listen_loop(self):
        """Main listening loop for incoming messages"""
        while self.running:
            try:
                if self.socket.poll(timeout=100):
                    raw_message = self.socket.recv()
                    message_data = json.loads(raw_message.decode())
                    message = Message.from_dict(message_data)
                    
                    # Handle message
                    self._handle_message(message)
                    
            except zmq.Error as e:
                if e.errno == zmq.ETERM:
                    break
                logger.error(f"Socket error in {self.agent_id}: {e}")
            except Exception as e:
                logger.error(f"Message handling error in {self.agent_id}: {e}")
    
    def _handle_message(self, message: Message):
        """Handle received message"""
        try:
            if message.message_type in self.message_handlers:
                self.message_handlers[message.message_type](message)
            else:
                logger.warning(f"No handler for message type: {message.message_type}")
                
        except Exception as e:
            logger.error(f"Error handling message in {self.agent_id}: {e}")
    
    def disconnect(self):
        """Disconnect from broker"""
        self.running = False
        
        if self.connected:
            # Send unregistration message
            self.send_message(
                "broker",
                MessageType.STATUS_UPDATE,
                {"action": "unregister"}
            )
            
        if self.socket:
            self.socket.close()
        
        self.context.term()
        self.connected = False
        logger.info(f"Agent {self.agent_id} disconnected")

# Example Usage
async def demo_communication():
    """Demonstrate agent communication system"""
    
    # Start broker
    broker = MessageBroker()
    broker.start_broker()
    
    await asyncio.sleep(1)  # Let broker start
    
    # Create agents
    agent1 = AgentCommunicator("ai_agent_1")
    agent2 = AgentCommunicator("text_agent_1")
    
    # Connect agents
    agent1.connect_to_broker()
    agent2.connect_to_broker()
    
    # Setup message handlers
    def handle_task_request(message: Message):
        logger.info(f"Agent {message.receiver_id} received task: {message.payload}")
    
    agent2.register_handler(MessageType.TASK_REQUEST, handle_task_request)
    
    # Start listening
    agent1.start_listening()
    agent2.start_listening()
    
    # Send test messages
    agent1.send_message(
        "text_agent_1",
        MessageType.TASK_REQUEST,
        {"text": "Hello, World!", "action": "format"}
    )
    
    # Wait for message processing
    await asyncio.sleep(2)
    
    # Get broker stats
    stats = broker.get_broker_stats()
    logger.info(f"Broker Stats: {json.dumps(stats, indent=2)}")
    
    # Cleanup
    agent1.disconnect()
    agent2.disconnect()
    broker.stop_broker()

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    asyncio.run(demo_communication())