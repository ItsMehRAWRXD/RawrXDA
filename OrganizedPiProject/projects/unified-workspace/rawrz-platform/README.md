# true

Generated production bundle with dynamic GUI that auto-builds from your engines.

- Port: 8080
- Auth token required: Yes
- Compression in app: Not detected
- Download route: `/api/download`

## Quickstart

### Launch Control Center (Web Interface) - RECOMMENDED

```bash
npm ci
cp .env.example .env  # set AUTH_TOKEN
npm start
```

**OR** on Windows:
```cmd
launch-control-center.bat
```

### Alternative Launch Methods

```bash
# Direct server launch
npm run server

# Development mode with hot reload
npm run dev

# CLI mode (for toggle management only)
npm run cli
```

### Default Access
- **Control Center**: http://localhost:8080/panel
- **Toggle Management**: http://localhost:8080/toggles
- **Health Check**: http://localhost:8080/health

## Dynamic GUI

The control panel at `/panel` automatically builds forms from engine manifests in `src/engines/`.

Each engine exports:
```js
module.exports = {
  id: 'crypto',
  name: 'Crypto',
  version: '1.0.0',
  actions: [
    {
      method: 'POST',
      path: '/api/hash',
      summary: 'Hash text',
      inputs: [
        { name: 'input', type: 'text', required: true, help: 'String to hash', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: false, options: ['sha256','sha512'] }
      ]
    }
  ]
};
```

The GUI will auto-render forms for each action. Just drop in new engines and refresh!

## Docker

```bash
docker compose up -d --build
```

## K8s

```bash
kubectl apply -f k8s/
```
