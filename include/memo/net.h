/* MNG/MEMO - networking: Ethernet drivers + a small IPv4 stack (ARP, ICMP, UDP, DHCP) */
#pragma once
#include <memo/kernel.h>

struct netdev {
    const char *name;
    uint8_t mac[6];
    int  (*send)(const void *frame, size_t len);
    void (*poll)(void);                     /* driver reads its RX ring and calls net_rx() */
    uint32_t rx, tx;
};

#define IP4(a, b, c, d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))

void netdev_register(struct netdev *d);
struct netdev *net_dev(void);
void net_rx(const void *frame, size_t len);           /* driver -> stack */

extern uint32_t net_ip, net_mask, net_gw, net_dns;    /* host byte order */
bool net_parse_ip(const char *s, uint32_t *ip);
void net_fmt_ip(uint32_t ip, char *buf, size_t n);

int  net_ping(uint32_t ip, int timeout_ms, int *rtt_ms);   /* 0 = reply received */
int  net_dhcp(int timeout_ms);                              /* 0 = lease obtained */
void net_arp_dump(void (*out)(const char *line));
