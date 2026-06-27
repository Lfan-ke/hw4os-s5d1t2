# 正经·S11 · 简易协议栈：ARP / IPv4 / UDP（软件，参考 xv6 net）

> 网络栈的「难」不在外设，而在**字节布局、字节序、校验和**这三件素朴的小事。
> 本课不碰真实网卡（virtio-net），而是在内核 buffer 上**纸面**完成报文的构造与解析，
> 用 **loopback 自发自收**把「我造的包，我自己能不能正确解出来」闭环验证。
> 接口与字节布局和真实网络完全一致——把 buffer 喂给真网卡就能上线。

## 0. 这节课在讲什么

三个分层、各一件核心技能：

1. **ARP**（链路解析）：以太头(14) + ARP(28)。「who-has 10.0.2.2 tell 10.0.2.15」——
   广播问某 IP 的 MAC。重点是字段摆位与**大端序**。
2. **IPv4**（网络层）：20 字节头 + **Internet Checksum**（RFC 1071 的 16-bit 反码和）。
3. **UDP**（传输层）：8 字节头 + **伪首部校验和**（把 src/dst IP、协议号、长度也算进校验，
   让校验跨越分层、能发现「投递到错地址」）。loopback 把构造好的 UDP 报文原样收回，
   解析、核对载荷。

> 全程纯软件、纯计算：没有中断、没有 DMA、没有分页。`kmain` 跑完三步自检即关机。

## 1. 你要实现的（`kernel/net.c` 三个填空点）

构造函数（`arp_build_request` / `ip_build_header` / `udp_build_datagram`）与校验和**核心**
（`cksum_accumulate` 累加、`cksum_fold` 回卷取反）都已给。你补三块：

### 填空点 1 — `ip_checksum(iph, ihl_bytes)`
对 IP 头全部字节（校验和字段此刻为 0）求 Internet Checksum：
```
return cksum_fold(cksum_accumulate(iph, ihl_bytes, 0));
```

### 填空点 2 — `udp_checksum(src, dst, udp_seg, udp_len)`
先摆 12 字节**伪首部** `src(4)|dst(4)|0|17|udp_len(2,大端)`，累加它得 `sum`，
再以 `sum` 为初值累加 UDP 段（校验和字段为 0），`fold`。结果为 0 时按 RFC 768 改填 `0xFFFF`。

### 填空点 3 — `udp_parse(ipframe, framelen, out)`
解析收到的 IPv4/UDP 报文，**校验通过**才填 `out` 并返回 0，否则返回 -1：
1. 长度/`ihl`/协议(==17) 检查；
2. IP 头校验：`cksum_fold(cksum_accumulate(ipframe, ihl, 0)) == 0`；
3. 取 `udp_len`，再做边界检查；
4. UDP 校验：伪首部 + UDP 段折叠为 0；
5. 填 `src/dst ip`、`sport/dport`、`payload = udp+8`、`paylen = udp_len-8`。

```
labctl run proper/S11-net
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `ARP_PASS` / `IP_PASS` / `UDP_PASS` / `ALL_PASS`，不出现 `FAIL` / `panic`。

## 2. 完成标准 (DoD)

- [ ] ARP 请求字节布局正确（广播目的、`oper=1`、sha/spa/tpa 摆位、大端类型字段）。
- [ ] IPv4 头 Internet Checksum 正确：含校验和字段在内全头折叠为 0；篡改任意字节即失效。
- [ ] UDP loopback：自发自收后解析出**完全一致的载荷**与端口；篡改一字节即被校验拒绝。
- [ ] 能说清：为什么 UDP 校验和要把**伪首部**（IP 的 src/dst）也算进去。

## 3. 关键点与坑

- **字节序**：网络字节序是大端，RV64 是小端。所有多字节字段必须 `(hi<<8)|lo` 手工拼字节，
  **不要**把 buffer 指针强转成 `uint16_t*` 直接读——那样在小端机上会读反。
- **反码和判 0**：Internet Checksum 的优雅性质——发送方把校验和填成「补码使总和为全 1」，
  接收方把校验和也算进去后，全段反码和一定折叠为 0。校验只需判这个 0，不必反推。
- **奇数长度**：累加以 16-bit 为单位，末尾落单的字节当作高字节补 0（`<<8`）。

## 4. 引申

- **真网卡**：把构造好的 frame 交给 virtio-net 的 TX 队列、从 RX 队列收，加 DMA descriptor。
- **ICMP / TCP**：ICMP 复用 IP 层 + 自己的 8 字节头（ping）；TCP 在 UDP 之上多了序号、
  握手、重传、滑动窗口——校验和算法同源（仍是 Internet Checksum + 伪首部）。
- **ARP 表 / 路由**：缓存 IP→MAC、按子网/网关选下一跳；本课只构造单个请求，不维护表。
