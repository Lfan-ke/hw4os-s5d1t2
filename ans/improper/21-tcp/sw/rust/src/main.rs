//! TCP 连接状态机：网络之魂 —— Rust 参考解。
//!
//! 母题：UDP 只是「把一包字节扔出去」，没有连接、没有可靠。TCP 在 UDP 那样的
//! 不可靠数据报之上，靠一台**状态机**与几个**序号字段**，凭空造出
//! 「一条有始有终、字节不丢不乱的连接」。本课就用一条 mock 通道在两个端点
//! 之间传「报文」结构（flags=SYN/ACK/FIN, seq, ack, len），亲手把这台状态机走一遍：
//!
//!   三次握手   CLOSED → SYN_SENT → ESTABLISHED        （建链）
//!   数据传输   发 N 字节，对端 ack = seq + len         （seq/ack 推进）
//!   四次挥手   ESTABLISHED → FIN_WAIT_* → TIME_WAIT → CLOSED （拆链）
//!
//! 四个判据：
//!   HANDSHAKE_PASS —— 三次握手后两端都 ESTABLISHED，且 seq/ack 对得上。
//!   DATA_PASS      —— 传一段数据，seq/ack 正确推进（发 N，ack=seq+N）。
//!   TEARDOWN_PASS  —— 四次挥手后两端都回到 CLOSED。
//!   STATE_PASS     —— 非法转移被拒、状态序正确（CLOSED 收数据该拒，等等）。
//!
//! 你只需填三处 // TODO：① 服务器收 SYN 回 SYN+ACK；② 客户端收 SYN+ACK 回 ACK；
//! ③ 收到数据段时 seq/ack 的推进（rcv_nxt += len，回一个纯 ACK）。
//! 其余（被动/主动关闭、挥手各态、mock 通道、harness）均已给定，勿改。
#![allow(dead_code)]

// ════════════════════════════════════════════════════════════════
// 报文标志位（可叠加）：一个报文可同时带 SYN 与 ACK。
// ════════════════════════════════════════════════════════════════
const SYN: u8 = 1;
const ACK: u8 = 2;
const FIN: u8 = 4;

fn has(flags: u8, bit: u8) -> bool {
    flags & bit != 0
}

/// 把 flags 渲染成人类可读串，方便诊断打印。
fn flag_str(f: u8) -> String {
    let mut v: Vec<&str> = Vec::new();
    if has(f, SYN) { v.push("SYN"); }
    if has(f, ACK) { v.push("ACK"); }
    if has(f, FIN) { v.push("FIN"); }
    if v.is_empty() { "-".to_string() } else { v.join("|") }
}

// ════════════════════════════════════════════════════════════════
// 报文：经 mock 通道在两端之间传递的「一个 TCP 段」。
//   present=false 表示「这一步没有报文要发」（如 SYN_RCVD 收到末尾 ACK 后无需回复）。
// ════════════════════════════════════════════════════════════════
#[derive(Clone, Copy, Default)]
struct Seg {
    present: bool,
    flags: u8,
    seq: u32, // 本段第一个字节的序号
    ack: u32, // 期望对端下一个字节的序号（= 我已收到的连续序号）
    len: u32, // 载荷字节数（SYN/FIN 各占 1 个「虚拟字节」，纯 ACK 占 0）
}

impl Seg {
    fn none() -> Seg {
        Seg::default()
    }
    fn new(flags: u8, seq: u32, ack: u32, len: u32) -> Seg {
        Seg { present: true, flags, seq, ack, len }
    }
}

// ════════════════════════════════════════════════════════════════
// TCP 状态（RFC 793 子集，够走完一条连接的生老病死）。
// ════════════════════════════════════════════════════════════════
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum State {
    Closed,
    Listen,
    SynSent,
    SynRcvd,
    Established,
    FinWait1,
    FinWait2,
    TimeWait,
    CloseWait,
    LastAck,
}

// ════════════════════════════════════════════════════════════════
// 一个端点（一台 TCP 状态机）。
//   snd_nxt：我要发的下一个序号；rcv_nxt：我期望收到的下一个序号（= 我回的 ack）。
//   SYN 与 FIN 各消耗 1 个序号；纯 ACK 不消耗。
// ════════════════════════════════════════════════════════════════
struct Tcp {
    name: &'static str,
    state: State,
    iss: u32,     // 初始发送序号（本课固定，方便对拍）
    snd_nxt: u32, // 下一个要发的序号
    rcv_nxt: u32, // 期望对端下一个序号
    last_accepted: bool, // 上一次 recv 是否被接受（非法转移会置 false）
    rejected: u32,       // 累计拒绝的非法报文数
}

impl Tcp {
    fn new(name: &'static str, iss: u32) -> Tcp {
        Tcp { name, state: State::Closed, iss, snd_nxt: iss, rcv_nxt: 0, last_accepted: false, rejected: 0 }
    }

    /// 服务器被动打开：进入 LISTEN，等客户端来敲门。
    fn listen(&mut self) {
        self.state = State::Listen;
    }

    /// 客户端主动打开：CLOSED → SYN_SENT，发一个 SYN（消耗 1 个序号）。
    fn active_open(&mut self) -> Seg {
        let s = Seg::new(SYN, self.iss, 0, 0);
        self.snd_nxt = self.iss + 1; // SYN 占 1 个序号
        self.state = State::SynSent;
        s
    }

    /// ESTABLISHED 时发 n 字节数据：seq=snd_nxt, ack=rcv_nxt（捎带确认），snd_nxt += n。
    fn send_data(&mut self, n: u32) -> Seg {
        if self.state != State::Established {
            return Seg::none();
        }
        let s = Seg::new(ACK, self.snd_nxt, self.rcv_nxt, n);
        self.snd_nxt += n;
        s
    }

    /// 主动关闭：发 FIN（消耗 1 个序号）。ESTABLISHED→FIN_WAIT_1；CLOSE_WAIT→LAST_ACK。
    fn active_close(&mut self) -> Seg {
        match self.state {
            State::Established => {
                let s = Seg::new(FIN, self.snd_nxt, self.rcv_nxt, 0);
                self.snd_nxt += 1;
                self.state = State::FinWait1;
                s
            }
            State::CloseWait => {
                let s = Seg::new(FIN, self.snd_nxt, self.rcv_nxt, 0);
                self.snd_nxt += 1;
                self.state = State::LastAck;
                s
            }
            _ => Seg::none(),
        }
    }

    /// TIME_WAIT 的 2MSL 定时器到期：→ CLOSED（本课直接触发，不真等时间）。
    fn tick(&mut self) {
        if self.state == State::TimeWait {
            self.state = State::Closed;
        }
    }

    /// 状态机核心：收到一个报文，更新状态/序号，返回要回发的报文（present=false 表示无需回复）。
    fn recv(&mut self, s: Seg) -> Seg {
        self.last_accepted = true; // 默认接受；落到末尾 catch-all 才置 false
        match self.state {
            // —— 服务器被动打开：LISTEN 收到纯 SYN ——
            State::Listen if has(s.flags, SYN) && !has(s.flags, ACK) => {
                // ───────────────── TODO ① 收 SYN 发 SYN+ACK ─────────────────
                // 1) 记下对端 SYN 的序号：rcv_nxt = s.seq + 1（SYN 占 1 个序号）。
                // 2) 回一个 SYN|ACK：seq = self.iss, ack = self.rcv_nxt, len = 0。
                // 3) 自己的 snd_nxt = self.iss + 1（我的 SYN 也占 1 个序号）。
                // 4) 状态 → SynRcvd，返回那个 SYN|ACK 段。
                self.rcv_nxt = s.seq + 1;
                let out = Seg::new(SYN | ACK, self.iss, self.rcv_nxt, 0);
                self.snd_nxt = self.iss + 1;
                self.state = State::SynRcvd;
                out
                // ────────────────────────────────────────────────────────────
            }
            // —— 客户端：SYN_SENT 收到 SYN+ACK ——
            State::SynSent if has(s.flags, SYN) && has(s.flags, ACK) => {
                // ───────────────── TODO ② 收 SYN+ACK 发 ACK ─────────────────
                // 1) rcv_nxt = s.seq + 1（确认对端的 SYN）。
                // 2) 回一个纯 ACK：seq = self.snd_nxt, ack = self.rcv_nxt, len = 0。
                // 3) 状态 → Established，返回那个 ACK 段。
                self.rcv_nxt = s.seq + 1;
                let out = Seg::new(ACK, self.snd_nxt, self.rcv_nxt, 0);
                self.state = State::Established;
                out
                // ────────────────────────────────────────────────────────────
            }
            // —— 服务器：SYN_RCVD 收到握手第三步的 ACK → ESTABLISHED（无需回复）——
            State::SynRcvd if has(s.flags, ACK) && !has(s.flags, SYN) => {
                self.state = State::Established;
                Seg::none()
            }
            // —— ESTABLISHED 收到数据段（len>0）——
            State::Established if s.len > 0 => {
                // ───────────────── TODO ③ 数据 seq/ack 推进 ─────────────────
                // 收到 [s.seq, s.seq+s.len) 这段字节：rcv_nxt = s.seq + s.len，
                // 回一个纯 ACK 告诉对端「我连续收到了多少」：
                //   seq = self.snd_nxt, ack = self.rcv_nxt, len = 0。
                self.rcv_nxt = s.seq + s.len;
                Seg::new(ACK, self.snd_nxt, self.rcv_nxt, 0)
                // ────────────────────────────────────────────────────────────
            }
            // —— ESTABLISHED 收到纯 ACK（对方确认了我们的数据）→ 无需回复 ——
            State::Established if has(s.flags, ACK) && s.len == 0 && !has(s.flags, FIN) => {
                Seg::none()
            }
            // —— ESTABLISHED 收到 FIN（被动关闭）→ 回 ACK，进 CLOSE_WAIT ——
            State::Established if has(s.flags, FIN) => {
                self.rcv_nxt = s.seq + 1; // FIN 占 1 个序号
                let out = Seg::new(ACK, self.snd_nxt, self.rcv_nxt, 0);
                self.state = State::CloseWait;
                out
            }
            // —— FIN_WAIT_1 收到对我们 FIN 的 ACK → FIN_WAIT_2 ——
            State::FinWait1 if has(s.flags, ACK) && !has(s.flags, FIN) => {
                self.state = State::FinWait2;
                Seg::none()
            }
            // —— FIN_WAIT_2 收到对端 FIN → 回 ACK，进 TIME_WAIT ——
            State::FinWait2 if has(s.flags, FIN) => {
                self.rcv_nxt = s.seq + 1;
                let out = Seg::new(ACK, self.snd_nxt, self.rcv_nxt, 0);
                self.state = State::TimeWait;
                out
            }
            // —— LAST_ACK 收到对我们 FIN 的 ACK → CLOSED ——
            State::LastAck if has(s.flags, ACK) => {
                self.state = State::Closed;
                Seg::none()
            }
            // —— 其它一律非法：拒绝（不改状态，记一笔）——
            _ => {
                self.last_accepted = false;
                self.rejected += 1;
                Seg::none()
            }
        }
    }
}

// 本课固定的初始序号，方便逐字段对拍（真实 TCP 用随机 ISN 防旧连接/盲攻击）。
const ISS_A: u32 = 1000; // 客户端
const ISS_B: u32 = 5000; // 服务器
const DATA_LEN: u32 = 10;

// ════════════════════════════════════════════════════════════════
// mock 通道驱动：把一端 recv 返回的报文喂给另一端。下面几个 do_* 复用之。
// ════════════════════════════════════════════════════════════════

/// 走完三次握手。成功返回 true，并让 a、b 都处于 ESTABLISHED。
fn do_handshake(a: &mut Tcp, b: &mut Tcp) -> bool {
    b.listen();
    let syn = a.active_open(); // A: CLOSED→SYN_SENT，发 SYN
    if a.state != State::SynSent {
        println!("HANDSHAKE_BAD A active_open 后应在 SYN_SENT，实为 {:?}", a.state);
        return false;
    }
    let synack = b.recv(syn); // B: LISTEN→SYN_RCVD，回 SYN|ACK
    if !synack.present {
        println!("HANDSHAKE_MISS B 收到 SYN 没回 SYN+ACK（TODO ① 没填？）");
        return false;
    }
    if synack.flags != (SYN | ACK) || synack.seq != ISS_B || synack.ack != ISS_A + 1 {
        println!(
            "HANDSHAKE_BAD B 的 SYN+ACK 字段错: flags={} seq={} ack={} 应=(SYN|ACK,{},{})",
            flag_str(synack.flags), synack.seq, synack.ack, ISS_B, ISS_A + 1
        );
        return false;
    }
    let ack = a.recv(synack); // A: SYN_SENT→ESTABLISHED，回 ACK
    if !ack.present {
        println!("HANDSHAKE_MISS A 收到 SYN+ACK 没回 ACK（TODO ② 没填？）");
        return false;
    }
    if ack.flags != ACK || ack.seq != ISS_A + 1 || ack.ack != ISS_B + 1 {
        println!(
            "HANDSHAKE_BAD A 的 ACK 字段错: flags={} seq={} ack={} 应=(ACK,{},{})",
            flag_str(ack.flags), ack.seq, ack.ack, ISS_A + 1, ISS_B + 1
        );
        return false;
    }
    let _ = b.recv(ack); // B: SYN_RCVD→ESTABLISHED
    true
}

/// 子题 1：三次握手。
fn check_handshake() -> bool {
    let mut a = Tcp::new("A", ISS_A);
    let mut b = Tcp::new("B", ISS_B);
    if !do_handshake(&mut a, &mut b) {
        return false;
    }
    let mut ok = true;
    if a.state != State::Established {
        println!("HANDSHAKE_BAD 握手后 A 应 ESTABLISHED，实为 {:?}", a.state);
        ok = false;
    }
    if b.state != State::Established {
        println!("HANDSHAKE_BAD 握手后 B 应 ESTABLISHED，实为 {:?}", b.state);
        ok = false;
    }
    // 握手后序号：A 发过 SYN(占 1)，B 发过 SYN(占 1)。
    if a.snd_nxt != ISS_A + 1 || a.rcv_nxt != ISS_B + 1 {
        println!("HANDSHAKE_BAD A 序号错: snd_nxt={} rcv_nxt={} 应=({},{})", a.snd_nxt, a.rcv_nxt, ISS_A + 1, ISS_B + 1);
        ok = false;
    }
    if b.snd_nxt != ISS_B + 1 || b.rcv_nxt != ISS_A + 1 {
        println!("HANDSHAKE_BAD B 序号错: snd_nxt={} rcv_nxt={} 应=({},{})", b.snd_nxt, b.rcv_nxt, ISS_B + 1, ISS_A + 1);
        ok = false;
    }
    if ok {
        println!("HANDSHAKE_PASS 三次握手完成，两端 ESTABLISHED（A.seq={} B.seq={}）", a.snd_nxt, b.snd_nxt);
    }
    ok
}

/// 子题 2：数据传输的 seq/ack 推进。
fn check_data() -> bool {
    let mut a = Tcp::new("A", ISS_A);
    let mut b = Tcp::new("B", ISS_B);
    if !do_handshake(&mut a, &mut b) {
        println!("DATA_MISS 握手没成，数据传输无从谈起（先过 HANDSHAKE）");
        return false;
    }
    let mut ok = true;

    // A 发 DATA_LEN 字节给 B。
    let dat = a.send_data(DATA_LEN);
    if !dat.present {
        println!("DATA_MISS A 没能发出数据段（不在 ESTABLISHED？）");
        return false;
    }
    if dat.seq != ISS_A + 1 || dat.len != DATA_LEN || dat.ack != ISS_B + 1 {
        println!("DATA_BAD A 数据段字段错: seq={} ack={} len={} 应=({},{},{})", dat.seq, dat.ack, dat.len, ISS_A + 1, ISS_B + 1, DATA_LEN);
        ok = false;
    }
    if a.snd_nxt != ISS_A + 1 + DATA_LEN {
        println!("DATA_BAD A 发完 {} 字节后 snd_nxt={} 应={}", DATA_LEN, a.snd_nxt, ISS_A + 1 + DATA_LEN);
        ok = false;
    }

    // B 收到数据，应把 rcv_nxt 推进 DATA_LEN，并回一个 ack = seq + len 的纯 ACK。
    let dack = b.recv(dat);
    if !dack.present {
        println!("DATA_MISS B 收到数据没回 ACK（TODO ③ 没填？）");
        return false;
    }
    if dack.flags != ACK || dack.ack != ISS_A + 1 + DATA_LEN {
        println!("DATA_BAD B 的 ACK 错: flags={} ack={} 应 ack={}（=seq+len）", flag_str(dack.flags), dack.ack, ISS_A + 1 + DATA_LEN);
        ok = false;
    }
    if b.rcv_nxt != ISS_A + 1 + DATA_LEN {
        println!("DATA_BAD B 收完后 rcv_nxt={} 应={}（=seq+len）", b.rcv_nxt, ISS_A + 1 + DATA_LEN);
        ok = false;
    }

    // A 收到这个纯 ACK 不必回复，且仍在 ESTABLISHED。
    let r = a.recv(dack);
    if r.present {
        println!("DATA_BAD A 收到纯 ACK 不该再回报文（会无限互 ACK）");
        ok = false;
    }
    if a.state != State::Established {
        println!("DATA_BAD A 传完数据后应仍 ESTABLISHED，实为 {:?}", a.state);
        ok = false;
    }

    if ok {
        println!("DATA_PASS 传 {} 字节：A.snd_nxt {}→{}，B.ack={}（seq+len 推进正确）", DATA_LEN, ISS_A + 1, a.snd_nxt, dack.ack);
    }
    ok
}

/// 子题 3：四次挥手回到 CLOSED。
fn check_teardown() -> bool {
    let mut a = Tcp::new("A", ISS_A);
    let mut b = Tcp::new("B", ISS_B);
    if !do_handshake(&mut a, &mut b) {
        println!("TEARDOWN_MISS 握手没成，无连接可拆（先过 HANDSHAKE）");
        return false;
    }
    let mut ok = true;

    // ① A 主动关闭：发 FIN，进 FIN_WAIT_1。
    let fin1 = a.active_close();
    if !fin1.present || a.state != State::FinWait1 {
        println!("TEARDOWN_BAD A active_close 应发 FIN 并进 FIN_WAIT_1，实为 {:?}", a.state);
        return false;
    }
    // ② B 收 FIN：回 ACK，进 CLOSE_WAIT。
    let ack1 = b.recv(fin1);
    if !ack1.present || b.state != State::CloseWait {
        println!("TEARDOWN_BAD B 收 FIN 应回 ACK 并进 CLOSE_WAIT，实为 {:?}", b.state);
        return false;
    }
    // ③ A 收到对 FIN 的 ACK：进 FIN_WAIT_2。
    let _ = a.recv(ack1);
    if a.state != State::FinWait2 {
        println!("TEARDOWN_BAD A 收到 FIN 的 ACK 应进 FIN_WAIT_2，实为 {:?}", a.state);
        ok = false;
    }
    // ④ B 也关闭：发 FIN，进 LAST_ACK。
    let fin2 = b.active_close();
    if !fin2.present || b.state != State::LastAck {
        println!("TEARDOWN_BAD B active_close 应发 FIN 并进 LAST_ACK，实为 {:?}", b.state);
        return false;
    }
    // ⑤ A 收 B 的 FIN：回 ACK，进 TIME_WAIT。
    let ack2 = a.recv(fin2);
    if !ack2.present || a.state != State::TimeWait {
        println!("TEARDOWN_BAD A 收 B 的 FIN 应回 ACK 并进 TIME_WAIT，实为 {:?}", a.state);
        return false;
    }
    // ⑥ B 收到末尾 ACK：进 CLOSED。
    let _ = b.recv(ack2);
    // ⑦ A 的 TIME_WAIT 定时器到期：进 CLOSED。
    a.tick();

    if a.state != State::Closed {
        println!("TEARDOWN_BAD 挥手后 A 应 CLOSED，实为 {:?}", a.state);
        ok = false;
    }
    if b.state != State::Closed {
        println!("TEARDOWN_BAD 挥手后 B 应 CLOSED，实为 {:?}", b.state);
        ok = false;
    }
    // 挥手各消耗 1 个 FIN 序号。
    if a.snd_nxt != ISS_A + 1 + 1 || b.snd_nxt != ISS_B + 1 + 1 {
        println!("TEARDOWN_BAD FIN 序号错: A.snd_nxt={} B.snd_nxt={} 应=({},{})", a.snd_nxt, b.snd_nxt, ISS_A + 2, ISS_B + 2);
        ok = false;
    }

    if ok {
        println!("TEARDOWN_PASS 四次挥手完成，两端回到 CLOSED");
    }
    ok
}

/// 子题 4：非法转移被拒、状态序正确。
fn check_state() -> bool {
    let mut ok = true;

    // (a) CLOSED 端点收到数据/ACK 报文 → 拒绝，状态不变。
    let mut c = Tcp::new("X", 9000);
    let r = c.recv(Seg::new(ACK, 1, 1, 4));
    if c.last_accepted || c.state != State::Closed || r.present {
        println!("STATE_BAD CLOSED 收数据报文该拒，却被接受/改了状态（state={:?}）", c.state);
        ok = false;
    }

    // (b) CLOSED 端点收到 FIN → 拒绝。
    let mut c2 = Tcp::new("X", 9000);
    let _ = c2.recv(Seg::new(FIN, 1, 1, 0));
    if c2.last_accepted {
        println!("STATE_BAD CLOSED 收 FIN 该拒，却被接受");
        ok = false;
    }

    // (c) ESTABLISHED 端点收到一个 SYN（重复建链）→ 拒绝，状态不变。
    let mut e = Tcp::new("Y", 9000);
    e.state = State::Established;
    e.snd_nxt = 9001;
    e.rcv_nxt = 7777;
    let r2 = e.recv(Seg::new(SYN, 1, 1, 0));
    if e.last_accepted || e.state != State::Established || r2.present {
        println!("STATE_BAD ESTABLISHED 收 SYN 该拒，却被接受/改了状态（state={:?}）", e.state);
        ok = false;
    }

    // (d) LISTEN 端点收到 FIN（不是 SYN）→ 拒绝。
    let mut l = Tcp::new("Z", 9000);
    l.listen();
    let _ = l.recv(Seg::new(FIN, 1, 1, 0));
    if l.last_accepted || l.state != State::Listen {
        println!("STATE_BAD LISTEN 只接受 SYN，收 FIN 该拒（state={:?}）", l.state);
        ok = false;
    }

    // (e) 状态序正确：完整生命周期里 A 依次经过的状态，应与 RFC 793 主动方路径一致。
    let mut a = Tcp::new("A", ISS_A);
    let mut b = Tcp::new("B", ISS_B);
    let mut seq: Vec<State> = vec![a.state];
    if do_handshake(&mut a, &mut b) {
        seq.push(a.state); // Established
        let dat = a.send_data(DATA_LEN);
        let dack = b.recv(dat);
        let _ = a.recv(dack); // 仍 Established
        let fin1 = a.active_close();
        seq.push(a.state); // FinWait1
        let ack1 = b.recv(fin1);
        let _ = a.recv(ack1);
        seq.push(a.state); // FinWait2
        let fin2 = b.active_close();
        let ack2 = a.recv(fin2);
        seq.push(a.state); // TimeWait
        let _ = b.recv(ack2);
        a.tick();
        seq.push(a.state); // Closed
        let want = vec![
            State::Closed,
            State::Established,
            State::FinWait1,
            State::FinWait2,
            State::TimeWait,
            State::Closed,
        ];
        if seq != want {
            println!("STATE_BAD A 状态序 {:?} 应={:?}", seq, want);
            ok = false;
        }
    } else {
        // 握手没成（TODO 没填）时，这一项无从校验——不在此处报错，
        // 让 HANDSHAKE_* 去定位；(a)~(d) 仍可独立通过。
        println!("STATE_BAD 状态序需先过握手（见 HANDSHAKE 诊断）");
        ok = false;
    }

    if ok {
        println!("STATE_PASS 非法转移全部被拒，主动方状态序与 RFC 793 一致");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_handshake();
    all &= check_data();
    all &= check_teardown();
    all &= check_state();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
