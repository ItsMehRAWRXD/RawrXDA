// net_masm_bridge.h
// C-callable bridge for MASM64 networking routines

#ifndef NET_MASM_BRIDGE_H
#define NET_MASM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// HTTP operations
long long HttpGet(const char* url, char* buffer, long long buffer_size);
long long HttpPost(const char* url, const char* data, long long data_size, char* buffer);

// WebSocket operations
long long WebSocketSend(void* socket_handle, const char* data, long long data_size);
long long WebSocketRecv(void* socket_handle, char* buffer, long long buffer_size);

// Low-level TCP operations
void* TcpConnect(const char* host, long long port);
void TcpClose(void* handle);
long long TcpSend(void* socket_handle, const char* data, long long data_size);
long long TcpSend(void* socket_handle, const char* data, long long data_size);
long long TcpRecv(void* socket_handle, char* buffer, long long buffer_size);

#ifdef __cplusplus
}
#endif

#endif // NET_MASM_BRIDGE_H
