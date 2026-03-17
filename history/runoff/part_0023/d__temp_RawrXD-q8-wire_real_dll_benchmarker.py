#!/usr/bin/env python3
"""
REAL GPU BENCHMARK - Uses InferenceEngine DLL to load actual models
"""

import ctypes
import os
import time
import csv
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

class InferenceEngine(ctypes.Structure):
    """Wrapper for InferenceEngine C++ class"""
    pass

class RealGPUBenchmarker:
    def __init__(self):
        self.models_dir = Path("D:\\OllamaModels")
        self.results_dir = Path("d:\\temp\\RawrXD-q8-wire\\test_results")
        self.results_dir.mkdir(exist_ok=True)
        
        # Load the DLL
        dll_path = Path("D:/temp/RawrXD-q8-wire/RawrXD-ModelLoader/build/bin/Release/RawrXD-ModelLoader.dll")
        if not dll_path.exists():
            raise FileNotFoundError(f"DLL not found: {dll_path}")
        
        self.dll = ctypes.CDLL(str(dll_path))
        self._setup_functions()
        
        self.results = []
    
    def _setup_functions(self):
        """Setup C++ function signatures"""
        # InferenceEngine* createEngine()
        self.dll.createEngine.restype = ctypes.POINTER(InferenceEngine)
        
        # bool loadModel(InferenceEngine* engine, const char* path)
        self.dll.loadModel.argtypes = [ctypes.POINTER(InferenceEngine), ctypes.c_char_p]
        self.dll.loadModel.restype = ctypes.c_bool
        
        # char* generate(InferenceEngine* engine, const char* prompt, int tokens)
        self.dll.generate.argtypes = [ctypes.POINTER(InferenceEngine), ctypes.c_char_p, ctypes.c_int]
        self.dll.generate.restype = ctypes.c_char_p
        
        # double tokensPerSecond(InferenceEngine* engine)
        self.dll.tokensPerSecond.argtypes = [ctypes.POINTER(InferenceEngine)]
        self.dll.tokensPerSecond.restype = ctypes.c_double
        
        # void unloadModel(InferenceEngine* engine)
        self.dll.unloadModel.argtypes = [ctypes.POINTER(InferenceEngine)]
        
        # void destroyEngine(InferenceEngine* engine)
        self.dll.destroyEngine.argtypes = [ctypes.POINTER(InferenceEngine)]
    
    def discover_models(self) -> List[Path]:
        """Find all GGUF models"""
        models = sorted(
            self.models_dir.glob("*.gguf"),
            key=lambda x: x.stat().st_size,
            reverse=True
        )
        return models
    
    def benchmark_model(self, model_path: Path, num_tokens: int = 128) -> Optional[Dict]:
        """Run real GPU benchmark on a model"""
        print(f"\n{'='*80}")
        print(f"Model: {model_path.name}")
        print(f"Size:  {model_path.stat().st_size / (1024**3):.1f} GB")
        print(f"{'='*80}")
        
        try:
            # Create engine
            engine = self.dll.createEngine()
            if not engine:
                print("❌ Failed to create engine")
                return None
            
            # Load model
            print("Loading model...", end='', flush=True)
            load_start = time.time()
            
            loaded = self.dll.loadModel(engine, str(model_path).encode('utf-8'))
            
            load_time = time.time() - load_start
            
            if not loaded:
                print(" FAILED")
                self.dll.destroyEngine(engine)
                return None
            
            print(f" OK ({load_time:.1f}s)")
            
            # Generate tokens
            prompt = "Write a short story about AI:"
            print(f"Generating {num_tokens} tokens...", end='', flush=True)
            
            gen_start = time.time()
            output_ptr = self.dll.generate(engine, prompt.encode('utf-8'), num_tokens)
            gen_time = time.time() - gen_start
            
            if not output_ptr:
                print(" FAILED")
                self.dll.unloadModel(engine)
                self.dll.destroyEngine(engine)
                return None
            
            output = output_ptr.decode('utf-8')
            print(f" OK")
            
            # Get TPS
            tps = self.dll.tokensPerSecond(engine)
            latency_ms = (gen_time * 1000) / num_tokens
            
            print(f"\n✓ RESULTS:")
            print(f"  Total Time:  {gen_time*1000:.2f} ms")
            print(f"  TPS:         {tps:.2f}")
            print(f"  Latency:     {latency_ms:.2f} ms/token")
            print(f"  Output:      {len(output)} chars")
            
            # Cleanup
            self.dll.unloadModel(engine)
            self.dll.destroyEngine(engine)
            
            return {
                'model': model_path.stem,
                'path': str(model_path),
                'size_gb': model_path.stat().st_size / (1024**3),
                'tokens_generated': num_tokens,
                'total_time_ms': gen_time * 1000,
                'tokens_per_sec': tps,
                'latency_ms': latency_ms,
                'output_length': len(output),
                'timestamp': datetime.now().isoformat()
            }
            
        except Exception as e:
            print(f"\n❌ ERROR: {e}")
            return None
    
    def run_all_benchmarks(self):
        """Benchmark all models"""
        print("\n" + "="*80)
        print("🚀 REAL GPU BENCHMARK - ACTUAL MODEL LOADING")
        print("="*80)
        
        models = self.discover_models()
        print(f"\nFound {len(models)} GGUF models")
        
        for i, model_path in enumerate(models, 1):
            print(f"\n[{i}/{len(models)}]")
            result = self.benchmark_model(model_path, num_tokens=128)
            
            if result:
                self.results.append(result)
            
            # Brief pause
            time.sleep(2)
    
    def export_results(self):
        """Export to CSV"""
        if not self.results:
            print("\n⚠️  No results to export")
            return
        
        csv_path = self.results_dir / "REAL_GPU_BENCHMARK_RESULTS.csv"
        
        with open(csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=self.results[0].keys())
            writer.writeheader()
            writer.writerows(self.results)
        
        print(f"\n✅ Results exported to: {csv_path}")
    
    def print_summary(self):
        """Print summary"""
        if not self.results:
            return
        
        print("\n" + "="*80)
        print("📊 BENCHMARK SUMMARY")
        print("="*80)
        
        print(f"\n{'Model':<40} {'Size (GB)':>10} {'TPS':>10} {'Latency (ms)':>15}")
        print("-"*80)
        
        for r in sorted(self.results, key=lambda x: x['tokens_per_sec'], reverse=True):
            print(f"{r['model']:<40} {r['size_gb']:>10.1f} {r['tokens_per_sec']:>10.2f} {r['latency_ms']:>15.2f}")
        
        avg_tps = sum(r['tokens_per_sec'] for r in self.results) / len(self.results)
        max_tps = max(r['tokens_per_sec'] for r in self.results)
        min_tps = min(r['tokens_per_sec'] for r in self.results)
        
        print("\n" + "-"*80)
        print(f"Models Tested: {len(self.results)}")
        print(f"Average TPS:   {avg_tps:.2f}")
        print(f"Max TPS:       {max_tps:.2f}")
        print(f"Min TPS:       {min_tps:.2f}")
        print("="*80)

def main():
    try:
        benchmarker = RealGPUBenchmarker()
        benchmarker.run_all_benchmarks()
        benchmarker.export_results()
        benchmarker.print_summary()
    except Exception as e:
        print(f"\n❌ FATAL ERROR: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
