/* S11 · 简易协议栈（软件，参考 xv6 net）：以太/ARP/IPv4/UDP 头的内核内构造与解析。
 *
 * 全部在内核 buffer 上「纸面」完成：不碰真实网卡（virtio-net），但报文字节布局、
 * 大端序、校验和算法与真实网络完全一致——loopback 自发自收即用本机解析自己构造的包。
 */
#ifndef OSLAB_NET_H
#define OSLAB_NET_H
#include <stdint.h>

/* —— 链路/网络层常量 —— */
#define ETH_ALEN        6          /* MAC 地址长度 */
#define IPV4_ALEN       4          /* IPv4 地址长度 */
#define ETH_HDR_LEN     14         /* 目的(6)+源(6)+类型(2) */
#define ARP_PKT_LEN     28         /* ARP(以太/IPv4) 固定 28 字节 */
#define IP_HDR_LEN      20         /* 无选项 IPv4 头 */
#define UDP_HDR_LEN     8

#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV4  0x0800

#define ARP_HTYPE_ETH   1          /* 硬件类型：以太网 */
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

#define IP_VER_IHL      0x45       /* version=4, ihl=5（20 字节） */
#define IP_DEFAULT_TTL  64
#define IPPROTO_UDP     17

/* UDP 解析结果。payload 指向接收 buffer 内部，prefix 已校验。 */
struct udp_recv {
    uint8_t  src_ip[IPV4_ALEN];
    uint8_t  dst_ip[IPV4_ALEN];
    uint16_t sport;
    uint16_t dport;
    const uint8_t *payload;
    uint32_t paylen;
};

/* —— freestanding 极简工具 —— */
void *kmemcpy(void *dst, const void *src, uint32_t n);
void *kmemset(void *dst, int c, uint32_t n);
int   kmemeq(const void *a, const void *b, uint32_t n);

/* —— 校验和核心（给定）：按 16-bit 大端字累加 / 折叠取反 —— */
uint32_t cksum_accumulate(const uint8_t *data, uint32_t len, uint32_t sum);
uint16_t cksum_fold(uint32_t sum);

/* —— 报文构造（给定）—— */
uint32_t arp_build_request(uint8_t *frame,
                           const uint8_t sha[ETH_ALEN], const uint8_t spa[IPV4_ALEN],
                           const uint8_t tpa[IPV4_ALEN]);
void     ip_build_header(uint8_t *iph,
                         const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                         uint8_t proto, uint16_t total_len);
uint32_t udp_build_datagram(uint8_t *buf,
                            const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                            uint16_t sport, uint16_t dport,
                            const void *payload, uint32_t paylen);

/* —— 校验和计算 / 解析（学生实现）—— */
uint16_t ip_checksum(const uint8_t *iph, uint32_t ihl_bytes);
uint16_t udp_checksum(const uint8_t src[IPV4_ALEN], const uint8_t dst[IPV4_ALEN],
                      const uint8_t *udp_seg, uint32_t udp_len);
int      udp_parse(const uint8_t *ipframe, uint32_t framelen, struct udp_recv *out);

#endif
