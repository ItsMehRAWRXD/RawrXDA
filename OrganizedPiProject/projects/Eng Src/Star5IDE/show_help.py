#!/usr/bin/env python3
"""
Show help information for the Modern RFI Scanner
"""

def show_help():
    print("""
Modern RFI/LFI Scanner - Educational Security Tool
==================================================

USAGE:
  python3 modern_rfi_scanner.py [OPTIONS]

OPTIONS:
  --targets FILE          File containing target URLs (one per line)
  --url URL              Single target URL to scan
  --type {rfi,lfi}       Type of vulnerability to scan for (default: rfi)
  --concurrent N          Maximum concurrent connections (default: 10)
  --timeout N            Request timeout in seconds (default: 10)
  --output FILE          Output file for results (default: scan_results.json)
  --verbose, -v          Enable verbose output
  --help, -h             Show this help message

EXAMPLES:
  # Scan a single URL for RFI vulnerabilities
  python3 modern_rfi_scanner.py --url "http://example.com/page.php?id=" --type rfi

  # Scan a single URL for LFI vulnerabilities  
  python3 modern_rfi_scanner.py --url "http://example.com/page.php?file=" --type lfi

  # Scan multiple targets from a file
  python3 modern_rfi_scanner.py --targets targets.txt --type rfi

  # Scan with custom settings
  python3 modern_rfi_scanner.py \\
      --targets targets.txt \\
      --type rfi \\
      --concurrent 20 \\
      --timeout 15 \\
      --output results.json \\
      --verbose

INPUT FILE FORMAT:
  Create a text file with target URLs, one per line:
  
  http://example1.com/page.php?id=
  http://example2.com/index.php?page=
  http://example3.com/view.php?file=

OUTPUT:
  The scanner generates:
  1. JSON Report (scan_results.json) - Machine-readable format
  2. Console Output - Human-readable summary

WARNING: This tool is for EDUCATIONAL PURPOSES ONLY. Only use on systems
you own or have explicit permission to test. Unauthorized scanning is illegal.

SETUP:
  To install dependencies:
  pip3 install aiohttp

  Or use the setup script:
  ./setup.sh
""")

if __name__ == "__main__":
    show_help()