/* MNG/MEMO shell - network commands */
#include <memo/net.h>
#include <memo/sched.h>
#include "shell.h"

static int cmd_ifconfig(int argc, char **argv) {
    struct netdev *d = net_dev();
    if (!d) { sh_puts("no network device found (supported: Intel e1000)\n"); return 1; }
    if (argc >= 3) {
        uint32_t ip, mask, gw = net_gw;
        if (!net_parse_ip(argv[1], &ip) || !net_parse_ip(argv[2], &mask) || (argc > 3 && !net_parse_ip(argv[3], &gw))) { sh_puts("usage: ifconfig [ip mask [gateway]]\n"); return 1; }
        net_ip = ip; net_mask = mask; net_gw = gw;
    }
    char a[20], b[20], c[20], e[20];
    net_fmt_ip(net_ip, a, sizeof a); net_fmt_ip(net_mask, b, sizeof b); net_fmt_ip(net_gw, c, sizeof c); net_fmt_ip(net_dns, e, sizeof e);
    sh_printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n  inet %s  netmask %s  gateway %s  dns %s\n  RX %u frames  TX %u frames\n",
              d->name, d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5], a, b, c, e, d->rx, d->tx);
    return 0;
}
SHELL_CMD(ifconfig, "[ip mask [gw]]", "show or set the network configuration", cmd_ifconfig);

static int cmd_ping(int argc, char **argv) {
    uint32_t ip;
    if (argc < 2 || !net_parse_ip(argv[1], &ip)) { sh_puts("usage: ping <a.b.c.d> [count]\n"); return 1; }
    if (!net_dev()) { sh_puts("ping: no network device\n"); return 1; }
    long n = 4; if (argc > 2) { bool ok; n = kstrtol(argv[2], 10, &ok); if (!ok || n < 1 || n > 100) n = 4; }
    int got = 0;
    for (long i = 0; i < n; i++) {
        int rtt;
        int rc = net_ping(ip, 1500, &rtt);
        if (rc == 0) { got++; sh_printf("reply from %s: seq=%ld time=%d ms\n", argv[1], i + 1, rtt); }
        else sh_printf("seq=%ld: %s\n", i + 1, rc == -EAGAIN ? "timeout" : strerror_k(rc));
        task_sleep_ms(300);
    }
    sh_printf("%ld sent, %d received\n", n, got);
    return got ? 0 : 1;
}
SHELL_CMD(ping, "<ip> [count]", "send ICMP echo requests", cmd_ping);

static void out_line(const char *l) { sh_puts(l); }
static int cmd_arp(int argc, char **argv) { (void)argc; (void)argv; net_arp_dump(out_line); return 0; }
SHELL_CMD(arp, "", "show the ARP cache", cmd_arp);

static int cmd_dhcp(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!net_dev()) { sh_puts("dhcp: no network device\n"); return 1; }
    sh_puts("requesting a lease...\n");
    int rc = net_dhcp(5000);
    if (rc) { sh_puts("dhcp: no answer\n"); return 1; }
    return cmd_ifconfig(1, argv);
}
SHELL_CMD(dhcp, "", "get an IP address via DHCP", cmd_dhcp);
