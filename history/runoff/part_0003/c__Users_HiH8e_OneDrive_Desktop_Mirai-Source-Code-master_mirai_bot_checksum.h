#pragma once

#include <stdint.h>
#ifdef _WIN32
#include "windows_compat.h"
#else
#include <linux/ip.h>
#endif

#include "includes.h"

uint16_t checksum_generic(uint16_t *, uint32_t);
uint16_t checksum_tcpudp(struct iphdr *, void *, uint16_t, int);
