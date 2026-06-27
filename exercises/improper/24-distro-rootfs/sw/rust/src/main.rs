//! 发行版与根文件系统：从光秃秃的内核到能进 shell 的 rootfs —— 学生填空版。
//!
//! 母题：内核只是引擎，光有它进不了 shell。要一个**根文件系统(rootfs)**——
//! 装上 init + 一堆工具——开机才有 userspace 可用。这就是「发行版」的雏形。
//! 四段逐题递进，全在一棵「内存文件树」上演练：
//!   (a) FHS 目录树：/bin /etc /dev /proc /sys /lib 摆对位置        → FHS_PASS
//!   (b) busybox 多合一二进制：同一程序按 argv[0] 分发成 ls/cat/...  → BUSYBOX_PASS
//!   (c) mock init(PID 1)：挂 /proc /sys、读 inittab、起 shell      → INIT_PASS
//!   (d) cpio 打包→解包还原整棵树（initramfs 概念）                 → INITRAMFS_PASS
//! 四段皆过再打印 ALL_PASS。
//!
//! 你要填两处：busybox 的 argv[0] 分发 + cpio 解包（解析头、重建文件）。
#![allow(dead_code)]

use std::collections::BTreeMap;

// ── 内存文件树：路径 → 节点 ─────────────────────────────────────────
#[derive(Clone, PartialEq, Eq, Debug)]
enum Node {
    Dir,
    File(Vec<u8>),
    Link(String),
}

#[derive(Clone, PartialEq, Eq)]
struct Tree {
    nodes: BTreeMap<String, Node>,
}

impl Tree {
    fn new() -> Self {
        Tree { nodes: BTreeMap::new() }
    }
    fn mkdir(&mut self, p: &str) {
        self.nodes.insert(p.to_string(), Node::Dir);
    }
    fn add_file(&mut self, p: &str, data: &[u8]) {
        self.nodes.insert(p.to_string(), Node::File(data.to_vec()));
    }
    fn symlink(&mut self, p: &str, target: &str) {
        self.nodes.insert(p.to_string(), Node::Link(target.to_string()));
    }
    fn get(&self, p: &str) -> Option<&Node> {
        self.nodes.get(p)
    }
    fn is_dir(&self, p: &str) -> bool {
        matches!(self.nodes.get(p), Some(Node::Dir))
    }
}

// ── (a) FHS 目录树（已给）──────────────────────────────────────────
fn build_rootfs() -> Tree {
    let mut t = Tree::new();
    for d in ["/bin", "/etc", "/dev", "/proc", "/sys", "/lib"] {
        t.mkdir(d);
    }
    t.add_file("/bin/busybox", b"<busybox multi-call binary>");
    for app in ["ls", "cat", "echo", "mount"] {
        t.symlink(&format!("/bin/{}", app), "/bin/busybox");
    }
    let inittab = "::sysinit:/bin/mount -t proc proc /proc\n\
                   ::sysinit:/bin/mount -t sysfs sysfs /sys\n\
                   ::askfirst:/bin/sh\n";
    t.add_file("/etc/inittab", inittab.as_bytes());
    t.add_file("/dev/null", b"");
    t.add_file("/lib/libc.so", b"<libc>");
    t
}

fn check_fhs() -> bool {
    let t = build_rootfs();
    for d in ["/bin", "/etc", "/dev", "/proc", "/sys", "/lib"] {
        if !t.is_dir(d) {
            println!("FHS_FAIL 缺目录 {}", d);
            return false;
        }
    }
    if !matches!(t.get("/bin/busybox"), Some(Node::File(_))) {
        println!("FHS_FAIL /bin/busybox 不是文件");
        return false;
    }
    for app in ["ls", "cat", "echo", "mount"] {
        let p = format!("/bin/{}", app);
        match t.get(&p) {
            Some(Node::Link(target)) if target == "/bin/busybox" => {}
            _ => {
                println!("FHS_FAIL /bin/{} 不是指向 busybox 的符号链接", app);
                return false;
            }
        }
    }
    println!("FHS_PASS");
    true
}

// ── (b) busybox 多合一二进制 ────────────────────────────────────────
fn basename(p: &str) -> &str {
    p.rsplit('/').next().unwrap_or(p)
}

/// 多合一入口：argv[0] 是被调用的名字，argv[1..] 是参数，返回该命令的输出。
fn busybox_main(argv: &[&str]) -> String {
    let cmd = basename(argv[0]);
    // TODO: 按 argv[0] 的 basename 分发成不同命令（busybox 多合一）：
    //   "echo"  -> argv[1..].join(" ")
    //   "ls"    -> argv[1..] 排序后 join("\n")
    //   "cat"   -> argv[1..].join("\n")
    //   "mount" -> format!("mounted {}", argv[1..].join(" "))
    //   其它    -> 含 "not found" 的提示，如 "busybox: applet not found: <cmd>"
    //   // TODO[a] match cmd 直接分发   // ELSE[b] 查一张 name->闭包 表
    // HINT: let mut v: Vec<&str> = argv[1..].to_vec(); v.sort(); v.join("\n")
    let _ = cmd; // ← 占位：返回空串 → BUSYBOX_FAIL
    String::new()
}

fn check_busybox() -> bool {
    // 关键：同一个 busybox_main，仅 argv[0] 不同 → 表现为不同命令。
    let cases: [(&[&str], &str); 4] = [
        (&["/bin/echo", "hello", "world"], "hello world"),
        (&["/bin/ls", "beta", "alpha", "gamma"], "alpha\nbeta\ngamma"),
        (&["/bin/cat", "line1", "line2"], "line1\nline2"),
        (&["/bin/mount", "-t", "proc", "proc", "/proc"], "mounted -t proc proc /proc"),
    ];
    for (argv, want) in cases {
        let got = busybox_main(argv);
        if got != want {
            println!("BUSYBOX_FAIL argv0={} got={:?} want={:?}", argv[0], got, want);
            return false;
        }
    }
    let nf = busybox_main(&["/bin/nope"]);
    if !nf.contains("not found") {
        println!("BUSYBOX_FAIL 未知 applet 应报 not found，实得 {:?}", nf);
        return false;
    }
    println!("BUSYBOX_PASS");
    true
}

// ── (c) mock init（PID 1）（已给）──────────────────────────────────
fn parse_inittab(content: &str) -> Vec<(String, String)> {
    let mut out = Vec::new();
    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') || !line.starts_with("::") {
            continue;
        }
        let rest = &line[2..];
        if let Some(idx) = rest.find(':') {
            out.push((rest[..idx].to_string(), rest[idx + 1..].to_string()));
        }
    }
    out
}

fn mock_init(t: &mut Tree) -> Vec<String> {
    let mut log = Vec::new();
    log.push("init: I am PID 1".to_string());
    let content = match t.get("/etc/inittab") {
        Some(Node::File(b)) => String::from_utf8_lossy(b).into_owned(),
        _ => {
            log.push("init: no /etc/inittab".to_string());
            return log;
        }
    };
    for (action, cmd) in parse_inittab(&content) {
        match action.as_str() {
            "sysinit" => {
                log.push(format!("init: sysinit {}", cmd));
                if cmd.contains("/proc") {
                    t.add_file("/proc/uptime", b"0.00 0.00");
                }
                if cmd.contains("/sys") {
                    t.add_file("/sys/kernel/ostype", b"EMMos");
                }
            }
            "askfirst" | "respawn" => {
                log.push(format!("init: spawn shell {}", cmd));
            }
            other => {
                log.push(format!("init: skip {}", other));
            }
        }
    }
    log
}

fn check_init() -> bool {
    let mut t = build_rootfs();
    let log = mock_init(&mut t);
    if log.first().map(|s| s.as_str()) != Some("init: I am PID 1") {
        println!("INIT_FAIL 第一步应宣告 PID 1");
        return false;
    }
    let joined = log.join("\n");
    if !joined.contains("sysinit") {
        println!("INIT_FAIL 未执行 sysinit 挂载");
        return false;
    }
    if !joined.contains("spawn shell") {
        println!("INIT_FAIL 未起 shell");
        return false;
    }
    if !matches!(t.get("/proc/uptime"), Some(Node::File(_))) {
        println!("INIT_FAIL /proc 未挂载");
        return false;
    }
    if !matches!(t.get("/sys/kernel/ostype"), Some(Node::File(_))) {
        println!("INIT_FAIL /sys 未挂载");
        return false;
    }
    let pos_sysinit = log.iter().position(|s| s.contains("sysinit")).unwrap();
    let pos_shell = log.iter().position(|s| s.contains("spawn shell")).unwrap();
    if pos_shell <= pos_sysinit {
        println!("INIT_FAIL shell 应在挂载之后才起");
        return false;
    }
    println!("INIT_PASS");
    true
}

// ── (d) cpio 打包 / 解包（initramfs）────────────────────────────────
// 记录布局：magic[4]="0707" | type(1) | namelen(LE32) | bodylen(LE32) | name | body
// 末尾一条 name=="TRAILER!!!" 的哨兵记录表示归档结束。
const CPIO_MAGIC: &[u8; 4] = b"0707";
const CPIO_TRAILER: &str = "TRAILER!!!";
const CPIO_HDR: usize = 13; // magic4 + type1 + namelen4 + bodylen4

fn write_entry(out: &mut Vec<u8>, typ: u8, name: &[u8], body: &[u8]) {
    out.extend_from_slice(CPIO_MAGIC);
    out.push(typ);
    out.extend_from_slice(&(name.len() as u32).to_le_bytes());
    out.extend_from_slice(&(body.len() as u32).to_le_bytes());
    out.extend_from_slice(name);
    out.extend_from_slice(body);
}

fn cpio_pack(t: &Tree) -> Vec<u8> {
    let mut out = Vec::new();
    for (path, node) in &t.nodes {
        let (typ, body): (u8, Vec<u8>) = match node {
            Node::Dir => (b'd', Vec::new()),
            Node::File(d) => (b'f', d.clone()),
            Node::Link(target) => (b'l', target.as_bytes().to_vec()),
        };
        write_entry(&mut out, typ, path.as_bytes(), &body);
    }
    write_entry(&mut out, b'd', CPIO_TRAILER.as_bytes(), &[]);
    out
}

/// 解析 cpio 归档，逐条还原成一棵文件树。遇 TRAILER 记录停止。
fn cpio_unpack(arc: &[u8]) -> Result<Tree, String> {
    let mut t = Tree::new();
    let mut off = 0usize;
    // TODO: 循环解析归档，逐条还原节点：
    //   1) 边界检查 off + CPIO_HDR <= arc.len()，否则 Err
    //   2) 校验 &arc[off..off+4] == CPIO_MAGIC，否则 Err
    //   3) typ = arc[off+4]
    //      namelen = LE32(arc[off+5..off+9])；bodylen = LE32(arc[off+9..off+13])
    //   4) name = arc[off+CPIO_HDR .. +namelen]；若 == CPIO_TRAILER 则 break
    //   5) body = 紧随其后的 bodylen 字节；按 typ 重建：
    //        b'd' -> t.mkdir、b'f' -> t.add_file、b'l' -> t.symlink(目标=body 解成字符串)
    //   6) off 前进到 body 之后，继续循环
    //   // TODO[a] 线性逐条解析（如上）   // ELSE[b] 先扫一遍记录边界再回填
    // HINT: u32::from_le_bytes([arc[off+5], arc[off+6], arc[off+7], arc[off+8]]) as usize
    let _ = (&mut t, &mut off, arc); // ← 占位
    Err("TODO: cpio 解包未实现".into()) // ← 返回 Err → INITRAMFS_FAIL
}

fn check_initramfs() -> bool {
    let mut t = build_rootfs();
    let _ = mock_init(&mut t); // 让 /proc /sys 虚拟条目也进树，归档更完整
    let arc = cpio_pack(&t);
    let restored = match cpio_unpack(&arc) {
        Ok(x) => x,
        Err(e) => {
            println!("INITRAMFS_FAIL 解包出错: {}", e);
            return false;
        }
    };
    if restored != t {
        println!(
            "INITRAMFS_FAIL 解包还原与原树不一致 (orig={} 项, got={} 项)",
            t.nodes.len(),
            restored.nodes.len()
        );
        return false;
    }
    println!("INITRAMFS_PASS");
    true
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────
fn main() {
    let mut all = true;
    all &= check_fhs();
    all &= check_busybox();
    all &= check_init();
    all &= check_initramfs();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
