/* MNG/MEMO - minimal IPv4 stack: Ethernet, ARP, ICMP echo, UDP, DHCP client */
#include <memo/arch.h>
#include <memo/net.h>
#include <memo/sched.h>

static struct netdev *dev;
uint32_t net_ip = IP4(10, 0, 2, 15), net_mask = IP4(255, 255, 255, 0), net_gw = IP4(10, 0, 2, 2), net_dns = IP4(10, 0, 2, 3);

static inline uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static inline uint32_t bswap32(uint32_t v) { return (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24); }
#define htons bswap16
#define ntohs bswap16
#define htonl bswap32
#define ntohl bswap32

struct eth  { uint8_t dst[6], src[6]; uint16_t type; } __packed;
struct arp  { uint16_t htype, ptype; uint8_t hlen, plen; uint16_t op; uint8_t sha[6]; uint32_t spa; uint8_t tha[6]; uint32_t tpa; } __packed;
struct ip4h { uint8_t vhl, tos; uint16_t len, id, frag; uint8_t ttl, proto; uint16_t csum; uint32_t src, dst; } __packed;
struct icmp { uint8_t type, code; uint16_t csum, id, seq; } __packed;
struct udph { uint16_t sport, dport, len, csum; } __packed;

#define ETH_ARP 0x0806
#define ETH_IP  0x0800
static const uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void netdev_register(struct netdev *d) { if (!dev) dev = d; }
struct netdev *net_dev(void) { return dev; }

static uint16_t csum(const void *data, size_t len) {
    const uint8_t *p = data; uint32_t sum = 0;
    for (; len > 1; len -= 2, p += 2) sum += (uint32_t)(p[0] << 8 | p[1]);
    if (len) sum += (uint32_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return htons((uint16_t)~sum);
}

bool net_parse_ip(const char *s, uint32_t *ip) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return false;
        uint32_t n = 0;
        while (*s >= '0' && *s <= '9') { n = n * 10 + (uint32_t)(*s++ - '0'); if (n > 255) return false; }
        v = (v << 8) | n;
        if (i < 3) { if (*s != '.') return false; s++; }
    }
    if (*s) return false;
    *ip = v; return true;
}
void net_fmt_ip(uint32_t ip, char *buf, size_t n) {
    snprintf(buf, n, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

/* ---- ARP cache ---- */
#define ARP_MAX 8
static struct { uint32_t ip; uint8_t mac[6]; bool valid; } arp_tab[ARP_MAX];
static void arp_learn(uint32_t ip, const uint8_t *mac) {
    int slot = -1;
    for (int i = 0; i < ARP_MAX; i++) { if (arp_tab[i].valid && arp_tab[i].ip == ip) { slot = i; break; } if (!arp_tab[i].valid && slot < 0) slot = i; }
    if (slot < 0) slot = (int)(ip % ARP_MAX);
    arp_tab[slot].ip = ip; memcpy(arp_tab[slot].mac, mac, 6); arp_tab[slot].valid = true;
}
static bool arp_lookup(uint32_t ip, uint8_t *mac) {
    for (int i = 0; i < ARP_MAX; i++) if (arp_tab[i].valid && arp_tab[i].ip == ip) { memcpy(mac, arp_tab[i].mac, 6); return true; }
    return false;
}
void net_arp_dump(void (*out)(const char *)) {
    char line[80], ip[20];
    for (int i = 0; i < ARP_MAX; i++) if (arp_tab[i].valid) {
        net_fmt_ip(arp_tab[i].ip, ip, sizeof ip);
        snprintf(line, sizeof line, "%-16s %02x:%02x:%02x:%02x:%02x:%02x\n", ip, arp_tab[i].mac[0], arp_tab[i].mac[1], arp_tab[i].mac[2], arp_tab[i].mac[3], arp_tab[i].mac[4], arp_tab[i].mac[5]);
        out(line);
    }
}

static void eth_send(const uint8_t *dst, uint16_t type, const void *payload, size_t len) {
    uint8_t frame[1600];
    if (!dev || len > 1500) return;
    struct eth *e = (struct eth *)frame;
    memcpy(e->dst, dst, 6); memcpy(e->src, dev->mac, 6); e->type = htons(type);
    memcpy(frame + sizeof *e, payload, len);
    dev->send(frame, sizeof *e + len);
    dev->tx++;
}

static void arp_request(uint32_t ip) {
    struct arp a = { htons(1), htons(ETH_IP), 6, 4, htons(1), {0}, htonl(net_ip), {0}, htonl(ip) };
    memcpy(a.sha, dev->mac, 6);
    eth_send(bcast, ETH_ARP, &a, sizeof a);
}

static bool resolve(uint32_t ip, uint8_t *mac) {
    uint32_t hop = ((ip & net_mask) == (net_ip & net_mask)) ? ip : net_gw;
    for (int tries = 0; tries < 4; tries++) {
        if (arp_lookup(hop, mac)) return true;
        arp_request(hop);
        for (int i = 0; i < 50 && !arp_lookup(hop, mac); i++) task_sleep_ms(5);
    }
    return arp_lookup(hop, mac);
}

static void ip_send_raw(const uint8_t *dmac, uint32_t src, uint32_t dst, uint8_t proto, const void *payload, size_t len) {
    uint8_t pkt[1500];
    if (len + sizeof(struct ip4h) > sizeof pkt) return;
    static uint16_t ident;
    struct ip4h *h = (struct ip4h *)pkt;
    h->vhl = 0x45; h->tos = 0; h->len = htons((uint16_t)(sizeof *h + len)); h->id = htons(++ident); h->frag = 0;
    h->ttl = 64; h->proto = proto; h->csum = 0; h->src = htonl(src); h->dst = htonl(dst);
    h->csum = csum(h, sizeof *h);
    memcpy(pkt + sizeof *h, payload, len);
    eth_send(dmac, ETH_IP, pkt, sizeof *h + len);
}

static int ip_send(uint32_t dst, uint8_t proto, const void *payload, size_t len) {
    uint8_t mac[6];
    if (!dev) return -ENODEV;
    if (!resolve(dst, mac)) return -EIO;
    ip_send_raw(mac, net_ip, dst, proto, payload, len);
    return 0;
}

/* ---- UDP ---- */
typedef void (*udp_cb)(uint32_t src_ip, uint16_t sport, const uint8_t *data, size_t len);
static struct { uint16_t port; udp_cb cb; } udp_binds[4];
static void udp_bind(uint16_t port, udp_cb cb) {
    for (int i = 0; i < 4; i++) if (!udp_binds[i].port || udp_binds[i].port == port) { udp_binds[i].port = port; udp_binds[i].cb = cb; return; }
}
static void udp_unbind(uint16_t port) { for (int i = 0; i < 4; i++) if (udp_binds[i].port == port) udp_binds[i].port = 0; }

/* ---- ICMP ping ---- */
static volatile struct { uint16_t id, seq; bool got; uint32_t t; } ping_st;

static void icmp_rx(uint32_t src, const uint8_t *p, size_t len) {
    if (len < sizeof(struct icmp)) return;
    const struct icmp *ic = (const struct icmp *)p;
    if (ic->type == 8) {                                           /* echo request -> reply */
        uint8_t buf[1480]; if (len > sizeof buf) return;
        memcpy(buf, p, len);
        struct icmp *r = (struct icmp *)buf; r->type = 0; r->csum = 0; r->csum = csum(buf, len);
        ip_send(src, 1, buf, len);
    } else if (ic->type == 0 && ntohs(ic->id) == ping_st.id && ntohs(ic->seq) == ping_st.seq) {
        ping_st.t = uptime_ms(); ping_st.got = true;
    }
}

int net_ping(uint32_t ip, int timeout_ms, int *rtt) {
    static uint16_t seq;
    uint8_t buf[sizeof(struct icmp) + 32];
    struct icmp *ic = (struct icmp *)buf;
    ic->type = 8; ic->code = 0; ic->csum = 0; ic->id = htons(0x4D45); ic->seq = htons(++seq);
    for (int i = 0; i < 32; i++) buf[sizeof *ic + i] = (uint8_t)('a' + i % 26);
    ic->csum = csum(buf, sizeof buf);
    ping_st.id = 0x4D45; ping_st.seq = seq; ping_st.got = false;
    uint32_t t0 = uptime_ms();
    int rc = ip_send(ip, 1, buf, sizeof buf);
    if (rc) return rc;
    while ((int)(uptime_ms() - t0) < timeout_ms) {
        if (ping_st.got) { if (rtt) *rtt = (int)(ping_st.t - t0); return 0; }
        task_sleep_ms(2);
    }
    return -EAGAIN;
}

/* ---- DHCP client ---- */
static struct { uint32_t xid, yiaddr, server, mask, gw, dns; uint8_t type; } dh;

static void dhcp_rx(uint32_t src, uint16_t sport, const uint8_t *d, size_t len) {
    (void)src; (void)sport;
    if (len < 240 || d[0] != 2) return;
    uint32_t xid = (uint32_t)(d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]);
    if (xid != dh.xid) return;
    dh.yiaddr = (uint32_t)(d[16] << 24 | d[17] << 16 | d[18] << 8 | d[19]);
    for (size_t i = 240; i + 1 < len && d[i] != 255;) {
        if (d[i] == 0) { i++; continue; }
        uint8_t op = d[i], l = d[i + 1]; const uint8_t *v = d + i + 2;
        if (i + 2 + l > len) break;
        uint32_t w = l >= 4 ? (uint32_t)(v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]) : 0;
        if (op == 53) dh.type = v[0];
        else if (op == 54) dh.server = w;
        else if (op == 1) dh.mask = w;
        else if (op == 3) dh.gw = w;
        else if (op == 6) dh.dns = w;
        i += 2u + l;
    }
}

static void dhcp_send(uint8_t type, uint32_t req_ip, uint32_t server) {
    uint8_t p[300]; memset(p, 0, sizeof p);
    p[0] = 1; p[1] = 1; p[2] = 6;
    p[4] = (uint8_t)(dh.xid >> 24); p[5] = (uint8_t)(dh.xid >> 16); p[6] = (uint8_t)(dh.xid >> 8); p[7] = (uint8_t)dh.xid;
    p[10] = 0x80;                                                 /* broadcast flag */
    memcpy(p + 28, dev->mac, 6);
    p[236] = 0x63; p[237] = 0x82; p[238] = 0x53; p[239] = 0x63;
    size_t o = 240;
    p[o++] = 53; p[o++] = 1; p[o++] = type;
    if (type == 3) {
        p[o++] = 50; p[o++] = 4; p[o++] = (uint8_t)(req_ip >> 24); p[o++] = (uint8_t)(req_ip >> 16); p[o++] = (uint8_t)(req_ip >> 8); p[o++] = (uint8_t)req_ip;
        p[o++] = 54; p[o++] = 4; p[o++] = (uint8_t)(server >> 24); p[o++] = (uint8_t)(server >> 16); p[o++] = (uint8_t)(server >> 8); p[o++] = (uint8_t)server;
    }
    p[o++] = 55; p[o++] = 3; p[o++] = 1; p[o++] = 3; p[o++] = 6;
    p[o++] = 255;
    uint8_t pkt[sizeof(struct udph) + sizeof p];
    struct udph *u = (struct udph *)pkt;
    u->sport = htons(68); u->dport = htons(67); u->len = htons((uint16_t)(sizeof *u + o)); u->csum = 0;
    memcpy(pkt + sizeof *u, p, o);
    ip_send_raw(bcast, 0, 0xFFFFFFFFu, 17, pkt, sizeof *u + o);
    dev->tx++;
}

int net_dhcp(int timeout_ms) {
    if (!dev) return -ENODEV;
    uint32_t old = net_ip;
    memset(&dh, 0, sizeof dh); dh.xid = 0x4D454D4Fu ^ uptime_ms();
    net_ip = 0;
    udp_bind(68, dhcp_rx);
    int rc = -EAGAIN;
    dhcp_send(1, 0, 0);
    uint32_t t0 = uptime_ms();
    while ((int)(uptime_ms() - t0) < timeout_ms && dh.type != 2) task_sleep_ms(5);
    if (dh.type == 2) {
        uint32_t offer = dh.yiaddr, srv = dh.server; dh.type = 0;
        dhcp_send(3, offer, srv);
        while ((int)(uptime_ms() - t0) < timeout_ms && dh.type != 5) task_sleep_ms(5);
        if (dh.type == 5) {
            net_ip = dh.yiaddr; if (dh.mask) net_mask = dh.mask; if (dh.gw) net_gw = dh.gw; if (dh.dns) net_dns = dh.dns;
            rc = 0;
        }
    }
    udp_unbind(68);
    if (rc) net_ip = old;
    return rc;
}

/* ---- receive path ---- */
void net_rx(const void *frame, size_t len) {
    if (len < sizeof(struct eth)) return;
    const struct eth *e = frame; const uint8_t *pl = (const uint8_t *)frame + sizeof *e; size_t pn = len - sizeof *e;
    if (dev) dev->rx++;
    uint16_t type = ntohs(e->type);
    if (type == ETH_ARP && pn >= sizeof(struct arp)) {
        const struct arp *a = (const struct arp *)pl;
        uint32_t spa = ntohl(a->spa), tpa = ntohl(a->tpa);
        arp_learn(spa, a->sha);
        if (ntohs(a->op) == 1 && tpa == net_ip && net_ip) {
            struct arp r = *a; r.op = htons(2);
            memcpy(r.tha, a->sha, 6); r.tpa = a->spa; memcpy(r.sha, dev->mac, 6); r.spa = htonl(net_ip);
            eth_send(a->sha, ETH_ARP, &r, sizeof r);
        }
    } else if (type == ETH_IP && pn >= sizeof(struct ip4h)) {
        const struct ip4h *h = (const struct ip4h *)pl;
        size_t hl = (size_t)(h->vhl & 15) * 4, tl = ntohs(h->len);
        if ((h->vhl >> 4) != 4 || hl < 20 || tl > pn || tl < hl) return;
        uint32_t dst = ntohl(h->dst), src = ntohl(h->src);
        if (dst != net_ip && dst != 0xFFFFFFFFu && net_ip != 0) return;
        const uint8_t *p = pl + hl; size_t plen = tl - hl;
        if (h->proto == 1) icmp_rx(src, p, plen);
        else if (h->proto == 17 && plen >= sizeof(struct udph)) {
            const struct udph *u = (const struct udph *)p;
            uint16_t dport = ntohs(u->dport); size_t ul = ntohs(u->len);
            if (ul < sizeof *u || ul > plen) return;
            for (int i = 0; i < 4; i++) if (udp_binds[i].port == dport && udp_binds[i].cb) udp_binds[i].cb(src, ntohs(u->sport), p + sizeof *u, ul - sizeof *u);
        }
    }
}

/* ---- polling thread ---- */
static void netd(void) {
    for (;;) { if (dev && dev->poll) dev->poll(); task_sleep_ms(2); }
}
static int net_init(void) { task_create("netd", netd); return 0; }
late_initcall(net_init);
