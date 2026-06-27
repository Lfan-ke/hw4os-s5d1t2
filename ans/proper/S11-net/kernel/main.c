/* S11 · 内核入口/测试驱动（给定，勿改）。
 * 三步自检（纯软件，不碰真实网卡）：
 *   1) 构造 ARP 请求并校验字节布局           → ARP_PASS
 *   2) 构造 IPv4 头并校验 Internet Checksum  → IP_PASS
 *   3) loopback：自发自收一个 UDP 包，解析后核对载荷 → UDP_PASS
 * 全过打印 ALL_PASS，kmain 返回触发关机。 */
#include "kernel.h"
#include "net.h"

/* 打印一个点分十进制 IPv4。 */
static void put_ip(const uint8_t ip[IPV4_ALEN]) {
    for (int i = 0; i < IPV4_ALEN; i++) {
        kputdec(ip[i]);
        if (i != IPV4_ALEN - 1) console_putchar('.');
    }
}

/* —— 测试常量 —— */
static const uint8_t SHA[ETH_ALEN]  = {0x52,0x54,0x00,0x12,0x34,0x56};
static const uint8_t SRC_IP[IPV4_ALEN] = {10,0,2,15};
static const uint8_t DST_IP[IPV4_ALEN] = {10,0,2,2};

/* —— 子项 1：ARP 请求字节布局 —— */
static int test_arp(void) {
    uint8_t f[64];
    uint32_t n = arp_build_request(f, SHA, SRC_IP, DST_IP);
    if (n != ETH_HDR_LEN + ARP_PKT_LEN) return 0;        /* 42 */

    /* 以太头：广播目的、源=SHA、类型=ARP */
    for (int i = 0; i < ETH_ALEN; i++) if (f[i] != 0xFF) return 0;
    if (!kmemeq(f + 6, SHA, ETH_ALEN)) return 0;
    if (f[12] != 0x08 || f[13] != 0x06) return 0;

    const uint8_t *a = f + ETH_HDR_LEN;
    if (a[0] != 0 || a[1] != 1) return 0;                /* htype=ethernet */
    if (a[2] != 0x08 || a[3] != 0x00) return 0;          /* ptype=ipv4 */
    if (a[4] != 6 || a[5] != 4) return 0;                /* hlen/plen */
    if (a[6] != 0 || a[7] != 1) return 0;                /* oper=request */
    if (!kmemeq(a + 8, SHA, ETH_ALEN)) return 0;         /* sha */
    if (!kmemeq(a + 14, SRC_IP, IPV4_ALEN)) return 0;    /* spa */
    for (int i = 0; i < ETH_ALEN; i++) if (a[18 + i] != 0) return 0; /* tha=0 */
    if (!kmemeq(a + 24, DST_IP, IPV4_ALEN)) return 0;    /* tpa */

    kputs("  arp who-has ");
    put_ip(DST_IP); kputs(" tell "); put_ip(SRC_IP);
    console_putchar('\n');
    return 1;
}

/* —— 子项 2：IPv4 头 + Internet Checksum —— */
static int test_ip(void) {
    uint8_t iph[IP_HDR_LEN];
    ip_build_header(iph, SRC_IP, DST_IP, IPPROTO_UDP, 0x0030);  /* total=48 */

    if (iph[0] != IP_VER_IHL) return 0;                  /* v4 ihl=5 */
    if (iph[2] != 0x00 || iph[3] != 0x30) return 0;      /* total_len */
    if (iph[8] != IP_DEFAULT_TTL) return 0;              /* ttl */
    if (iph[9] != IPPROTO_UDP) return 0;                 /* proto */
    /* 校验和字段必须已填且非 0 */
    if (iph[10] == 0 && iph[11] == 0) return 0;
    /* 关键性质：含校验和字段在内全头反码和折叠应为 0 */
    if (cksum_fold(cksum_accumulate(iph, IP_HDR_LEN, 0)) != 0) return 0;
    /* 篡改一字节后校验应失效 */
    uint8_t bad[IP_HDR_LEN];
    kmemcpy(bad, iph, IP_HDR_LEN);
    bad[5] ^= 0xFF;
    if (cksum_fold(cksum_accumulate(bad, IP_HDR_LEN, 0)) == 0) return 0;

    uint16_t c = (uint16_t)((iph[10] << 8) | iph[11]);
    kputs("  ip "); put_ip(SRC_IP); kputs(" > "); put_ip(DST_IP);
    kputs(" proto=17 cksum="); kputhex(c); console_putchar('\n');
    return 1;
}

/* —— 子项 3：UDP loopback 自发自收 —— */
static int test_udp(void) {
    static const char msg[] = "RISCV-UDP-LOOPBACK-OK";
    uint32_t msglen = sizeof(msg) - 1;
    const uint16_t SPORT = 12345, DPORT = 53;

    uint8_t tx[128];
    uint32_t txlen = udp_build_datagram(tx, SRC_IP, DST_IP, SPORT, DPORT, msg, msglen);
    if (txlen != IP_HDR_LEN + UDP_HDR_LEN + msglen) return 0;

    /* loopback：把发送 buffer 原样搬到接收 buffer（自发自收） */
    uint8_t rx[128];
    kmemcpy(rx, tx, txlen);

    struct udp_recv r;
    if (udp_parse(rx, txlen, &r) != 0) return 0;
    if (r.sport != SPORT || r.dport != DPORT) return 0;
    if (!kmemeq(r.src_ip, SRC_IP, IPV4_ALEN)) return 0;
    if (!kmemeq(r.dst_ip, DST_IP, IPV4_ALEN)) return 0;
    if (r.paylen != msglen) return 0;
    if (!kmemeq(r.payload, msg, msglen)) return 0;

    /* 负面用例：篡改一个载荷字节后，UDP 校验应使解析失败 */
    uint8_t bad[128];
    kmemcpy(bad, tx, txlen);
    bad[txlen - 1] ^= 0xFF;
    struct udp_recv r2;
    if (udp_parse(bad, txlen, &r2) == 0) return 0;

    kputs("  udp "); kputdec(SPORT); kputs(" > "); kputdec(DPORT);
    kputs(" len="); kputdec(r.paylen); kputs(" payload=\"");
    for (uint32_t i = 0; i < r.paylen; i++) console_putchar(r.payload[i]);
    kputs("\"\n");
    return 1;
}

void kmain(void) {
    kputs("\n[S11] tiny network stack: ARP / IPv4 / UDP (software, loopback)\n");

    int arp_ok = test_arp();
    if (arp_ok) kputs("ARP_PASS\n"); else kputs("ARP_mismatch\n");

    int ip_ok = test_ip();
    if (ip_ok) kputs("IP_PASS\n"); else kputs("IP_mismatch\n");

    int udp_ok = test_udp();
    if (udp_ok) kputs("UDP_PASS\n"); else kputs("UDP_mismatch\n");

    if (arp_ok && ip_ok && udp_ok)
        kputs("ALL_PASS\n");
}
