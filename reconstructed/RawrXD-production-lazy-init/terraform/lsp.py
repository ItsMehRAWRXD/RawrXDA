#!/usr/bin/env python3
"""
RawrXD Terraform LSP Server
Language Server Protocol implementation for Terraform HCL files

This server wraps the terraform.exe compiler and converts its output
to LSP diagnostics, completions, and other features.
"""

import json
import sys
import os
import subprocess
import tempfile
import threading
import time
from typing import Dict, List, Optional, Any
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class TerraformLSP:
    def __init__(self):
        self.root_uri = None
        self.root_path = None
        self.terraform_exe = self._find_terraform_exe()
        self.diagnostics = {}  # file_uri -> list of diagnostics
        self.documents = {}    # file_uri -> content
        self.process_id = None

    def _find_terraform_exe(self) -> str:
        """Find the terraform.exe executable"""
        # Check current directory first
        current_dir = os.path.dirname(os.path.abspath(__file__))
        terraform_path = os.path.join(current_dir, 'terraform.exe')

        if os.path.exists(terraform_path):
            return terraform_path

        # Check parent directories
        parent_dir = os.path.dirname(current_dir)
        terraform_path = os.path.join(parent_dir, 'terraform.exe')

        if os.path.exists(terraform_path):
            return terraform_path

        # Check system PATH
        try:
            result = subprocess.run(['where', 'terraform.exe'],
                                  capture_output=True, text=True, shell=True)
            if result.returncode == 0:
                return result.stdout.strip().split('\n')[0]
        except:
            pass

        # Default fallback
        return 'terraform.exe'

    def send_message(self, message: Dict[str, Any]) -> None:
        """Send a JSON-RPC message to the client"""
        json_str = json.dumps(message, separators=(',', ':'))
        content_length = len(json_str.encode('utf-8'))
        response = f"Content-Length: {content_length}\r\n\r\n{json_str}"
        sys.stdout.write(response)
        sys.stdout.flush()
        logger.debug(f"Sent: {json_str[:200]}...")

    def send_response(self, id: int, result: Any = None, error: Dict = None) -> None:
        """Send a JSON-RPC response"""
        response = {
            "jsonrpc": "2.0",
            "id": id
        }
        if error:
            response["error"] = error
        else:
            response["result"] = result
        self.send_message(response)

    def send_notification(self, method: str, params: Dict = None) -> None:
        """Send a JSON-RPC notification"""
        notification = {
            "jsonrpc": "2.0",
            "method": method
        }
        if params:
            notification["params"] = params
        self.send_message(notification)

    def read_message(self) -> Optional[Dict[str, Any]]:
        """Read a JSON-RPC message from stdin"""
        try:
            # Read Content-Length header
            line = sys.stdin.readline().strip()
            if not line:
                return None

            if not line.startswith("Content-Length: "):
                logger.warning(f"Invalid header: {line}")
                return None

            content_length = int(line[16:])

            # Skip empty line
            empty_line = sys.stdin.readline()
            if empty_line.strip():
                logger.warning(f"Expected empty line, got: {empty_line}")
                return None

            # Read content
            content = sys.stdin.read(content_length)
            message = json.loads(content)
            logger.debug(f"Received: {content[:200]}...")
            return message

        except Exception as e:
            logger.error(f"Error reading message: {e}")
            return None

    def compile_terraform(self, file_path: str) -> List[Dict]:
        """Run terraform compiler and parse diagnostics"""
        diagnostics = []

        try:
            # Create a temporary file with the content
            with tempfile.NamedTemporaryFile(mode='w', suffix='.tf', delete=False) as f:
                if file_path in self.documents:
                    f.write(self.documents[file_path])
                temp_file = f.name

            # Run terraform compiler
            result = subprocess.run(
                [self.terraform_exe, temp_file],
                capture_output=True,
                text=True,
                timeout=10
            )

            # Clean up temp file
            try:
                os.unlink(temp_file)
            except:
                pass

            # Parse output for diagnostics
            # Since terraform.exe currently just prints success/failure,
            # we'll create mock diagnostics for now
            if result.returncode != 0:
                diagnostics.append({
                    "range": {
                        "start": {"line": 0, "character": 0},
                        "end": {"line": 0, "character": 10}
                    },
                    "severity": 1,  # Error
                    "message": f"Compilation failed: {result.stderr.strip() or 'Unknown error'}",
                    "source": "terraform"
                })

        except subprocess.TimeoutExpired:
            diagnostics.append({
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "severity": 1,
                "message": "Compilation timeout",
                "source": "terraform"
            })
        except Exception as e:
            diagnostics.append({
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "severity": 1,
                "message": f"Compilation error: {str(e)}",
                "source": "terraform"
            })

        return diagnostics

    def handle_initialize(self, id: int, params: Dict) -> None:
        """Handle initialize request"""
        self.root_uri = params.get("rootUri")
        self.root_path = params.get("rootPath")
        self.process_id = params.get("processId")

        capabilities = {
            "textDocumentSync": {
                "openClose": True,
                "change": 1,  # Full content changes
                "save": True
            },
            "completionProvider": {
                "resolveProvider": False,
                "triggerCharacters": [".", "[", "{"]
            },
            "diagnosticProvider": {
                "interFileDependencies": False,
                "workspaceDiagnostics": False
            },
            "hoverProvider": True
        }

        result = {
            "capabilities": capabilities
        }

        self.send_response(id, result)

    def handle_text_document_did_open(self, params: Dict) -> None:
        """Handle textDocument/didOpen"""
        doc = params["textDocument"]
        uri = doc["uri"]
        content = doc["text"]
        version = doc["version"]

        self.documents[uri] = content

        # Run diagnostics
        diagnostics = self.compile_terraform(uri)
        if diagnostics:
            self.send_notification("textDocument/publishDiagnostics", {
                "uri": uri,
                "diagnostics": diagnostics
            })

    def handle_text_document_did_change(self, params: Dict) -> None:
        """Handle textDocument/didChange"""
        doc = params["textDocument"]
        uri = doc["uri"]
        changes = params["contentChanges"]

        # For now, assume full content changes
        if changes:
            self.documents[uri] = changes[0]["text"]

        # Run diagnostics
        diagnostics = self.compile_terraform(uri)
        self.send_notification("textDocument/publishDiagnostics", {
            "uri": uri,
            "diagnostics": diagnostics
        })

    def handle_text_document_completion(self, id: int, params: Dict) -> None:
        """Handle textDocument/completion"""
        # Basic completion for Terraform keywords
        completions = [
            {"label": "resource", "kind": 14, "detail": "Resource block"},
            {"label": "variable", "kind": 14, "detail": "Variable declaration"},
            {"label": "module", "kind": 14, "detail": "Module block"},
            {"label": "output", "kind": 14, "detail": "Output declaration"},
            {"label": "provider", "kind": 14, "detail": "Provider configuration"},
            {"label": "data", "kind": 14, "detail": "Data source"},
            {"label": "locals", "kind": 14, "detail": "Local values"},
        ]

        self.send_response(id, {"items": completions})

    def handle_text_document_hover(self, id: int, params: Dict) -> None:
        """Handle textDocument/hover"""
        # Basic hover information
        hover_text = "Terraform HCL configuration file"
        result = {
            "contents": {
                "kind": "markdown",
                "value": hover_text
            }
        }
        self.send_response(id, result)

    def handle_shutdown(self, id: int) -> None:
        """Handle shutdown request"""
        self.send_response(id, None)

    def handle_exit(self) -> None:
        """Handle exit notification"""
        sys.exit(0)

    def process_message(self, message: Dict) -> None:
        """Process a JSON-RPC message"""
        method = message.get("method")
        id = message.get("id")
        params = message.get("params", {})

        if method == "initialize":
            self.handle_initialize(id, params)
        elif method == "shutdown":
            self.handle_shutdown(id)
        elif method == "textDocument/didOpen":
            self.handle_text_document_did_open(params)
        elif method == "textDocument/didChange":
            self.handle_text_document_did_change(params)
        elif method == "textDocument/completion":
            self.handle_text_document_completion(id, params)
        elif method == "textDocument/hover":
            self.handle_text_document_hover(id, params)
        elif method == "exit":
            self.handle_exit()
        else:
            logger.warning(f"Unhandled method: {method}")

def main():
    """Main LSP server loop"""
    logger.info("Starting RawrXD Terraform LSP Server")
    logger.info(f"Terraform executable: {TerraformLSP()._find_terraform_exe()}")

    server = TerraformLSP()

    while True:
        message = server.read_message()
        if message is None:
            break

        try:
            server.process_message(message)
        except Exception as e:
            logger.error(f"Error processing message: {e}")
            if "id" in message:
                server.send_response(message["id"], None, {
                    "code": -32603,
                    "message": f"Internal error: {str(e)}"
                })

if __name__ == "__main__":
    main()