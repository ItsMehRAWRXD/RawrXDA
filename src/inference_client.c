/**
 * inference_client.c - Minimal WinSock HTTP inference client
 *
 * Direct raw-socket HTTP POST to llama-server OpenAI-compatible API
 * Parses SSE (Server-Sent Events) stream for token-by-token delivery
 *
 * Zero dependencies beyond ws2_32.dll (WinSock2)
 * Compiles with: cl /O2 /LD inference_client.c /link ws2_32.lib /DEF:inference_client.def
 *            or: gcc -shared -O2 -o inference_client.dll inference_client.c -lws2_32
 */

#define INFERENCE_CLIENT_EXPORTS
#include "inference_client.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

/* ── Internal helpers ─────────────────────────────────────────────── */

static volatile LONG g_wsa_init = 0;

static void set_error(InferResult* r, int code, const char* msg) {
    if (!r) return;
    r->status = code;
    strncpy(r->error, msg, sizeof(r->error) - 1);
    r->error[sizeof(r->error) - 1] = '\0';
}

static void clear_result(InferResult* r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

/**
 * Build the HTTP POST request for chat completions.
 * Returns bytes written to buf (excluding null terminator).
 * The JSON body uses minimal escaping for the user prompt.
 */
static int build_request(char* buf, int buf_size,
                         const char* host, uint16_t port,
                         const char* prompt, int max_tokens,
                         float temperature, int stream)
{
    /* JSON-escape the prompt: escape quotes, backslashes, and control chars */
    char escaped[4096];
    int ei = 0;
    for (int i = 0; prompt[i] && ei < (int)sizeof(escaped) - 6; i++) {
        char c = prompt[i];
        if (c == '"')       { escaped[ei++] = '\\'; escaped[ei++] = '"'; }
        else if (c == '\\') { escaped[ei++] = '\\'; escaped[ei++] = '\\'; }
        else if (c == '\n') { escaped[ei++] = '\\'; escaped[ei++] = 'n'; }
        else if (c == '\r') { escaped[ei++] = '\\'; escaped[ei++] = 'r'; }
        else if (c == '\t') { escaped[ei++] = '\\'; escaped[ei++] = 't'; }
        else                { escaped[ei++] = c; }
    }
    escaped[ei] = '\0';

    /* Build JSON body */
    char body[8192];
    int body_len = snprintf(body, sizeof(body),
        "{\"model\":\"default\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"max_tokens\":%d,"
        "\"temperature\":%.2f,"
        "\"stream\":%s}",
        escaped, max_tokens, temperature, stream ? "true" : "false");

    /* Build HTTP request */
    int total = snprintf(buf, buf_size,
        "POST /v1/chat/completions HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        host, port, body_len, body);

    return total;
}

/**
 * Connect to the server via raw TCP socket.
 * Returns INVALID_SOCKET on failure.
 */
static SOCKET connect_to_server(const char* host, uint16_t port, int timeout_ms,
                                InferResult* result)
{
    struct sockaddr_in addr;
    SOCKET sock;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        set_error(result, INFER_ERR_CONNECT, "socket() failed");
        return INVALID_SOCKET;
    }

    /* Set recv timeout */
    if (timeout_ms > 0) {
        DWORD tv = (DWORD)timeout_ms;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    }

    /* Disable Nagle for lower latency */
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        char msg[128];
        snprintf(msg, sizeof(msg), "connect() to %s:%d failed (WSA %d)", host, port, WSAGetLastError());
        set_error(result, INFER_ERR_CONNECT, msg);
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

/**
 * Extract the "content" field value from an SSE data JSON line.
 * Looks for: "content":"<value>"
 * Returns pointer into buf and sets *out_len. Returns NULL if not found.
 */
static const char* extract_content(const char* json, int* out_len) {
    /* Find "content":" */
    const char* key = "\"content\":\"";
    const char* p = strstr(json, key);
    if (!p) return NULL;

    p += strlen(key); /* now points to first char of content value */

    /* Find closing quote (handle escaped quotes) */
    const char* start = p;
    while (*p) {
        if (*p == '\\' && *(p + 1)) {
            p += 2; /* skip escaped char */
            continue;
        }
        if (*p == '"') break;
        p++;
    }

    *out_len = (int)(p - start);
    return start;
}

/**
 * Unescape a JSON string segment in-place.
 * Returns the new length.
 */
static int json_unescape(char* buf, int len) {
    int r = 0, w = 0;
    while (r < len) {
        if (buf[r] == '\\' && r + 1 < len) {
            char c = buf[r + 1];
            switch (c) {
                case 'n':  buf[w++] = '\n'; r += 2; break;
                case 'r':  buf[w++] = '\r'; r += 2; break;
                case 't':  buf[w++] = '\t'; r += 2; break;
                case '"':  buf[w++] = '"';  r += 2; break;
                case '\\': buf[w++] = '\\'; r += 2; break;
                case '/':  buf[w++] = '/';  r += 2; break;
                default:   buf[w++] = buf[r++]; break;
            }
        } else {
            buf[w++] = buf[r++];
        }
    }
    return w;
}

/* ── Public API ───────────────────────────────────────────────────── */

INFER_API int __stdcall Infer_Init(void) {
    if (InterlockedCompareExchange(&g_wsa_init, 1, 0) == 0) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            InterlockedExchange(&g_wsa_init, 0);
            return INFER_ERR_WSASTARTUP;
        }
    }
    return INFER_OK;
}

INFER_API void __stdcall Infer_Shutdown(void) {
    if (InterlockedCompareExchange(&g_wsa_init, 0, 1) == 1) {
        WSACleanup();
    }
}

INFER_API void __stdcall Infer_DefaultConfig(InferConfig* config) {
    if (!config) return;
    config->host       = "127.0.0.1";
    config->port       = 8081;
    config->_pad       = 0;
    config->max_tokens = 256;
    config->temperature = 0.0f;
    config->timeout_ms = 30000;
}

INFER_API int __stdcall Infer_Complete(
    const char*        prompt,
    const InferConfig* config,
    InferResult*       result)
{
    InferConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        Infer_DefaultConfig(&cfg);
    }
    if (!cfg.host) cfg.host = "127.0.0.1";

    clear_result(result);

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    /* Build non-streaming request */
    char request[16384];
    int req_len = build_request(request, sizeof(request),
                                cfg.host, cfg.port,
                                prompt, cfg.max_tokens,
                                cfg.temperature, 0 /* stream=false */);

    /* Connect */
    SOCKET sock = connect_to_server(cfg.host, cfg.port, cfg.timeout_ms, result);
    if (sock == INVALID_SOCKET) return result->status;

    /* Send */
    if (send(sock, request, req_len, 0) == SOCKET_ERROR) {
        set_error(result, INFER_ERR_SEND, "send() failed");
        closesocket(sock);
        return INFER_ERR_SEND;
    }

    /* Receive full response */
    char response[65536];
    int total_recv = 0;
    while (total_recv < (int)sizeof(response) - 1) {
        int n = recv(sock, response + total_recv, (int)sizeof(response) - 1 - total_recv, 0);
        if (n <= 0) break;
        total_recv += n;
    }
    response[total_recv] = '\0';
    closesocket(sock);

    QueryPerformanceCounter(&t1);
    if (result) {
        result->elapsed_us = (int64_t)((t1.QuadPart - t0.QuadPart) * 1000000 / freq.QuadPart);
    }

    /* Skip HTTP headers — find \r\n\r\n */
    char* body = strstr(response, "\r\n\r\n");
    if (!body) {
        set_error(result, INFER_ERR_PARSE, "No HTTP body separator found");
        return INFER_ERR_PARSE;
    }
    body += 4;

    /* Extract content from JSON response */
    int content_len = 0;
    const char* content = extract_content(body, &content_len);
    if (content && result) {
        int copy_len = content_len < (int)sizeof(result->text) - 1 ? content_len : (int)sizeof(result->text) - 1;
        memcpy(result->text, content, copy_len);
        result->text[copy_len] = '\0';
        /* Unescape JSON */
        int new_len = json_unescape(result->text, copy_len);
        result->text[new_len] = '\0';
    }

    /* Try to extract usage.prompt_tokens and usage.completion_tokens */
    if (result) {
        const char* pt = strstr(body, "\"prompt_tokens\":");
        if (pt) result->prompt_tokens = atoi(pt + 16);
        const char* ct = strstr(body, "\"completion_tokens\":");
        if (ct) result->completion_tokens = atoi(ct + 20);
    }

    result->status = INFER_OK;
    return INFER_OK;
}

INFER_API int __stdcall Infer_Stream(
    const char*        prompt,
    const InferConfig* config,
    TokenCallback      callback,
    void*              user_data,
    InferResult*       result)
{
    InferConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        Infer_DefaultConfig(&cfg);
    }
    if (!cfg.host) cfg.host = "127.0.0.1";

    InferResult local_result;
    if (!result) result = &local_result;
    clear_result(result);

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    /* Build streaming request */
    char request[16384];
    int req_len = build_request(request, sizeof(request),
                                cfg.host, cfg.port,
                                prompt, cfg.max_tokens,
                                cfg.temperature, 1 /* stream=true */);

    /* Connect */
    SOCKET sock = connect_to_server(cfg.host, cfg.port, cfg.timeout_ms, result);
    if (sock == INVALID_SOCKET) return result->status;

    /* Send */
    if (send(sock, request, req_len, 0) == SOCKET_ERROR) {
        set_error(result, INFER_ERR_SEND, "send() failed");
        closesocket(sock);
        return INFER_ERR_SEND;
    }

    /* Receive and parse SSE stream */
    char recv_buf[8192];
    char line_buf[4096];
    int  line_pos = 0;
    int  past_headers = 0;
    int  text_pos = 0;  /* position in result->text for accumulation */
    int  token_count = 0;

    for (;;) {
        int n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) break; /* connection closed or error */
        recv_buf[n] = '\0';

        /* Process byte by byte to split into lines */
        for (int i = 0; i < n; i++) {
            char c = recv_buf[i];

            if (c == '\n') {
                line_buf[line_pos] = '\0';

                /* Skip HTTP headers */
                if (!past_headers) {
                    /* Blank line after headers */
                    if (line_pos == 0 || (line_pos == 1 && line_buf[0] == '\r')) {
                        past_headers = 1;
                    }
                    line_pos = 0;
                    continue;
                }

                /* Strip trailing \r */
                if (line_pos > 0 && line_buf[line_pos - 1] == '\r') {
                    line_buf[--line_pos] = '\0';
                }

                /* Check for SSE data line */
                if (line_pos > 6 && strncmp(line_buf, "data: ", 6) == 0) {
                    const char* data = line_buf + 6;

                    /* Check for stream end */
                    if (strcmp(data, "[DONE]") == 0) {
                        goto done;
                    }

                    /* Extract content from this SSE JSON chunk */
                    int content_len = 0;
                    const char* content = extract_content(data, &content_len);
                    if (content && content_len > 0) {
                        /* Unescape into temp buffer */
                        char token[512];
                        int tlen = content_len < (int)sizeof(token) - 1 ? content_len : (int)sizeof(token) - 1;
                        memcpy(token, content, tlen);
                        token[tlen] = '\0';
                        tlen = json_unescape(token, tlen);
                        token[tlen] = '\0';

                        token_count++;

                        /* Call the token callback */
                        if (callback) {
                            int abort = callback(token, tlen, user_data);
                            if (abort) {
                                set_error(result, INFER_ERR_ABORTED, "Aborted by callback");
                                closesocket(sock);
                                return INFER_ERR_ABORTED;
                            }
                        }

                        /* Accumulate in result text */
                        if (text_pos + tlen < (int)sizeof(result->text) - 1) {
                            memcpy(result->text + text_pos, token, tlen);
                            text_pos += tlen;
                            result->text[text_pos] = '\0';
                        }
                    }
                }

                line_pos = 0;
            } else {
                if (line_pos < (int)sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = c;
                }
            }
        }
    }

done:
    closesocket(sock);

    QueryPerformanceCounter(&t1);
    result->elapsed_us = (int64_t)((t1.QuadPart - t0.QuadPart) * 1000000 / freq.QuadPart);
    result->completion_tokens = token_count;
    result->status = INFER_OK;

    return INFER_OK;
}

/* ── DLL entry point ──────────────────────────────────────────────── */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL; (void)lpvReserved;
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            Infer_Shutdown();
            break;
    }
    return TRUE;
}
