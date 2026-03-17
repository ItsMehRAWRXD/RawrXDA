"""
Multi-AV Scanner Python Client Library
Simple Python library for interacting with the scanner API
"""

import requests
import json
import time
from typing import Dict, List, Optional
from pathlib import Path


class ScannerClient:
    """Client for Multi-AV Scanner API"""
    
    def __init__(self, api_url: str = "http://localhost:5000", api_token: Optional[str] = None):
        """
        Initialize scanner client
        
        Args:
            api_url: Base URL of the scanner API
            api_token: API authentication token
        """
        self.api_url = api_url.rstrip('/')
        self.api_token = api_token
        self.session = requests.Session()
        
        if api_token:
            self.session.headers.update({'X-Auth-Token': api_token})
    
    def generate_token(self, user_id: str = "default") -> str:
        """
        Generate new API token
        
        Args:
            user_id: User identifier
            
        Returns:
            Generated API token
        """
        response = self.session.post(
            f"{self.api_url}/api/v1/token/generate",
            json={"user_id": user_id}
        )
        response.raise_for_status()
        
        data = response.json()
        if data.get('success'):
            self.api_token = data['token']
            self.session.headers.update({'X-Auth-Token': self.api_token})
            return self.api_token
        else:
            raise Exception(f"Failed to generate token: {data.get('error')}")
    
    def get_av_list(self) -> Dict:
        """
        Get list of available AV engines
        
        Returns:
            Dictionary with available engines
        """
        response = self.session.get(f"{self.api_url}/api/v1/get/avlist")
        response.raise_for_status()
        
        data = response.json()
        if data.get('success'):
            return data['data']
        else:
            raise Exception(f"Failed to get AV list: {data.get('error')}")
    
    def scan_file(self, file_path: str, av_list: Optional[List[str]] = None, wait: bool = True) -> Dict:
        """
        Scan a file with multiple AV engines
        
        Args:
            file_path: Path to file to scan
            av_list: List of AV engine names (None = all engines)
            wait: Wait for scan to complete before returning
            
        Returns:
            Scan results dictionary
        """
        if not self.api_token:
            raise Exception("API token not configured. Call generate_token() first.")
        
        file_path = Path(file_path)
        if not file_path.exists():
            raise FileNotFoundError(f"File not found: {file_path}")
        
        # Prepare request
        files = {'file': open(file_path, 'rb')}
        data = {'avList': ','.join(av_list) if av_list else 'all'}
        
        # Upload file
        response = self.session.post(
            f"{self.api_url}/api/v1/file/scan",
            files=files,
            data=data
        )
        response.raise_for_status()
        
        result = response.json()
        if not result.get('success'):
            raise Exception(f"Scan failed: {result.get('error')}")
        
        scan_token = result['scan_token']
        
        if wait:
            return self.wait_for_results(scan_token)
        else:
            return {'scan_token': scan_token, 'status': 'pending'}
    
    def get_scan_status(self, scan_token: str) -> Dict:
        """
        Get scan status
        
        Args:
            scan_token: Scan token from scan_file()
            
        Returns:
            Status dictionary
        """
        response = self.session.get(f"{self.api_url}/api/v1/file/status/{scan_token}")
        response.raise_for_status()
        
        return response.json()
    
    def get_scan_results(self, scan_token: str) -> Dict:
        """
        Get scan results
        
        Args:
            scan_token: Scan token from scan_file()
            
        Returns:
            Results dictionary
        """
        response = self.session.get(f"{self.api_url}/api/v1/file/result/{scan_token}")
        response.raise_for_status()
        
        return response.json()
    
    def wait_for_results(self, scan_token: str, timeout: int = 300, poll_interval: int = 2) -> Dict:
        """
        Wait for scan to complete and return results
        
        Args:
            scan_token: Scan token from scan_file()
            timeout: Maximum time to wait in seconds
            poll_interval: Time between status checks in seconds
            
        Returns:
            Scan results
        """
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            status = self.get_scan_status(scan_token)
            
            if status.get('status') == 'completed':
                return self.get_scan_results(scan_token)
            
            time.sleep(poll_interval)
        
        raise TimeoutError(f"Scan did not complete within {timeout} seconds")
    
    def get_stats(self) -> Dict:
        """
        Get API statistics
        
        Returns:
            Statistics dictionary
        """
        if not self.api_token:
            raise Exception("API token not configured")
        
        response = self.session.get(f"{self.api_url}/api/v1/stats")
        response.raise_for_status()
        
        return response.json()
    
    def health_check(self) -> Dict:
        """
        Check API health
        
        Returns:
            Health status dictionary
        """
        response = self.session.get(f"{self.api_url}/api/v1/health")
        response.raise_for_status()
        
        return response.json()


# Example usage
if __name__ == "__main__":
    # Initialize client
    client = ScannerClient()
    
    # Generate token
    print("Generating API token...")
    token = client.generate_token()
    print(f"Token: {token}\n")
    
    # Get available engines
    print("Available AV engines:")
    av_list = client.get_av_list()
    for engine in av_list.get('file', []):
        print(f"  - {engine}")
    print()
    
    # Scan a file
    test_file = input("Enter path to file to scan (or press Enter to skip): ").strip()
    
    if test_file:
        print(f"\nScanning {test_file}...")
        try:
            results = client.scan_file(test_file)
            
            print("\n" + "=" * 60)
            print("SCAN RESULTS")
            print("=" * 60)
            print(f"File: {results['file_name']}")
            print(f"Hash: {results['file_hash']}")
            print(f"Scan Time: {results['scan_time']}")
            print()
            
            detection_count = 0
            for result in results['data']:
                status_symbol = "✓" if result['status'] == 'ok' else "✗"
                print(f"{status_symbol} {result['avname']:20s} - {result['flagname']}")
                if result['flagname'] != 'Clean':
                    detection_count += 1
            
            print()
            print(f"Detections: {detection_count}/{len(results['data'])}")
            
            if detection_count > 0:
                print("\n⚠️  WARNING: Threats detected!")
            else:
                print("\n✓ File appears to be clean")
                
        except Exception as e:
            print(f"Error scanning file: {e}")
    
    # Get stats
    try:
        stats = client.get_stats()
        print(f"\nAPI Statistics:")
        print(f"  Total scans: {stats.get('total_scans', 0)}")
        print(f"  Your scans: {stats.get('your_scans', 0)}")
        print(f"  Available engines: {stats.get('available_engines', 0)}")
    except Exception as e:
        print(f"Error getting stats: {e}")
