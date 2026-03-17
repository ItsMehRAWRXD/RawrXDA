# Multi-AI Aggregator Server

Server component for the Cursor Multi-AI Extension that provides REST API endpoints for multiple AI services.

## Features

- **Multi-Service Support**: Amazon Q, Claude, ChatGPT, Gemini, Kimi
- **REST API**: Clean endpoints for AI service integration
- **Health Monitoring**: Service availability checking
- **CORS Support**: Cross-origin requests for web clients
- **Error Handling**: Graceful error responses
- **Configuration**: JSON-based configuration

## Quick Start

1. **Install Dependencies**:
   ```bash
   cd D:\cursor-multi-ai-extension
   npm install express cors axios
   ```

2. **Set Environment Variables** (optional):
   ```bash
   # For Amazon Q (AWS Bedrock)
   set AWS_ACCESS_KEY_ID=your_aws_access_key
   set AWS_SECRET_ACCESS_KEY=your_aws_secret_key
   set AWS_REGION=us-east-1

   # For Anthropic Claude
   set ANTHROPIC_API_KEY=your_anthropic_api_key

   # For OpenAI ChatGPT
   set OPENAI_API_KEY=your_openai_api_key

   # For Google Gemini
   set GOOGLE_API_KEY=your_google_api_key

   # For Kimi AI
   set KIMI_API_KEY=your_kimi_api_key
   ```

3. **Start the Server**:
   ```bash
   node server.js
   ```

4. **Test the Server**:
   ```bash
   curl http://localhost:3003/health
   ```

## API Endpoints

### Health Check
```
GET /health
```
Returns server status and uptime information.

### Service Status
```
GET /services
```
Returns availability status of all AI services.

### Chat with Specific Service
```
POST /chat/:service
Content-Type: application/json

{
  "message": "Your question here"
}
```

**Supported Services:**
- `amazonq` - Amazon Q (AWS Bedrock)
- `claude` - Anthropic Claude
- `chatgpt` - OpenAI ChatGPT
- `gemini` - Google Gemini
- `kimi` - Kimi AI

### Chat with All Services
```
POST /chat/all
Content-Type: application/json

{
  "message": "Your question here"
}
```

### Configuration
```
GET /config
```
Returns server configuration and available services.

## Configuration

Edit `server-config.json` to customize:

```json
{
  "timeout": 30000,
  "maxRetries": 3,
  "enableLogging": true,
  "cors": {
    "origin": "*",
    "methods": ["GET", "POST"],
    "allowedHeaders": ["Content-Type", "Authorization"]
  }
}
```

## Integration with Extension

The server is designed to work with the Cursor Multi-AI Extension:

1. **Extension Configuration**: Set `multiAI.serverUrl` to `http://localhost:3003`
2. **Fallback Mode**: Extension will use built-in responses if server is unavailable
3. **Service Selection**: Choose preferred AI service in extension settings

## Development

### Running in Development Mode
```bash
npm install nodemon
npm run dev
```

### Testing
```bash
# Test health endpoint
curl http://localhost:3003/health

# Test service status
curl http://localhost:3003/services

# Test chat endpoint
curl -X POST http://localhost:3003/chat/chatgpt \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello, how are you?"}'
```

## Troubleshooting

### Common Issues

1. **Port Already in Use**:
   - Change port in server.js: `this.port = 3004`
   - Update extension configuration accordingly

2. **API Key Errors**:
   - Verify environment variables are set correctly
   - Check API key validity and permissions

3. **CORS Issues**:
   - Update CORS configuration in server-config.json
   - Ensure extension is making requests to correct origin

4. **Service Unavailable**:
   - Check network connectivity
   - Verify API endpoints are accessible
   - Review service-specific error messages

### Logs

Enable logging in server-config.json:
```json
{
  "enableLogging": true
}
```

## Security Considerations

- **API Keys**: Store securely in environment variables
- **CORS**: Restrict origins in production
- **Rate Limiting**: Consider implementing rate limiting for production use
- **Authentication**: Add authentication for production deployments

## License

MIT License - see LICENSE file for details.
