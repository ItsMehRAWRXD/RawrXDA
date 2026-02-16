# SSL Certificates for RawrXD Web

Place your SSL certificates here for production deployment:

- `cert.pem` - SSL certificate
- `key.pem` - Private key

**Development (self-signed):**
```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout key.pem -out cert.pem \
  -subj "/CN=localhost"
```

**Production:** Use Let's Encrypt or your CA.
