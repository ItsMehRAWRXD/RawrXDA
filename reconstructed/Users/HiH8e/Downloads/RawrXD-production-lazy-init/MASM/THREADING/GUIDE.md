# MASM x64 Threading & Synchronization Guide

This document describes the pure-MASM threading and synchronization primitives implemented in Phase 3 of the RawrXD-QtShell project.

## Architecture

The threading system is built directly on Windows Win32 APIs, providing a zero-dependency, high-performance foundation for asynchronous operations in assembly.

### Core Components

1.  **Mutexes (`asm_sync.asm`)**:
    *   Wraps Win32 `CRITICAL_SECTION`.
    *   Supports recursive locking.
    *   Functions: `asm_mutex_create`, `asm_mutex_lock`, `asm_mutex_unlock`, `asm_mutex_destroy`.

2.  **Events (`asm_sync.asm`)**:
    *   Wraps Win32 Event objects.
    *   Supports manual and auto-reset modes.
    *   Functions: `asm_event_create`, `asm_event_set`, `asm_event_reset`, `asm_event_wait`, `asm_event_destroy`.

3.  **Semaphores (`asm_sync.asm`)**:
    *   Wraps Win32 Semaphore objects.
    *   Used for resource counting and thread pool signaling.
    *   Functions: `asm_semaphore_create`, `asm_semaphore_wait`, `asm_semaphore_release`, `asm_semaphore_destroy`.

4.  **Atomic Operations (`asm_sync.asm`)**:
    *   Uses `lock`-prefixed x64 instructions.
    *   Functions: `asm_atomic_increment`, `asm_atomic_decrement`, `asm_atomic_add`, `asm_atomic_cmpxchg`, `asm_atomic_xchg`.

5.  **Thread Pool (`asm_thread_pool.asm`)**:
    *   A fixed-size pool of worker threads.
    *   Task queue with mutex protection and semaphore signaling.
    *   Functions: `asm_thread_pool_create`, `asm_thread_pool_enqueue`, `asm_thread_pool_destroy`.

## Usage from C++

The threading primitives are exposed via `asm_runtime.hpp`.

```cpp
#include "asm_runtime.hpp"

// Create a thread pool with 4 threads
ThreadPoolHandle pool = asm_thread_pool_create(4);

// Enqueue a task
asm_thread_pool_enqueue(pool, [](void* ctx) {
    // Task logic here
}, nullptr);

// Shutdown
asm_thread_pool_destroy(pool);
```

## Usage from MASM

Include `asm_sync.inc` in your assembly files.

```asm
include asm_sync.inc

.data
    mutex dq 0

.code
    call asm_mutex_create
    mov mutex, rax
    
    mov rcx, mutex
    call asm_mutex_lock
    ; ... critical section ...
    mov rcx, mutex
    call asm_mutex_unlock
```

## Performance Considerations

*   **Zero-Copy**: The thread pool uses a simple linked-list queue to avoid memory copies.
*   **Alignment**: All synchronization structures are 16-byte aligned for optimal cache performance.
*   **Win32 Integration**: By using native Win32 primitives, we ensure compatibility with OS-level scheduling and power management.
