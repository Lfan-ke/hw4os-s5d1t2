/* S11 · 简易协议栈实现（学生版：构造给定；IP/UDP 校验和与 UDP 解析为填空点）。
 *
 * 字节序约定：网络字节序 = 大端。所有多字节字段都「高字节在前」手工摆放，
 * 不依赖主机字节序（RV64 是小端，靠手摆字节才正确）。
 */
#include "net.h"

/* —— freestanding 工具：加属性避免被优化成对自身的 memcpy/memset 递归 —— */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemset(void *dst, int c, uint32_t n) {
    uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}
int kmemeq(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

/* —— 校验和核心（给定）——
 * Internet Checksum（RFC 1071）：把数据看成一串 16-bit 大端字累加进 32 位 sum，
 * 奇数末字节当作高字节补 0。fold 把进位回卷进低 16 位并取反。 */
uint32_t cksum_accumulate(const uint8_t *data, uint32_t len, uint32_t sum) {
    uint32_t i = 0;
    for (; i + 1 < len; i += 2)
        sum += ((uint32_t)data[i] << 8) | (uint32_t)data[i + 1];
    if (i < len)                       /* 奇数长度：末字节为高字节 */
        sum += (uint32_t)data[i] << 8;
    return sum;
}
uint16_t cksum_fold(uint32_t sum) {
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

/* —— ARP 请求构造（给定）——
 * 以太头(14) + ARP(28)。广播目的 MAC=ff..ff，oper=request，问 tpa 的 MAC。 */
uint32_t arp_build_request(uint8_t *frame,
                           const uint8_t sha[ETH_ALEN], const uint8_t spa[IPV4_ALEN],
                           const uint8_t tpa[IPV4_ALEN]) {
    uint8_t *e = frame;
    kmemset(e + 0, 0xFF, ETH_ALEN);          /* dst MAC = 广播 */
    kmemcpy(e + 6, sha, ETH_ALEN);           /* src MAC = sha */
    e[12] = ETHERTYPE_ARP >> 8; e[13] = ETHERTYPE_ARP & 0xFF;

    uint8_t *a = frame + ETH_HDR_LEN;
    a[0] = ARP_HTYPE_ETH >> 8;  a[1] = ARP_HTYPE_ETH & 0xFF;
    a[2] = ETHERTYPE_IPV4 >> 8; a[3] = ETHERTYPE_IPV4 & 0xFF;
    a[4] = ETH_ALEN;            a[5] = IPV4_ALEN;
    a[6] = ARP_OP_REQUEST >> 8; a[7] = ARP_OP_REQUEST & 0xFF;
    kmemcpy(a + 8,  sha, ETH_ALEN);          /* sender hw addr */
    kmemcpy(a + 14, spa, IPV4_ALEN);         /* sender proto addr */
    kmemset(a + 18, 0,   ETH_ALEN);          /* target hw addr：未知，置 0 */
    kmemcpy(a + 24, tpa, IPV4_ALEN);         /* target proto addr */
    return ETH_HDR_LEN + ARP_PKT_LEN;        /* 42 */
}

/* —— IPv4 头构造（给定）——
 * 摆好除校验和外的全部字段，校验和字段先置 0，再调 ip_checksum 填入。 */
void ip_build_header(uint8_t *iph,
                     const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                     uint8_t proto, uint16_t total_len) {
    iph[0] = IP_VER_IHL;                     /* v4 + ihl=5 */
    iph[1] = 0;                              /* DSCP/ECN */
    iph[2] = total_len >> 8; iph[3] = total_len & 0xFF;
    iph[4] = 0; iph[5] = 0;                  /* id */
    iph[6] = 0x40; iph[7] = 0;               /* flags=DF, frag=0 */
    iph[8] = IP_DEFAULT_TTL;
    iph[9] = proto;
    iph[10] = 0; iph[11] = 0;                /* checksum 占位 */
    kmemcpy(iph + 12, src, IPV4_ALEN);
    kmemcpy(iph + 16, dst, IPV4_ALEN);
    uint16_t c = ip_checksum(iph, IP_HDR_LEN);
    iph[10] = c >> 8; iph[11] = c & 0xFF;
}

/* —— UDP 数据报构造（给定）——
 * 布局：[IPv4 头 20][UDP 头 8][payload]。先摆 UDP 段(校验和置 0)，算伪首部校验和填回，
 * 再用总长摆 IP 头。 */
uint32_t udp_build_datagram(uint8_t *buf,
                            const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                            uint16_t sport, uint16_t dport,
                            const void *payload, uint32_t paylen) {
    uint16_t udp_len = (uint16_t)(UDP_HDR_LEN + paylen);
    uint8_t *udp = buf + IP_HDR_LEN;
    udp[0] = sport >> 8; udp[1] = sport & 0xFF;
    udp[2] = dport >> 8; udp[3] = dport & 0xFF;
    udp[4] = udp_len >> 8; udp[5] = udp_len & 0xFF;
    udp[6] = 0; udp[7] = 0;                   /* checksum 占位 */
    kmemcpy(udp + UDP_HDR_LEN, payload, paylen);

    uint16_t c = udp_checksum(src, dst, udp, udp_len);
    udp[6] = c >> 8; udp[7] = c & 0xFF;

    ip_build_header(buf, src, dst, IPPROTO_UDP, (uint16_t)(IP_HDR_LEN + udp_len));
    return IP_HDR_LEN + udp_len;
}

/* =========================================================================
 * 填空点 1：IPv4 头校验和。
 *   Internet Checksum：对头部全部 ihl_bytes 字节（校验和字段此时为 0）累加成 16-bit
 *   反码和。直接用给定的 cksum_accumulate + cksum_fold 组合即可。
 *   HINT：return cksum_fold(cksum_accumulate(iph, ihl_bytes, 0));
 * ========================================================================= */
uint16_t ip_checksum(const uint8_t *iph, uint32_t ihl_bytes) {
    (void)iph; (void)ihl_bytes;
    /* TODO: 实现 IP 头 Internet Checksum。下面占位让 IP 暂不通过：*/
    return 0;
}

/* =========================================================================
 * 填空点 2：UDP 伪首部校验和。
 *   UDP 校验和覆盖「伪首部(12) + UDP 头 + 数据」：
 *     伪首部 = src_ip(4) | dst_ip(4) | 0(1) | proto=17(1) | udp_len(2，大端)
 *   累加伪首部，再接着累加 udp_seg[0..udp_len)（校验和字段为 0），fold。
 *   特例：若结果为 0x0000，按 RFC 768 改填 0xFFFF（0 表示「不校验」）。
 * ========================================================================= */
uint16_t udp_checksum(const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                      const uint8_t *udp_seg, uint32_t udp_len) {
    (void)src; (void)dst; (void)udp_seg; (void)udp_len;
    /* TODO: 摆伪首部 → 累加伪首部 + UDP 段 → fold。下面占位让 UDP 暂不通过：*/
    return 0;
}

/* =========================================================================
 * 填空点 3：解析收到的 IPv4/UDP 数据报，校验后填 out。返回 0 成功 / -1 丢弃。
 *   步骤：
 *     1) framelen >= 20；ihl = (ipframe[0] & 0x0F) * 4；framelen >= ihl + 8。
 *     2) 协议必须是 UDP（ipframe[9] == 17）。
 *     3) 校验 IP 头：cksum_fold(cksum_accumulate(ipframe, ihl, 0)) == 0 才合法。
 *     4) udp = ipframe + ihl；udp_len = (udp[4]<<8)|udp[5]；framelen >= ihl + udp_len。
 *     5) 校验 UDP：伪首部用 ipframe+12(src)/ipframe+16(dst)，
 *        cksum_fold(伪首部累加 + udp 段累加) == 0 才合法（段含校验和字段）。
 *     6) 填 out：src/dst ip、sport=(udp[0]<<8)|udp[1]、dport=(udp[2]<<8)|udp[3]、
 *        payload = udp + 8、paylen = udp_len - 8。
 * ========================================================================= */
int udp_parse(const uint8_t *ipframe, uint32_t framelen, struct udp_recv *out) {
    (void)ipframe; (void)framelen; (void)out;
    /* TODO: 边界/协议检查 → 校验 IP 头与 UDP 段 → 填 out。下面占位让 UDP 暂不通过：*/
    return -1;
}
