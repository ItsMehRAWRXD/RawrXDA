/**
 * Windows Compatibility Layer
 * Resolves header conflicts, network structures, and function definitions
 */

#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32

// ============================================================================
// WINDOWS HEADER ORDER - CRITICAL FOR AVOIDING CONFLICTS
// ============================================================================
// Must include winsock2.h BEFORE windows.h to avoid redefinition errors
#ifndef _WINSOCK2API_
#define _WIN32_WINNT 0x0601 // Target Windows 7+
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

// ============================================================================
// NETWORK STRUCTURE DEFINITIONS
// ============================================================================

// IP Header structure (equivalent to Linux's iphdr)
#ifndef IPHDR_DEFINED
#define IPHDR_DEFINED
struct iphdr
{
  unsigned char ihl : 4;     // IP header length
  unsigned char version : 4; // Version
  unsigned char tos;         // Type of service
  unsigned short tot_len;    // Total length
  unsigned short id;         // Identification
  unsigned short frag_off;   // Fragment offset
  unsigned char ttl;         // Time to live
  unsigned char protocol;    // Protocol
  unsigned short check;      // Checksum
  unsigned int saddr;        // Source address
  unsigned int daddr;        // Destination address
};
#endif

// TCP Header structure (equivalent to Linux's tcphdr)
#ifndef TCPHDR_DEFINED
#define TCPHDR_DEFINED
struct tcphdr
{
  unsigned short source;   // Source port
  unsigned short dest;     // Destination port
  unsigned int seq;        // Sequence number
  unsigned int ack_seq;    // Acknowledgment number
  unsigned short res1 : 4; // Reserved
  unsigned short doff : 4; // Data offset
  unsigned short fin : 1;  // FIN flag
  unsigned short syn : 1;  // SYN flag
  unsigned short rst : 1;  // RST flag
  unsigned short psh : 1;  // PSH flag
  unsigned short ack : 1;  // ACK flag
  unsigned short urg : 1;  // URG flag
  unsigned short res2 : 2; // Reserved
  unsigned short window;   // Window size
  unsigned short check;    // Checksum
  unsigned short urg_ptr;  // Urgent pointer
};
#endif

// UDP Header structure
#ifndef UDPHDR_DEFINED
#define UDPHDR_DEFINED
struct udphdr
{
  unsigned short source; // Source port
  unsigned short dest;   // Destination port
  unsigned short len;    // Length
  unsigned short check;  // Checksum
};
#endif

// ============================================================================
// SOCKET COMPATIBILITY MACROS
// ============================================================================

// Socket close compatibility
#ifndef close
#define close(s) closesocket(s)
#endif

// Socket length type
#ifndef socklen_t
typedef int socklen_t;
#endif

// MSG_NOSIGNAL doesn't exist on Windows (SIGPIPE doesn't exist)
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Socket error compatibility
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

// ============================================================================
// FILE CONTROL COMPATIBILITY
// ============================================================================

// fcntl flags (Windows uses ioctlsocket instead)
#ifndef F_GETFL
#define F_GETFL 3
#endif

#ifndef F_SETFL
#define F_SETFL 4
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0x800
#endif

// Implement fcntl for socket operations
static inline int fcntl_socket(int fd, int cmd, int flags)
{
  if (cmd == F_SETFL)
  {
    u_long mode = (flags & O_NONBLOCK) ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode);
  }
  return 0;
}

// Override fcntl macro for socket file descriptors
#ifndef fcntl
#define fcntl(fd, cmd, ...) fcntl_socket(fd, cmd, ##__VA_ARGS__)
#endif

// ============================================================================
// PROCESS/SIGNAL COMPATIBILITY
// ============================================================================

// Process functions
#ifndef fork
#define fork() (-1) // Fork not supported on Windows
#endif

#ifndef getpid
#define getpid() GetCurrentProcessId()
#endif

// Signal definitions
#ifndef SIGKILL
#define SIGKILL 9
#endif

#ifndef SIGTERM
#define SIGTERM 15
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef SIGHUP
#define SIGHUP 1
#endif

// Signal handler type
typedef void (*sighandler_t)(int);

// Stub signal handler
static inline sighandler_t signal(int signum, sighandler_t handler)
{
  return SIG_DFL;
}

// ============================================================================
// SLEEP FUNCTIONS
// ============================================================================

#ifndef sleep
#define sleep(s) Sleep((s) * 1000)
#endif

#ifndef usleep
#define usleep(us) Sleep(((us) + 999) / 1000)
#endif

// ============================================================================
// PATH/FILE COMPATIBILITY
// ============================================================================

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

// ============================================================================
// PROTOCOL CONSTANTS
// ============================================================================

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif

#ifndef IPPROTO_RAW
#define IPPROTO_RAW 255
#endif

// IP Header Include option
#ifndef IP_HDRINCL
#define IP_HDRINCL 2
#endif

// ============================================================================
// ERROR HANDLING
// ============================================================================

// Map Windows socket errors to POSIX errno values
#ifndef errno
#define errno WSAGetLastError()
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif

// ============================================================================
// MEMORY/STRING COMPATIBILITY
// ============================================================================

#ifndef bzero
#define bzero(b, len) memset((b), 0, (len))
#endif

#ifndef bcopy
#define bcopy(src, dst, len) memmove((dst), (src), (len))
#endif

// ============================================================================
// NETWORK BYTE ORDER (if not already defined)
// ============================================================================

#ifndef htons
#define htons(x) ((unsigned short)((((unsigned short)(x) & 0xFF) << 8) | \
                                   (((unsigned short)(x) & 0xFF00) >> 8)))
#endif

#ifndef ntohs
#define ntohs(x) htons(x)
#endif

#ifndef htonl
#define htonl(x) ((((unsigned long)(x) & 0xFF) << 24) |    \
                  (((unsigned long)(x) & 0xFF00) << 8) |   \
                  (((unsigned long)(x) & 0xFF0000) >> 8) | \
                  (((unsigned long)(x) & 0xFF000000) >> 24))
#endif

#ifndef ntohl
#define ntohl(x) htonl(x)
#endif

// ============================================================================
// INITIALIZATION HELPER
// ============================================================================

// WSA Startup helper - call this at program start
static inline int winsock_init(void)
{
  WSADATA wsaData;
  return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

// WSA Cleanup helper - call this at program end
static inline void winsock_cleanup(void)
{
  WSACleanup();
}

#endif // _WIN32

#endif // WINDOWS_COMPAT_H
