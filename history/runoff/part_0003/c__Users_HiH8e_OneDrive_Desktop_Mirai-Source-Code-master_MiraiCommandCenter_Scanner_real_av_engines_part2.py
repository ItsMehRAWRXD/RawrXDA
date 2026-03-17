"""
REAL Multi-AV Scanner Engine - Part 2
Additional real antivirus integrations
"""

import os
import subprocess
import time
import re
import shutil
import winreg
from real_av_engines_part1 import RealAntivirusEngine, ScanResult, ScanStatus
from typing import Tuple, Optional

class RealKasperskyEngine(RealAntivirusEngine):
    """REAL Kaspersky integration using avp.com"""
    
    def __init__(self):
        super().__init__("Kaspersky")
    
    def _detect_installation(self) -> bool:
        """Detect Kaspersky installation"""
        possible_paths = [
            r"C:\Program Files (x86)\Kaspersky Lab\Kaspersky\avp.com",
            r"C:\Program Files\Kaspersky Lab\Kaspersky\avp.com",
            r"C:\Program Files (x86)\Kaspersky Lab\Kaspersky Anti-Virus\avp.com",
            r"C:\Program Files\Kaspersky Lab\Kaspersky Anti-Virus\avp.com"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Kaspersky not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, "SCAN", file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Kaspersky output"""
        
        for line in stdout.split('\n'):
            if "detected" in line.lower():
                match = re.search(r'detected:\s*(.+)', line, re.IGNORECASE)
                if match:
                    return ScanStatus.INFECTED, match.group(1).strip()
                return ScanStatus.INFECTED, "Threat detected"
        
        if returncode == 0:
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealBitdefenderEngine(RealAntivirusEngine):
    """REAL Bitdefender integration using bdc.exe or bdss.exe"""
    
    def __init__(self):
        super().__init__("Bitdefender")
    
    def _detect_installation(self) -> bool:
        """Detect Bitdefender installation"""
        possible_paths = [
            r"C:\Program Files\Bitdefender\Bitdefender Security\bdss.exe",
            r"C:\Program Files\Bitdefender\Antivirus Free\bdc.exe",
            r"C:\Program Files\Bitdefender Antivirus Plus\bdss.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Bitdefender not installed"
            )
        
        try:
            # Different commands for different executables
            if "bdss.exe" in self.executable_path:
                cmd = [self.executable_path, "scan", file_path]
            else:
                cmd = [self.executable_path, "/scan", file_path]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Bitdefender output"""
        
        match = re.search(r'infected with:\s*(.+)', stdout, re.IGNORECASE)
        if match:
            return ScanStatus.INFECTED, match.group(1).strip()
        
        if "infected" in stdout.lower() or "malware" in stdout.lower():
            return ScanStatus.INFECTED, "Malware detected"
        
        if returncode == 0 or "clean" in stdout.lower():
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealESETEngine(RealAntivirusEngine):
    """REAL ESET NOD32 integration using ecls.exe"""
    
    def __init__(self):
        super().__init__("ESET NOD32")
    
    def _detect_installation(self) -> bool:
        """Detect ESET installation"""
        possible_paths = [
            r"C:\Program Files\ESET\ESET Security\ecls.exe",
            r"C:\Program Files (x86)\ESET\ESET Security\ecls.exe",
            r"C:\Program Files\ESET\ESET NOD32 Antivirus\ecls.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="ESET not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, file_path, "/no-quarantine", "/base-dir=C:\\ProgramData\\ESET\\ESET Security\\Modules"],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse ESET output"""
        
        # ESET uses XML-like output
        match = re.search(r'name="([^"]+)"', stdout)
        if match:
            return ScanStatus.INFECTED, match.group(1)
        
        # Alternative parsing
        for line in stdout.split('\n'):
            if "threat" in line.lower() or "virus" in line.lower():
                parts = line.split('=')
                if len(parts) > 1:
                    return ScanStatus.INFECTED, parts[1].strip()
        
        if returncode == 0:
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealMalwarebytesEngine(RealAntivirusEngine):
    """REAL Malwarebytes integration using mbamcmd.exe"""
    
    def __init__(self):
        super().__init__("Malwarebytes")
    
    def _detect_installation(self) -> bool:
        """Detect Malwarebytes installation"""
        possible_paths = [
            r"C:\Program Files\Malwarebytes\Anti-Malware\mbamcmd.exe",
            r"C:\Program Files (x86)\Malwarebytes\Anti-Malware\mbamcmd.exe",
            r"C:\Program Files\Malwarebytes' Anti-Malware\mbam.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Malwarebytes not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, "/scan", f"/path={file_path}"],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Malwarebytes output"""
        
        for line in stdout.split('\n'):
            if any(keyword in line for keyword in ["Malware", "PUP", "Trojan", "Adware"]):
                # Extract detection name
                parts = line.split(',')
                if len(parts) > 1:
                    return ScanStatus.INFECTED, parts[1].strip()
                return ScanStatus.INFECTED, "Malware detected"
        
        if "No threats detected" in stdout or returncode == 0:
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealSophosEngine(RealAntivirusEngine):
    """REAL Sophos integration using sav32cli.exe"""
    
    def __init__(self):
        super().__init__("Sophos")
    
    def _detect_installation(self) -> bool:
        """Detect Sophos installation"""
        possible_paths = [
            r"C:\Program Files (x86)\Sophos\Sophos Anti-Virus\sav32cli.exe",
            r"C:\Program Files\Sophos\Sophos Anti-Virus\sav32cli.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Sophos not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, "-ss", "-nc", "-nb", file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Sophos output"""
        
        match = re.search(r">>> Virus '([^']+)' found", stdout)
        if match:
            return ScanStatus.INFECTED, match.group(1)
        
        if "Virus" in stdout or "found" in stdout.lower():
            return ScanStatus.INFECTED, "Virus detected"
        
        if returncode == 0 or "clean" in stdout.lower():
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealNortonEngine(RealAntivirusEngine):
    """REAL Norton/Symantec integration using ccScan.exe"""
    
    def __init__(self):
        super().__init__("Norton")
    
    def _detect_installation(self) -> bool:
        """Detect Norton installation"""
        possible_paths = [
            r"C:\Program Files\Norton Security\Engine\*\ccScan.exe",
            r"C:\Program Files (x86)\Norton Security\Engine\*\ccScan.exe",
            r"C:\Program Files\NortonLifeLock\Norton Security\Engine\*\ccScan.exe"
        ]
        
        import glob
        for path_pattern in possible_paths:
            matches = glob.glob(path_pattern)
            if matches:
                self.executable_path = matches[0]
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Norton not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Norton output"""
        
        for line in stdout.split('\n'):
            if "infected" in line.lower() or "threat" in line.lower():
                # Extract threat name
                match = re.search(r':\s*([A-Z][^\n]+)', line)
                if match:
                    return ScanStatus.INFECTED, match.group(1).strip()
                return ScanStatus.INFECTED, "Threat detected"
        
        if returncode == 0:
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealTrendMicroEngine(RealAntivirusEngine):
    """REAL Trend Micro integration using UfSeAgnt.exe"""
    
    def __init__(self):
        super().__init__("Trend Micro")
    
    def _detect_installation(self) -> bool:
        """Detect Trend Micro installation"""
        possible_paths = [
            r"C:\Program Files (x86)\Trend Micro\OfficeScan Client\UfSeAgnt.exe",
            r"C:\Program Files\Trend Micro\OfficeScan Client\UfSeAgnt.exe",
            r"C:\Program Files (x86)\Trend Micro\Security Agent\UfSeAgnt.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="Trend Micro not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, "-m", file_path],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse Trend Micro output"""
        
        if "virus" in stdout.lower() or "malware" in stdout.lower():
            for line in stdout.split('\n'):
                if "virus" in line.lower():
                    parts = line.split(':')
                    if len(parts) > 1:
                        return ScanStatus.INFECTED, parts[1].strip()
            return ScanStatus.INFECTED, "Malware detected"
        
        if returncode == 0:
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None


class RealAVGEngine(RealAntivirusEngine):
    """REAL AVG integration using avgscanx.exe"""
    
    def __init__(self):
        super().__init__("AVG")
    
    def _detect_installation(self) -> bool:
        """Detect AVG installation"""
        possible_paths = [
            r"C:\Program Files\AVG\Antivirus\avgscanx.exe",
            r"C:\Program Files (x86)\AVG\Antivirus\avgscanx.exe",
            r"C:\Program Files\AVG\AVG Antivirus\avgscanx.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.executable_path = path
                return True
        
        return False
    
    def scan(self, file_path: str) -> ScanResult:
        start_time = time.time()
        
        if not self.available:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.NOT_AVAILABLE,
                detection=None,
                scan_time=0,
                error_message="AVG not installed"
            )
        
        try:
            result = subprocess.run(
                [self.executable_path, file_path, "/scan=/full"],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            scan_time = time.time() - start_time
            status, detection = self.parse_output(result.stdout, result.stderr, result.returncode)
            
            return ScanResult(
                av_name=self.name,
                status=status,
                detection=detection,
                scan_time=scan_time
            )
                
        except subprocess.TimeoutExpired:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.TIMEOUT,
                detection=None,
                scan_time=self.timeout
            )
        except Exception as e:
            return ScanResult(
                av_name=self.name,
                status=ScanStatus.ERROR,
                detection=None,
                scan_time=time.time() - start_time,
                error_message=str(e)
            )
    
    def parse_output(self, stdout: str, stderr: str, returncode: int) -> Tuple[ScanStatus, Optional[str]]:
        """Parse AVG output"""
        
        for line in stdout.split('\n'):
            if "infected" in line.lower() or "virus" in line.lower():
                match = re.search(r':\s*([^\n]+)', line)
                if match:
                    return ScanStatus.INFECTED, match.group(1).strip()
                return ScanStatus.INFECTED, "Virus detected"
        
        if returncode == 0 or "clean" in stdout.lower():
            return ScanStatus.CLEAN, None
        
        return ScanStatus.ERROR, None
