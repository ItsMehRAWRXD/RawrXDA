#pragma once

#include <string>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

// TLS/HTTPS support for API server
class TLSContext {
public:
    TLSContext();
    ~TLSContext();
    
    // Initialize TLS context with certificates
    bool Initialize(const std::string& cert_path, 
                   const std::string& key_path,
                   const std::string& ca_path = "");
    
    // Check if TLS is enabled and initialized
    bool IsInitialized() const { return initialized_; }
    
    // Accept TLS connection
    bool AcceptConnection(int socket_fd, void** tls_session);
    
    // Send data over TLS
    int Send(void* tls_session, const char* data, size_t length);
    
    // Receive data over TLS
    int Receive(void* tls_session, char* buffer, size_t length);
    
    // Close TLS connection
    void CloseConnection(void* tls_session);
    
    // Get last error message
    std::string GetLastError() const { return last_error_; }

private:
    bool initialized_;
    std::string last_error_;
    
#ifndef _WIN32
    SSL_CTX* ssl_ctx_;
#else
    void* ssl_ctx_; // Placeholder for Windows (could use Schannel or WinSSL)
#endif
    
    void LogTLSError(const std::string& operation);
};

// TLS Configuration Helper
struct TLSConfig {
    std::string cert_file;
    std::string key_file;
    std::string ca_file;
    bool verify_peer = false;
    std::vector<std::string> allowed_ciphers;
    std::string tls_version = "TLSv1.2"; // Minimum version
    
    bool IsValid() const {
        return !cert_file.empty() && !key_file.empty();
    }
};
