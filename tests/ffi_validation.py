"""
RawrXD FFI Validation Wrapper (Python)
Validates C FFI accessibility from outside the C++ world.
Tests: rawrxd_init, rawrxd_stream, rawrxd_free
"""
import ctypes
import os
import sys
import time

class RawrXDFFI:
    """Python wrapper for RawrXD C FFI exports."""
    
    def __init__(self, dll_path: str):
        self.dll = ctypes.CDLL(dll_path)
        self._setup_types()
        self.context = None
        self.tokens_received = []
        
    def _setup_types(self):
        # rawrxd_init(const char* gguf_path) -> RawrXD_Context
        self.dll.rawrxd_init.argtypes = [ctypes.c_char_p]
        self.dll.rawrxd_init.restype = ctypes.c_void_p
        
        # rawrxd_stream(ctx, prompt, callback, user_data) -> int
        self.dll.rawrxd_stream.argtypes = [
            ctypes.c_void_p,      # ctx
            ctypes.c_char_p,      # prompt
            ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p),  # callback
            ctypes.c_void_p       # user_data
        ]
        self.dll.rawrxd_stream.restype = ctypes.c_int
        
        # rawrxd_free(ctx)
        self.dll.rawrxd_free.argtypes = [ctypes.c_void_p]
        self.dll.rawrxd_free.restype = None
        
        # rawrxd_ringbuffer_read(ctx) -> const char*
        self.dll.rawrxd_ringbuffer_read.argtypes = [ctypes.c_void_p]
        self.dll.rawrxd_ringbuffer_read.restype = ctypes.c_char_p
    
    def init(self, model_path: str = "test.gguf") -> bool:
        """Initialize RawrXD context."""
        self.context = self.dll.rawrxd_init(model_path.encode('utf-8'))
        return self.context is not None
    
    def stream(self, prompt: str) -> list:
        """Stream tokens through callback. Returns list of tokens."""
        self.tokens_received = []
        
        @ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)
        def token_callback(token, user_data):
            decoded = token.decode('utf-8') if token else ""
            self.tokens_received.append(decoded)
            print(f"  [Token] {decoded}")
        
        result = self.dll.rawrxd_stream(
            self.context,
            prompt.encode('utf-8'),
            token_callback,
            None
        )
        return self.tokens_received if result == 0 else []
    
    def ringbuffer_read(self) -> str:
        """Read from zero-copy ringbuffer."""
        result = self.dll.rawrxd_ringbuffer_read(self.context)
        return result.decode('utf-8') if result else ""
    
    def free(self):
        """Release context."""
        if self.context:
            self.dll.rawrxd_free(self.context)
            self.context = None


def run_validation():
    """Run full FFI validation suite."""
    print("=" * 60)
    print("RawrXD FFI Validation (Python → C DLL)")
    print("=" * 60)
    
    dll_path = os.path.join(os.path.dirname(__file__), "RawrXD_FFI.dll")
    if not os.path.exists(dll_path):
        print(f"ERROR: DLL not found at {dll_path}")
        print("Build with: cl /LD rawrxd_ffi_shim.cpp /FeRawrXD_FFI.dll")
        return False
    
    print(f"\n[1/5] Loading DLL: {dll_path}")
    try:
        rxd = RawrXDFFI(dll_path)
        print("  ✓ DLL loaded successfully")
    except Exception as e:
        print(f"  ✗ Failed to load DLL: {e}")
        return False
    
    print("\n[2/5] Testing rawrxd_init()")
    start = time.time()
    success = rxd.init("F:/models/Qwen2.5-Coder-32B-Instruct-Q4_K_M.gguf")
    elapsed = (time.time() - start) * 1000
    if success:
        print(f"  ✓ Context initialized in {elapsed:.2f}ms")
    else:
        print("  ✗ Context initialization failed")
        return False
    
    print("\n[3/5] Testing rawrxd_stream()")
    start = time.time()
    tokens = rxd.stream("Generate optimized MASM code for matrix multiplication")
    elapsed = (time.time() - start) * 1000
    if tokens:
        print(f"  ✓ Streamed {len(tokens)} tokens in {elapsed:.2f}ms")
        print(f"  ✓ Token rate: {len(tokens)/elapsed*1000:.1f} tokens/sec")
    else:
        print("  ✗ Stream failed")
        return False
    
    print("\n[4/5] Testing rawrxd_ringbuffer_read()")
    start = time.time()
    rb_data = rxd.ringbuffer_read()
    elapsed = (time.time() - start) * 1000
    if rb_data:
        print(f"  ✓ RingBuffer read in {elapsed:.2f}ms: {rb_data}")
    else:
        print("  ✗ RingBuffer read failed")
        return False
    
    print("\n[5/5] Testing rawrxd_free()")
    start = time.time()
    rxd.free()
    elapsed = (time.time() - start) * 1000
    print(f"  ✓ Context freed in {elapsed:.2f}ms")
    
    print("\n" + "=" * 60)
    print("FFI VALIDATION: ALL TESTS PASSED")
    print("=" * 60)
    print(f"Summary:")
    print(f"  DLL Load:     ✓")
    print(f"  Init:         ✓ ({elapsed:.2f}ms)")
    print(f"  Stream:       ✓ ({len(tokens)} tokens)")
    print(f"  RingBuffer:   ✓")
    print(f"  Free:         ✓")
    print("=" * 60)
    return True


if __name__ == "__main__":
    success = run_validation()
    sys.exit(0 if success else 1)
