#pragma once

/*
 * Cross-platform abstraction layer for system functions
 * Provides unified interface for Windows and Unix/Linux platforms
 */

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Process management
typedef struct {
#ifdef _WIN32
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;
#else
    pid_t pid;
#endif
} xprocess_t;

// Sleep functions
void xsleep(unsigned int seconds);
void xmsleep(unsigned int milliseconds);

// Process functions
int xfork(void);                           // Fork process (Windows: CreateProcess)
int xgetpid(void);                         // Get current process ID
int xgetppid(void);                        // Get parent process ID
int xkill(int pid, int sig);              // Kill process
int xwaitpid(int pid, int *status, int options); // Wait for process

// Process creation and execution
int xexecve(const char *path, char *const argv[], char *const envp[]);
int xsystem(const char *command);
xprocess_t *xcreate_process(const char *path, char *const argv[], char *const envp[]);
int xwait_process(xprocess_t *proc, int *exit_code);
void xfree_process(xprocess_t *proc);

// Signal handling
typedef void (*xsignal_handler_t)(int);
xsignal_handler_t xsignal(int sig, xsignal_handler_t handler);
int xkill_signal(int pid, int sig);

// File operations
int xopen(const char *path, int flags, ...);
int xclose(int fd);
ssize_t xread(int fd, void *buf, size_t count);
ssize_t xwrite(int fd, const void *buf, size_t count);
off_t xlseek(int fd, off_t offset, int whence);
int xfsync(int fd);
int xunlink(const char *path);
int xrename(const char *oldpath, const char *newpath);

// Directory operations
int xmkdir(const char *path, int mode);
int xrmdir(const char *path);
int xchdir(const char *path);
char *xgetcwd(char *buf, size_t size);

// File status and permissions
int xstat(const char *path, struct stat *buf);
int xfstat(int fd, struct stat *buf);
int xchmod(const char *path, int mode);
int xaccess(const char *path, int mode);

// Environment variables
char *xgetenv(const char *name);
int xsetenv(const char *name, const char *value, int overwrite);
int xunsetenv(const char *name);

// Path handling
char *xpath_join(const char *base, const char *path);
char *xpath_dirname(const char *path);
char *xpath_basename(const char *path);
char *xpath_normalize(const char *path);
int xpath_exists(const char *path);
int xpath_is_absolute(const char *path);

// String utilities
char *xstrdup(const char *str);
int xstrcasecmp(const char *s1, const char *s2);
int xstrncasecmp(const char *s1, const char *s2, size_t n);

// Memory utilities
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void xfree(void *ptr);

// Platform-specific constants for signals
#ifdef _WIN32
#define XSIGHUP     1
#define XSIGINT     2
#define XSIGQUIT    3
#define XSIGILL     4
#define XSIGABRT    6
#define XSIGFPE     8
#define XSIGKILL    9
#define XSIGPIPE    13
#define XSIGALRM    14
#define XSIGTERM    15
#define XSIGUSR1    10
#define XSIGUSR2    12
#define XSIGCHLD    17
#define XSIGCONT    18
#define XSIGSTOP    19
#define XSIGTSTP    20
#else
#define XSIGHUP     SIGHUP
#define XSIGINT     SIGINT
#define XSIGQUIT    SIGQUIT
#define XSIGILL     SIGILL
#define XSIGABRT    SIGABRT
#define XSIGFPE     SIGFPE
#define XSIGKILL    SIGKILL
#define XSIGPIPE    SIGPIPE
#define XSIGALRM    SIGALRM
#define XSIGTERM    SIGTERM
#define XSIGUSR1    SIGUSR1
#define XSIGUSR2    SIGUSR2
#define XSIGCHLD    SIGCHLD
#define XSIGCONT    SIGCONT
#define XSIGSTOP    SIGSTOP
#define XSIGTSTP    SIGTSTP
#endif

// Path separator
#ifdef _WIN32
#define XPATH_SEP '\\'
#define XPATH_SEP_STR "\\"
#define XPATH_ALT_SEP '/'
#else
#define XPATH_SEP '/'
#define XPATH_SEP_STR "/"
#endif

// Error codes
#define XERROR_SUCCESS          0
#define XERROR_INVALID_PARAM    -1
#define XERROR_ACCESS_DENIED    -2
#define XERROR_NOT_FOUND        -3
#define XERROR_ALREADY_EXISTS   -4
#define XERROR_NO_MEMORY        -5
#define XERROR_IO_ERROR         -6
#define XERROR_TIMEOUT          -7
#define XERROR_UNKNOWN          -99

// Get last error
int xget_last_error(void);
const char *xget_error_string(int error_code);

// Initialization and cleanup
int xinit_platform(void);
void xcleanup_platform(void);

#ifdef __cplusplus
}
#endif