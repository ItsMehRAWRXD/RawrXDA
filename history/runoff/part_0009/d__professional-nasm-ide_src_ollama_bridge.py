"""
Ollama Native Bridge - Python FFI Interface
Provides Python bindings to the native ASM Ollama wrapper
"""

import ctypes
import os
from typing import Optional, List, Dict, Any
from pathlib import Path
from dataclasses import dataclass


# ==============================================================================
# Structure Definitions (matching ASM)
# ==============================================================================

class OllamaConfig(ctypes.Structure):
    """Ollama configuration structure"""
    _fields_ = [
        ("host_ptr", ctypes.c_void_p),
        ("host_len", ctypes.c_uint64),
        ("port", ctypes.c_uint16),
        ("timeout_ms", ctypes.c_uint64),
        ("socket_fd", ctypes.c_int32),
        ("connected", ctypes.c_uint8),
        ("keepalive", ctypes.c_uint8),
        ("retry_count", ctypes.c_uint8),
        ("padding", ctypes.c_uint8),
    ]


class OllamaRequest(ctypes.Structure):
    """Ollama request structure"""
    _fields_ = [
        ("model_ptr", ctypes.c_void_p),
        ("model_len", ctypes.c_uint64),
        ("prompt_ptr", ctypes.c_void_p),
        ("prompt_len", ctypes.c_uint64),
        ("system_ptr", ctypes.c_void_p),
        ("system_len", ctypes.c_uint64),
        ("context_ptr", ctypes.c_void_p),
        ("context_len", ctypes.c_uint64),
        ("messages_ptr", ctypes.c_void_p),
        ("messages_count", ctypes.c_uint64),
        ("temperature", ctypes.c_int32),
        ("top_p", ctypes.c_int32),
        ("top_k", ctypes.c_int32),
        ("num_predict", ctypes.c_int32),
        ("stream", ctypes.c_uint8),
        ("raw", ctypes.c_uint8),
        ("use_chat_api", ctypes.c_uint8),
        ("padding", ctypes.c_uint8),
    ]


class OllamaResponse(ctypes.Structure):
    """Ollama response structure"""
    _fields_ = [
        ("response_ptr", ctypes.c_void_p),
        ("response_len", ctypes.c_uint64),
        ("response_capacity", ctypes.c_uint64),
        ("context_ptr", ctypes.c_void_p),
        ("context_len", ctypes.c_uint64),
        ("done", ctypes.c_uint8),
        ("padding", ctypes.c_uint8 * 7),
        ("total_duration", ctypes.c_uint64),
        ("load_duration", ctypes.c_uint64),
        ("prompt_eval_count", ctypes.c_uint64),
        ("prompt_eval_dur", ctypes.c_uint64),
        ("eval_count", ctypes.c_uint64),
        ("eval_duration", ctypes.c_uint64),
    ]


# ==============================================================================
# Python Dataclasses for Easier Use
# ==============================================================================

@dataclass
class GenerateRequest:
    """High-level request structure"""
    model: str
    prompt: str
    system: Optional[str] = None
    context: Optional[List[int]] = None
    temperature: float = 0.7
    top_p: float = 0.9
    top_k: int = 40
    num_predict: int = 512
    stream: bool = False
    raw: bool = False


@dataclass
class GenerateResponse:
    """High-level response structure"""
    response: str
    done: bool
    context: Optional[List[int]] = None
    total_duration: int = 0
    load_duration: int = 0
    prompt_eval_count: int = 0
    eval_count: int = 0


# ==============================================================================
# Ollama Bridge Class
# ==============================================================================

class OllamaBridge:
    """Python interface to native ASM Ollama wrapper"""
    
    def __init__(self, lib_path: Optional[str] = None):
        """
        Initialize Ollama bridge
        
        Args:
            lib_path: Path to compiled .so/.dll (auto-detected if None)
        """
        self.lib = None
        self.initialized = False
        self._response_buffer = None
        
        # Auto-detect library path
        if lib_path is None:
            lib_path = self._find_library()
        
        if lib_path and os.path.exists(lib_path):
            self._load_library(lib_path)
        else:
            print(f"[OllamaBridge] Warning: Native library not found at {lib_path}")
            print("[OllamaBridge] Falling back to Python-only mode")
    
    def _find_library(self) -> str:
        """Auto-detect the compiled library"""
        base_dir = Path(__file__).parent.parent
        
        # Check common locations
        candidates = [
            base_dir / "lib" / "ollama_native.so",
            base_dir / "lib" / "ollama_native.dll",
            base_dir / "build" / "ollama_native.so",
            base_dir / "build" / "ollama_native.dll",
            Path("/usr/local/lib/ollama_native.so"),
        ]
        
        for path in candidates:
            if isinstance(path, str):
                path = Path(path)
            if path.exists():
                return str(path)
        return ""
    
    def _load_library(self, lib_path: str):
        """Load the native library and bind functions"""
        try:
            self.lib = ctypes.CDLL(lib_path)
            
            # Minimal API - only essential functions
            # ollama_init(config: OllamaConfig*, host: char*, host_len: uint64, port: uint16) -> int
            self.lib.ollama_init.argtypes = [
                ctypes.POINTER(OllamaConfig),
                ctypes.c_char_p,
                ctypes.c_uint64,
                ctypes.c_uint16
            ]
            self.lib.ollama_init.restype = ctypes.c_int
            
            # ollama_connect(config: OllamaConfig*) -> int
            self.lib.ollama_connect.argtypes = [ctypes.POINTER(OllamaConfig)]
            self.lib.ollama_connect.restype = ctypes.c_int
            
            # ollama_generate(config: OllamaConfig*, request: OllamaRequest*, response: OllamaResponse*) -> int
            self.lib.ollama_generate.argtypes = [
                ctypes.POINTER(OllamaConfig),
                ctypes.POINTER(OllamaRequest),
                ctypes.POINTER(OllamaResponse)
            ]
            self.lib.ollama_generate.restype = ctypes.c_int
            
            # ollama_list_models(config: OllamaConfig*, buffer: void*, buffer_size: uint64) -> int
            self.lib.ollama_list_models.argtypes = [
                ctypes.POINTER(OllamaConfig),
                ctypes.c_void_p,
                ctypes.c_uint64
            ]
            self.lib.ollama_list_models.restype = ctypes.c_int
            
            # ollama_close(config: OllamaConfig*) -> int
            self.lib.ollama_close.argtypes = [ctypes.POINTER(OllamaConfig)]
            self.lib.ollama_close.restype = ctypes.c_int
            
            print(f"[OllamaBridge] Loaded native library: {lib_path}")
            
        except Exception as e:
            print(f"[OllamaBridge] Error loading library: {e}")
            self.lib = None
    
    def init(self, host: str = "127.0.0.1", port: int = 11434) -> bool:
        """
        Initialize Ollama connection
        
        Args:
            host: Ollama server hostname/IP
            port: Ollama server port
            
        Returns:
            True if initialization successful
        """
        if not self.lib:
            print("[OllamaBridge] Native library not available")
            return False
        
        try:
            host_bytes = host.encode('utf-8')
            result = self.lib.ollama_init(host_bytes, port)
            self.initialized = (result == 1)
            
            if self.initialized:
                # Allocate response buffer
                self._response_buffer = ctypes.create_string_buffer(128 * 1024)
            
            return self.initialized
            
        except Exception as e:
            print(f"[OllamaBridge] Init error: {e}")
            return False
    
    def generate(self, request: GenerateRequest) -> Optional[GenerateResponse]:
        """
        Generate text completion (prompt-only; context and messages are currently ignored due to current implementation).
        TODO: Future support for context and messages will be added.
        
        Args:
            request: GenerateRequest object
            
        Returns:
            GenerateResponse object or None on failure

        Note:
            This function ignores context and messages fields in the request.
            Only prompt-based generation is supported; chat functionality is not implemented.
        """
        if not self.initialized:
            print("[OllamaBridge] Not initialized - call init() first")
            return None
        
        try:
            # Prepare request structure
            req = OllamaRequest()
            
            # Model
            model_bytes = request.model.encode('utf-8')
            model_buffer = ctypes.create_string_buffer(model_bytes)
            req.model_ptr = ctypes.cast(ctypes.byref(model_buffer), ctypes.c_void_p)
            req.model_len = len(model_bytes)
            
            # Prompt
            prompt_bytes = request.prompt.encode('utf-8')
            req.prompt_ptr = ctypes.cast(prompt_bytes, ctypes.c_void_p)
            req.prompt_len = len(prompt_bytes)
            
            # System prompt (optional)
            if request.system:
                system_bytes = request.system.encode('utf-8')
                req.system_ptr = ctypes.cast(system_bytes, ctypes.c_void_p)
                req.system_len = len(system_bytes)
            else:
                req.system_ptr = None
                req.system_len = 0
            
            # Context (optional)
            req.context_ptr = None
            req.context_len = 0
            
            # Parameters (scaled by 1000 for fixed-point)
            req.temperature = int(request.temperature * 1000)
            req.top_p = int(request.top_p * 1000)
            req.top_k = request.top_k
            req.num_predict = request.num_predict
            
            # Flags
            req.stream = 1 if request.stream else 0
            req.raw = 1 if request.raw else 0
            req.use_chat_api = 0
            
            # Prepare response structure
            resp = OllamaResponse()
            resp.response_ptr = ctypes.cast(self._response_buffer, ctypes.c_void_p)
            resp.response_capacity = len(self._response_buffer)
            resp.response_len = 0
            resp.done = 0
            
            # Call native function
            result = self.lib.ollama_generate(ctypes.byref(req), ctypes.byref(resp))
            
            if result != 1:
                print("[OllamaBridge] Generate failed")
                return None
            
            # Extract response
            response_text = self._response_buffer.value.decode('utf-8', errors='ignore')
            
            return GenerateResponse(
                response=response_text,
                done=(resp.done == 1),
                context=None,  # TODO: Extract context if present
                total_duration=resp.total_duration,
                load_duration=resp.load_duration,
                prompt_eval_count=resp.prompt_eval_count,
                eval_count=resp.eval_count,
            )
            
        except Exception as e:
            print(f"[OllamaBridge] Generate error: {e}")
            return None
    
    def chat(self, request: GenerateRequest) -> Optional[GenerateResponse]:
        """
        Chat with context preservation
        
        Args:
            request: GenerateRequest object
            
        Returns:
            GenerateResponse object or None on failure
        """
        if not self.initialized:
            return None
        
        # Similar to generate but uses chat API
        # TODO: Implement chat-specific logic
        return self.generate(request)
    
    def list_models(self) -> List[str]:
        """
        List available models
        
        Returns:
            List of model names
        """
        if not self.initialized:
            return []
        
        # TODO: Implement model listing
        # Need to allocate buffer for model data
        return []
    
    def pull_model(self, model_name: str) -> bool:
        """
        Download a model
        
        Args:
            model_name: Name of model to download
            
        Returns:
            True if successful
        """
        if not self.initialized:
            return False
        
        try:
            name_bytes = model_name.encode('utf-8')
            result = self.lib.ollama_pull_model(name_bytes, len(name_bytes))
            return result == 1
        except Exception as e:
            print(f"[OllamaBridge] Pull model error: {e}")
            return False
    
    def delete_model(self, model_name: str) -> bool:
        """
        Delete a model
        
        Args:
            model_name: Name of model to delete
            
        Returns:
            True if successful
        """
        if not self.initialized:
            return False
        
        try:
            name_bytes = model_name.encode('utf-8')
            result = self.lib.ollama_delete_model(name_bytes, len(name_bytes))
            return result == 1
        except Exception as e:
            print(f"[OllamaBridge] Delete model error: {e}")
            return False
    
    def close(self):
        """Close Ollama connection"""
        if self.initialized and self.lib:
            try:
                self.lib.ollama_close()
            except Exception as e:
                print(f"[OllamaBridge] Close error: {e}")
            finally:
                self.initialized = False
    
    def __del__(self):
        """Cleanup on deletion"""
        self.close()


# ==============================================================================
# Convenience Functions
# ==============================================================================

# Global instance
_global_bridge: Optional[OllamaBridge] = None


def get_bridge() -> OllamaBridge:
    """Get or create global Ollama bridge instance"""
    global _global_bridge
    if _global_bridge is None:
        _global_bridge = OllamaBridge()
    return _global_bridge


def init_ollama(host: str = "127.0.0.1", port: int = 11434) -> bool:
    """Initialize global Ollama connection"""
    bridge = get_bridge()
    return bridge.init(host, port)


def generate(
    model: str,
    prompt: str,
    system: Optional[str] = None,
    temperature: float = 0.7,
    **kwargs
) -> Optional[str]:
    """
    Quick generation function
    
    Args:
        model: Model name (e.g., "llama2", "codellama")
        prompt: User prompt
        system: System prompt (optional)
        temperature: Temperature (0.0-2.0)
        **kwargs: Additional parameters
        
    Returns:
        Generated text or None on failure
    """
    bridge = get_bridge()
    
    if not bridge.initialized:
        bridge.init()
    
    request = GenerateRequest(
        model=model,
        prompt=prompt,
        system=system,
        temperature=temperature,
        **kwargs
    )
    
    response = bridge.generate(request)
    return response.response if response else None


def chat(
    model: str,
    messages: List[Dict[str, str]],
    **kwargs
) -> Optional[str]:
    """
    Quick chat function
    
    Args:
        model: Model name
        messages: List of message dicts with 'role' and 'content'
        **kwargs: Additional parameters
        
    Returns:
        Response text or None on failure
    """
    # TODO: Implement proper chat message handling
    if not messages:
        return None
    
    # Extract last user message as prompt
    prompt = messages[-1].get('content', '')
    
    # Extract system message if present
    system = None
    for msg in messages:
        if msg.get('role') == 'system':
            system = msg.get('content')
            break
    
    return generate(model, prompt, system=system, **kwargs)


# ==============================================================================
# Example Usage
# ==============================================================================

if __name__ == "__main__":
    import sys
    
    print("=" * 70)
    print("Ollama Native Bridge - Test Suite")
    print("=" * 70)
    
    # Initialize
    print("\n[1] Initializing Ollama connection...")
    if not init_ollama():
        print("ERROR: Failed to initialize Ollama")
        sys.exit(1)
    
    print("SUCCESS: Connected to Ollama")
    
    # Test generation
    print("\n[2] Testing text generation...")
    response = generate(
        model="llama2",
        prompt="Write a haiku about programming in assembly language.",
        temperature=0.8,
    )
    
    if response:
        print(f"Response:\n{response}")
    else:
        print("ERROR: Generation failed")
    
    # Cleanup
    print("\n[3] Cleaning up...")
    bridge = get_bridge()
    bridge.close()
    
    print("\n" + "=" * 70)
    print("Test complete!")
    print("=" * 70)
