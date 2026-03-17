#!/usr/bin/env python3
# =============================================================================
# RawrXD License Authority
# Generates RSA-4096 Keys & Signs Hardware-Locked Licenses
# =============================================================================
#
# This is the SERVER-SIDE AUTHORITY. It lives on your secure, offline build
# machine. It generates the RSA-4096 keypair and signs licenses.
#
# It produces the .reg file that customers double-click to unlock the
# 800B Dual-Engine.
#
# The private key NEVER leaves this machine. The public key is compiled
# into the client binary as a MASM byte array.
#
# Usage:
#   python license_authority.py --gen-keys
#   python license_authority.py --hwid E4D199228811 --features 800B_DUAL,FLASH_ATTN,ENTERPRISE
#   python license_authority.py --hwid E4D199228811 --features 7F --days 365 --out pro.reg
#   python license_authority.py --export-pub-inc RawrXD_PublicKey.inc
#
# Requirements:
#   pip install cryptography
# =============================================================================

import struct
import time
import argparse
import sys
import os
from pathlib import Path

try:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding, rsa
    from cryptography.hazmat.backends import default_backend
    from cryptography.exceptions import InvalidSignature
except ImportError:
    print("[!] FATAL: 'cryptography' package required.")
    print("    pip install cryptography")
    sys.exit(1)

# =============================================================================
# Constants (Must match RawrXD_Common.inc)
# =============================================================================
MAGIC             = 0x52415752       # "RAWR" little-endian
VERSION           = 0x00010000       # v1.0
RSA_KEY_BITS      = 4096
RSA_SIG_BYTES     = 512              # 4096 / 8

# License header: Magic(4), Ver(4), Feat(8), Issue(8), Expire(8), HWID(8), Seat(2), Flags(2), Reserved(32)
HEADER_FORMAT     = "<IIQQQQHH32s"
HEADER_SIZE       = struct.calcsize(HEADER_FORMAT)  # 76 bytes

# Feature bitmasks — must match RawrXD_Common.inc and enterprise_license.h
FEATURES = {
    "800B_DUAL":    0x01,   # FEATURE_800B_DUAL_ENGINE
    "AVX512":       0x02,   # FEATURE_AVX512_PREMIUM
    "SWARM":        0x04,   # FEATURE_DISTRIBUTED_SWARM
    "GPU_4BIT":     0x08,   # FEATURE_GPU_QUANT_4BIT
    "ENTERPRISE":   0x10,   # FEATURE_ENTERPRISE_SUPPORT
    "UNLIMITED":    0x20,   # FEATURE_UNLIMITED_CONTEXT
    "FLASH_ATTN":   0x40,   # FEATURE_FLASH_ATTENTION
    "MULTI_GPU":    0x80,   # FEATURE_MULTI_GPU
}

FEATURE_ALL       = 0xFF

# Tier presets for convenience
TIERS = {
    "community":    0x00,
    "pro":          0x01 | 0x02 | 0x40,           # 800B + AVX512 + Flash
    "enterprise":   FEATURE_ALL,
}


# =============================================================================
# Key Generation
# =============================================================================
def generate_keys(output_dir="."):
    """Generates RSA-4096 Keypair (PEM format)."""
    print(f"[*] Generating RSA-{RSA_KEY_BITS} Keypair (this may take a moment)...")
    
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=RSA_KEY_BITS,
        backend=default_backend()
    )
    
    os.makedirs(output_dir, exist_ok=True)
    
    # Save Private Key (KEEP SAFE — NEVER DISTRIBUTE)
    priv_path = Path(output_dir) / "master_private.pem"
    with open(priv_path, "wb") as f:
        f.write(private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption()
        ))
    print(f"[+] Private key: {priv_path}  *** KEEP THIS SAFE ***")
        
    # Save Public Key (embedded into client binary)
    public_key = private_key.public_key()
    pub_path = Path(output_dir) / "master_public.pem"
    with open(pub_path, "wb") as f:
        f.write(public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        ))
    print(f"[+] Public key:  {pub_path}")

    # Also save DER format for direct ASM embedding
    pub_der_path = Path(output_dir) / "master_public.der"
    pub_der = public_key.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    with open(pub_der_path, "wb") as f:
        f.write(pub_der)
    print(f"[+] Public DER:  {pub_der_path} ({len(pub_der)} bytes)")
    
    print(f"\n[+] Keys generated successfully.")
    print(f"    Next: python license_authority.py --export-pub-inc RawrXD_PublicKey.inc")
    
    return private_key, public_key


# =============================================================================
# Key Loading
# =============================================================================
def load_private_key(path="master_private.pem"):
    """Load private key from PEM file."""
    p = Path(path)
    if not p.exists():
        print(f"[!] Error: {path} not found. Run --gen-keys first.")
        sys.exit(1)
    with open(p, "rb") as f:
        return serialization.load_pem_private_key(
            f.read(), password=None, backend=default_backend()
        )


def load_public_key(path="master_public.pem"):
    """Load public key from PEM file."""
    p = Path(path)
    if not p.exists():
        print(f"[!] Error: {path} not found. Run --gen-keys first.")
        sys.exit(1)
    with open(p, "rb") as f:
        return serialization.load_pem_public_key(
            f.read(), backend=default_backend()
        )


# =============================================================================
# Public Key Export (for ASM embedding)
# =============================================================================
def export_public_key_inc(output_path, key_path="master_public.pem"):
    """Export public key as MASM .inc byte array for compilation into the client."""
    pub_key = load_public_key(key_path)
    
    # Get DER encoding (raw binary, no PEM armor)
    der_bytes = pub_key.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    
    with open(output_path, "w") as f:
        f.write("; =============================================================================\n")
        f.write("; RawrXD RSA-4096 Public Key — Auto-generated by license_authority.py\n")
        f.write(f"; Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"; Key Size:  RSA-{RSA_KEY_BITS}\n")
        f.write(f"; DER Size:  {len(der_bytes)} bytes\n")
        f.write("; WARNING:   Do not edit manually. Regenerate with --export-pub-inc\n")
        f.write("; =============================================================================\n\n")
        
        f.write(f"RSA_PUB_KEY_DER_SIZE  EQU  {len(der_bytes)}\n\n")
        f.write("ALIGN 16\n")
        f.write("RSA_PUBLIC_KEY_BLOB LABEL BYTE\n")
        
        for i in range(0, len(der_bytes), 16):
            chunk = der_bytes[i:i+16]
            hex_vals = ",".join(f"0{b:02X}h" for b in chunk)
            f.write(f"    DB {hex_vals}\n")
        
        f.write(f"\n; Total: {len(der_bytes)} bytes\n")
        f.write("; End of RSA public key blob\n")
    
    print(f"[+] MASM include file written: {output_path} ({len(der_bytes)} bytes)")
    print(f"    Copy to src/asm/ and INCLUDE in RawrXD_EnterpriseLicense.asm")


# =============================================================================
# License Payload Construction
# =============================================================================
def create_license_payload(hwid_hex, feature_mask, days_valid, seats=1, flags=0):
    """Build the packed license header (must match ASM LICENSE_HEADER struct)."""
    hwid = int(hwid_hex, 16)
    issue_ts = int(time.time())
    expiry_ts = 0 if days_valid == 0 else issue_ts + (days_valid * 86400)
    
    payload = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        feature_mask,
        issue_ts,
        expiry_ts,
        hwid,
        seats,
        flags,
        b'\x00' * 32      # Reserved
    )
    
    assert len(payload) == HEADER_SIZE, f"Payload size mismatch: {len(payload)} != {HEADER_SIZE}"
    return payload


def parse_license_payload(data):
    """Unpack a license header for display."""
    if len(data) < HEADER_SIZE:
        return None
    fields = struct.unpack(HEADER_FORMAT, data[:HEADER_SIZE])
    return {
        "magic":     fields[0],
        "version":   fields[1],
        "features":  fields[2],
        "issued":    fields[3],
        "expiry":    fields[4],
        "hwid":      fields[5],
        "seats":     fields[6],
        "flags":     fields[7],
    }


# =============================================================================
# Signing
# =============================================================================
def sign_license(payload, private_key):
    """Sign the license payload with RSA-4096 + SHA-256 (PKCS#1 v1.5)."""
    signature = private_key.sign(
        payload,
        padding.PKCS1v15(),     # Standard padding for CryptoAPI compatibility
        hashes.SHA256()
    )
    return signature


def verify_license(payload, signature, public_key):
    """Verify a license signature."""
    try:
        public_key.verify(
            signature,
            payload,
            padding.PKCS1v15(),
            hashes.SHA256()
        )
        return True
    except InvalidSignature:
        return False


# =============================================================================
# .reg File Generation
# =============================================================================
def write_reg_file(filename, payload, signature):
    """Writes a Windows .reg file for one-click license installation.
    
    Customer double-clicks this file → Registry updated → RawrXD boots,
    reads license + signature → RSA verify → HWID match → 800B Kernel decrypted.
    """
    def to_hex_str(data, line_width=25):
        """Format bytes as .reg hex: xx,xx,xx with line continuation."""
        parts = [f"{b:02x}" for b in data]
        lines = []
        for i in range(0, len(parts), line_width):
            chunk = ",".join(parts[i:i+line_width])
            if i + line_width < len(parts):
                chunk += ",\\"
            lines.append(chunk)
        return "\n  ".join(lines)

    reg_content = f"""Windows Registry Editor Version 5.00

; =============================================================================
; RawrXD Enterprise License
; Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}
; Double-click to install. Requires matching HWID.
; =============================================================================

[HKEY_CURRENT_USER\\Software\\RawrXD\\License]
"Data"=hex:{to_hex_str(payload)}
"Signature"=hex:{to_hex_str(signature)}
"Version"=dword:{VERSION:08x}
"InstalledAt"="{time.strftime('%Y-%m-%dT%H:%M:%S')}"
"""
    
    with open(filename, "w") as f:
        f.write(reg_content)
    
    print(f"[+] License .reg file: {filename}")
    print(f"    Payload:   {len(payload)} bytes")
    print(f"    Signature: {len(signature)} bytes (RSA-{RSA_KEY_BITS})")


# =============================================================================
# .rawrlic Binary File (alternative to .reg)
# =============================================================================
def write_rawrlic_file(filename, payload, signature):
    """Write combined binary license file (payload + signature)."""
    # Pad signature to exactly RSA_SIG_BYTES
    sig_padded = signature[:RSA_SIG_BYTES].ljust(RSA_SIG_BYTES, b'\x00')
    
    with open(filename, "wb") as f:
        f.write(payload)
        f.write(sig_padded)
    
    print(f"[+] Binary license: {filename} ({len(payload) + RSA_SIG_BYTES} bytes)")


# =============================================================================
# Feature Mask Parsing
# =============================================================================
def parse_features(feature_str):
    """Parse feature string: hex value, comma-separated names, or tier name."""
    if not feature_str:
        return FEATURE_ALL  # Default: all features
    
    # Check tier presets
    lower = feature_str.strip().lower()
    if lower in TIERS:
        return TIERS[lower]
    
    # Check comma-separated feature names
    if "," in feature_str or any(f.strip().upper() in FEATURES for f in [feature_str]):
        mask = 0
        for f in feature_str.split(","):
            f = f.strip().upper()
            if f in FEATURES:
                mask |= FEATURES[f]
            else:
                print(f"[!] Unknown feature: '{f}'")
                print(f"    Available: {', '.join(FEATURES.keys())}")
                sys.exit(1)
        return mask
    
    # Try hex
    try:
        return int(feature_str, 16)
    except ValueError:
        pass
    
    # Try single feature name
    upper = feature_str.strip().upper()
    if upper in FEATURES:
        return FEATURES[upper]
    
    print(f"[!] Cannot parse feature mask: '{feature_str}'")
    print(f"    Use hex (e.g., 7F), names (e.g., 800B_DUAL,FLASH_ATTN), or tiers (community/pro/enterprise)")
    sys.exit(1)


def features_to_names(mask):
    """Convert feature bitmask to human-readable string."""
    names = [name for name, bit in FEATURES.items() if mask & bit]
    return ", ".join(names) if names else "(none)"


# =============================================================================
# Display
# =============================================================================
def print_license_info(hdr):
    """Pretty-print license header."""
    print(f"\n  ┌─────────────────────────────────────────────┐")
    print(f"  │         RawrXD Enterprise License            │")
    print(f"  ├─────────────────────────────────────────────┤")
    print(f"  │ Magic:      0x{hdr['magic']:08X} {'✓' if hdr['magic'] == MAGIC else '✗ INVALID'}")
    print(f"  │ Version:    0x{hdr['version']:08X}")
    print(f"  │ HWID:       0x{hdr['hwid']:016X}")
    print(f"  │ Features:   0x{hdr['features']:016X}")
    print(f"  │              ({features_to_names(hdr['features'])})")
    print(f"  │ Seats:      {hdr['seats']}")
    print(f"  │ Issued:     {time.ctime(hdr['issued'])}")
    if hdr['expiry'] == 0:
        print(f"  │ Expires:    NEVER (perpetual)")
    else:
        expired = " *** EXPIRED ***" if hdr['expiry'] < time.time() else ""
        print(f"  │ Expires:    {time.ctime(hdr['expiry'])}{expired}")
    print(f"  └─────────────────────────────────────────────┘")


# =============================================================================
# Commands
# =============================================================================
def cmd_verify(args):
    """Verify a .rawrlic or .reg license file."""
    path = Path(args.verify)
    
    if path.suffix == ".rawrlic":
        data = path.read_bytes()
        if len(data) < HEADER_SIZE + RSA_SIG_BYTES:
            print(f"[!] File too small: {len(data)} bytes")
            return 1
        payload = data[:HEADER_SIZE]
        signature = data[HEADER_SIZE:HEADER_SIZE + RSA_SIG_BYTES]
    else:
        print(f"[!] Verification only supports .rawrlic files")
        return 1
    
    hdr = parse_license_payload(payload)
    if not hdr:
        print("[!] Failed to parse license header")
        return 1
    
    print_license_info(hdr)
    
    pub_key = load_public_key(args.pub_key if args.pub_key else "master_public.pem")
    if verify_license(payload, signature, pub_key):
        print(f"\n[+] Signature: VALID ✓ (RSA-{RSA_KEY_BITS} + SHA-256)")
        return 0
    else:
        print(f"\n[!] Signature: INVALID ✗")
        return 1


# =============================================================================
# Main
# =============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="RawrXD License Authority — RSA-4096 Key & License Signer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 1. Generate master keypair (ONCE, on secure machine)
  python license_authority.py --gen-keys

  # 2. Export public key as MASM .inc for client compilation
  python license_authority.py --export-pub-inc src/asm/RawrXD_PublicKey.inc

  # 3. Issue enterprise license (all features, perpetual, locked to HWID)
  python license_authority.py --hwid E4D199228811 --features enterprise

  # 4. Issue pro license (800B + Flash Attention, 1 year)
  python license_authority.py --hwid E4D199228811 --features 800B_DUAL,FLASH_ATTN --days 365

  # 5. Issue by hex mask
  python license_authority.py --hwid E4D199228811 --features 7F --out vip.reg

  # 6. Verify a license file
  python license_authority.py --verify license.rawrlic

Feature Names:
  800B_DUAL, AVX512, SWARM, GPU_4BIT, ENTERPRISE, UNLIMITED, FLASH_ATTN, MULTI_GPU

Tier Presets:
  community  = 0x00 (no gated features)
  pro        = 0x43 (800B + AVX512 + Flash Attention)
  enterprise = 0xFF (everything)
"""
    )
    
    parser.add_argument("--gen-keys", action="store_true",
                        help="Generate new RSA-4096 master keypair")
    parser.add_argument("--export-pub-inc", metavar="FILE",
                        help="Export public key as MASM .inc byte array")
    parser.add_argument("--hwid",
                        help="Client Hardware ID (hex string from RawrXD --hwid)")
    parser.add_argument("--features", default="enterprise",
                        help="Feature mask: hex, comma-names, or tier (default: enterprise)")
    parser.add_argument("--days", type=int, default=0,
                        help="Validity in days (0 = perpetual, default: 0)")
    parser.add_argument("--seats", type=int, default=1,
                        help="Seat count (default: 1)")
    parser.add_argument("--out", default="license",
                        help="Output base filename (default: license)")
    parser.add_argument("--verify", metavar="FILE",
                        help="Verify a .rawrlic license file")
    parser.add_argument("--pub-key", metavar="FILE",
                        help="Public key PEM for --verify (default: master_public.pem)")
    parser.add_argument("--key-dir", default=".",
                        help="Directory for key files (default: current dir)")
    
    args = parser.parse_args()
    
    # --gen-keys
    if args.gen_keys:
        generate_keys(args.key_dir)
        return 0
    
    # --export-pub-inc
    if args.export_pub_inc:
        export_public_key_inc(args.export_pub_inc)
        return 0
    
    # --verify
    if args.verify:
        return cmd_verify(args)
    
    # --hwid (sign a license)
    if not args.hwid:
        parser.print_help()
        return 0
    
    # Parse feature mask
    mask = parse_features(args.features)
    
    print(f"[*] RawrXD License Authority")
    print(f"[*] Signing license for HWID: {args.hwid}")
    print(f"[*] Feature Mask: 0x{mask:016X} ({features_to_names(mask)})")
    print(f"[*] Validity: {'Perpetual' if args.days == 0 else f'{args.days} days'}")
    print(f"[*] Seats: {args.seats}")
    
    # Load private key
    pk = load_private_key(str(Path(args.key_dir) / "master_private.pem"))
    
    # Build and sign
    payload = create_license_payload(args.hwid, mask, args.days, args.seats)
    sig = sign_license(payload, pk)
    
    # Parse for display
    hdr = parse_license_payload(payload)
    print_license_info(hdr)
    
    # Write outputs
    reg_file = f"{args.out}.reg"
    lic_file = f"{args.out}.rawrlic"
    
    write_reg_file(reg_file, payload, sig)
    write_rawrlic_file(lic_file, payload, sig)
    
    print(f"\n[+] Done. Deliver '{reg_file}' to the customer.")
    print(f"    They double-click → RawrXD verifies RSA-{RSA_KEY_BITS} → HWID match → 800B Kernel decrypted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
