#!/usr/bin/env python3
"""
Multi-Model GPU Benchmark Harness
Tests all available GGUF and Ollama models for performance metrics
"""

import os
import json
import time
import subprocess
import csv
import statistics
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple
import sys
import re

class MultiModelBenchmark:
    def __init__(self):
        self.gguf_dir = Path("d:\\OllamaModels")
        self.results_dir = Path("d:\\temp\\RawrXD-q8-wire\\test_results")
        self.results_dir.mkdir(exist_ok=True)
        self.models = {
            'gguf': self._discover_gguf_models()
        }
        self.results = {'gguf': []}
        self.gpu_benchmark_exe = Path(r"D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build\bin\Release\gpu_inference_benchmark.exe")
        
    def _discover_gguf_models(self) -> List[Dict]:
        """Discover all GGUF files in OllamaModels directory"""
        gguf_models = []
        for gguf_file in self.gguf_dir.glob("*.gguf"):
            size_gb = gguf_file.stat().st_size / (1024**3)
            gguf_models.append({
                'name': gguf_file.stem,
                'path': str(gguf_file),
                'size_gb': round(size_gb, 2),
                'format': 'GGUF'
            })
        return sorted(gguf_models, key=lambda x: x['size_gb'], reverse=True)
    
    def _discover_ollama_models(self) -> List[Dict]:
        """Ollama models not used - using own GPU backend instead"""
        return []
    
    def print_discovery_summary(self):
        """Print discovered models summary"""
        print("\n" + "="*70)
        print("🚀 MULTI-MODEL GPU TESTING - DISCOVERY SUMMARY")
        print("="*70)
        
        print(f"\n📦 GGUF Models Discovered: {len(self.models['gguf'])}")
        print("-" * 70)
        for model in self.models['gguf']:
            print(f"  • {model['name']:<40} {model['size_gb']:>6.1f} GB")
        
        print("\n" + "="*70)
        print(f"TOTAL GGUF MODELS AVAILABLE: {len(self.models['gguf'])}")
        print(f"  • Using GPU Backend: RawrXD-ModelLoader (Vulkan)")
        print(f"  • Benchmark Tool: gpu_inference_benchmark.exe")
        print("="*70)
    
    def test_gguf_batch(self, batch_size: int = 1, repetitions: int = 3):
        """Test all GGUF models using gpu_inference_benchmark.exe"""
        print(f"\n📊 GGUF TESTING - Batch Size: {batch_size}, Repetitions: {repetitions}")
        print("="*70)
        
        if not self.gpu_benchmark_exe.exists():
            print(f"❌ ERROR: gpu_inference_benchmark.exe not found at {self.gpu_benchmark_exe}")
            print("   Please build the project first:")
            print("   cd D:\\temp\\RawrXD-q8-wire\\RawrXD-ModelLoader\\build")
            print("   cmake --build . --config Release --target gpu_inference_benchmark -j 8")
            return
        
        for i, model in enumerate(self.models['gguf'], 1):
            print(f"\n[{i}/{len(self.models['gguf'])}] Testing {model['name']}")
            print(f"      Path: {model['path']}")
            print(f"      Size: {model['size_gb']} GB")
            
            # Run actual GPU benchmarks
            tps_results = []
            latency_results = []
            
            for rep in range(repetitions):
                try:
                    bench_result = self._run_gpu_benchmark(model['path'], model['name'])
                    if bench_result:
                        tps_results.append(bench_result['tps'])
                        latency_results.append(bench_result['latency_ms'])
                        print(f"      Rep {rep+1}: {bench_result['tps']:.2f} TPS ({bench_result['latency_ms']:.2f} ms/token)")
                    else:
                        print(f"      Rep {rep+1}: ❌ Benchmark failed")
                except Exception as e:
                    print(f"      Rep {rep+1}: ❌ Error - {str(e)}")
            
            if tps_results:
                result = {
                    'model': model['name'],
                    'format': 'GGUF',
                    'size_gb': model['size_gb'],
                    'batch_size': batch_size,
                    'mean_tps': statistics.mean(tps_results),
                    'std_tps': statistics.stdev(tps_results) if len(tps_results) > 1 else 0,
                    'min_tps': min(tps_results),
                    'max_tps': max(tps_results),
                    'mean_latency_ms': statistics.mean(latency_results),
                    'std_latency_ms': statistics.stdev(latency_results) if len(latency_results) > 1 else 0,
                    'timestamp': datetime.now().isoformat()
                }
                self.results['gguf'].append(result)
                print(f"      Mean: {result['mean_tps']:.2f} ± {result['std_tps']:.2f} TPS")
                print(f"      Latency: {result['mean_latency_ms']:.2f} ± {result['std_latency_ms']:.2f} ms/token")
    
    def _run_gpu_benchmark(self, model_path: str, model_name: str) -> Dict:
        """Run gpu_inference_benchmark.exe and parse GPU results"""
        try:
            result = subprocess.run(
                [str(self.gpu_benchmark_exe), '--model', model_path, '--tokens', '256', '--runs', '1'],
                capture_output=True,
                timeout=120,
                encoding='utf-8',
                errors='ignore'
            )
            
            if result.returncode != 0:
                print(f"        Warning: benchmark returned code {result.returncode}")
                return None
            
            # Parse output for GPU TPS and latency metrics
            output = result.stdout + result.stderr
            
            # Look for GPU Vulkan section specifically
            gpu_section_match = re.search(
                r'=== BENCHMARK:.*?\(GPU Vulkan\).*?Tokens/Sec:\s*([0-9.]+).*?Avg Latency/Token:\s*([0-9.]+)\s*ms',
                output,
                re.DOTALL
            )
            
            if gpu_section_match:
                tps = float(gpu_section_match.group(1))
                latency_ms = float(gpu_section_match.group(2))
                return {'tps': tps, 'latency_ms': latency_ms}
            
            # Fallback: try the performance comparison section
            gpu_speedup_match = re.search(r'GPU \(Vulkan\).*?(\d+\.\d+)', output)
            if gpu_speedup_match:
                # Try to find GPU TPS from the comparison table
                tps_match = re.search(r'Tokens/Sec\s+\d+\.\d+\s+(\d+\.\d+)', output)
                if tps_match:
                    tps = float(tps_match.group(1))
                    latency_ms = 1000.0 / tps if tps > 0 else 0
                    return {'tps': tps, 'latency_ms': latency_ms}
            
            return None
        except subprocess.TimeoutExpired:
            print(f"        Timeout: benchmark took too long")
            return None
        except Exception as e:
            print(f"        Exception: {str(e)}")
            return None
    
    def test_ollama_models(self, sample_size: int = 10, repetitions: int = 2):
        """Not used - using own GPU backend instead"""
        pass
    
    def export_results_csv(self):
        """Export results to CSV files"""
        # GGUF results
        if self.results['gguf']:
            gguf_csv = self.results_dir / "GGUF_BENCHMARK_RESULTS.csv"
            with open(gguf_csv, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=[
                    'model', 'format', 'size_gb', 'batch_size', 
                    'mean_tps', 'std_tps', 'min_tps', 'max_tps', 
                    'mean_latency_ms', 'std_latency_ms', 'timestamp'
                ])
                writer.writeheader()
                writer.writerows(self.results['gguf'])
            print(f"\n✅ GGUF results exported to: {gguf_csv}")
    
    def generate_summary_report(self):
        """Generate text summary of results"""
        report = []
        report.append("\n" + "="*70)
        report.append("📊 MULTI-MODEL GPU BENCHMARK RESULTS SUMMARY")
        report.append("="*70)
        
        # GGUF Summary
        if self.results['gguf']:
            report.append("\n📦 GGUF MODELS RESULTS")
            report.append("-" * 70)
            report.append(f"{'Model':<45} {'Mean TPS':>12} {'Std Dev':>10}")
            report.append("-" * 70)
            for result in sorted(self.results['gguf'], key=lambda x: x['mean_tps'], reverse=True):
                report.append(f"{result['model']:<45} {result['mean_tps']:>12.2f} {result['std_tps']:>10.2f}")
            
            best_gguf = max(self.results['gguf'], key=lambda x: x['mean_tps'])
            report.append(f"\n🏆 Fastest GGUF: {best_gguf['model']} ({best_gguf['mean_tps']:.2f} TPS)")
        
        # Overall Summary
        report.append("\n" + "="*70)
        report.append("📈 OVERALL SUMMARY")
        report.append("="*70)
        report.append(f"GGUF Models Tested: {len(self.results['gguf'])}")
        report.append(f"GPU Backend: RawrXD-ModelLoader (Vulkan)")
        report.append(f"Benchmark Tool: gpu_inference_benchmark.exe")
        
        report.append("\n" + "="*70)
        
        return "\n".join(report)


def main():
    print("\n🚀 Initializing Multi-Model GPU Benchmark Harness...")
    
    benchmark = MultiModelBenchmark()
    benchmark.print_discovery_summary()
    
    # Run tests
    print("\n" + "="*70)
    print("TESTING PHASES")
    print("="*70)
    
    # Phase A: GGUF Testing with GPU Backend
    print("\n[PHASE A] GGUF Direct Testing with GPU Backend")
    benchmark.test_gguf_batch(batch_size=1, repetitions=3)
    
    # Export results
    print("\n[EXPORT] Exporting results...")
    benchmark.export_results_csv()
    
    # Generate summary
    summary = benchmark.generate_summary_report()
    print(summary)
    
    # Save summary report
    report_file = benchmark.results_dir / "TESTING_SUMMARY_REPORT.txt"
    with open(report_file, 'w') as f:
        f.write(summary)
    print(f"\n✅ Summary report saved to: {report_file}")
    
    print("\n" + "="*70)
    print("✅ MULTI-MODEL TESTING COMPLETE")
    print("="*70)
    print(f"\nResults directory: {benchmark.results_dir}")
    print("Next steps:")
    print("  1. Review CSV results")
    print("  2. Generate performance comparison graphs")
    print("  3. Commit results to GitHub")


if __name__ == '__main__':
    main()
