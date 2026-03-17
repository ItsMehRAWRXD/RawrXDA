(async () => {
    const rawKeyHex = "7a1c0d7f3b9f4c8e2f6d5a4b3c2e1f0d9a8b7c6e5f4d3c2b1a00987766554433";
    
    // Helper to convert hex to Uint8Array
    const hexToBytes = hex => new Uint8Array(hex.match(/.{1,2}/g).map(byte => parseInt(byte, 16)));
    const bytesToBase64Url = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
    
    const rawKey = hexToBytes(rawKeyHex);
    
    // PKCS#8 Header for Ed25519
    const pkcs8Header = hexToBytes("302e020100300506032b657004220420");
    const pkcs8Key = new Uint8Array(pkcs8Header.length + rawKey.length);
    pkcs8Key.set(pkcs8Header);
    pkcs8Key.set(rawKey, pkcs8Header.length);
    
    try {
        const key = await crypto.subtle.importKey(
            "pkcs8",
            pkcs8Key,
            { name: "Ed25519", namedCurve: "Ed25519" },
            false,
            ["sign"]
        );
        
        const iat = Math.floor(Date.now() / 1000) - 30;
        const exp = iat + 31536000; // 1 year
        
        const header = { alg: "EdDSA", crv: "Ed25519", kid: 7 };
        const payload = {
            aud: "https://copilot-proxy.githubusercontent.com",
            iss: "copilot-cursor",
            iat,
            exp,
            entitlements: {
                model: "gpt-4-turbo",
                context: 32768,
                tools: true,
                agent: true,
                rapid: true,
                unlimited: true
            },
            deviceId: crypto.randomUUID(),
            userId: 31337
        };
        
        const headerB64 = bytesToBase64Url(new TextEncoder().encode(JSON.stringify(header)));
        const payloadB64 = bytesToBase64Url(new TextEncoder().encode(JSON.stringify(payload)));
        const signingInput = new TextEncoder().encode(`${headerB64}.${payloadB64}`);
        
        const signature = await crypto.subtle.sign("Ed25519", key, signingInput);
        const signatureB64 = bytesToBase64Url(new Uint8Array(signature));
        
        const jwt = `${headerB64}.${payloadB64}.${signatureB64}`;
        
        console.log("%c✅ PRO JWT GENERATED!", "color: green; font-weight: bold; font-size: 1.2em;");
        console.log("Token:", jwt);
        
        if (window.github && window.github.copilot && window.github.copilot.signIn) {
            await window.github.copilot.signIn(jwt);
            console.log("%c🚀 AUTHENTICATION INJECTED SUCCESSFULLY!", "color: cyan; font-weight: bold;");
            console.log("Please restart Cursor now.");
        } else {
            console.error("❌ github.copilot.signIn not found. Are you in the Cursor DevTools console?");
        }
    } catch (e) {
        console.error("❌ Error generating token:", e);
    }
})();
