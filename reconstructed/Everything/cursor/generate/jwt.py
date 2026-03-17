import time
import json
import base64
import uuid

try:
    from cryptography.hazmat.primitives.asymmetric import ed25519
except ImportError:
    print("❌ Python 'cryptography' library not found.")
    print("   Please run: pip install cryptography")
    exit(1)

# The private key (32 bytes)
key_hex = '7a1c0d7f3b9f4c8e2f6d5a4b3c2e1f0d9a8b7c6e5f4d3c2b1a00987766554433'
private_key_bytes = bytes.fromhex(key_hex)
private_key = ed25519.Ed25519PrivateKey.from_private_bytes(private_key_bytes)

# Timestamps
iat = int(time.time()) - 30
exp = iat + 31536000  # 1 year

# Payload
payload = {
    "aud": "https://copilot-proxy.githubusercontent.com",
    "iss": "copilot-cursor",
    "iat": iat,
    "exp": exp,
    "entitlements": {
        "model": "gpt-4-turbo",
        "context": 32768,
        "tools": True,
        "agent": True,
        "rapid": True,
        "unlimited": True
    },
    "deviceId": str(uuid.uuid4()),
    "userId": 31337
}

# Header
header = {
    "alg": "EdDSA",
    "crv": "Ed25519",
    "kid": 7
}

def base64url_encode(data):
    if isinstance(data, dict):
        data = json.dumps(data, separators=(',', ':')).encode('utf-8')
    return base64.urlsafe_b64encode(data).decode('utf-8').rstrip('=')

# Encode
header_b64 = base64url_encode(header)
payload_b64 = base64url_encode(payload)
signing_input = f"{header_b64}.{payload_b64}".encode('utf-8')

# Sign
signature = private_key.sign(signing_input)
signature_b64 = base64.urlsafe_b64encode(signature).decode('utf-8').rstrip('=')

# Final JWT
jwt = f"{header_b64}.{payload_b64}.{signature_b64}"

print("\n✅ PRO JWT GENERATED SUCCESSFULLY (via Python)!")
print("-" * 80)
print(jwt)
print("-" * 80)
print("\n🚀 NEXT STEPS:")
print("1. Copy the token above.")
print("2. Open Cursor and press Ctrl+Shift+P -> 'Toggle Developer Tools'.")
print("3. In the Console, type: await github.copilot.signIn(\"PASTE_TOKEN_HERE\")")
print("4. Restart Cursor.")

# Save to file
with open("E:/Everything/cursor/cursor_token.txt", "w") as f:
    f.write(jwt)
print(f"\n📄 Token also saved to: E:/Everything/cursor/cursor_token.txt")
