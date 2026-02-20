#!/usr/bin/env python3
"""
RawrXD License Generator — Portable Python License Signer
===========================================================

Cross-platform RSA-4096 key generation and license signing tool.
Uses the `cryptography` library for all crypto operations.

Usage:
    python license_generator.py genkey [--output-dir DIR]
    python license_generator.py issue [options]
    python license_generator.py verify <license.rawrlic> [--pub-key KEY]
    python license_generator.py hwid

Requirements:
    pip install cryptography

Architecture:
    - RSA-4096 with SHA-256 signing (PKCS#1 v1.5)
    - License blob format matches MASM LICENSE_HEADER (64 bytes)
    - Output: .rawrlic (blob + 512-byte signature)
"""

import argparse
import struct
import time
import os
import sys
import hashlib
import platform
import subprocess
from pathlib import Path

try:
    from cryptography.hazmat.primitives.asymmetric import rsa, padding
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.backends import default_backend
    from cryptography.exceptions import InvalidSignature
except ImportError:
    print("ERROR: 'cryptography' package required. Install with:")
    print("  pip install cryptography")
    sys.exit(1)

# =============================================================================
# Constants (must match RawrXD_Common.inc and RawrXD_KeyGen.cpp)
# =============================================================================
LICENSE_MAGIC   = 0x5258444C   # "RXDL" little-endian
LICENSE_VERSION = 0x0200       # v2.0
RSA_SIG_BYTES   = 512          # 4096 / 8
RSA_KEY_BITS    = 4096

# License types
LICENSE_TRIAL      = 1
LICENSE_ENTERPRISE = 2
LICENSE_OEM        = 3

# Feature bitmasks
FEAT_DUAL_ENGINE    = 0x01
FEAT_AVX512         = 0x02
FEAT_DISTRIBUTED    = 0x04
FEAT_GPU_QUANT      = 0x08
FEAT_ENTERPRISE_SUP = 0x10
FEAT_UNLIMITED_CTX  = 0x20
FEAT_FLASH_ATTN     = 0x40
FEAT_MULTI_GPU      = 0x80
FEAT_ALL            = 0xFF

FEATURE_NAMES = {
    FEAT_DUAL_ENGINE:    "DualEngine800B",
    FEAT_AVX512:         "AVX512Premium",
    FEAT_DISTRIBUTED:    "DistributedSwarm",
    FEAT_GPU_QUANT:      "GPUQuant4Bit",
    FEAT_ENTERPRISE_SUP: "EnterpriseSupport",
    FEAT_UNLIMITED_CTX:  "UnlimitedContext",
    FEAT_FLASH_ATTN:     "FlashAttention",
    FEAT_MULTI_GPU:      "MultiGPU",
}

LICENSE_TYPE_NAMES = {
    LICENSE_TRIAL:      "Trial",
    LICENSE_ENTERPRISE: "Enterprise",
    LICENSE_OEM:        "OEM",
}

# License header format (64 bytes, little-endian, packed)
# Must match MASM LICENSE_HEADER and C++ LicenseHeader
HEADER_FORMAT = "<IHHQQQQIIxxxxxxxx"
HEADER_SIZE   = 64


# =============================================================================
# License Header
# =============================================================================
def pack_license_header(
    license_type: int = LICENSE_ENTERPRISE,
    features: int = FEAT_ALL,
    hwid: int = 0,
    days: int = 365,
    max_model_gb: int = 800,
    max_context_k: int = 200,
) -> bytes:
    """Pack a license header into 64 bytes."""
    now = int(time.time())
    expiry = now + (days * 86400) if days > 0 else 0

    # struct format: magic(I), version(H), type(H), features(Q), hwid(Q),
    #               issued(Q), expiry(Q), maxModelGB(I), maxContextK(I), reserved(8 bytes)
    data = struct.pack(
        "<IHHQQQQII",
        LICENSE_MAGIC,
        LICENSE_VERSION,
        license_type,
        features,
        hwid,
        now,
        expiry,
        max_model_gb,
        max_context_k,
    )
    # Pad with 8 reserved bytes to reach 64
    data += b'\x00' * 8

    assert len(data) == HEADER_SIZE, f"Header size mismatch: {len(data)} != {HEADER_SIZE}"
    return data


def unpack_license_header(data: bytes) -> dict:
    """Unpack a 64-byte license header."""
    if len(data) < HEADER_SIZE:
        raise ValueError(f"Data too short: {len(data)} < {HEADER_SIZE}")

    fields = struct.unpack("<IHHQQQQII", data[:56])
    return {
        "magic":         fields[0],
        "version":       fields[1],
        "license_type":  fields[2],
        "features":      fields[3],
        "hwid":          fields[4],
        "issued":        fields[5],
        "expiry":        fields[6],
        "max_model_gb":  fields[7],
        "max_context_k": fields[8],
    }


def features_to_str(mask: int) -> str:
    """Convert feature bitmask to human-readable string."""
    names = [name for bit, name in FEATURE_NAMES.items() if mask & bit]
    return ", ".join(names) if names else "None"


# =============================================================================
# Key Management
# =============================================================================
def generate_keypair(output_dir: str = ".") -> tuple:
    """Generate RSA-4096 key pair and save to files."""
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=RSA_KEY_BITS,
        backend=default_backend(),
    )

    # Save private key (PEM, encrypted with password)
    priv_path = Path(output_dir) / "rawrxd_private.pem"
    priv_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    priv_path.write_bytes(priv_pem)
    print(f"[OK] Private key saved to {priv_path}")

    # Save public key (PEM)
    public_key = private_key.public_key()
    pub_path = Path(output_dir) / "rawrxd_public.pem"
    pub_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    pub_path.write_bytes(pub_pem)
    print(f"[OK] Public key saved to {pub_path}")

    # Save public key (DER for embedding)
    pub_der_path = Path(output_dir) / "rawrxd_public.der"
    pub_der = public_key.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    pub_der_path.write_bytes(pub_der)
    print(f"[OK] Public key DER saved to {pub_der_path} ({len(pub_der)} bytes)")

    # Generate MASM include file
    inc_path = Path(output_dir) / "rawrxd_pubkey.inc"
    with open(inc_path, "w") as f:
        f.write("; Auto-generated by license_generator.py\n")
        f.write(f"; Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"; Key size: RSA-{RSA_KEY_BITS}\n\n")
        f.write(f"RSA_PUBLIC_KEY_DER_SIZE EQU {len(pub_der)}\n\n")
        f.write("RSA_PUBLIC_KEY_DER LABEL BYTE\n")
        for i in range(0, len(pub_der), 16):
            chunk = pub_der[i:i+16]
            hex_vals = ",".join(f"0{b:02X}h" for b in chunk)
            f.write(f"    DB {hex_vals}\n")
        f.write("\n; End of RSA public key DER blob\n")
    print(f"[OK] MASM include saved to {inc_path}")

    return private_key, public_key


def load_private_key(path: str = "rawrxd_private.pem"):
    """Load private key from PEM file."""
    data = Path(path).read_bytes()
    return serialization.load_pem_private_key(data, password=None, backend=default_backend())


def load_public_key(path: str = "rawrxd_public.pem"):
    """Load public key from PEM file."""
    data = Path(path).read_bytes()
    return serialization.load_pem_public_key(data, backend=default_backend())


# =============================================================================
# Signing & Verification
# =============================================================================
def sign_blob(private_key, blob: bytes) -> bytes:
    """Sign a license blob with RSA-4096 + SHA-256 (PKCS#1 v1.5)."""
    signature = private_key.sign(
        blob,
        padding.PKCS1v15(),
        hashes.SHA256(),
    )
    # Pad to exactly RSA_SIG_BYTES
    if len(signature) < RSA_SIG_BYTES:
        signature += b'\x00' * (RSA_SIG_BYTES - len(signature))
    return signature[:RSA_SIG_BYTES]


def verify_blob(public_key, blob: bytes, signature: bytes) -> bool:
    """Verify a license blob signature."""
    try:
        public_key.verify(
            signature,
            blob,
            padding.PKCS1v15(),
            hashes.SHA256(),
        )
        return True
    except InvalidSignature:
        return False


# =============================================================================
# HWID Generation (approximate match to ASM Shield_GenerateHWID)
# =============================================================================
def generate_hwid() -> int:
    """Generate a hardware fingerprint similar to ASM Shield_GenerateHWID.
    
    Note: This is a Python approximation. The definitive HWID comes from
    the ASM implementation using CPUID + VolumeSerial + HwProfile.
    Use RawrXD_KeyGen.exe --hwid for the authoritative value.
    """
    parts = []

    # CPU info
    if platform.system() == "Windows":
        try:
            result = subprocess.run(
                ["wmic", "cpu", "get", "ProcessorId"],
                capture_output=True, text=True, timeout=5
            )
            cpu_id = result.stdout.strip().split('\n')[-1].strip()
            parts.append(cpu_id.encode())
        except Exception:
            parts.append(platform.processor().encode())
    else:
        parts.append(platform.processor().encode())

    # Volume serial (Windows)
    if platform.system() == "Windows":
        try:
            result = subprocess.run(
                ["cmd", "/c", "vol", "c:"],
                capture_output=True, text=True, timeout=5
            )
            for line in result.stdout.split('\n'):
                if '-' in line:
                    serial = line.strip().split()[-1]
                    parts.append(serial.encode())
                    break
        except Exception:
            pass

    # Machine ID
    parts.append(platform.node().encode())
    parts.append(platform.machine().encode())

    # Hash everything together
    combined = b'|'.join(parts)
    h = hashlib.sha256(combined).digest()
    
    # Take first 8 bytes as uint64
    hwid = struct.unpack("<Q", h[:8])[0]

    # Murmur3-style avalanche
    hwid ^= hwid >> 33
    hwid = (hwid * 0xFF51AFD7ED558CCD) & 0xFFFFFFFFFFFFFFFF
    hwid ^= hwid >> 33
    hwid = (hwid * 0xC4CEB9FE1A85EC53) & 0xFFFFFFFFFFFFFFFF
    hwid ^= hwid >> 33

    return hwid


# =============================================================================
# CLI Commands
# =============================================================================
def cmd_genkey(args):
    """Generate RSA-4096 key pair."""
    output_dir = args.output_dir or "."
    os.makedirs(output_dir, exist_ok=True)
    generate_keypair(output_dir)
    print(f"\n[OK] RSA-{RSA_KEY_BITS} key pair generated in {output_dir}/")


def cmd_issue(args):
    """Issue a signed license."""
    # Parse license type
    type_map = {"trial": LICENSE_TRIAL, "enterprise": LICENSE_ENTERPRISE, "oem": LICENSE_OEM}
    license_type = type_map.get(args.type, LICENSE_ENTERPRISE)

    # Parse features
    features = int(args.features, 16) if args.features else FEAT_ALL

    # Parse HWID
    hwid = int(args.hwid, 16) if args.hwid else 0

    # Build header
    blob = pack_license_header(
        license_type=license_type,
        features=features,
        hwid=hwid,
        days=args.days,
        max_model_gb=args.model_gb,
        max_context_k=args.context_k,
    )

    # Print license details
    hdr = unpack_license_header(blob)
    print(f"License Details:")
    print(f"  Type:       {LICENSE_TYPE_NAMES.get(license_type, 'Unknown')}")
    print(f"  Features:   0x{features:016X} ({features_to_str(features)})")
    print(f"  HWID:       0x{hwid:016X} {'(floating)' if hwid == 0 else '(locked)'}")
    print(f"  Issued:     {time.ctime(hdr['issued'])}")
    if hdr['expiry']:
        print(f"  Expires:    {time.ctime(hdr['expiry'])}")
    else:
        print(f"  Expires:    NEVER (perpetual)")
    print(f"  Max Model:  {args.model_gb} GB")
    print(f"  Max Ctx:    {args.context_k}K tokens")

    # Load private key
    key_path = args.key or "rawrxd_private.pem"
    try:
        private_key = load_private_key(key_path)
    except Exception as e:
        print(f"\n[ERROR] Cannot load private key from {key_path}: {e}")
        print("Run 'genkey' first to generate a key pair.")
        return 1

    # Sign
    signature = sign_blob(private_key, blob)
    print(f"\n[OK] License signed ({len(signature)} byte RSA-{RSA_KEY_BITS} signature)")

    # Write .rawrlic
    output = args.output or "license.rawrlic"
    combined = blob + signature
    Path(output).write_bytes(combined)
    print(f"[OK] License written to {output} ({len(combined)} bytes)")

    # Write .reg file
    reg_path = Path(output).with_suffix('.reg')
    with open(reg_path, 'w') as f:
        f.write("Windows Registry Editor Version 5.00\n\n")
        f.write("[HKEY_CURRENT_USER\\SOFTWARE\\RawrXD\\Enterprise]\n")
        
        f.write('"LicenseBlob"=hex:')
        hex_blob = ','.join(f'{b:02x}' for b in blob)
        f.write(hex_blob + '\n')

        f.write('"LicenseSignature"=hex:')
        hex_sig = ','.join(f'{b:02x}' for b in signature)
        f.write(hex_sig + '\n')

        f.write(f'"LicenseType"=dword:{license_type:08x}\n')
        f.write(f'"Version"=dword:{LICENSE_VERSION:08x}\n')

    print(f"[OK] Registry file written to {reg_path}")
    return 0


def cmd_verify(args):
    """Verify a .rawrlic license file."""
    lic_path = args.license
    data = Path(lic_path).read_bytes()

    if len(data) < HEADER_SIZE + RSA_SIG_BYTES:
        print(f"[ERROR] File too small ({len(data)} bytes, need >= {HEADER_SIZE + RSA_SIG_BYTES})")
        return 1

    blob = data[:HEADER_SIZE]
    sig  = data[HEADER_SIZE:HEADER_SIZE + RSA_SIG_BYTES]

    # Unpack header
    hdr = unpack_license_header(blob)

    # Validate magic
    if hdr['magic'] != LICENSE_MAGIC:
        print(f"[ERROR] Invalid magic: 0x{hdr['magic']:08X} (expected 0x{LICENSE_MAGIC:08X})")
        return 1

    print(f"License file: {lic_path}")
    print(f"  Magic:      0x{hdr['magic']:08X} (RXDL)")
    print(f"  Version:    0x{hdr['version']:04X}")
    print(f"  Type:       {LICENSE_TYPE_NAMES.get(hdr['license_type'], 'Unknown')}")
    print(f"  Features:   0x{hdr['features']:016X} ({features_to_str(hdr['features'])})")
    print(f"  HWID:       0x{hdr['hwid']:016X}")
    print(f"  Issued:     {time.ctime(hdr['issued'])}")
    if hdr['expiry']:
        print(f"  Expires:    {time.ctime(hdr['expiry'])}")
        if hdr['expiry'] < time.time():
            print(f"  *** LICENSE EXPIRED ***")
    else:
        print(f"  Expires:    NEVER (perpetual)")
    print(f"  Max Model:  {hdr['max_model_gb']} GB")
    print(f"  Max Ctx:    {hdr['max_context_k']}K tokens")

    # Verify signature
    pub_path = args.pub_key or "rawrxd_public.pem"
    try:
        public_key = load_public_key(pub_path)
        valid = verify_blob(public_key, blob, sig)
        if valid:
            print(f"\n[OK] Signature VALID (RSA-{RSA_KEY_BITS} + SHA-256)")
        else:
            print(f"\n[FAIL] Signature INVALID")
            return 1
    except Exception as e:
        print(f"\n[WARN] Cannot verify signature: {e}")
        print(f"       (public key: {pub_path})")
        return 1

    return 0


def cmd_hwid(args):
    """Print hardware ID."""
    hwid = generate_hwid()
    print(f"Hardware ID (HWID): 0x{hwid:016X}")
    print(f"\nNote: This is a Python approximation. For the authoritative HWID,")
    print(f"use: RawrXD_KeyGen.exe --hwid")
    print(f"\nUse with: python license_generator.py issue --hwid {hwid:016X}")


# =============================================================================
# Main
# =============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="RawrXD License Generator — RSA-4096 License Signer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate key pair
  python license_generator.py genkey

  # Issue enterprise license (all features, 1 year, floating)
  python license_generator.py issue

  # Issue trial license locked to HWID, 30 days
  python license_generator.py issue --type trial --days 30 --hwid DEADBEEF12345678

  # Verify a license file
  python license_generator.py verify license.rawrlic

  # Print machine HWID
  python license_generator.py hwid
""",
    )

    subparsers = parser.add_subparsers(dest="command", help="Command to execute")

    # genkey
    p_genkey = subparsers.add_parser("genkey", help="Generate RSA-4096 key pair")
    p_genkey.add_argument("--output-dir", "-o", default=".", help="Output directory")

    # issue
    p_issue = subparsers.add_parser("issue", help="Issue a signed license")
    p_issue.add_argument("--type", "-t", default="enterprise",
                          choices=["trial", "enterprise", "oem"])
    p_issue.add_argument("--features", "-f", default=None,
                          help="Feature bitmask in hex (default: FF = all)")
    p_issue.add_argument("--hwid", default=None,
                          help="Target HWID in hex (default: 0 = floating)")
    p_issue.add_argument("--days", "-d", type=int, default=365,
                          help="Validity in days (default: 365, 0 = perpetual)")
    p_issue.add_argument("--model-gb", type=int, default=800,
                          help="Max model size in GB (default: 800)")
    p_issue.add_argument("--context-k", type=int, default=200,
                          help="Max context length in K tokens (default: 200)")
    p_issue.add_argument("--key", "-k", default=None,
                          help="Private key PEM file (default: rawrxd_private.pem)")
    p_issue.add_argument("--output", "-o", default="license.rawrlic",
                          help="Output file (default: license.rawrlic)")

    # verify
    p_verify = subparsers.add_parser("verify", help="Verify a license file")
    p_verify.add_argument("license", help="Path to .rawrlic file")
    p_verify.add_argument("--pub-key", default=None,
                           help="Public key PEM file (default: rawrxd_public.pem)")

    # hwid
    subparsers.add_parser("hwid", help="Print hardware fingerprint")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 0

    handlers = {
        "genkey": cmd_genkey,
        "issue":  cmd_issue,
        "verify": cmd_verify,
        "hwid":   cmd_hwid,
    }

    handler = handlers.get(args.command)
    if handler:
        return handler(args) or 0
    else:
        parser.print_help()
        return 1


if __name__ == "__main__":
    sys.exit(main())
