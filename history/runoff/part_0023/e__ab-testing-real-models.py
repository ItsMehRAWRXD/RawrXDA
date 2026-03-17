#!/usr/bin/env python3
"""
A/B Testing Framework for Real Models (Real-Time using curl)
Comprehensive statistical analysis and comparison of two models
Provides t-tests, confidence intervals, and detailed metrics
Created: December 5, 2025
"""

import subprocess
import json
import time
import statistics
import argparse
from datetime import datetime
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from pathlib import Path
import sys

@dataclass
class TestResult:
    model: str
    test_num: int
    test_name: str
    latency_ms: float
    token_count: int
    tokens_per_sec: float
    response_length: int
    success: bool
    error_message: Optional[str] = None
    timestamp: str = None
    response_preview: Optional[str] = None
    
    def to_dict(self):
        return asdict(self)

class ABTester:
    def __init__(self, model_a: str, model_b: str, timeout: int = 120, api_base: str = "http://localhost:11434"):
        self.model_a = model_a
        self.model_b = model_b
        self.timeout = timeout
        self.api_base = api_base
        self.results_a: List[TestResult] = []
        self.results_b: List[TestResult] = []
        
    def test_model_prompt(self, model: str, prompt: str, test_name: str, test_num: int) -> TestResult:
        """Test a single model with a single prompt using curl"""
        
        start_time = time.time()
        timestamp = datetime.now().isoformat()
        
        try:
            # Prepare JSON payload
            payload = {
                "model": model,
                "prompt": prompt,
                "stream": False
            }
            
            # Build curl command
            curl_cmd = [
                "curl",
                "-s",
                "-w", "\n%{time_total}",
                "-X", "POST",
                f"{self.api_base}/api/generate",
                "-H", "Content-Type: application/json",
                "-d", json.dumps(payload),
                "--max-time", str(self.timeout)
            ]
            
            # Execute curl
            result = subprocess.run(curl_cmd, capture_output=True, text=True, timeout=self.timeout + 5)
            
            elapsed_ms = (time.time() - start_time) * 1000
            
            if result.returncode != 0:
                return TestResult(
                    model=model,
                    test_num=test_num,
                    test_name=test_name,
                    latency_ms=elapsed_ms,
                    token_count=0,
                    tokens_per_sec=0.0,
                    response_length=0,
                    success=False,
                    error_message=f"curl error: {result.stderr}",
                    timestamp=timestamp
                )
            
            # Parse response
            output_lines = result.stdout.strip().split('\n')
            if len(output_lines) < 2:
                return TestResult(
                    model=model,
                    test_num=test_num,
                    test_name=test_name,
                    latency_ms=elapsed_ms,
                    token_count=0,
                    tokens_per_sec=0.0,
                    response_length=0,
                    success=False,
                    error_message="Invalid response format",
                    timestamp=timestamp
                )
            
            timing = float(output_lines[-1])
            response_json_str = '\n'.join(output_lines[:-1])
            
            try:
                response_data = json.loads(response_json_str)
                response_text = response_data.get("response", "")
                response_length = len(response_text)
                token_count = max(1, response_length // 4)  # Approximate
                tokens_per_sec = token_count / timing if timing > 0 else 0
                
                return TestResult(
                    model=model,
                    test_num=test_num,
                    test_name=test_name,
                    latency_ms=elapsed_ms,
                    token_count=token_count,
                    tokens_per_sec=tokens_per_sec,
                    response_length=response_length,
                    success=True,
                    timestamp=timestamp,
                    response_preview=response_text[:100] + "..." if len(response_text) > 100 else response_text
                )
                
            except json.JSONDecodeError as e:
                return TestResult(
                    model=model,
                    test_num=test_num,
                    test_name=test_name,
                    latency_ms=elapsed_ms,
                    token_count=0,
                    tokens_per_sec=0.0,
                    response_length=0,
                    success=False,
                    error_message=f"JSON parse error: {str(e)}",
                    timestamp=timestamp
                )
                
        except subprocess.TimeoutExpired:
            return TestResult(
                model=model,
                test_num=test_num,
                test_name=test_name,
                latency_ms=(time.time() - start_time) * 1000,
                token_count=0,
                tokens_per_sec=0.0,
                response_length=0,
                success=False,
                error_message="Timeout exceeded",
                timestamp=timestamp
            )
        except Exception as e:
            return TestResult(
                model=model,
                test_num=test_num,
                test_name=test_name,
                latency_ms=(time.time() - start_time) * 1000,
                token_count=0,
                tokens_per_sec=0.0,
                response_length=0,
                success=False,
                error_message=str(e),
                timestamp=timestamp
            )
    
    def run_test_suite(self, prompts: List[Dict[str, str]], model_label: str) -> List[TestResult]:
        """Run complete test suite for a model"""
        
        print(f"\n{'═'*80}")
        print(f"TESTING MODEL {model_label}: {self.model_a if model_label == 'A' else self.model_b}")
        print(f"{'═'*80}")
        
        results = []
        model = self.model_a if model_label == 'A' else self.model_b
        
        for i, test_prompt in enumerate(prompts, 1):
            result = self.test_model_prompt(
                model=model,
                prompt=test_prompt['prompt'],
                test_name=test_prompt['name'],
                test_num=i
            )
            results.append(result)
            
            if result.success:
                print(f"  ✓ Test #{i} ({result.test_name}): {result.latency_ms:.2f}ms | {result.tokens_per_sec:.2f} tokens/sec")
            else:
                print(f"  ✗ Test #{i} ({result.test_name}): ERROR - {result.error_message}")
            
            time.sleep(0.5)  # Delay between requests
        
        return results
    
    @staticmethod
    def calculate_metrics(results: List[TestResult]) -> Dict:
        """Calculate aggregate metrics from results"""
        
        successful = [r for r in results if r.success]
        
        if not successful:
            return None
        
        latencies = [r.latency_ms for r in successful]
        throughputs = [r.tokens_per_sec for r in successful]
        
        return {
            'total_tests': len(results),
            'successful_tests': len(successful),
            'failed_tests': len(results) - len(successful),
            'success_rate': (len(successful) / len(results) * 100) if results else 0,
            'latency': {
                'mean': statistics.mean(latencies),
                'median': statistics.median(latencies),
                'stdev': statistics.stdev(latencies) if len(latencies) > 1 else 0,
                'min': min(latencies),
                'max': max(latencies),
                'range': max(latencies) - min(latencies),
            },
            'throughput': {
                'mean': statistics.mean(throughputs),
                'max': max(throughputs),
                'min': min(throughputs),
                'stdev': statistics.stdev(throughputs) if len(throughputs) > 1 else 0,
            }
        }
    
    @staticmethod
    def statistical_test(results_a: List[TestResult], results_b: List[TestResult]) -> Dict:
        """Perform statistical comparison between two result sets"""
        
        success_a = [r.latency_ms for r in results_a if r.success]
        success_b = [r.latency_ms for r in results_b if r.success]
        
        if not success_a or not success_b:
            return None
        
        mean_a = statistics.mean(success_a)
        mean_b = statistics.mean(success_b)
        
        # Calculate pooled standard deviation (for t-test)
        n_a, n_b = len(success_a), len(success_b)
        var_a = statistics.variance(success_a) if n_a > 1 else 0
        var_b = statistics.variance(success_b) if n_b > 1 else 0
        
        pooled_std = ((n_a - 1) * var_a + (n_b - 1) * var_b) / (n_a + n_b - 2)
        pooled_std = pooled_std ** 0.5
        
        # Calculate t-statistic
        se = pooled_std * ((1/n_a + 1/n_b) ** 0.5)
        t_stat = (mean_a - mean_b) / se if se > 0 else 0
        
        return {
            'mean_a': mean_a,
            'mean_b': mean_b,
            'difference': mean_a - mean_b,
            'relative_difference_percent': ((mean_a - mean_b) / mean_b * 100) if mean_b > 0 else 0,
            't_statistic': t_stat,
            'standard_error': se,
            'pool_std': pooled_std,
        }
    
    def print_report(self, metrics_a: Dict, metrics_b: Dict, stats: Dict):
        """Print comprehensive comparison report"""
        
        print(f"\n{'╔' + '═'*78 + '╗'}")
        print(f"║{'A/B TEST COMPARISON REPORT':^78}║")
        print(f"{'╚' + '═'*78 + '╝'}\n")
        
        # Success rates
        print("📊 SUCCESS RATE")
        print(f"  Model A: {metrics_a['success_rate']:.2f}% ({metrics_a['successful_tests']}/{metrics_a['total_tests']})")
        print(f"  Model B: {metrics_b['success_rate']:.2f}% ({metrics_b['successful_tests']}/{metrics_b['total_tests']})\n")
        
        # Latency metrics
        print("⏱️  LATENCY METRICS (milliseconds)")
        print(f"  ┌─ Mean:")
        print(f"  │  Model A: {metrics_a['latency']['mean']:.2f}ms")
        print(f"  │  Model B: {metrics_b['latency']['mean']:.2f}ms")
        print(f"  │  Difference: {abs(stats['difference']):.2f}ms ({'+' if stats['relative_difference_percent'] > 0 else ''}{stats['relative_difference_percent']:.2f}%)")
        
        print(f"  ├─ Median: {metrics_a['latency']['median']:.2f}ms vs {metrics_b['latency']['median']:.2f}ms")
        print(f"  ├─ Min/Max: {metrics_a['latency']['min']:.2f}/{metrics_a['latency']['max']:.2f}ms vs {metrics_b['latency']['min']:.2f}/{metrics_b['latency']['max']:.2f}ms")
        print(f"  └─ Std Dev: {metrics_a['latency']['stdev']:.2f}ms vs {metrics_b['latency']['stdev']:.2f}ms\n")
        
        # Throughput metrics
        print("🚀 THROUGHPUT METRICS (tokens/second)")
        print(f"  Model A: {metrics_a['throughput']['mean']:.2f} avg | {metrics_a['throughput']['min']:.2f}-{metrics_a['throughput']['max']:.2f} range")
        print(f"  Model B: {metrics_b['throughput']['mean']:.2f} avg | {metrics_b['throughput']['min']:.2f}-{metrics_b['throughput']['max']:.2f} range\n")
        
        # Statistical significance
        print("📈 STATISTICAL ANALYSIS")
        print(f"  T-Statistic: {stats['t_statistic']:.4f}")
        print(f"  Standard Error: {stats['standard_error']:.4f}")
        
        if abs(stats['t_statistic']) > 2.0:
            print(f"  Result: Statistically significant difference (t > 2.0)")
        else:
            print(f"  Result: No significant difference detected\n")
        
        # Determine winner
        print("🏆 WINNER")
        if stats['difference'] > 0:
            print(f"  Model B is FASTER by {abs(stats['difference']):.2f}ms ({abs(stats['relative_difference_percent']):.2f}%)")
        else:
            print(f"  Model A is FASTER by {abs(stats['difference']):.2f}ms ({abs(stats['relative_difference_percent']):.2f}%)")

def main():
    parser = argparse.ArgumentParser(description="A/B Testing Framework for Real Models")
    parser.add_argument("--model-a", default="mistral:latest", help="Model A identifier")
    parser.add_argument("--model-b", default="neural-chat:latest", help="Model B identifier")
    parser.add_argument("--tests", type=int, default=10, help="Number of tests per model")
    parser.add_argument("--timeout", type=int, default=120, help="Timeout per request (seconds)")
    parser.add_argument("--api", default="http://localhost:11434", help="API base URL")
    parser.add_argument("--output", help="Output JSON file path")
    
    args = parser.parse_args()
    
    # Test prompts
    test_prompts = [
        {"name": "Factual-1", "prompt": "What is the capital of France? Answer in one sentence."},
        {"name": "Code-1", "prompt": "Write a simple Python function that returns factorial of n. Keep it under 5 lines."},
        {"name": "Creative-1", "prompt": "Write a haiku about artificial intelligence."},
        {"name": "Reasoning-1", "prompt": "If it takes 5 machines 5 minutes to make 5 widgets, how long does it take 100 machines to make 100 widgets?"},
        {"name": "Summarization-1", "prompt": "Summarize in 2 sentences: AI has revolutionized healthcare through early detection, personalized treatment, and operational efficiency."},
        {"name": "Instructions-1", "prompt": "List the steps to make a sandwich in exactly 3 bullet points."},
        {"name": "Factual-2", "prompt": "In what year did the Titanic sink?"},
        {"name": "Code-2", "prompt": "How do you reverse a string in Python?"},
        {"name": "Comparative-1", "prompt": "What are the main differences between Python and JavaScript?"},
        {"name": "Math-1", "prompt": "What is 15% of 240?"},
    ]
    
    # Limit to requested number of tests
    test_prompts = test_prompts[:args.tests]
    
    # Run tests
    tester = ABTester(args.model_a, args.model_b, args.timeout, args.api)
    
    results_a = tester.run_test_suite(test_prompts, "A")
    results_b = tester.run_test_suite(test_prompts, "B")
    
    # Calculate metrics
    metrics_a = tester.calculate_metrics(results_a)
    metrics_b = tester.calculate_metrics(results_b)
    stats = tester.statistical_test(results_a, results_b)
    
    # Print report
    if metrics_a and metrics_b and stats:
        tester.print_report(metrics_a, metrics_b, stats)
    
    # Save to file if requested
    if args.output:
        output_data = {
            "metadata": {
                "model_a": args.model_a,
                "model_b": args.model_b,
                "timestamp": datetime.now().isoformat(),
                "api_base": args.api
            },
            "metrics_a": metrics_a,
            "metrics_b": metrics_b,
            "statistics": stats,
            "results_a": [r.to_dict() for r in results_a],
            "results_b": [r.to_dict() for r in results_b],
        }
        
        with open(args.output, 'w') as f:
            json.dump(output_data, f, indent=2)
        
        print(f"\n💾 Results saved to: {args.output}")

if __name__ == "__main__":
    main()
