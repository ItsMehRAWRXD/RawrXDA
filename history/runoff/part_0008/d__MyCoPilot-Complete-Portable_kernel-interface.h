// systems-kernel-interface-c.mdc
// Low-level kernel interface for direct system calls

#ifndef KERNEL_INTERFACE_H
#define KERNEL_INTERFACE_H

#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

// Direct syscall wrapper macros
#define SYSCALL0(num) syscall(num)
#define SYSCALL1(num, a1) syscall(num, a1)
#define SYSCALL2(num, a1, a2) syscall(num, a1, a2)
#define SYSCALL3(num, a1, a2, a3) syscall(num, a1, a2, a3)
#define SYSCALL4(num, a1, a2, a3, a4) syscall(num, a1, a2, a3, a4)
#define SYSCALL5(num, a1, a2, a3, a4, a5) syscall(num, a1, a2, a3, a4, a5)
#define SYSCALL6(num, a1, a2, a3, a4, a5, a6) syscall(num, a1, a2, a3, a4, a5, a6)

// Memory management interface
typedef struct {
    void *base_addr;
    size_t size;
    int protection;
    int flags;
    off_t offset;
} memory_mapping_t;

// Performance counter interface
typedef struct {
    uint64_t cycles;
    uint64_t instructions;
    uint64_t cache_references;
    uint64_t cache_misses;
    uint64_t branch_misses;
} perf_counters_t;

// Security context for sandboxing
typedef struct {
    uid_t uid;
    gid_t gid;
    uint32_t capabilities;
    char *chroot_path;
    int seccomp_fd;
} security_context_t;

// Kernel interface functions
int kernel_open_file(const char *path, int flags, mode_t mode);
ssize_t kernel_read_file(int fd, void *buffer, size_t count);
ssize_t kernel_write_file(int fd, const void *buffer, size_t count);
int kernel_close_file(int fd);

void* kernel_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int kernel_munmap(void *addr, size_t length);
int kernel_mprotect(void *addr, size_t length, int prot);

pid_t kernel_create_process(const char *executable, char *const argv[], char *const envp[]);
int kernel_wait_process(pid_t pid, int *status, int options);
int kernel_kill_process(pid_t pid, int signal);

int kernel_setup_perf_counters(perf_counters_t *counters);
int kernel_read_perf_counters(perf_counters_t *counters);

int kernel_create_sandbox(security_context_t *context);
int kernel_enter_sandbox(security_context_t *context);
int kernel_exit_sandbox(void);

// Implementation
inline int kernel_open_file(const char *path, int flags, mode_t mode) {
    return (int)SYSCALL3(__NR_open, path, flags, mode);
}

inline ssize_t kernel_read_file(int fd, void *buffer, size_t count) {
    return (ssize_t)SYSCALL3(__NR_read, fd, buffer, count);
}

inline ssize_t kernel_write_file(int fd, const void *buffer, size_t count) {
    return (ssize_t)SYSCALL3(__NR_write, fd, buffer, count);
}

inline int kernel_close_file(int fd) {
    return (int)SYSCALL1(__NR_close, fd);
}

inline void* kernel_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void*)SYSCALL6(__NR_mmap, addr, length, prot, flags, fd, offset);
}

inline int kernel_munmap(void *addr, size_t length) {
    return (int)SYSCALL2(__NR_munmap, addr, length);
}

inline int kernel_mprotect(void *addr, size_t length, int prot) {
    return (int)SYSCALL3(__NR_mprotect, addr, length, prot);
}

inline pid_t kernel_create_process(const char *executable, char *const argv[], char *const envp[]) {
    pid_t pid = (pid_t)SYSCALL0(__NR_fork);
    if (pid == 0) {
        // Child process
        SYSCALL3(__NR_execve, executable, argv, envp);
    }
    return pid;
}

inline int kernel_wait_process(pid_t pid, int *status, int options) {
    return (int)SYSCALL4(__NR_wait4, pid, status, options, NULL);
}

inline int kernel_kill_process(pid_t pid, int signal) {
    return (int)SYSCALL2(__NR_kill, pid, signal);
}

// Performance monitoring setup
inline int kernel_setup_perf_counters(perf_counters_t *counters) {
    // Setup hardware performance counters via perf_event_open
    struct perf_event_attr attr = {0};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_HW_CPU_CYCLES;
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    
    int fd = (int)SYSCALL5(__NR_perf_event_open, &attr, 0, -1, -1, 0);
    return fd;
}

inline int kernel_read_perf_counters(perf_counters_t *counters) {
    // Read performance counter values
    uint64_t value;
    int result = (int)SYSCALL3(__NR_read, perf_fd, &value, sizeof(value));
    if (result > 0) {
        counters->cycles = value;
    }
    return result;
}

// Sandbox implementation using seccomp and namespaces
inline int kernel_create_sandbox(security_context_t *context) {
    // Create new user namespace
    int flags = CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS;
    return (int)SYSCALL2(__NR_unshare, flags);
}

inline int kernel_enter_sandbox(security_context_t *context) {
    // Change root directory
    if (context->chroot_path) {
        SYSCALL1(__NR_chroot, context->chroot_path);
        SYSCALL1(__NR_chdir, "/");
    }
    
    // Drop privileges
    SYSCALL1(__NR_setgid, context->gid);
    SYSCALL1(__NR_setuid, context->uid);
    
    // Apply seccomp filter
    if (context->seccomp_fd >= 0) {
        SYSCALL2(__NR_prctl, PR_SET_SECCOMP, SECCOMP_MODE_FILTER);
    }
    
    return 0;
}

inline int kernel_exit_sandbox(void) {
    // Cleanup sandbox resources
    return 0;
}

#endif // KERNEL_INTERFACE_H