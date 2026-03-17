# Universal Model Router - Security & API Key Management Guide

## Executive Summary

This guide provides comprehensive security best practices for deploying the Universal Model Router in production environments, with specific focus on API key management, data privacy, and secure communication.

---

## Security Principles

### ✅ DO
- Store API keys in environment variables or secure vaults
- Use HTTPS/TLS for all cloud communications
- Validate all inputs and responses
- Implement rate limiting and request throttling
- Monitor and log all API calls
- Rotate API keys regularly
- Use role-based access control
- Enable request signing and verification

### ❌ DON'T
- Hardcode API keys in source code
- Log sensitive data (API keys, user prompts)
- Use plain HTTP for production
- Trust client-supplied model names without validation
- Expose error messages containing sensitive data
- Share API keys between environments
- Use expired or revoked keys
- Disable SSL/TLS verification

---

## API Key Management

### Best Practices

#### 1. Secure Storage

```cpp
// ❌ WRONG: Hardcoded in code
const QString API_KEY = "sk-abc123def456...";

// ✅ CORRECT: Use environment variables
QString api_key = qgetenv("OPENAI_API_KEY");
if (api_key.isEmpty()) {
    qCritical() << "Error: OPENAI_API_KEY not set!";
    return false;
}

// ✅ BETTER: Use vault service
SecureVault vault("https://vault.company.com");
QString api_key = vault.getSecret("openai_api_key");
```

#### 2. Load Keys Safely

```cpp
// Load from environment
QMap<QString, QString> load_api_keys() {
    QMap<QString, QString> keys;
    
    keys["OPENAI"] = qgetenv("OPENAI_API_KEY");
    keys["ANTHROPIC"] = qgetenv("ANTHROPIC_API_KEY");
    keys["GOOGLE"] = qgetenv("GOOGLE_API_KEY");
    keys["MOONSHOT"] = qgetenv("MOONSHOT_API_KEY");
    keys["AZURE"] = qgetenv("AZURE_OPENAI_KEY");
    keys["AWS_KEY"] = qgetenv("AWS_ACCESS_KEY_ID");
    keys["AWS_SECRET"] = qgetenv("AWS_SECRET_ACCESS_KEY");
    
    // Validate all keys are set
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        if (it.value().isEmpty()) {
            qWarning() << "Missing API key:" << it.key();
            // Gracefully handle missing keys
            // (feature may be disabled, but app continues)
        }
    }
    
    return keys;
}
```

#### 3. Protect During Transmission

```cpp
// Enable secure communication
QSslConfiguration config = QSslConfiguration::defaultConfiguration();

// Force TLS 1.2 or higher
config.setProtocol(QSsl::TlsV1_2OrLater);

// Verify certificates
config.setPeerVerifyMode(QSslSocket::VerifyPeer);
config.setCaCertificates(QSslCertificate::fromPath(
    "/path/to/ca-bundle.crt",
    QSsl::Pem
));

// Apply to all network requests
QSslConfiguration::setDefaultConfiguration(config);

// In CloudApiClient:
void CloudApiClient::setup_ssl() {
    QSslConfiguration ssl_config = QSslConfiguration::defaultConfiguration();
    ssl_config.setProtocol(QSsl::TlsV1_2OrLater);
    ssl_config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    
    // Apply to network request
    QNetworkRequest request(url);
    request.setSslConfiguration(ssl_config);
}
```

#### 4. Prevent Leakage in Logs

```cpp
// ❌ WRONG: Logs API key
qDebug() << "Using API key:" << config.api_key;
qDebug() << "Request:" << buildRequest(config);

// ✅ CORRECT: Mask sensitive data
QString mask_api_key(const QString& key) {
    if (key.length() < 8) return "***";
    return key.left(4) + "..." + key.right(4);
}

qDebug() << "Using API key:" << mask_api_key(config.api_key);

// ✅ CORRECT: Don't log full prompts (may contain sensitive data)
auto res = ai->generate(sensitive_prompt, model);
qDebug() << "Generated response, tokens:" << res.metadata["tokens_used"];
// Don't log: sensitive_prompt or res.content
```

#### 5. Rotation Strategy

```cpp
struct ApiKeyRotation {
    QString api_key;
    QDateTime created_at;
    QDateTime expires_at;
    bool is_active = true;
    
    bool is_expired() const {
        return QDateTime::currentDateTime() > expires_at;
    }
};

class SecureApiKeyManager {
    QMap<QString, QVector<ApiKeyRotation>> key_history;
    
    void rotate_api_key(const QString& provider, const QString& new_key) {
        // Deactivate current key
        for (auto& rotation : key_history[provider]) {
            if (rotation.is_active) {
                rotation.is_active = false;
            }
        }
        
        // Add new key
        ApiKeyRotation new_rotation;
        new_rotation.api_key = new_key;
        new_rotation.created_at = QDateTime::currentDateTime();
        new_rotation.expires_at = new_rotation.created_at.addDays(90);
        new_rotation.is_active = true;
        
        key_history[provider].append(new_rotation);
        
        qInfo() << "Rotated API key for" << provider;
    }
    
    void check_expiring_keys() {
        for (auto it = key_history.begin(); it != key_history.end(); ++it) {
            for (const auto& rotation : it.value()) {
                if (rotation.is_active) {
                    int days_until_expiry = 
                        QDateTime::currentDateTime().daysTo(rotation.expires_at);
                    
                    if (days_until_expiry < 7) {
                        qWarning() << it.key() << "API key expires in" 
                                  << days_until_expiry << "days";
                    }
                }
            }
        }
    }
};
```

---

## Request & Response Security

### Input Validation

```cpp
// Validate model name
bool validate_model_name(const QString& model_name) {
    // Only allow alphanumeric, dash, underscore
    QRegularExpression valid_pattern("^[a-zA-Z0-9_-]+$");
    
    if (!valid_pattern.match(model_name).hasMatch()) {
        qWarning() << "Invalid model name:" << model_name;
        return false;
    }
    
    // Verify model exists in config
    auto available = ai->getAvailableModels();
    if (!available.contains(model_name)) {
        qWarning() << "Model not found:" << model_name;
        return false;
    }
    
    return true;
}

// Validate prompt length
bool validate_prompt(const QString& prompt) {
    const int MAX_PROMPT_LENGTH = 100000;  // 100k characters
    
    if (prompt.isEmpty()) {
        qWarning() << "Empty prompt";
        return false;
    }
    
    if (prompt.length() > MAX_PROMPT_LENGTH) {
        qWarning() << "Prompt too long:" << prompt.length() 
                  << "bytes (max:" << MAX_PROMPT_LENGTH << ")";
        return false;
    }
    
    return true;
}

// Safe generation with validation
GenerationResult safe_generate(const QString& prompt, const QString& model) {
    // Validate inputs
    if (!validate_prompt(prompt) || !validate_model_name(model)) {
        return GenerationResult{
            false,
            "Invalid input parameters"
        };
    }
    
    // Generate with error handling
    try {
        return ai->generate(prompt, model);
    } catch (const std::exception& e) {
        qCritical() << "Generation exception:" << e.what();
        return GenerationResult{
            false,
            "Generation failed"  // Don't expose internal error
        };
    }
}
```

### Response Sanitization

```cpp
// Sanitize response before displaying
QString sanitize_response(const QString& response) {
    // Remove any potentially dangerous content
    // (in case API returns malicious content - rare but possible)
    
    // Remove script tags
    QString sanitized = response;
    sanitized.remove(QRegularExpression("<script[^>]*>.*?</script>", 
                     QRegularExpression::CaseInsensitiveOption));
    
    // Remove event handlers
    sanitized.remove(QRegularExpression("on\\w+\\s*=", 
                     QRegularExpression::CaseInsensitiveOption));
    
    // HTML encode if displaying in web context
    // (Qt handles this automatically for widgets)
    
    return sanitized;
}

// Verify response is valid
bool validate_response(const GenerationResult& result) {
    // Check for suspiciously large responses
    if (result.content.length() > 1000000) {
        qWarning() << "Response suspiciously large:" << result.content.length();
        return false;
    }
    
    // Check for valid UTF-8
    if (!result.content.isEmpty()) {
        auto encoded = result.content.toUtf8();
        if (!QString::fromUtf8(encoded, encoded.length()).isValid()) {
            qWarning() << "Invalid UTF-8 in response";
            return false;
        }
    }
    
    return true;
}
```

---

## Network Security

### HTTPS/TLS Configuration

```cpp
void setup_secure_network() {
    // Require HTTPS for all cloud connections
    
    // In CloudApiClient::generate():
    QUrl url = build_endpoint_url();  // Should be https://
    
    if (url.scheme() != "https") {
        qCritical() << "Non-HTTPS URL detected:" << url.toString();
        return {};
    }
    
    // Configure SSL
    QSslConfiguration ssl_config = QSslConfiguration::defaultConfiguration();
    
    // Minimum TLS 1.2
    ssl_config.setProtocol(QSsl::TlsV1_2OrLater);
    
    // Verify peer certificate
    ssl_config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    
    // Set CA certificates
    QList<QSslCertificate> ca_certs = QSslCertificate::fromPath(
        ":/certs/ca-bundle.crt"
    );
    if (!ca_certs.isEmpty()) {
        ssl_config.setCaCertificates(ca_certs);
    }
    
    QNetworkRequest request(url);
    request.setSslConfiguration(ssl_config);
    
    return request;
}

// Handle SSL errors gracefully
void handle_ssl_error(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
        qCritical() << "SSL Handshake Failed!";
        qCritical() << "SSL Errors:" << reply->sslErrors();
        
        // Don't ignore errors - fail securely
        reply->abort();
        
        return;
    }
}
```

---

## Data Privacy & Compliance

### GDPR Compliance

```cpp
// Track data usage for GDPR compliance
struct DataUsageLog {
    QDateTime timestamp;
    QString user_id;  // Anonymized hash
    QString model_name;
    int prompt_length;  // Not the actual prompt
    QString response_hash;  // Not the actual response
    int tokens_used;
};

class GdprCompliantLogger {
    QVector<DataUsageLog> logs;
    
    void log_request(const QString& user_id, 
                     const QString& prompt,
                     const GenerationResult& result) {
        DataUsageLog log;
        log.timestamp = QDateTime::currentDateTime();
        log.user_id = hash_user_id(user_id);  // Anonymize
        log.model_name = result.model_name;
        log.prompt_length = prompt.length();  // Length only, not content
        log.response_hash = hash_response(result.content);  // Hash only
        log.tokens_used = result.metadata["tokens_used"].toInt();
        
        logs.append(log);
    }
    
    QString hash_user_id(const QString& user_id) {
        // Use HMAC-SHA256 with salt
        // This is one-way and irreversible
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(user_id.toUtf8());
        hash.addData("salt-value");  // Use same salt for consistency
        return hash.result().toHex();
    }
    
    void delete_user_data(const QString& user_id) {
        QString hashed = hash_user_id(user_id);
        
        // Remove all logs for this user
        for (int i = logs.size() - 1; i >= 0; --i) {
            if (logs[i].user_id == hashed) {
                logs.removeAt(i);
            }
        }
        
        qInfo() << "Deleted all data for user:" << hashed;
    }
};
```

---

## Security Monitoring & Auditing

### Audit Logging

```cpp
struct AuditLog {
    QDateTime timestamp;
    QString action;         // "generate", "rotate_key", "access_denied"
    QString user_id;        // Hash
    QString resource;       // Model name
    bool success = false;
    QString error_message;
};

class AuditLogger {
    QFile audit_log;
    QMutex log_mutex;
    
    void log_action(const AuditLog& entry) {
        QMutexLocker lock(&log_mutex);
        
        audit_log.open(QIODevice::Append | QIODevice::Text);
        QTextStream stream(&audit_log);
        
        stream << entry.timestamp.toString(Qt::ISODate) << " | "
               << entry.action << " | "
               << entry.user_id << " | "
               << entry.resource << " | "
               << (entry.success ? "OK" : "FAILED") << " | "
               << entry.error_message << "\n";
        
        audit_log.close();
    }
};
```

---

## Security Checklist

- [ ] No API keys in source code
- [ ] All keys in environment variables
- [ ] HTTPS/TLS enabled for all cloud connections
- [ ] Input validation on all user inputs
- [ ] Output sanitization before display
- [ ] Audit logging enabled
- [ ] Rate limiting configured
- [ ] GDPR compliance verified
- [ ] SSL/TLS 1.2+
- [ ] Key rotation schedule established
- [ ] Access control implemented
- [ ] Security monitoring enabled

---

## Conclusion

Security is not optional - it's essential for production deployment:

✅ Store secrets securely (environment variables, vaults)
✅ Use HTTPS/TLS 1.2+ for all communications
✅ Validate all inputs, sanitize all outputs
✅ Implement audit logging and monitoring
✅ Rotate API keys regularly (every 90 days)
✅ Follow principle of least privilege
✅ Maintain compliance with GDPR, HIPAA, etc.

With these practices, your Universal Model Router deployment will be secure and production-ready.
