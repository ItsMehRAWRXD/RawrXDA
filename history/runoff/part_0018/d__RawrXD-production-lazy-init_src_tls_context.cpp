#include "tls_context.h"
#include <iostream>
#include <fstream>

TLSContext::TLSContext() : initialized_(false), ssl_ctx_(nullptr) {
}

TLSContext::~TLSContext() {
#ifndef _WIN32
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
    }
#endif
}

bool TLSContext::Initialize(const std::string& cert_path, 
                            const std::string& key_path,
                            const std::string& ca_path) {
    if (initialized_) {
        return true;
    }
    
#ifdef _WIN32
    // Windows implementation would use Schannel/SChannel
    // Placeholder implementation
    last_error_ = "TLS not yet implemented on Windows (use Schannel)";
    return false;
#else
    // Initialize OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    // Create SSL context
    const SSL_METHOD* method = TLS_server_method();
    ssl_ctx_ = SSL_CTX_new(method);
    
    if (!ssl_ctx_) {
        last_error_ = "Unable to create SSL context";
        LogTLSError("SSL_CTX_new");
        return false;
    }
    
    // Set minimum TLS version
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);
    
    // Load certificate
    if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        last_error_ = "Failed to load certificate: " + cert_path;
        LogTLSError("SSL_CTX_use_certificate_file");
        return false;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        last_error_ = "Failed to load private key: " + key_path;
        LogTLSError("SSL_CTX_use_PrivateKey_file");
        return false;
    }
    
    // Verify private key
    if (!SSL_CTX_check_private_key(ssl_ctx_)) {
        last_error_ = "Private key does not match certificate";
        LogTLSError("SSL_CTX_check_private_key");
        return false;
    }
    
    // Load CA if provided
    if (!ca_path.empty()) {
        if (SSL_CTX_load_verify_locations(ssl_ctx_, ca_path.c_str(), nullptr) <= 0) {
            last_error_ = "Failed to load CA certificate: " + ca_path;
            LogTLSError("SSL_CTX_load_verify_locations");
            return false;
        }
    }
    
    initialized_ = true;
    return true;
#endif
}

bool TLSContext::AcceptConnection(int socket_fd, void** tls_session) {
#ifdef _WIN32
    return false;
#else
    if (!initialized_) {
        last_error_ = "TLS context not initialized";
        return false;
    }
    
    SSL* ssl = SSL_new(ssl_ctx_);
    if (!ssl) {
        last_error_ = "Failed to create SSL object";
        LogTLSError("SSL_new");
        return false;
    }
    
    SSL_set_fd(ssl, socket_fd);
    
    if (SSL_accept(ssl) <= 0) {
        last_error_ = "SSL handshake failed";
        LogTLSError("SSL_accept");
        SSL_free(ssl);
        return false;
    }
    
    *tls_session = ssl;
    return true;
#endif
}

int TLSContext::Send(void* tls_session, const char* data, size_t length) {
#ifdef _WIN32
    return -1;
#else
    if (!tls_session) {
        return -1;
    }
    
    SSL* ssl = static_cast<SSL*>(tls_session);
    return SSL_write(ssl, data, static_cast<int>(length));
#endif
}

int TLSContext::Receive(void* tls_session, char* buffer, size_t length) {
#ifdef _WIN32
    return -1;
#else
    if (!tls_session) {
        return -1;
    }
    
    SSL* ssl = static_cast<SSL*>(tls_session);
    return SSL_read(ssl, buffer, static_cast<int>(length));
#endif
}

void TLSContext::CloseConnection(void* tls_session) {
#ifndef _WIN32
    if (tls_session) {
        SSL* ssl = static_cast<SSL*>(tls_session);
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
#endif
}

void TLSContext::LogTLSError(const std::string& operation) {
#ifndef _WIN32
    unsigned long err = ERR_get_error();
    char err_buf[256];
    ERR_error_string_n(err, err_buf, sizeof(err_buf));
    std::cerr << "[TLS] " << operation << " failed: " << err_buf << std::endl;
#endif
}
