/* TCP 连接状态机：网络之魂 —— C 参考解。
 *
 * 母题：UDP 只是「把一包字节扔出去」，没有连接、没有可靠。TCP 在不可靠数据报之上，
 * 靠一台**状态机**与几个**序号字段**，凭空造出「一条有始有终、字节不丢不乱的连接」。
 * 本课用一条 mock 通道在两个端点之间传「报文」结构（flags=SYN/ACK/FIN, seq, ack, len），
 * 亲手把状态机走一遍：
 *
 *   三次握手 CLOSED→SYN_SENT→ESTABLISHED；数据 发 N 字节 ack=seq+N；
 *   四次挥手 ESTABLISHED→FIN_WAIT_*→TIME_WAIT→CLOSED。
 *
 * 判据：HANDSHAKE_PASS / DATA_PASS / TEARDOWN_PASS / STATE_PASS，全过 ALL_PASS。
 * 失败诊断：*_MISS（没回报文，多半是 TODO 没填）/ *_BAD（字段或状态错）。
 *
 * 你只需填三处 TODO：① 服务器收 SYN 回 SYN+ACK；② 客户端收 SYN+ACK 回 ACK；
 * ③ 收到数据段时 seq/ack 的推进。其余均已给定，勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── 报文标志位（可叠加）── */
#define SYN 1
#define ACK 2
#define FIN 4

static int has(uint8_t flags, uint8_t bit) { return (flags & bit) != 0; }

/* 把 flags 渲染成可读串，写入 buf（至少 16 字节）。 */
static void flag_str(uint8_t f, char *buf) {
    buf[0] = 0;
    if (has(f, SYN)) strcat(buf, buf[0] ? "|SYN" : "SYN");
    if (has(f, ACK)) strcat(buf, buf[0] ? "|ACK" : "ACK");
    if (has(f, FIN)) strcat(buf, buf[0] ? "|FIN" : "FIN");
    if (!buf[0]) strcpy(buf, "-");
}

/* ── 报文：mock 通道传递的一个 TCP 段。present=0 表示「这一步无报文要发」。── */
typedef struct {
    int present;
    uint8_t flags;
    uint32_t seq, ack, len;
} Seg;

static Seg seg_none(void) {
    Seg s;
    memset(&s, 0, sizeof(s));
    return s;
}
static Seg seg_new(uint8_t flags, uint32_t seq, uint32_t ack, uint32_t len) {
    Seg s = {1, flags, seq, ack, len};
    return s;
}

/* ── TCP 状态（RFC 793 子集）── */
enum {
    CLOSED,
    LISTEN,
    SYN_SENT,
    SYN_RCVD,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    TIME_WAIT,
    CLOSE_WAIT,
    LAST_ACK,
};

static const char *state_name(int s) {
    static const char *n[] = {"CLOSED",     "LISTEN",     "SYN_SENT",  "SYN_RCVD",
                              "ESTABLISHED", "FIN_WAIT_1", "FIN_WAIT_2", "TIME_WAIT",
                              "CLOSE_WAIT",  "LAST_ACK"};
    return n[s];
}

/* ── 一个端点（一台 TCP 状态机）。SYN/FIN 各消耗 1 个序号；纯 ACK 不消耗。── */
typedef struct {
    const char *name;
    int state;
    uint32_t iss;     /* 初始发送序号（本课固定，方便对拍） */
    uint32_t snd_nxt; /* 下一个要发的序号 */
    uint32_t rcv_nxt; /* 期望对端下一个序号（= 我回的 ack） */
    int last_accepted; /* 上次 recv 是否被接受 */
    uint32_t rejected; /* 累计拒绝的非法报文数 */
} Tcp;

static void tcp_init(Tcp *t, const char *name, uint32_t iss) {
    t->name = name;
    t->state = CLOSED;
    t->iss = iss;
    t->snd_nxt = iss;
    t->rcv_nxt = 0;
    t->last_accepted = 0;
    t->rejected = 0;
}

static void tcp_listen(Tcp *t) { t->state = LISTEN; }

/* 客户端主动打开：CLOSED→SYN_SENT，发 SYN（消耗 1 个序号）。 */
static Seg tcp_active_open(Tcp *t) {
    Seg s = seg_new(SYN, t->iss, 0, 0);
    t->snd_nxt = t->iss + 1;
    t->state = SYN_SENT;
    return s;
}

/* ESTABLISHED 时发 n 字节：seq=snd_nxt, ack=rcv_nxt（捎带确认），snd_nxt += n。 */
static Seg tcp_send_data(Tcp *t, uint32_t n) {
    if (t->state != ESTABLISHED) return seg_none();
    Seg s = seg_new(ACK, t->snd_nxt, t->rcv_nxt, n);
    t->snd_nxt += n;
    return s;
}

/* 主动关闭：发 FIN（消耗 1 个序号）。ESTABLISHED→FIN_WAIT_1；CLOSE_WAIT→LAST_ACK。 */
static Seg tcp_active_close(Tcp *t) {
    if (t->state == ESTABLISHED) {
        Seg s = seg_new(FIN, t->snd_nxt, t->rcv_nxt, 0);
        t->snd_nxt += 1;
        t->state = FIN_WAIT_1;
        return s;
    }
    if (t->state == CLOSE_WAIT) {
        Seg s = seg_new(FIN, t->snd_nxt, t->rcv_nxt, 0);
        t->snd_nxt += 1;
        t->state = LAST_ACK;
        return s;
    }
    return seg_none();
}

/* TIME_WAIT 的 2MSL 定时器到期：→ CLOSED（本课直接触发）。 */
static void tcp_tick(Tcp *t) {
    if (t->state == TIME_WAIT) t->state = CLOSED;
}

/* 状态机核心：收一个报文，更新状态/序号，返回要回发的报文（present=0 表示无需回复）。 */
static Seg tcp_recv(Tcp *t, Seg s) {
    t->last_accepted = 1; /* 默认接受；落到末尾 default 才置 0 */

    /* —— 服务器被动打开：LISTEN 收到纯 SYN —— */
    if (t->state == LISTEN && has(s.flags, SYN) && !has(s.flags, ACK)) {
        /* ───────────── TODO ① 收 SYN 发 SYN+ACK ─────────────
         * 1) rcv_nxt = s.seq + 1（SYN 占 1 个序号）。
         * 2) 回一个 SYN|ACK：seq=iss, ack=rcv_nxt, len=0。
         * 3) snd_nxt = iss + 1（我的 SYN 也占 1 个序号）。
         * 4) 状态 → SYN_RCVD，返回那个 SYN|ACK 段。 */
        t->rcv_nxt = s.seq + 1;
        Seg out = seg_new(SYN | ACK, t->iss, t->rcv_nxt, 0);
        t->snd_nxt = t->iss + 1;
        t->state = SYN_RCVD;
        return out;
        /* ──────────────────────────────────────────────────── */
    }
    /* —— 客户端：SYN_SENT 收到 SYN+ACK —— */
    if (t->state == SYN_SENT && has(s.flags, SYN) && has(s.flags, ACK)) {
        /* ───────────── TODO ② 收 SYN+ACK 发 ACK ─────────────
         * 1) rcv_nxt = s.seq + 1。
         * 2) 回纯 ACK：seq=snd_nxt, ack=rcv_nxt, len=0。
         * 3) 状态 → ESTABLISHED，返回那个 ACK 段。 */
        t->rcv_nxt = s.seq + 1;
        Seg out = seg_new(ACK, t->snd_nxt, t->rcv_nxt, 0);
        t->state = ESTABLISHED;
        return out;
        /* ──────────────────────────────────────────────────── */
    }
    /* —— 服务器：SYN_RCVD 收到第三步 ACK → ESTABLISHED（无需回复）—— */
    if (t->state == SYN_RCVD && has(s.flags, ACK) && !has(s.flags, SYN)) {
        t->state = ESTABLISHED;
        return seg_none();
    }
    /* —— ESTABLISHED 收到数据段（len>0）—— */
    if (t->state == ESTABLISHED && s.len > 0) {
        /* ───────────── TODO ③ 数据 seq/ack 推进 ─────────────
         * rcv_nxt = s.seq + s.len；回纯 ACK：seq=snd_nxt, ack=rcv_nxt, len=0。 */
        t->rcv_nxt = s.seq + s.len;
        return seg_new(ACK, t->snd_nxt, t->rcv_nxt, 0);
        /* ──────────────────────────────────────────────────── */
    }
    /* —— ESTABLISHED 收到纯 ACK（确认我们的数据）→ 无需回复 —— */
    if (t->state == ESTABLISHED && has(s.flags, ACK) && s.len == 0 && !has(s.flags, FIN)) {
        return seg_none();
    }
    /* —— ESTABLISHED 收到 FIN（被动关闭）→ 回 ACK，进 CLOSE_WAIT —— */
    if (t->state == ESTABLISHED && has(s.flags, FIN)) {
        t->rcv_nxt = s.seq + 1;
        Seg out = seg_new(ACK, t->snd_nxt, t->rcv_nxt, 0);
        t->state = CLOSE_WAIT;
        return out;
    }
    /* —— FIN_WAIT_1 收到对我们 FIN 的 ACK → FIN_WAIT_2 —— */
    if (t->state == FIN_WAIT_1 && has(s.flags, ACK) && !has(s.flags, FIN)) {
        t->state = FIN_WAIT_2;
        return seg_none();
    }
    /* —— FIN_WAIT_2 收到对端 FIN → 回 ACK，进 TIME_WAIT —— */
    if (t->state == FIN_WAIT_2 && has(s.flags, FIN)) {
        t->rcv_nxt = s.seq + 1;
        Seg out = seg_new(ACK, t->snd_nxt, t->rcv_nxt, 0);
        t->state = TIME_WAIT;
        return out;
    }
    /* —— LAST_ACK 收到对我们 FIN 的 ACK → CLOSED —— */
    if (t->state == LAST_ACK && has(s.flags, ACK)) {
        t->state = CLOSED;
        return seg_none();
    }
    /* —— 其它一律非法：拒绝（不改状态，记一笔）—— */
    t->last_accepted = 0;
    t->rejected++;
    return seg_none();
}

/* 本课固定的初始序号（真实 TCP 用随机 ISN 防旧连接/盲攻击）。 */
#define ISS_A 1000u /* 客户端 */
#define ISS_B 5000u /* 服务器 */
#define DATA_LEN 10u

/* ════════════ mock 通道驱动 ════════════ */

/* 走完三次握手；成功返回 1，且 a、b 都 ESTABLISHED。 */
static int do_handshake(Tcp *a, Tcp *b) {
    tcp_listen(b);
    Seg syn = tcp_active_open(a); /* A: CLOSED→SYN_SENT，发 SYN */
    if (a->state != SYN_SENT) {
        printf("HANDSHAKE_BAD A active_open 后应在 SYN_SENT，实为 %s\n", state_name(a->state));
        return 0;
    }
    Seg synack = tcp_recv(b, syn); /* B: LISTEN→SYN_RCVD，回 SYN|ACK */
    if (!synack.present) {
        printf("HANDSHAKE_MISS B 收到 SYN 没回 SYN+ACK（TODO ① 没填？）\n");
        return 0;
    }
    char fb[16];
    flag_str(synack.flags, fb);
    if (synack.flags != (SYN | ACK) || synack.seq != ISS_B || synack.ack != ISS_A + 1) {
        printf("HANDSHAKE_BAD B 的 SYN+ACK 字段错: flags=%s seq=%u ack=%u 应=(SYN|ACK,%u,%u)\n", fb,
               synack.seq, synack.ack, ISS_B, ISS_A + 1);
        return 0;
    }
    Seg ack = tcp_recv(a, synack); /* A: SYN_SENT→ESTABLISHED，回 ACK */
    if (!ack.present) {
        printf("HANDSHAKE_MISS A 收到 SYN+ACK 没回 ACK（TODO ② 没填？）\n");
        return 0;
    }
    flag_str(ack.flags, fb);
    if (ack.flags != ACK || ack.seq != ISS_A + 1 || ack.ack != ISS_B + 1) {
        printf("HANDSHAKE_BAD A 的 ACK 字段错: flags=%s seq=%u ack=%u 应=(ACK,%u,%u)\n", fb, ack.seq,
               ack.ack, ISS_A + 1, ISS_B + 1);
        return 0;
    }
    (void)tcp_recv(b, ack); /* B: SYN_RCVD→ESTABLISHED */
    return 1;
}

static int check_handshake(void) {
    Tcp a, b;
    tcp_init(&a, "A", ISS_A);
    tcp_init(&b, "B", ISS_B);
    if (!do_handshake(&a, &b)) return 0;
    int ok = 1;
    if (a.state != ESTABLISHED) {
        printf("HANDSHAKE_BAD 握手后 A 应 ESTABLISHED，实为 %s\n", state_name(a.state));
        ok = 0;
    }
    if (b.state != ESTABLISHED) {
        printf("HANDSHAKE_BAD 握手后 B 应 ESTABLISHED，实为 %s\n", state_name(b.state));
        ok = 0;
    }
    if (a.snd_nxt != ISS_A + 1 || a.rcv_nxt != ISS_B + 1) {
        printf("HANDSHAKE_BAD A 序号错: snd_nxt=%u rcv_nxt=%u 应=(%u,%u)\n", a.snd_nxt, a.rcv_nxt,
               ISS_A + 1, ISS_B + 1);
        ok = 0;
    }
    if (b.snd_nxt != ISS_B + 1 || b.rcv_nxt != ISS_A + 1) {
        printf("HANDSHAKE_BAD B 序号错: snd_nxt=%u rcv_nxt=%u 应=(%u,%u)\n", b.snd_nxt, b.rcv_nxt,
               ISS_B + 1, ISS_A + 1);
        ok = 0;
    }
    if (ok)
        printf("HANDSHAKE_PASS 三次握手完成，两端 ESTABLISHED（A.seq=%u B.seq=%u）\n", a.snd_nxt,
               b.snd_nxt);
    return ok;
}

static int check_data(void) {
    Tcp a, b;
    tcp_init(&a, "A", ISS_A);
    tcp_init(&b, "B", ISS_B);
    if (!do_handshake(&a, &b)) {
        printf("DATA_MISS 握手没成，数据传输无从谈起（先过 HANDSHAKE）\n");
        return 0;
    }
    int ok = 1;

    Seg dat = tcp_send_data(&a, DATA_LEN); /* A 发 DATA_LEN 字节 */
    if (!dat.present) {
        printf("DATA_MISS A 没能发出数据段（不在 ESTABLISHED？）\n");
        return 0;
    }
    if (dat.seq != ISS_A + 1 || dat.len != DATA_LEN || dat.ack != ISS_B + 1) {
        printf("DATA_BAD A 数据段字段错: seq=%u ack=%u len=%u 应=(%u,%u,%u)\n", dat.seq, dat.ack,
               dat.len, ISS_A + 1, ISS_B + 1, DATA_LEN);
        ok = 0;
    }
    if (a.snd_nxt != ISS_A + 1 + DATA_LEN) {
        printf("DATA_BAD A 发完 %u 字节后 snd_nxt=%u 应=%u\n", DATA_LEN, a.snd_nxt,
               ISS_A + 1 + DATA_LEN);
        ok = 0;
    }

    Seg dack = tcp_recv(&b, dat); /* B 收数据，回 ack=seq+len */
    if (!dack.present) {
        printf("DATA_MISS B 收到数据没回 ACK（TODO ③ 没填？）\n");
        return 0;
    }
    char fb[16];
    flag_str(dack.flags, fb);
    if (dack.flags != ACK || dack.ack != ISS_A + 1 + DATA_LEN) {
        printf("DATA_BAD B 的 ACK 错: flags=%s ack=%u 应 ack=%u（=seq+len）\n", fb, dack.ack,
               ISS_A + 1 + DATA_LEN);
        ok = 0;
    }
    if (b.rcv_nxt != ISS_A + 1 + DATA_LEN) {
        printf("DATA_BAD B 收完后 rcv_nxt=%u 应=%u（=seq+len）\n", b.rcv_nxt, ISS_A + 1 + DATA_LEN);
        ok = 0;
    }

    Seg r = tcp_recv(&a, dack); /* A 收纯 ACK，不必回 */
    if (r.present) {
        printf("DATA_BAD A 收到纯 ACK 不该再回报文（会无限互 ACK）\n");
        ok = 0;
    }
    if (a.state != ESTABLISHED) {
        printf("DATA_BAD A 传完数据后应仍 ESTABLISHED，实为 %s\n", state_name(a.state));
        ok = 0;
    }

    if (ok)
        printf("DATA_PASS 传 %u 字节：A.snd_nxt %u→%u，B.ack=%u（seq+len 推进正确）\n", DATA_LEN,
               ISS_A + 1, a.snd_nxt, dack.ack);
    return ok;
}

static int check_teardown(void) {
    Tcp a, b;
    tcp_init(&a, "A", ISS_A);
    tcp_init(&b, "B", ISS_B);
    if (!do_handshake(&a, &b)) {
        printf("TEARDOWN_MISS 握手没成，无连接可拆（先过 HANDSHAKE）\n");
        return 0;
    }
    int ok = 1;

    Seg fin1 = tcp_active_close(&a); /* ① A 发 FIN，进 FIN_WAIT_1 */
    if (!fin1.present || a.state != FIN_WAIT_1) {
        printf("TEARDOWN_BAD A active_close 应发 FIN 并进 FIN_WAIT_1，实为 %s\n", state_name(a.state));
        return 0;
    }
    Seg ack1 = tcp_recv(&b, fin1); /* ② B 回 ACK，进 CLOSE_WAIT */
    if (!ack1.present || b.state != CLOSE_WAIT) {
        printf("TEARDOWN_BAD B 收 FIN 应回 ACK 并进 CLOSE_WAIT，实为 %s\n", state_name(b.state));
        return 0;
    }
    (void)tcp_recv(&a, ack1); /* ③ A 进 FIN_WAIT_2 */
    if (a.state != FIN_WAIT_2) {
        printf("TEARDOWN_BAD A 收到 FIN 的 ACK 应进 FIN_WAIT_2，实为 %s\n", state_name(a.state));
        ok = 0;
    }
    Seg fin2 = tcp_active_close(&b); /* ④ B 发 FIN，进 LAST_ACK */
    if (!fin2.present || b.state != LAST_ACK) {
        printf("TEARDOWN_BAD B active_close 应发 FIN 并进 LAST_ACK，实为 %s\n", state_name(b.state));
        return 0;
    }
    Seg ack2 = tcp_recv(&a, fin2); /* ⑤ A 回 ACK，进 TIME_WAIT */
    if (!ack2.present || a.state != TIME_WAIT) {
        printf("TEARDOWN_BAD A 收 B 的 FIN 应回 ACK 并进 TIME_WAIT，实为 %s\n", state_name(a.state));
        return 0;
    }
    (void)tcp_recv(&b, ack2); /* ⑥ B 进 CLOSED */
    tcp_tick(&a);             /* ⑦ A 的 TIME_WAIT 到期 → CLOSED */

    if (a.state != CLOSED) {
        printf("TEARDOWN_BAD 挥手后 A 应 CLOSED，实为 %s\n", state_name(a.state));
        ok = 0;
    }
    if (b.state != CLOSED) {
        printf("TEARDOWN_BAD 挥手后 B 应 CLOSED，实为 %s\n", state_name(b.state));
        ok = 0;
    }
    if (a.snd_nxt != ISS_A + 2 || b.snd_nxt != ISS_B + 2) {
        printf("TEARDOWN_BAD FIN 序号错: A.snd_nxt=%u B.snd_nxt=%u 应=(%u,%u)\n", a.snd_nxt,
               b.snd_nxt, ISS_A + 2, ISS_B + 2);
        ok = 0;
    }

    if (ok) printf("TEARDOWN_PASS 四次挥手完成，两端回到 CLOSED\n");
    return ok;
}

static int check_state(void) {
    int ok = 1;

    /* (a) CLOSED 收数据/ACK 报文 → 拒绝，状态不变。 */
    Tcp c;
    tcp_init(&c, "X", 9000);
    Seg r = tcp_recv(&c, seg_new(ACK, 1, 1, 4));
    if (c.last_accepted || c.state != CLOSED || r.present) {
        printf("STATE_BAD CLOSED 收数据报文该拒，却被接受/改了状态（state=%s）\n", state_name(c.state));
        ok = 0;
    }

    /* (b) CLOSED 收 FIN → 拒绝。 */
    Tcp c2;
    tcp_init(&c2, "X", 9000);
    (void)tcp_recv(&c2, seg_new(FIN, 1, 1, 0));
    if (c2.last_accepted) {
        printf("STATE_BAD CLOSED 收 FIN 该拒，却被接受\n");
        ok = 0;
    }

    /* (c) ESTABLISHED 收 SYN（重复建链）→ 拒绝，状态不变。 */
    Tcp e;
    tcp_init(&e, "Y", 9000);
    e.state = ESTABLISHED;
    e.snd_nxt = 9001;
    e.rcv_nxt = 7777;
    Seg r2 = tcp_recv(&e, seg_new(SYN, 1, 1, 0));
    if (e.last_accepted || e.state != ESTABLISHED || r2.present) {
        printf("STATE_BAD ESTABLISHED 收 SYN 该拒，却被接受/改了状态（state=%s）\n", state_name(e.state));
        ok = 0;
    }

    /* (d) LISTEN 收 FIN（不是 SYN）→ 拒绝。 */
    Tcp l;
    tcp_init(&l, "Z", 9000);
    tcp_listen(&l);
    (void)tcp_recv(&l, seg_new(FIN, 1, 1, 0));
    if (l.last_accepted || l.state != LISTEN) {
        printf("STATE_BAD LISTEN 只接受 SYN，收 FIN 该拒（state=%s）\n", state_name(l.state));
        ok = 0;
    }

    /* (e) 状态序正确：A 完整生命周期经过的状态，应与 RFC 793 主动方路径一致。 */
    Tcp a, b;
    tcp_init(&a, "A", ISS_A);
    tcp_init(&b, "B", ISS_B);
    int seq[8];
    int n = 0;
    seq[n++] = a.state; /* CLOSED */
    if (do_handshake(&a, &b)) {
        seq[n++] = a.state; /* ESTABLISHED */
        Seg dat = tcp_send_data(&a, DATA_LEN);
        Seg dack = tcp_recv(&b, dat);
        (void)tcp_recv(&a, dack);
        Seg fin1 = tcp_active_close(&a);
        seq[n++] = a.state; /* FIN_WAIT_1 */
        Seg ack1 = tcp_recv(&b, fin1);
        (void)tcp_recv(&a, ack1);
        seq[n++] = a.state; /* FIN_WAIT_2 */
        Seg fin2 = tcp_active_close(&b);
        Seg ack2 = tcp_recv(&a, fin2);
        seq[n++] = a.state; /* TIME_WAIT */
        (void)tcp_recv(&b, ack2);
        tcp_tick(&a);
        seq[n++] = a.state; /* CLOSED */
        int want[6] = {CLOSED, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, TIME_WAIT, CLOSED};
        int match = (n == 6);
        for (int i = 0; i < n && match; i++)
            if (seq[i] != want[i]) match = 0;
        if (!match) {
            printf("STATE_BAD A 状态序不符 RFC 793 主动方路径\n");
            ok = 0;
        }
    } else {
        printf("STATE_BAD 状态序需先过握手（见 HANDSHAKE 诊断）\n");
        ok = 0;
    }

    if (ok) printf("STATE_PASS 非法转移全部被拒，主动方状态序与 RFC 793 一致\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_handshake();
    all &= check_data();
    all &= check_teardown();
    all &= check_state();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
