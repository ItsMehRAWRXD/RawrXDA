// Windows-specific includes and compatibility layer
// Complete Windows API compatibility for Mirai

#ifndef INCLUDES_WINDOWS_H
#define INCLUDES_WINDOWS_H

#ifdef _WIN32

// Prevent Windows.h conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Define Windows version
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7+
#endif

// Include order is critical - WinSock2 must come before Windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Link required libraries
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ntdll.lib")

// Network structure definitions (Windows-compatible)
#pragma pack(push, 1)

struct iphdr {
    uint8_t  ihl:4,
             version:4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

struct tcphdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t res1:4,
             doff:4,
             fin:1,
             syn:1,
             rst:1,
             psh:1,
             ack:1,
             urg:1,
             ece:1,
             cwr:1;
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
};

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

struct icmphdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

struct gre_header {
    uint16_t flags;
    uint16_t protocol;
};

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

#pragma pack(pop)

// POSIX compatibility layer - avoid conflicts
#ifndef MIRAI_POSIX_COMPAT_DEFINED
#define MIRAI_POSIX_COMPAT_DEFINED

// Override close() to avoid conflicts
#ifdef close
#undef close
#endif
#define close(s) closesocket((SOCKET)(s))

// Sleep functions
#ifdef sleep  
#undef sleep
#endif
#define sleep(s) Sleep((s) * 1000)

#ifdef usleep
#undef usleep  
#endif
#define usleep(us) Sleep(((us) + 999) / 1000)

// Process ID
#ifdef getpid
#undef getpid
#endif  
#define getpid() GetCurrentProcessId()

#endif // MIRAI_POSIX_COMPAT_DEFINED

// Socket compatibility
#ifndef MIRAI_SOCKET_COMPAT_DEFINED
#define MIRAI_SOCKET_COMPAT_DEFINED

typedef int socklen_t;

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Network constants
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

#ifndef IPPROTO_GRE  
#define IPPROTO_GRE 47
#endif

#endif // MIRAI_SOCKET_COMPAT_DEFINED

// Signal handling compatibility
#ifndef MIRAI_SIGNAL_COMPAT_DEFINED
#define MIRAI_SIGNAL_COMPAT_DEFINED

#define SIGKILL 9
#define SIGTERM 15
#define SIGPIPE 13

typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

// Stub signal function for Windows
static inline sighandler_t signal(int sig, sighandler_t handler) {
    UNREFERENCED_PARAMETER(sig);
    UNREFERENCED_PARAMETER(handler);
    return SIG_DFL;
}

#endif // MIRAI_SIGNAL_COMPAT_DEFINED

// File/directory constants
#ifndef MAX_PATH
#define PATH_MAX MAX_PATH
#endif

// Network constants
#define SHUT_RDWR SD_BOTH

// Process constants  
#define KILLER_MIN_PID 100
#define KILLER_RESTART_SCAN_TIME 600
#define KILLER_SCAN_INTERVAL 30

// Scanner constants
#define SCANNER_MAX_CONNS 1000
#define SCANNER_CONN_TIMEOUT 30
#define SCANNER_SCAN_INTERVAL 1000
#define SCANNER_RATELIMIT_PER_NETWORK 256

#endif // _WIN32

#endif // INCLUDES_WINDOWS_H
