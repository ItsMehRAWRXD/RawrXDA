# Production API Server - Complete Implementation

A comprehensive, production-ready API server for RawrXD with TLS/SSL, REST, GraphQL, structured error handling, and OIDC/JWKS authentication.

## Features

### 1. TLS/SSL Configuration ✓

**Secure connections with certificate management**

```cpp
TLSConfig tlsConfig;
tlsConfig.enabled = true;
tlsConfig.certificatePath = "/etc/certs/server.crt";
tlsConfig.privateKeyPath = "/etc/certs/server.key";
tlsConfig.minTLSVersion = 12; // TLS 1.2+
tlsConfig.cipherSuites = {
    "ECDHE-RSA-AES256-GCM-SHA384",
    "ECDHE-RSA-AES128-GCM-SHA256",
    "DHE-RSA-AES256-GCM-SHA384"
};

server->configureTLS(tlsConfig);
```

**Features:**
- Automatic certificate loading and validation
- TLS 1.2+ enforcement
- Modern cipher suite support
- Configurable certificate rotation
- SSL/TLS error handling and logging

### 2. OIDC/JWKS Authentication ✓

**Enterprise-grade token validation and user authentication**

```cpp
OIDCConfig oidcConfig;
oidcConfig.enabled = true;
oidcConfig.issuerUrl = "https://auth.example.com";
oidcConfig.clientId = "rawrxd-app";
oidcConfig.jwksUrl = "https://auth.example.com/.well-known/jwks.json";
oidcConfig.audience = "https://api.example.com";
oidcConfig.tokenExpirationSeconds = 3600;

server->configureOIDC(oidcConfig);
```

**Features:**
- Automatic JWKS fetching and caching
- JWT token validation with signature verification
- Claim validation (issuer, audience, expiration)
- Token cache for performance
- Automatic token refresh
- Claims extraction and validation

### 3. REST API Endpoints ✓

**Standard HTTP endpoints with authentication**

```cpp
// Public endpoint
server->registerRESTEndpoint("GET", "/health",
    [](const RequestContext& ctx) {
        ResponseBuilder response;
        QJsonObject data;
        data["status"] = "healthy";
        return response.withData(data).build();
    },
    false // No authentication required
);

// Protected endpoint
server->registerRESTEndpoint("POST", "/api/users",
    [](const RequestContext& ctx) {
        // Handle user creation
        ResponseBuilder response;
        QJsonObject userData;
        userData["id"] = "usr_123";
        return response.withData(userData).build();
    },
    true // Authentication required
);
```

**Supported HTTP Methods:**
- GET - Retrieve resources
- POST - Create resources
- PUT - Update resources
- DELETE - Delete resources
- OPTIONS - CORS preflight
- PATCH - Partial updates

### 4. GraphQL Endpoints ✓

**Modern GraphQL queries and mutations with authentication**

```cpp
// Register GraphQL Query
server->registerGraphQLQuery("getUser",
    "query GetUser($id: ID!) { user(id: $id) { id username email } }",
    [](const QJsonObject& args, const AuthContext& auth) {
        QJsonObject user;
        user["id"] = args["id"];
        user["username"] = "john_doe";
        user["email"] = "john@example.com";
        
        QJsonObject result;
        result["user"] = user;
        return result;
    },
    true // Authentication required
);

// Register GraphQL Mutation
server->registerGraphQLMutation("createUser",
    "mutation CreateUser($username: String!, $email: String!) { ... }",
    [](const QJsonObject& args, const AuthContext& auth) {
        // Handle user creation
        QJsonObject user;
        user["id"] = "usr_new";
        user["username"] = args["username"];
        user["email"] = args["email"];
        
        QJsonObject result;
        result["createUser"] = user;
        return result;
    },
    true // Authentication required
);
```

**Supported Operations:**
- Queries - Read-only operations
- Mutations - Data-modifying operations
- Subscriptions - Real-time updates (planned)
- Introspection - Schema discovery

### 5. Structured Error Responses ✓

**Consistent error handling across all endpoints**

```cpp
// Automatically generated error response
ErrorResponse err = server->createError(
    404,                    // HTTP Status Code
    "User not found",       // User-friendly message
    "NotFound",            // Error type
    requestId              // Request ID for tracing
);

// JSON Response Format:
{
    "code": 404,
    "message": "User not found",
    "errorType": "NotFound",
    "requestId": "550e8400-e29b-41d4-a716-446655440000",
    "timestamp": 1702638000000,
    "details": {
        "resource": "User",
        "resourceId": "usr_123"
    }
}
```

**Error Types:**
- `ValidationError` - Input validation failed
- `AuthenticationError` - Missing or invalid credentials
- `AuthorizationError` - Insufficient permissions
- `NotFound` - Resource not found
- `ConflictError` - Resource already exists
- `RateLimitExceeded` - Too many requests
- `InternalError` - Server error
- `ServiceUnavailable` - Service temporarily down

### 6. Request Validation ✓

**Automatic validation of requests and responses**

```cpp
// Middleware for content type validation
server->use([](RequestContext& ctx) {
    if (ctx.method == "POST" || ctx.method == "PUT") {
        QString contentType = ctx.headers["Content-Type"].toString();
        if (!contentType.contains("application/json")) {
            return false; // Reject request
        }
    }
    return true; // Continue processing
});

// Validate request structure
ErrorResponse validation = server->validateRequest(ctx);
if (validation.code != 200) {
    // Return validation error
}
```

### 7. Rate Limiting ✓

**Per-IP and global rate limiting**

**Configuration:**
```json
{
    "rateLimit": {
        "enabled": true,
        "requestsPerMinute": 1000,
        "requestsPerSecond": 100,
        "burstSize": 150,
        "enablePerIPLimiting": true
    }
}
```

**Response When Rate Limited:**
```json
{
    "code": 429,
    "message": "Too Many Requests",
    "errorType": "RateLimitExceeded",
    "requestId": "550e8400-e29b-41d4-a716-446655440000",
    "timestamp": 1702638000000
}
```

### 8. Middleware Pipeline ✓

**Execute custom logic for all requests**

```cpp
// Authentication middleware
server->use([server](RequestContext& ctx) {
    if (ctx.path.startsWith("/api/")) {
        QString token = ctx.headers["Authorization"].toString();
        AuthContext auth;
        if (!server->validateJWT(token, auth)) {
            ctx.auth.authenticated = false;
            return false; // Reject
        }
        ctx.auth = auth;
    }
    return true; // Continue
});

// CORS middleware
server->use([](RequestContext& ctx) {
    if (ctx.method == "OPTIONS") {
        // Handle CORS preflight
    }
    return true;
});

// Logging middleware
server->use([](RequestContext& ctx) {
    qInfo() << ctx.method << ctx.path << "from" << ctx.remoteAddress;
    return true;
});
```

### 9. Metrics & Monitoring ✓

**Track server performance and usage**

```cpp
// Get server statistics
QJsonObject stats = server->getServerStats();
/*
{
    "isRunning": true,
    "port": 8443,
    "totalRequests": 12345,
    "successfulRequests": 12100,
    "failedRequests": 245,
    "averageLatencyMs": 45,
    "tlsEnabled": true,
    "oidcEnabled": true,
    "cachedTokens": 234,
    "registeredRESTEndpoints": 8,
    "registeredGraphQLQueries": 5,
    "registeredGraphQLMutations": 3
}
*/
```

**Metrics Available:**
- Total requests processed
- Successful vs failed requests
- Average response latency
- Active token cache size
- Registered endpoints
- TLS/OIDC status

### 10. Security Features ✓

**Production-ready security**

- **HTTPS/TLS 1.2+** - Encrypted connections
- **JWT Validation** - Token signature and claim verification
- **Token Caching** - Improved performance with security
- **Rate Limiting** - DDoS protection
- **CORS Support** - Cross-origin request handling
- **Security Headers** - XSS, clickjacking, sniffing prevention
- **Input Validation** - Automatic request validation
- **Request ID Tracing** - Track requests across services

## Usage Examples

### Example 1: Start Server with TLS and OIDC

```cpp
auto server = std::make_unique<ProductionAPIServer>();

// Configure TLS
TLSConfig tls;
tls.enabled = true;
tls.certificatePath = "./certs/server.crt";
tls.privateKeyPath = "./certs/server.key";
server->configureTLS(tls);

// Configure OIDC
OIDCConfig oidc;
oidc.enabled = true;
oidc.issuerUrl = "https://auth.example.com";
oidc.jwksUrl = "https://auth.example.com/.well-known/jwks.json";
server->configureOIDC(oidc);

// Start server
server->start(8443);
```

### Example 2: Register REST Endpoint with Error Handling

```cpp
server->registerRESTEndpoint("GET", "/api/users/*", 
    [](const RequestContext& ctx) {
        ResponseBuilder response;
        response.withRequestId(ctx.requestId);
        
        QString userId = ctx.path.split("/").last();
        if (userId.isEmpty()) {
            return response
                .withError(400, "User ID required", "ValidationError")
                .build();
        }
        
        // Fetch user
        QJsonObject user;
        user["id"] = userId;
        user["username"] = "john";
        user["email"] = "john@example.com";
        
        response.withData(user);
        return response.build();
    },
    true // Requires authentication
);
```

### Example 3: Register GraphQL Query

```cpp
server->registerGraphQLQuery("listUsers",
    "query ListUsers { users { id username email } }",
    [](const QJsonObject& args, const AuthContext& auth) {
        QJsonArray users;
        
        QJsonObject user1;
        user1["id"] = "usr_001";
        user1["username"] = "alice";
        user1["email"] = "alice@example.com";
        users.append(user1);
        
        QJsonObject result;
        result["users"] = users;
        return result;
    },
    true // Requires authentication
);
```

### Example 4: Handle Requests with Middleware

```cpp
// Add logging middleware
server->use([](RequestContext& ctx) {
    qInfo() << QString("[%1] %2 %3")
        .arg(ctx.requestId)
        .arg(ctx.method)
        .arg(ctx.path);
    return true;
});

// Add validation middleware
server->use([](RequestContext& ctx) {
    if (ctx.body.isEmpty() && (ctx.method == "POST" || ctx.method == "PUT")) {
        return false; // Reject empty body
    }
    return true;
});
```

## Configuration Files

### Server Configuration (JSON)

```json
{
    "server": {
        "port": 8443,
        "logRequests": true,
        "metricsEnabled": true
    },
    "tls": {
        "enabled": true,
        "certificatePath": "/etc/certs/server.crt",
        "privateKeyPath": "/etc/certs/server.key",
        "minTLSVersion": 12
    },
    "oidc": {
        "enabled": true,
        "issuerUrl": "https://auth.example.com",
        "jwksUrl": "https://auth.example.com/.well-known/jwks.json",
        "audience": "https://api.example.com"
    }
}
```

## API Endpoints Summary

| Method | Path | Auth Required | Description |
|--------|------|---------------|-------------|
| GET | `/health` | No | Health check |
| GET | `/status` | Yes | Server status |
| GET | `/metrics` | Yes | Prometheus metrics |
| POST | `/graphql` | Yes | GraphQL endpoint |
| GET | `/api/users` | Yes | List users |
| POST | `/api/users` | Yes | Create user |
| GET | `/api/users/{id}` | Yes | Get user |
| PUT | `/api/users/{id}` | Yes | Update user |
| DELETE | `/api/users/{id}` | Yes | Delete user |

## Security Best Practices

1. **Always enable HTTPS/TLS** in production
2. **Use strong cipher suites** (ECDHE-based)
3. **Enable OIDC authentication** for sensitive endpoints
4. **Implement rate limiting** to prevent abuse
5. **Validate all inputs** before processing
6. **Log security events** for audit trails
7. **Rotate certificates** regularly
8. **Monitor token cache** for suspicious activity
9. **Use environment variables** for secrets
10. **Enable request ID tracking** for debugging

## Performance Considerations

- Token cache reduces JWKS validation overhead
- Rate limiting prevents resource exhaustion
- Middleware short-circuits invalid requests early
- GraphQL reduces over-fetching compared to REST
- Metrics enable performance monitoring
- Connection pooling for database operations

## Troubleshooting

### TLS Certificate Errors
```cpp
// Check certificate validity
bool valid = server->reloadTLSCertificates();
if (!valid) {
    // Certificate validation failed
}
```

### OIDC Token Validation Failures
```cpp
// Refresh OIDC configuration
bool refreshed = server->refreshOIDCConfiguration();
if (!refreshed) {
    // JWKS refresh failed
}
```

### Rate Limiting Issues
```cpp
// Check current request rate
QJsonObject stats = server->getServerStats();
int avgLatency = stats["averageLatencyMs"].toInt();
```

## Dependencies

- Qt 6.7.3 or later (Core, Network, Concurrent)
- OpenSSL 1.1.1+ (for TLS)
- Standard C++20

## License

Part of the RawrXD Agentic IDE project.
