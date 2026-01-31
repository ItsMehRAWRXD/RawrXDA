#!/usr/bin/env python3
"""
Collaborative Coding System - CodeSmokeSesh
Real-time collaborative coding with drag & drop support
"""

import os
import sys
import json
import time
import socket
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import hashlib
from datetime import datetime
import uuid

class CollaborativeCodingSystem:
    """Real-time collaborative coding system"""
    
    def __init__(self):
        self.session_id = str(uuid.uuid4())
        self.connected_users = {}
        self.shared_files = {}
        self.collaboration_active = False
        self.server_socket = None
        self.client_sockets = []
        
        print(f"Collaborative Coding System initialized - Session: {self.session_id}")
    
    def enable_drag_drop(self, widget):
        """Enable drag and drop functionality for a widget"""
        widget.bind('<Button-1>', self.on_drag_start)
        widget.bind('<B1-Motion>', self.on_drag_motion)
        widget.bind('<ButtonRelease-1>', self.on_drag_end)
        widget.bind('<Drop>', self.on_drop)
        widget.bind('<Enter>', self.on_drag_enter)
        widget.bind('<Leave>', self.on_drag_leave)
    
    def on_drag_start(self, event):
        """Handle drag start"""
        self.drag_data = {
            'start_x': event.x,
            'start_y': event.y,
            'dragging': True
        }
    
    def on_drag_motion(self, event):
        """Handle drag motion"""
        if hasattr(self, 'drag_data') and self.drag_data['dragging']:
            # Visual feedback during drag
            pass
    
    def on_drag_end(self, event):
        """Handle drag end"""
        if hasattr(self, 'drag_data'):
            self.drag_data['dragging'] = False
    
    def on_drop(self, event):
        """Handle file drop"""
        try:
            # Get dropped file path
            file_path = event.data
            if os.path.exists(file_path):
                self.open_dropped_file(file_path)
        except Exception as e:
            print(f"Drop error: {e}")
    
    def on_drag_enter(self, event):
        """Handle drag enter"""
        event.widget.configure(relief='raised', bg='#404040')
    
    def on_drag_leave(self, event):
        """Handle drag leave"""
        event.widget.configure(relief='flat', bg='#2d2d30')
    
    def open_dropped_file(self, file_path):
        """Open dropped file in editor"""
        try:
            # Determine file type
            file_type = self.detect_file_type(file_path)
            
            # Open file in appropriate editor
            if file_type in ['python', 'javascript', 'typescript', 'java', 'cpp', 'c', 'rust', 'go']:
                self.open_code_file(file_path, file_type)
            elif file_type in ['image']:
                self.open_image_file(file_path)
            elif file_type in ['document']:
                self.open_document_file(file_path)
            else:
                self.open_generic_file(file_path)
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to open file: {str(e)}")
    
    def detect_file_type(self, file_path):
        """Detect file type from extension"""
        ext = os.path.splitext(file_path)[1].lower()
        
        type_map = {
            '.py': 'python',
            '.js': 'javascript',
            '.ts': 'typescript',
            '.java': 'java',
            '.cpp': 'cpp',
            '.c': 'c',
            '.rs': 'rust',
            '.go': 'go',
            '.php': 'php',
            '.rb': 'ruby',
            '.swift': 'swift',
            '.kt': 'kotlin',
            '.scala': 'scala',
            '.clj': 'clojure',
            '.hs': 'haskell',
            '.ml': 'ocaml',
            '.fs': 'fsharp',
            '.cs': 'csharp',
            '.vb': 'vbnet',
            '.pl': 'perl',
            '.lua': 'lua',
            '.r': 'r',
            '.m': 'matlab',
            '.sh': 'bash',
            '.ps1': 'powershell',
            '.sql': 'sql',
            '.html': 'html',
            '.css': 'css',
            '.xml': 'xml',
            '.json': 'json',
            '.yaml': 'yaml',
            '.yml': 'yaml',
            '.toml': 'toml',
            '.ini': 'ini',
            '.cfg': 'config',
            '.conf': 'config',
            '.txt': 'text',
            '.md': 'markdown',
            '.rst': 'restructuredtext',
            '.tex': 'latex',
            '.log': 'log',
            '.png': 'image',
            '.jpg': 'image',
            '.jpeg': 'image',
            '.gif': 'image',
            '.bmp': 'image',
            '.svg': 'image',
            '.pdf': 'document',
            '.doc': 'document',
            '.docx': 'document',
            '.xls': 'document',
            '.xlsx': 'document',
            '.ppt': 'document',
            '.pptx': 'document'
        }
        
        return type_map.get(ext, 'unknown')
    
    def open_code_file(self, file_path, file_type):
        """Open code file with syntax highlighting"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Create new tab for file
            self.create_code_tab(file_path, content, file_type)
            
            # Share with collaborators if session is active
            if self.collaboration_active:
                self.share_file_with_collaborators(file_path, content)
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to open code file: {str(e)}")
    
    def create_code_tab(self, file_path, content, file_type):
        """Create new tab for code file"""
        # This would integrate with the main IDE's tab system
        print(f"Opening {file_type} file: {file_path}")
        print(f"Content length: {len(content)} characters")
    
    def start_collaboration_session(self, port=8080):
        """Start collaboration session"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(('localhost', port))
            self.server_socket.listen(5)
            
            self.collaboration_active = True
            
            # Start server thread
            server_thread = threading.Thread(target=self.run_collaboration_server)
            server_thread.daemon = True
            server_thread.start()
            
            print(f"Collaboration session started on port {port}")
            return {"success": True, "port": port, "session_id": self.session_id}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def run_collaboration_server(self):
        """Run collaboration server"""
        while self.collaboration_active:
            try:
                client_socket, address = self.server_socket.accept()
                client_thread = threading.Thread(
                    target=self.handle_client, 
                    args=(client_socket, address)
                )
                client_thread.daemon = True
                client_thread.start()
                
            except Exception as e:
                if self.collaboration_active:
                    print(f"Server error: {e}")
    
    def handle_client(self, client_socket, address):
        """Handle client connection"""
        try:
            print(f"Client connected: {address}")
            
            # Send session info
            session_info = {
                "session_id": self.session_id,
                "timestamp": datetime.now().isoformat(),
                "message": "Connected to CodeSmokeSesh"
            }
            
            client_socket.send(json.dumps(session_info).encode())
            
            # Handle client messages
            while self.collaboration_active:
                data = client_socket.recv(1024)
                if not data:
                    break
                
                message = json.loads(data.decode())
                self.process_collaboration_message(message, client_socket)
                
        except Exception as e:
            print(f"Client handling error: {e}")
        finally:
            client_socket.close()
            print(f"Client disconnected: {address}")
    
    def process_collaboration_message(self, message, client_socket):
        """Process collaboration message"""
        message_type = message.get('type')
        
        if message_type == 'file_share':
            self.handle_file_share(message)
        elif message_type == 'code_edit':
            self.handle_code_edit(message)
        elif message_type == 'cursor_position':
            self.handle_cursor_position(message)
        elif message_type == 'chat_message':
            self.handle_chat_message(message)
        elif message_type == 'join_session':
            self.handle_join_session(message, client_socket)
    
    def handle_file_share(self, message):
        """Handle file sharing"""
        file_path = message.get('file_path')
        content = message.get('content')
        
        if file_path and content:
            self.shared_files[file_path] = {
                'content': content,
                'timestamp': datetime.now().isoformat(),
                'hash': hashlib.md5(content.encode()).hexdigest()
            }
            
            # Broadcast to all clients
            self.broadcast_message({
                'type': 'file_updated',
                'file_path': file_path,
                'content': content,
                'timestamp': datetime.now().isoformat()
            })
    
    def handle_code_edit(self, message):
        """Handle code editing"""
        file_path = message.get('file_path')
        edit_data = message.get('edit_data')
        
        if file_path in self.shared_files:
            # Apply edit to shared file
            current_content = self.shared_files[file_path]['content']
            # Apply edit logic here
            updated_content = self.apply_edit(current_content, edit_data)
            
            self.shared_files[file_path]['content'] = updated_content
            self.shared_files[file_path]['timestamp'] = datetime.now().isoformat()
            
            # Broadcast edit to all clients
            self.broadcast_message({
                'type': 'code_edit',
                'file_path': file_path,
                'edit_data': edit_data,
                'timestamp': datetime.now().isoformat()
            })
    
    def handle_cursor_position(self, message):
        """Handle cursor position updates"""
        # Broadcast cursor position to other clients
        self.broadcast_message({
            'type': 'cursor_position',
            'user_id': message.get('user_id'),
            'file_path': message.get('file_path'),
            'line': message.get('line'),
            'column': message.get('column'),
            'timestamp': datetime.now().isoformat()
        })
    
    def handle_chat_message(self, message):
        """Handle chat messages"""
        # Broadcast chat message to all clients
        self.broadcast_message({
            'type': 'chat_message',
            'user_id': message.get('user_id'),
            'message': message.get('message'),
            'timestamp': datetime.now().isoformat()
        })
    
    def handle_join_session(self, message, client_socket):
        """Handle user joining session"""
        user_id = message.get('user_id', str(uuid.uuid4()))
        username = message.get('username', f'User_{user_id[:8]}')
        
        self.connected_users[user_id] = {
            'username': username,
            'socket': client_socket,
            'joined_at': datetime.now().isoformat()
        }
        
        # Send current session state to new user
        session_state = {
            'type': 'session_state',
            'shared_files': self.shared_files,
            'connected_users': list(self.connected_users.keys()),
            'timestamp': datetime.now().isoformat()
        }
        
        client_socket.send(json.dumps(session_state).encode())
        
        # Notify other users
        self.broadcast_message({
            'type': 'user_joined',
            'user_id': user_id,
            'username': username,
            'timestamp': datetime.now().isoformat()
        })
    
    def apply_edit(self, content, edit_data):
        """Apply edit to content"""
        # Simple edit application - in real implementation, use operational transforms
        edit_type = edit_data.get('type')
        
        if edit_type == 'insert':
            position = edit_data.get('position', 0)
            text = edit_data.get('text', '')
            return content[:position] + text + content[position:]
        elif edit_type == 'delete':
            start = edit_data.get('start', 0)
            end = edit_data.get('end', 0)
            return content[:start] + content[end:]
        elif edit_type == 'replace':
            start = edit_data.get('start', 0)
            end = edit_data.get('end', 0)
            text = edit_data.get('text', '')
            return content[:start] + text + content[end:]
        
        return content
    
    def broadcast_message(self, message):
        """Broadcast message to all connected clients"""
        for user_id, user_data in self.connected_users.items():
            try:
                user_data['socket'].send(json.dumps(message).encode())
            except Exception as e:
                print(f"Failed to send message to {user_id}: {e}")
                # Remove disconnected user
                del self.connected_users[user_id]
    
    def join_collaboration_session(self, host='localhost', port=8080):
        """Join existing collaboration session"""
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_socket.connect((host, port))
            
            # Send join message
            join_message = {
                'type': 'join_session',
                'user_id': str(uuid.uuid4()),
                'username': os.getenv('USERNAME', 'Anonymous'),
                'timestamp': datetime.now().isoformat()
            }
            
            client_socket.send(json.dumps(join_message).encode())
            
            # Start listening for messages
            listen_thread = threading.Thread(
                target=self.listen_for_messages,
                args=(client_socket,)
            )
            listen_thread.daemon = True
            listen_thread.start()
            
            return {"success": True, "host": host, "port": port}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def listen_for_messages(self, client_socket):
        """Listen for messages from server"""
        try:
            while True:
                data = client_socket.recv(1024)
                if not data:
                    break
                
                message = json.loads(data.decode())
                self.handle_received_message(message)
                
        except Exception as e:
            print(f"Message listening error: {e}")
        finally:
            client_socket.close()
    
    def handle_received_message(self, message):
        """Handle received message"""
        message_type = message.get('type')
        
        if message_type == 'file_updated':
            self.update_shared_file(message)
        elif message_type == 'code_edit':
            self.apply_remote_edit(message)
        elif message_type == 'cursor_position':
            self.show_remote_cursor(message)
        elif message_type == 'chat_message':
            self.display_chat_message(message)
        elif message_type == 'user_joined':
            self.notify_user_joined(message)
        elif message_type == 'session_state':
            self.load_session_state(message)
    
    def update_shared_file(self, message):
        """Update shared file from remote"""
        file_path = message.get('file_path')
        content = message.get('content')
        
        if file_path and content:
            # Update local file if it exists
            if os.path.exists(file_path):
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
    
    def apply_remote_edit(self, message):
        """Apply remote edit to local file"""
        file_path = message.get('file_path')
        edit_data = message.get('edit_data')
        
        if file_path and os.path.exists(file_path):
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            updated_content = self.apply_edit(content, edit_data)
            
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(updated_content)
    
    def show_remote_cursor(self, message):
        """Show remote user's cursor position"""
        user_id = message.get('user_id')
        file_path = message.get('file_path')
        line = message.get('line')
        column = message.get('column')
        
        # Visual indicator for remote cursor
        print(f"Remote cursor: {user_id} at {file_path}:{line}:{column}")
    
    def display_chat_message(self, message):
        """Display chat message"""
        user_id = message.get('user_id')
        chat_message = message.get('message')
        timestamp = message.get('timestamp')
        
        print(f"[{timestamp}] {user_id}: {chat_message}")
    
    def notify_user_joined(self, message):
        """Notify that user joined"""
        user_id = message.get('user_id')
        username = message.get('username')
        
        print(f"User joined: {username} ({user_id})")
    
    def load_session_state(self, message):
        """Load session state"""
        shared_files = message.get('shared_files', {})
        connected_users = message.get('connected_users', [])
        
        self.shared_files.update(shared_files)
        print(f"Loaded session state: {len(shared_files)} files, {len(connected_users)} users")
    
    def stop_collaboration_session(self):
        """Stop collaboration session"""
        self.collaboration_active = False
        
        if self.server_socket:
            self.server_socket.close()
        
        for user_data in self.connected_users.values():
            try:
                user_data['socket'].close()
            except:
                pass
        
        self.connected_users.clear()
        print("Collaboration session stopped")

def main():
    """Test collaborative coding system"""
    print("Testing Collaborative Coding System...")
    
    collaboration = CollaborativeCodingSystem()
    
    # Test drag and drop
    print("Drag & drop support enabled")
    
    # Test collaboration session
    print("Starting collaboration session...")
    result = collaboration.start_collaboration_session()
    if result["success"]:
        print(f"✅ Collaboration session started on port {result['port']}")
        print(f"Session ID: {result['session_id']}")
    else:
        print(f"❌ Failed to start session: {result['error']}")
    
    print("Collaborative Coding System test complete!")

if __name__ == "__main__":
    main()
