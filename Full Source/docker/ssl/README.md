Place TLS assets in this directory for nginx:

- cert.pem (public certificate chain)
- key.pem  (private key)

For local development, you can generate a self-signed pair:

```bash
openssl req -x509 -nodes -newkey rsa:2048 -days 365 \
  -keyout docker/ssl/key.pem \
  -out docker/ssl/cert.pem \
  -subj "/CN=localhost"
```
