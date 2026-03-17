"""
Additional Antivirus Engine Integrations
Extend multi_av_scanner.py with more engines
"""

import subprocess
import re
import time
from multi_av_scanner import AntivirusEngine, ScanResult, ScanStatus
from typing import Optional

class AviraEngine(AntivirusEngine):
    """Avira Antivirus integration"""
    
    def __init__(self):
        super().__init__(
            name="Avira",
            path=r"C:\Program Files (x86)\Avira\Antivirus\avscan.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, f"/PATH={file_path}", "/HEURLEVEL=2", "/SCAN"],
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
        match = re.search(r'ALERT:\s*\[([^\]]+)\]', output)
        if match:
            return match.group(1)
        return None

class KasperskyEngine(AntivirusEngine):
    """Kaspersky Antivirus integration"""
    
    def __init__(self):
        super().__init__(
            name="Kaspersky",
            path=r"C:\Program Files (x86)\Kaspersky Lab\Kaspersky\avp.com"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "SCAN", file_path],
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
        for line in output.split('\n'):
            if "detected" in line.lower():
                match = re.search(r'detected:\s*(.+)', line, re.IGNORECASE)
                if match:
                    return match.group(1).strip()
        return None

class BitdefenderEngine(AntivirusEngine):
    """Bitdefender Antivirus integration"""
    
    def __init__(self):
        super().__init__(
            name="Bitdefender",
            path=r"C:\Program Files\Bitdefender\Bitdefender Security\bdss.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "scan", file_path],
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
        match = re.search(r'infected with:\s*(.+)', output, re.IGNORECASE)
        if match:
            return match.group(1).strip()
        return None

class ESETEngine(AntivirusEngine):
    """ESET NOD32 Antivirus integration"""
    
    def __init__(self):
        super().__init__(
            name="ESET NOD32",
            path=r"C:\Program Files\ESET\ESET Security\ecls.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, file_path, "/no-quarantine"],
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
        match = re.search(r'name="([^"]+)"', output)
        if match:
            return match.group(1)
        return None

class MalwarebytesEngine(AntivirusEngine):
    """Malwarebytes integration"""
    
    def __init__(self):
        super().__init__(
            name="Malwarebytes",
            path=r"C:\Program Files\Malwarebytes\Anti-Malware\mbamcmd.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "/scan", f"/path={file_path}"],
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
        for line in output.split('\n'):
            if "Malware" in line or "PUP" in line or "Trojan" in line:
                parts = line.split(',')
                if len(parts) > 1:
                    return parts[1].strip()
        return None

class SophosEngine(AntivirusEngine):
    """Sophos Antivirus integration"""
    
    def __init__(self):
        super().__init__(
            name="Sophos",
            path=r"C:\Program Files\Sophos\Sophos Anti-Virus\sav32cli.exe"
        )
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [self.path, "-ss", "-nc", "-nb", file_path],
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
        match = re.search(r'>>> Virus \'([^\']+)\' found', output)
        if match:
            return match.group(1)
        return None

# Register all engines
AVAILABLE_ENGINES = {
    "avira": AviraEngine,
    "kaspersky": KasperskyEngine,
    "bitdefender": BitdefenderEngine,
    "eset": ESETEngine,
    "malwarebytes": MalwarebytesEngine,
    "sophos": SophosEngine
}

def get_engine_by_name(name: str) -> Optional[AntivirusEngine]:
    """Get engine instance by name"""
    engine_class = AVAILABLE_ENGINES.get(name.lower())
    if engine_class:
        return engine_class()
    return None
