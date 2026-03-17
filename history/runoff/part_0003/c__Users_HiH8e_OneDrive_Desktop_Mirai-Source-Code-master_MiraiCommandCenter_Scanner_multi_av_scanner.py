"""
Multi-AV Scanner Engine
Similar to KleenScan - Scan files with multiple antivirus engines
"""

import os
import sys
import json
import subprocess
import hashlib
import time
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass
from enum import Enum

class ScanStatus(Enum):
    PENDING = "pending"
    SCANNING = "scanning"
    OK = "ok"
    TIMEOUT = "timeout"
    FAILED = "failed"
    INCOMPLETE = "incomplete"

@dataclass
class ScanResult:
    av_name: str
    status: ScanStatus
    detection: Optional[str]
    scan_time: float
    error_message: Optional[str] = None

class AntivirusEngine:
    """Base class for antivirus engine integration"""
    
    def __init__(self, name: str, path: str, timeout: int = 30):
        self.name = name
        self.path = path
        self.timeout = timeout
        self.available = self.check_availability()
    
    def check_availability(self) -> bool:
        """Check if AV engine is installed and accessible"""
        return os.path.exists(self.path)
    
    def scan(self, file_path: str) -> ScanResult:
        """Scan file with this AV engine"""
        raise NotImplementedError("Subclasses must implement scan()")
    
    def parse_output(self, output: str) -> Optional[str]:
        """Parse AV output to extract detection name"""
        raise NotImplementedError("Subclasses must implement parse_output()")

class WindowsDefenderEngine(AntivirusEngine):
    """Windows Defender integration"""
    
    def __init__(self):
        super().__init__(
            name="Windows Defender",
            path=r"C:\Program Files\Windows Defender\MpCmdRun.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "-Scan", "-ScanType", "3", "-File", file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            detection = self.parse_output(result.stdout)
            
            if result.returncode == 0:
                return ScanResult(
                    av_name=self.name,
                    status=ScanStatus.OK,
                    detection=detection,
                    scan_time=scan_time
                )
            else:
                return ScanResult(
                    av_name=self.name,
                    status=ScanStatus.OK,
                    detection=detection if detection else None,
                    scan_time=scan_time
                )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout,
                error_message="Scan timeout"
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.FAILED,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, output: str) -> Optional[str]:
        """Extract threat name from Windows Defender output"""
        for line in output.split('\n'):
            if "Threat" in line or "found" in line.lower():
                parts = line.split(':')
                if len(parts) > 1:
                    return parts[1].strip()
        return None

class ClamAVEngine(AntivirusEngine):
    """ClamAV integration"""
    
    def __init__(self, clamscan_path: str = "clamscan"):
        super().__init__(
            name="ClamAV",
            path=clamscan_path
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "--no-summary", file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            detection = self.parse_output(result.stdout)
            
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.OK,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout,
                error_message="Scan timeout"
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.FAILED,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, output: str) -> Optional[str]:
        """Extract virus name from ClamAV output"""
        for line in output.split('\n'):
            if "FOUND" in line:
                parts = line.split(':')
                if len(parts) >= 2:
                    return parts[1].replace("FOUND", "").strip()
        return None

class MultiAVScanner:
    """Multi-engine antivirus scanner"""
    
    def __init__(self):
        self.engines: List[AntivirusEngine] = []
        self.scan_history: List[Dict] = []
        self.initialize_engines()
    
    def initialize_engines(self):
        """Initialize available AV engines"""
        
        # Add Windows Defender
        defender = WindowsDefenderEngine()
        if defender.available:
            self.engines.append(defender)
            print(f"[+] Loaded: {defender.name}")
        
        # Add ClamAV
        clamav = ClamAVEngine()
        if clamav.available:
            self.engines.append(clamav)
            print(f"[+] Loaded: {clamav.name}")
        
        # Add more engines here as needed
        # self.engines.append(AviraEngine())
        # self.engines.append(KasperskyEngine())
        
        print(f"\n[*] Total engines loaded: {len(self.engines)}")
    
    def scan_file(self, file_path: str, av_list: Optional[List[str]] = None) -> Dict:
        """
        Scan file with multiple AV engines
        
        Args:
            file_path: Path to file to scan
            av_list: List of specific AV names to use (None = use all)
        
        Returns:
            Dictionary with scan results
        """
        
        if not os.path.exists(file_path):
            return {
                "error": "File not found",
                "file_path": file_path
            }
        
        # Calculate file hash
        file_hash = self.calculate_hash(file_path)
        file_size = os.path.getsize(file_path)
        
        results = {
            "file_path": file_path,
            "file_name": os.path.basename(file_path),
            "file_size": file_size,
            "md5": file_hash['md5'],
            "sha256": file_hash['sha256'],
            "scan_time": time.strftime("%Y-%m-%d %H:%M:%S"),
            "engines_used": [],
            "detections": [],
            "results": []
        }
        
        # Filter engines if specific list provided
        engines_to_use = self.engines
        if av_list:
            engines_to_use = [e for e in self.engines if e.name.lower() in [av.lower() for av in av_list]]
        
        print(f"\n[*] Scanning: {os.path.basename(file_path)}")
        print(f"[*] Size: {file_size} bytes")
        print(f"[*] MD5: {file_hash['md5']}")
        print(f"[*] SHA256: {file_hash['sha256']}")
        print(f"[*] Engines: {len(engines_to_use)}\n")
        
        # Scan with each engine
        for engine in engines_to_use:
            print(f"[*] Scanning with {engine.name}...", end=" ")
            
            scan_result = engine.scan(file_path)
            
            results["engines_used"].append(engine.name)
            results["results"].append({
                "av_name": scan_result.av_name,
                "status": scan_result.status.value,
                "detection": scan_result.detection,
                "scan_time": round(scan_result.scan_time, 2),
                "error": scan_result.error_message
            })
            
            if scan_result.detection:
                results["detections"].append({
                    "av_name": scan_result.av_name,
                    "detection": scan_result.detection
                })
                print(f"⚠️  DETECTED: {scan_result.detection}")
            else:
                if scan_result.status == ScanStatus.OK:
                    print("✅ Clean")
                elif scan_result.status == ScanStatus.TIMEOUT:
                    print("⏱️  Timeout")
                elif scan_result.status == ScanStatus.FAILED:
                    print(f"❌ Failed: {scan_result.error_message}")
        
        # Summary
        detection_count = len(results["detections"])
        total_engines = len(engines_to_use)
        
        results["summary"] = {
            "total_engines": total_engines,
            "detections": detection_count,
            "clean": total_engines - detection_count,
            "detection_rate": f"{(detection_count/total_engines*100):.1f}%"
        }
        
        print(f"\n[*] Summary: {detection_count}/{total_engines} detections ({results['summary']['detection_rate']})")
        
        # Save to history
        self.scan_history.append(results)
        
        return results
    
    def calculate_hash(self, file_path: str) -> Dict[str, str]:
        """Calculate MD5 and SHA256 hashes"""
        md5 = hashlib.md5()
        sha256 = hashlib.sha256()
        
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b''):
                md5.update(chunk)
                sha256.update(chunk)
        
        return {
            "md5": md5.hexdigest(),
            "sha256": sha256.hexdigest()
        }
    
    def export_results(self, output_file: str = "scan_results.json"):
        """Export scan history to JSON"""
        with open(output_file, 'w') as f:
            json.dump(self.scan_history, f, indent=2)
        print(f"\n[*] Results exported to: {output_file}")
    
    def get_available_engines(self) -> List[str]:
        """Get list of available engine names"""
        return [engine.name for engine in self.engines]

def main():
    """Main function for CLI usage"""
    
    print("=" * 60)
    print("Multi-AV Scanner - Similar to KleenScan")
    print("=" * 60)
    
    scanner = MultiAVScanner()
    
    if len(sys.argv) < 2:
        print("\nUsage:")
        print(f"  python {sys.argv[0]} <file_to_scan>")
        print(f"  python {sys.argv[0]} <file_to_scan> --engines defender,clamav")
        print("\nAvailable engines:")
        for engine in scanner.get_available_engines():
            print(f"  - {engine}")
        sys.exit(1)
    
    file_path = sys.argv[1]
    
    # Check for specific engines
    av_list = None
    if "--engines" in sys.argv:
        idx = sys.argv.index("--engines")
        if idx + 1 < len(sys.argv):
            av_list = sys.argv[idx + 1].split(',')
    
    # Scan file
    results = scanner.scan_file(file_path, av_list)
    
    # Export results
    scanner.export_results()
    
    # Print detection summary
    if results.get("detections"):
        print("\n[!] DETECTIONS FOUND:")
        for detection in results["detections"]:
            print(f"    {detection['av_name']}: {detection['detection']}")
    else:
        print("\n[✓] No detections - file appears clean")

if __name__ == "__main__":
    main()
