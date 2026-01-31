# RawrZ Security Platform - Security Configuration

## Overview

The RawrZ Security Platform has been enhanced with comprehensive security measures to protect against common web vulnerabilities and ensure secure operation of the botnet management system.

## Security Features Implemented

### 1. Enhanced Authentication & Session Management

- **Bearer Token Authentication**: Secure API access using Bearer tokens
- **Session Management**: Automatic session creation and validation with 30-minute timeout
- **Session Cleanup**: Automatic cleanup of expired sessions every 5 minutes
- **IP Tracking**: All sessions track client IP addresses for security monitoring

### 2. Rate Limiting

- **General Rate Limiting**: 100 requests per 15 minutes per IP
- **API Rate Limiting**: 60 requests per minute per IP for API endpoints
- **Authentication Rate Limiting**: 5 authentication attempts per 15 minutes per IP
- **WebSocket Rate Limiting**: Maximum 10 WebSocket connections per minute per IP

### 3. Input Validation & Sanitization

- **Express Validator**: Comprehensive input validation for all API endpoints
- **Payload Size Limits**: 5MB limit for JSON payloads
- **XSS Protection**: Detection and blocking of potential XSS attempts
- **Command Validation**: Strict validation of botnet commands and parameters

### 4. Security Headers (Helmet.js)

- **Content Security Policy (CSP)**: Strict CSP with minimal allowed sources
- **X-Frame-Options**: Set to 'DENY' to prevent clickjacking
- **X-Content-Type-Options**: 'nosniff' to prevent MIME type sniffing
- **X-XSS-Protection**: Enabled for additional XSS protection
- **HSTS**: HTTP Strict Transport Security with 1-year max-age
- **Referrer Policy**: 'strict-origin-when-cross-origin'
- **Cross-Origin Policies**: Strict CORS, COEP, COOP, and CORP policies

### 5. CORS Configuration

- **Origin Validation**: Only allows specified origins (configurable via environment)
- **Credential Support**: Secure credential handling
- **Method Restrictions**: Limited to GET, POST, PUT, DELETE, OPTIONS
- **Header Control**: Strict control over allowed and exposed headers

### 6. WebSocket Security

- **Connection Rate Limiting**: Prevents WebSocket flooding attacks
- **Message Size Validation**: 1MB limit on WebSocket messages
- **Message Structure Validation**: Strict validation of message format
- **Connection Cleanup**: Automatic cleanup of inactive connections

### 7. Security Logging

- **Comprehensive Logging**: All security events are logged with timestamps
- **IP Tracking**: All requests include IP address logging
- **Session Tracking**: Security events linked to session IDs
- **Performance Monitoring**: Long-running requests are flagged

### 8. Error Handling

- **Secure Error Messages**: No sensitive information in error responses
- **Error Logging**: All errors logged with security context
- **Graceful Degradation**: System continues operating despite individual failures

## Configuration

### Environment Variables

```bash
# Authentication
AUTH_TOKEN=your_secure_token_here

# CORS Origins (comma-separated)
ALLOWED_ORIGINS=http://localhost:8080,http://127.0.0.1:8080

# Server Configuration
PORT=8080
```

### Security Headers Configuration

The following security headers are automatically applied:

```javascript
{
  "Content-Security-Policy": "default-src 'self'; script-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; ...",
  "X-Frame-Options": "DENY",
  "X-Content-Type-Options": "nosniff",
  "X-XSS-Protection": "1; mode=block",
  "Strict-Transport-Security": "max-age=31536000; includeSubDomains; preload",
  "Referrer-Policy": "strict-origin-when-cross-origin"
}
```

## API Security

### Protected Endpoints

All sensitive endpoints require authentication:
- `/hash` - Cryptographic hashing
- `/encrypt` - Data encryption
- `/decrypt` - Data decryption
- `/api/botnet/*` - Botnet management
- `/api/http-bot-*` - HTTP bot operations
- `/api/irc-bot-*` - IRC bot operations

### Input Validation Rules

- **Hash Input**: 1-10,000 characters, valid algorithms only
- **Encryption Input**: 1-50,000 characters, valid algorithms only
- **Botnet Commands**: Strict whitelist of allowed actions
- **Target Validation**: 1-100 character limits
- **Parameter Validation**: Object structure validation

## WebSocket Security

### Connection Security

- Rate limiting: 10 connections per minute per IP
- Message size limit: 1MB per message
- Structure validation: All messages must have valid type and data
- Automatic cleanup: Inactive connections removed after 10 minutes

### Message Types

Valid WebSocket message types:
- `handshake` - Bot connection handshake
- `command_result` - Command execution results
- `heartbeat` - Keep-alive messages
- `irc_connect` - IRC connection requests
- `irc_message` - IRC message handling
- `irc_command` - IRC command execution

## Monitoring & Logging

### Security Events Logged

- Authentication attempts (success/failure)
- Rate limit violations
- Invalid input attempts
- WebSocket connection attempts
- Command executions
- Error conditions
- Session management events

### Log Format

```json
{
  "timestamp": "2024-01-20T10:30:00.000Z",
  "level": "SECURITY",
  "event": "rate_limit_exceeded",
  "ip": "192.168.1.100",
  "userAgent": "Mozilla/5.0...",
  "sessionId": "abc123...",
  "details": "API rate limit exceeded"
}
```

## Best Practices

### For Administrators

1. **Set Strong AUTH_TOKEN**: Use a cryptographically secure random token
2. **Configure ALLOWED_ORIGINS**: Restrict CORS to known domains only
3. **Monitor Logs**: Regularly review security logs for suspicious activity
4. **Update Dependencies**: Keep all security packages updated
5. **Use HTTPS**: Always use HTTPS in production environments

### For Developers

1. **Validate All Input**: Never trust client input
2. **Use Authentication**: Always require authentication for sensitive operations
3. **Log Security Events**: Log all security-relevant events
4. **Handle Errors Securely**: Don't expose sensitive information in errors
5. **Test Security**: Regularly test security configurations

## Security Testing

### Recommended Tests

1. **Rate Limiting**: Test rate limit enforcement
2. **Input Validation**: Test with malicious input
3. **Authentication**: Test authentication bypass attempts
4. **CORS**: Test cross-origin request handling
5. **WebSocket**: Test WebSocket security measures
6. **Headers**: Verify security headers are present

### Tools

- OWASP ZAP for security scanning
- Burp Suite for penetration testing
- Custom scripts for rate limiting tests
- Browser dev tools for header verification

## Incident Response

### Security Incident Procedures

1. **Immediate Response**: Block suspicious IPs if necessary
2. **Log Analysis**: Review security logs for attack patterns
3. **System Check**: Verify system integrity
4. **Documentation**: Document incident details
5. **Prevention**: Update security measures if needed

### Contact Information

For security issues, contact the system administrator immediately.

## Updates & Maintenance

### Regular Security Tasks

- Weekly: Review security logs
- Monthly: Update dependencies
- Quarterly: Security configuration review
- Annually: Full security audit

### Version History

- v1.0.0: Initial security implementation
- v1.1.0: Enhanced rate limiting and session management
- v1.2.0: Comprehensive input validation and WebSocket security

---

**Note**: This security configuration is designed for a controlled environment. Additional security measures may be required for production deployments in untrusted networks.
