# Copilot Auth Service (scaffold)

Lightweight Node/Express scaffold to perform GitHub OAuth and securely store tokens.

Usage

1. Copy `.env.example` to `.env` and set `CLIENT_ID`, `CLIENT_SECRET`, `REDIRECT_URI`, and `ENCRYPTION_KEY`.

2. Install and start:

```powershell
cd services\copilot-auth
npm install
npm start
```

3. Open `http://localhost:3000/authorize` to begin the OAuth flow.

Notes
- This scaffold uses GitHub's OAuth endpoints. Copilot-specific integrations may require additional steps or scopes. Use this as a secure starting point.
- Tokens are stored in `tokens.json` encrypted with AES-256-GCM using `ENCRYPTION_KEY`.
- For production, protect the `/token` endpoint and run behind TLS.
