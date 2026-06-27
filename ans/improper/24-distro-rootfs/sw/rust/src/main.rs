//! 发行版与根文件系统：从光秃秃的内核到能进 shell 的 rootfs —— Rust 参考解。
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
//! 学生填两处：busybox 的 argv[0] 分发 + cpio 解包（解析头、重建文件）。
#![allow(dead_code)]

use std::collections::BTreeMap;

// ── 内存文件树：路径 → 节点 ─────────────────────────────────────────
// 文件系统在这里被抽象成「路径字符串 → 节点」的扁平映射。
// 节点三态：目录 / 文件(含字节内容) / 符号链接(含目标路径)。
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

// ── (a) FHS 目录树 ──────────────────────────────────────────────────
// Filesystem Hierarchy Standard：每类东西有约定的家。
//   /bin 可执行、/etc 配置、/dev 设备节点、/proc /sys 内核虚拟、/lib 共享库。
fn build_rootfs() -> Tree {
    let mut t = Tree::new();
    for d in ["/bin", "/etc", "/dev", "/proc", "/sys", "/lib"] {
        t.mkdir(d);
    }
    // busybox = 一个多合一二进制。
    t.add_file("/bin/busybox", b"<busybox multi-call binary>");
    // 各「命令」都是指向 busybox 的符号链接（这正是省空间的关键）。
    for app in ["ls", "cat", "echo", "mount"] {
        t.symlink(&format!("/bin/{}", app), "/bin/busybox");
    }
    // init 读的开机脚本。
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
// 一个二进制，按被调用的名字（argv[0] 的 basename）分发成不同 applet。
// ls/cat/echo/mount 都是指向它的符号链接，所以磁盘上只存一份代码。
fn basename(p: &str) -> &str {
    p.rsplit('/').next().unwrap_or(p)
}

/// 多合一入口：argv[0] 是被调用的名字，argv[1..] 是参数，返回该命令的输出。
fn busybox_main(argv: &[&str]) -> String {
    let cmd = basename(argv[0]);
    match cmd {
        "echo" => argv[1..].join(" "),
        "ls" => {
            let mut v: Vec<&str> = argv[1..].to_vec();
            v.sort();
            v.join("\n")
        }
        "cat" => argv[1..].join("\n"),
        "mount" => format!("mounted {}", argv[1..].join(" ")),
        other => format!("busybox: applet not found: {}", other),
    }
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

// ── (c) mock init（PID 1）────────────────────────────────────────────
// 内核启动后第一个用户进程就是 init（PID 1）。它读 /etc/inittab，
// 按动作把 /proc /sys 等虚拟文件系统挂上，最后 spawn 一个 shell。
fn parse_inittab(content: &str) -> Vec<(String, String)> {
    // 每行形如 ::action:command
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
                // 「挂载」= 在文件树里让虚拟文件系统的条目出现。
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
// 把整棵文件树压成一段字节归档；内核启动时把它解开成根文件系统再跑 /init。
// 记录布局（自定义、易解析）：
//   magic[4]="0707" | type(1: 'd'/'f'/'l') | namelen(LE32) | bodylen(LE32) | name | body
// 末尾一条 name=="TRAILER!!!" 的哨兵记录表示归档结束（致敬真实 cpio）。
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
    loop {
        if off + CPIO_HDR > arc.len() {
            return Err("归档在头部处被截断".into());
        }
        if &arc[off..off + 4] != CPIO_MAGIC {
            return Err(format!("magic 不匹配 @ {}", off));
        }
        let typ = arc[off + 4];
        let namelen =
            u32::from_le_bytes([arc[off + 5], arc[off + 6], arc[off + 7], arc[off + 8]]) as usize;
        let bodylen =
            u32::from_le_bytes([arc[off + 9], arc[off + 10], arc[off + 11], arc[off + 12]]) as usize;
        let nstart = off + CPIO_HDR;
        let nend = nstart + namelen;
        let bend = nend + bodylen;
        if bend > arc.len() {
            return Err("归档在内容处被截断".into());
        }
        let name = String::from_utf8_lossy(&arc[nstart..nend]).into_owned();
        if name == CPIO_TRAILER {
            break;
        }
        let body = &arc[nend..bend];
        match typ {
            b'd' => t.mkdir(&name),
            b'f' => t.add_file(&name, body),
            b'l' => t.symlink(&name, &String::from_utf8_lossy(body)),
            other => return Err(format!("未知节点类型 {}", other)),
        }
        off = bend;
    }
    Ok(t)
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
