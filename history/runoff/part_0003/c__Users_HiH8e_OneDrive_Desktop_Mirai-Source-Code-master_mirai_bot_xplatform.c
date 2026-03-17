/*
 * Cross-platform abstraction layer implementation
 */

#include "xplatform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
static WSADATA g_wsaData;
static BOOL g_wsa_initialized = FALSE;
#else
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#endif

// Platform initialization
int xinit_platform(void)
{
#ifdef _WIN32
    if (!g_wsa_initialized) {
        if (WSAStartup(MAKEWORD(2, 2), &g_wsaData) != 0)
            return XERROR_IO_ERROR;
        g_wsa_initialized = TRUE;
    }
#endif
    return XERROR_SUCCESS;
}

void xcleanup_platform(void)
{
#ifdef _WIN32
    if (g_wsa_initialized) {
        WSACleanup();
        g_wsa_initialized = FALSE;
    }
#endif
}

// Sleep functions
void xsleep(unsigned int seconds)
{
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

void xmsleep(unsigned int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// Process functions
int xgetpid(void)
{
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

int xgetppid(void)
{
#ifdef _WIN32
    // Windows doesn't have a direct equivalent to getppid()
    // This is a simplified implementation
    return 0;
#else
    return (int)getppid();
#endif
}

int xfork(void)
{
#ifdef _WIN32
    // Windows doesn't support fork(), return error
    return -1;
#else
    return (int)fork();
#endif
}

int xkill(int pid, int sig)
{
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (hProcess == NULL)
        return -1;
    
    BOOL result = TerminateProcess(hProcess, (UINT)sig);
    CloseHandle(hProcess);
    return result ? 0 : -1;
#else
    return kill((pid_t)pid, sig);
#endif
}

int xwaitpid(int pid, int *status, int options)
{
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (hProcess == NULL)
        return -1;
    
    DWORD result = WaitForSingleObject(hProcess, INFINITE);
    if (result == WAIT_OBJECT_0 && status) {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode))
            *status = (int)exitCode;
    }
    
    CloseHandle(hProcess);
    return (result == WAIT_OBJECT_0) ? pid : -1;
#else
    return (int)waitpid((pid_t)pid, status, options);
#endif
}

// Process creation
xprocess_t *xcreate_process(const char *path, char *const argv[], char *const envp[])
{
    xprocess_t *proc = (xprocess_t *)malloc(sizeof(xprocess_t));
    if (!proc)
        return NULL;

#ifdef _WIN32
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    // Build command line from argv
    size_t cmdlen = 0;
    for (int i = 0; argv[i]; i++) {
        cmdlen += strlen(argv[i]) + 3; // quotes + space
    }
    
    char *cmdline = (char *)malloc(cmdlen + 1);
    if (!cmdline) {
        free(proc);
        return NULL;
    }
    
    cmdline[0] = '\0';
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(cmdline, " ");
        strcat(cmdline, "\"");
        strcat(cmdline, argv[i]);
        strcat(cmdline, "\"");
    }
    
    BOOL success = CreateProcessA(
        path,           // Application name
        cmdline,        // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        FALSE,          // Inherit handles
        0,              // Creation flags
        NULL,           // Environment (use parent's)
        NULL,           // Current directory (use parent's)
        &si,            // Startup info
        &pi             // Process information
    );
    
    free(cmdline);
    
    if (success) {
        proc->hProcess = pi.hProcess;
        proc->hThread = pi.hThread;
        proc->processId = pi.dwProcessId;
        proc->threadId = pi.dwThreadId;
    } else {
        free(proc);
        return NULL;
    }
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (envp) {
            execve(path, argv, envp);
        } else {
            execv(path, argv);
        }
        exit(1); // If execve fails
    } else if (pid > 0) {
        // Parent process
        proc->pid = pid;
    } else {
        // Fork failed
        free(proc);
        return NULL;
    }
#endif

    return proc;
}

int xwait_process(xprocess_t *proc, int *exit_code)
{
    if (!proc)
        return -1;

#ifdef _WIN32
    DWORD result = WaitForSingleObject(proc->hProcess, INFINITE);
    if (result == WAIT_OBJECT_0 && exit_code) {
        DWORD exitCode;
        if (GetExitCodeProcess(proc->hProcess, &exitCode))
            *exit_code = (int)exitCode;
    }
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    int status;
    pid_t result = waitpid(proc->pid, &status, 0);
    if (result == proc->pid && exit_code) {
        if (WIFEXITED(status))
            *exit_code = WEXITSTATUS(status);
        else
            *exit_code = -1;
    }
    return (result == proc->pid) ? 0 : -1;
#endif
}

void xfree_process(xprocess_t *proc)
{
    if (!proc)
        return;

#ifdef _WIN32
    if (proc->hProcess) CloseHandle(proc->hProcess);
    if (proc->hThread) CloseHandle(proc->hThread);
#endif
    free(proc);
}

// File operations
int xopen(const char *path, int flags, ...)
{
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);

#ifdef _WIN32
    // Convert Unix flags to Windows flags
    int win_flags = 0;
    if (flags & O_RDONLY) win_flags |= _O_RDONLY;
    if (flags & O_WRONLY) win_flags |= _O_WRONLY;
    if (flags & O_RDWR) win_flags |= _O_RDWR;
    if (flags & O_CREAT) win_flags |= _O_CREAT;
    if (flags & O_TRUNC) win_flags |= _O_TRUNC;
    if (flags & O_APPEND) win_flags |= _O_APPEND;
    win_flags |= _O_BINARY; // Always binary mode
    
    return _open(path, win_flags, mode);
#else
    return open(path, flags, mode);
#endif
}

int xclose(int fd)
{
#ifdef _WIN32
    return _close(fd);
#else
    return close(fd);
#endif
}

ssize_t xread(int fd, void *buf, size_t count)
{
#ifdef _WIN32
    return _read(fd, buf, (unsigned int)count);
#else
    return read(fd, buf, count);
#endif
}

ssize_t xwrite(int fd, const void *buf, size_t count)
{
#ifdef _WIN32
    return _write(fd, buf, (unsigned int)count);
#else
    return write(fd, buf, count);
#endif
}

off_t xlseek(int fd, off_t offset, int whence)
{
#ifdef _WIN32
    return _lseek(fd, offset, whence);
#else
    return lseek(fd, offset, whence);
#endif
}

int xunlink(const char *path)
{
#ifdef _WIN32
    return _unlink(path);
#else
    return unlink(path);
#endif
}

int xrename(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

// Directory operations
int xmkdir(const char *path, int mode)
{
#ifdef _WIN32
    (void)mode; // Windows doesn't use mode parameter
    return _mkdir(path);
#else
    return mkdir(path, (mode_t)mode);
#endif
}

int xrmdir(const char *path)
{
#ifdef _WIN32
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

int xchdir(const char *path)
{
#ifdef _WIN32
    return _chdir(path);
#else
    return chdir(path);
#endif
}

char *xgetcwd(char *buf, size_t size)
{
#ifdef _WIN32
    return _getcwd(buf, (int)size);
#else
    return getcwd(buf, size);
#endif
}

// Environment variables
char *xgetenv(const char *name)
{
    return getenv(name);
}

int xsetenv(const char *name, const char *value, int overwrite)
{
#ifdef _WIN32
    if (!overwrite && getenv(name) != NULL)
        return 0;
    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

int xunsetenv(const char *name)
{
#ifdef _WIN32
    return _putenv_s(name, "");
#else
    return unsetenv(name);
#endif
}

// Path handling
char *xpath_join(const char *base, const char *path)
{
    if (!base || !path)
        return NULL;

    size_t base_len = strlen(base);
    size_t path_len = strlen(path);
    size_t total_len = base_len + path_len + 2; // +2 for separator and null terminator

    char *result = (char *)malloc(total_len);
    if (!result)
        return NULL;

    strcpy(result, base);
    
    // Add separator if needed
    if (base_len > 0 && result[base_len - 1] != XPATH_SEP
#ifdef _WIN32
        && result[base_len - 1] != XPATH_ALT_SEP
#endif
        ) {
        result[base_len] = XPATH_SEP;
        result[base_len + 1] = '\0';
    }

    strcat(result, path);
    return result;
}

char *xpath_normalize(const char *path)
{
    if (!path)
        return NULL;

    char *result = xstrdup(path);
    if (!result)
        return NULL;

#ifdef _WIN32
    // Convert forward slashes to backslashes on Windows
    for (char *p = result; *p; p++) {
        if (*p == '/')
            *p = '\\';
    }
#endif

    return result;
}

int xpath_exists(const char *path)
{
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

int xpath_is_absolute(const char *path)
{
    if (!path || !*path)
        return 0;

#ifdef _WIN32
    // Windows absolute paths: C:\... or \\server\share\...
    return (isalpha(path[0]) && path[1] == ':') || 
           (path[0] == '\\' && path[1] == '\\');
#else
    return path[0] == '/';
#endif
}

// String utilities
char *xstrdup(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    char *result = (char *)malloc(len + 1);
    if (result)
        strcpy(result, str);
    return result;
}

int xstrcasecmp(const char *s1, const char *s2)
{
#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef _WIN32
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
}

// Memory utilities
void *xmalloc(size_t size)
{
    return malloc(size);
}

void *xcalloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *xrealloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void xfree(void *ptr)
{
    free(ptr);
}

// Error handling
int xget_last_error(void)
{
#ifdef _WIN32
    DWORD error = GetLastError();
    switch (error) {
        case ERROR_ACCESS_DENIED: return XERROR_ACCESS_DENIED;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return XERROR_NOT_FOUND;
        case ERROR_ALREADY_EXISTS: return XERROR_ALREADY_EXISTS;
        case ERROR_NOT_ENOUGH_MEMORY: return XERROR_NO_MEMORY;
        case ERROR_TIMEOUT: return XERROR_TIMEOUT;
        default: return XERROR_UNKNOWN;
    }
#else
    switch (errno) {
        case EACCES: return XERROR_ACCESS_DENIED;
        case ENOENT: return XERROR_NOT_FOUND;
        case EEXIST: return XERROR_ALREADY_EXISTS;
        case ENOMEM: return XERROR_NO_MEMORY;
        case ETIMEDOUT: return XERROR_TIMEOUT;
        default: return XERROR_UNKNOWN;
    }
#endif
}

const char *xget_error_string(int error_code)
{
    switch (error_code) {
        case XERROR_SUCCESS: return "Success";
        case XERROR_INVALID_PARAM: return "Invalid parameter";
        case XERROR_ACCESS_DENIED: return "Access denied";
        case XERROR_NOT_FOUND: return "Not found";
        case XERROR_ALREADY_EXISTS: return "Already exists";
        case XERROR_NO_MEMORY: return "Out of memory";
        case XERROR_IO_ERROR: return "I/O error";
        case XERROR_TIMEOUT: return "Timeout";
        default: return "Unknown error";
    }
}