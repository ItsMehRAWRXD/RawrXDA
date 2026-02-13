#!/usr/bin/env python3
"""
RawrXD Benchmark Suite Runner
Complete utility to discover, build, and run all benchmarks
"""

import os
import subprocess
import sys
import json
import time
from pathlib import Path
from datetime import datetime

class BenchmarkRunner:
    def __init__(self, base_dir=".", build_dir="build"):
        self.base_dir = Path(base_dir)
        self.build_dir = self.base_dir / build_dir
        self.bin_dir = self.build_dir / "bin" / "Release"
        self.results = {}
        self.start_time = None
        self.end_time = None
        
    def find_benchmark_executables(self):
        """Discover all benchmark executables"""
        benchmarks = []
        
        if not self.bin_dir.exists():
            print(f"❌ Build directory not found: {self.bin_dir}")
            return benchmarks
        
        for exe in self.bin_dir.glob("*benchmark*.exe"):
            benchmarks.append(exe)
        
        for exe in self.bin_dir.glob("*bench*.exe"):
            if "benchmark" not in exe.name:
                benchmarks.append(exe)
        
        return sorted(benchmarks)
    
    def run_benchmark(self, exe_path, model_path=None, args=None):
        """Run a single benchmark executable"""
        exe_name = exe_path.name
        print(f"\n{'='*70}")
        print(f"Running: {exe_name}")
        print(f"{'='*70}")
        
        cmd = [str(exe_path)]
        if model_path:
            cmd.extend(["-m", str(model_path)])
        if args:
            cmd.extend(args)
        
        try:
            start = time.time()
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            end = time.time()
            
            elapsed = end - start
            
            print(result.stdout)
            if result.stderr:
                print("STDERR:", result.stderr)
            
            return {
                "name": exe_name,
                "success": result.returncode == 0,
                "return_code": result.returncode,
                "elapsed_sec": elapsed,
                "stdout": result.stdout[:1000],  # First 1000 chars
                "timestamp": datetime.now().isoformat()
            }
            
        except subprocess.TimeoutExpired:
            print(f"❌ Timeout: {exe_name} exceeded 5 minutes")
            return {
                "name": exe_name,
                "success": False,
                "return_code": -1,
                "elapsed_sec": 300,
                "error": "Timeout",
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            print(f"❌ Error running {exe_name}: {e}")
            return {
                "name": exe_name,
                "success": False,
                "return_code": -1,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }
    
    def run_all(self, model_path=None):
        """Run all discovered benchmarks"""
        self.start_time = time.time()
        
        benchmarks = self.find_benchmark_executables()
        
        if not benchmarks:
            print("❌ No benchmark executables found!")
            print(f"   Searched in: {self.bin_dir}")
            return False
        
        print(f"\n✓ Found {len(benchmarks)} benchmark(s):\n")
        for i, bench in enumerate(benchmarks, 1):
            print(f"  {i}. {bench.name}")
        
        print(f"\n{'='*70}")
        print("STARTING BENCHMARK SUITE")
        print(f"{'='*70}")
        
        for bench in benchmarks:
            result = self.run_benchmark(bench, model_path)
            self.results[bench.name] = result
        
        self.end_time = time.time()
        self.print_summary()
        self.export_results()
        
        return all(r["success"] for r in self.results.values())
    
    def print_summary(self):
        """Print summary of all benchmark results"""
        elapsed = self.end_time - self.start_time if self.start_time else 0
        
        print(f"\n\n{'='*70}")
        print("BENCHMARK SUITE SUMMARY")
        print(f"{'='*70}\n")
        
        passed = sum(1 for r in self.results.values() if r["success"])
        total = len(self.results)
        
        print(f"Total Benchmarks:  {total}")
        print(f"Passed:            {passed}")
        print(f"Failed:            {total - passed}")
        print(f"Total Time:        {elapsed:.2f} seconds\n")
        
        print("Results:\n")
        print(f"{'Benchmark':<40} {'Status':<10} {'Time (s)':<10}")
        print(f"{'-'*70}")
        
        for name, result in self.results.items():
            status = "✅ PASS" if result["success"] else "❌ FAIL"
            elapsed_sec = result.get("elapsed_sec", 0)
            print(f"{name:<40} {status:<10} {elapsed_sec:>7.2f}s")
        
        print(f"{'-'*70}\n")
        
        if passed == total:
            print("🎉 ALL BENCHMARKS PASSED!")
        elif passed >= total * 0.75:
            print("⚠ MOST BENCHMARKS PASSED - SOME ISSUES")
        else:
            print("❌ MULTIPLE BENCHMARK FAILURES")
    
    def export_results(self):
        """Export results to JSON"""
        output_file = self.base_dir / "benchmark_suite_results.json"
        
        try:
            with open(output_file, "w") as f:
                json.dump({
                    "timestamp": datetime.now().isoformat(),
                    "total_time_sec": self.end_time - self.start_time if self.start_time else 0,
                    "benchmark_count": len(self.results),
                    "passed": sum(1 for r in self.results.values() if r["success"]),
                    "results": self.results
                }, f, indent=2)
            
            print(f"✓ Results exported to: {output_file}")
        except Exception as e:
            print(f"✗ Failed to export results: {e}")

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="RawrXD Benchmark Suite Runner"
    )
    parser.add_argument(
        "-m", "--model",
        help="Path to GGUF model file",
        default="models/ministral-3b-instruct-v0.3-Q4_K_M.gguf"
    )
    parser.add_argument(
        "-d", "--directory",
        help="RawrXD-ModelLoader directory",
        default="."
    )
    parser.add_argument(
        "-b", "--build-dir",
        help="Build directory",
        default="build"
    )
    parser.add_argument(
        "--list",
        help="List available benchmarks and exit",
        action="store_true"
    )
    parser.add_argument(
        "-v", "--verbose",
        help="Verbose output",
        action="store_true"
    )
    
    args = parser.parse_args()
    
    runner = BenchmarkRunner(args.directory, args.build_dir)
    
    if args.list:
        benchmarks = runner.find_benchmark_executables()
        if benchmarks:
            print("Available benchmarks:\n")
            for bench in benchmarks:
                print(f"  • {bench.name}")
        else:
            print("No benchmarks found")
        return 0
    
    print("\n" + "="*70)
    print("RawrXD BENCHMARK SUITE RUNNER")
    print("="*70)
    print(f"Model: {args.model}")
    print(f"Build: {runner.build_dir}")
    print("="*70 + "\n")
    
    success = runner.run_all(args.model)
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
