#!/usr/bin/env python3
"""
NASM IDE WebSocket Server
Handles real-time communication between IDE frontend and swarm backend
"""

import asyncio
import json
import websockets
import sys
from datetime import datetime

# Connected clients
connected_clients = set()

# Sample file contents for demonstration
sample_files = {
    "main.asm": """section .data
    hello db 'Hello, World!', 0
    hello_len equ $ - hello

section .text
    global _start

_start:
    ; write system call
    mov rax, 1        ; sys_write
    mov rdi, 1        ; stdout
    mov rsi, hello    ; message
    mov rdx, hello_len ; length
    syscall

    ; exit system call
    mov rax, 60       ; sys_exit
    mov rdi, 0        ; exit status
    syscall""",
    
    "utils.asm": """section .text
global strlen
global strcpy

strlen:
    xor rax, rax
.loop:
    cmp byte [rdi + rax], 0
    je .done
    inc rax
    jmp .loop
.done:
    ret

strcpy:
    xor rax, rax
.loop:
    mov dl, byte [rsi + rax]
    mov byte [rdi + rax], dl
    test dl, dl
    jz .done
    inc rax
    jmp .loop
.done:
    ret""",
    
    "build.bat": """@echo off
echo Building NASM project...
nasm -f elf64 main.asm -o main.o
nasm -f elf64 utils.asm -o utils.o
ld main.o utils.o -o program
echo Build complete!
pause"""
}

async def handle_client(websocket, path):
    """Handle new client connections"""
    connected_clients.add(websocket)
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Client connected: {websocket.remote_address}")
    
    try:
        # Send initial files to populate the IDE
        await send_initial_files(websocket)
        
        async for message in websocket:
            try:
                data = json.loads(message)
                await handle_message(websocket, data)
            except json.JSONDecodeError:
                print(f"[ERROR] Invalid JSON received: {message}")
            except Exception as e:
                print(f"[ERROR] Message handling error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Client disconnected")
    except Exception as e:
        print(f"[ERROR] Client handler error: {e}")
    finally:
        connected_clients.discard(websocket)

async def send_initial_files(websocket):
    """Send initial file list and content to the IDE"""
    # Send file list
    await websocket.send(json.dumps({
        "type": "file_list",
        "files": list(sample_files.keys())
    }))
    
    # Send each file content
    for filename, content in sample_files.items():
        await websocket.send(json.dumps({
            "type": "file_content",
            "filename": filename,
            "content": content,
            "fileType": get_file_type(filename)
        }))
        print(f"Open file: {filename}")

async def handle_message(websocket, data):
    """Handle incoming messages from IDE"""
    message_type = data.get('type')
    
    if message_type == 'file_request':
        filename = data.get('filename')
        if filename in sample_files:
            await websocket.send(json.dumps({
                "type": "file_content",
                "filename": filename,
                "content": sample_files[filename],
                "fileType": get_file_type(filename)
            }))
    
    elif message_type == 'syntax_check':
        # Trigger syntax highlighting
        await websocket.send(json.dumps({
            "type": "syntax_check",
            "content": data.get('content', ''),
            "fileType": data.get('fileType', 'asm')
        }))
    
    elif message_type == 'menu_action':
        menu_type = data.get('menu')
        print(f"Show menu: {menu_type}")
        
        # Send menu action response
        await websocket.send(json.dumps({
            "type": "menu_response",
            "menu": menu_type,
            "action": "show"
        }))
    
    elif message_type == 'status_request':
        # Send swarm status
        await websocket.send(json.dumps({
            "type": "swarm_status",
            "agents": {
                "agent_0": {"status": "active", "load": "200MB"},
                "agent_1": {"status": "active", "load": "180MB"},
                "agent_2": {"status": "active", "load": "220MB"},
                "agent_3": {"status": "active", "load": "190MB"},
                "agent_4": {"status": "active", "load": "170MB"}
            }
        }))
    
    elif message_type == 'ai_request':
        # Simulate AI response
        await websocket.send(json.dumps({
            "type": "ai_response",
            "agent": "NASM Assistant",
            "response": f"Analyzing your code... Found {len(data.get('content', '').split('\\n'))} lines of assembly code."
        }))

def get_file_type(filename):
    """Determine file type from extension"""
    if filename.endswith('.asm') or filename.endswith('.s'):
        return 'asm'
    elif filename.endswith('.bat') or filename.endswith('.cmd'):
        return 'bat'
    elif filename.endswith('.c') or filename.endswith('.h'):
        return 'c'
    else:
        return 'text'

async def broadcast_message(message):
    """Broadcast message to all connected clients"""
    if connected_clients:
        await asyncio.gather(
            *[client.send(json.dumps(message)) for client in connected_clients],
            return_exceptions=True
        )

async def main():
    """Start the websocket server"""
    host = 'localhost'
    port = 8765
    
    print("========================================")
    print("NASM IDE WebSocket Server")
    print("========================================")
    print(f"Server: ws://{host}:{port}")
    print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Python: {sys.version.split()[0]}")
    print("========================================")
    print("Waiting for IDE connections...")
    print("Press Ctrl+C to stop")
    print()
    
    try:
        async with websockets.serve(handle_client, host, port):
            await asyncio.Future()  # Run forever
            
    except KeyboardInterrupt:
        print("\\n[SHUTDOWN] Server stopped")
    except Exception as e:
        print(f"[ERROR] Server error: {e}")

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\\nServer shutdown complete")
    except Exception as e:
        print(f"[FATAL] {e}")
        sys.exit(1)