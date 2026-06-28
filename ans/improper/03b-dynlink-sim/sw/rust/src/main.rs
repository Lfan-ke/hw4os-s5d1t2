//! 动态链接加载器（软件建模）—— Rust 参考解。
//!
//! 母题（先想清楚 做什么/有什么用/为什么）：
//!   100 个程序都要用 `puts`。**静态链接** = 每个程序各塞一份库的拷贝——单文件好部署，
//!   但 100 份 = 100 倍空间，改一个 bug 要逐个重编。**动态链接** = 程序只存「需要谁」(名字)
//!   + 一张空 GOT，库**只留一份**共享；由 `ld.so` 按【符号名 key】到唯一共享库查表、
//!   把「基址+偏移」**回填**进每个程序**私有的 GOT**。于是空间小、换一个 `.so` 全体修好。
//!
//! 逐题从「意境」走向「接近真实 ld.so」（走完即等价真实重定位，接力棒交给正经实验 S09b-linking）：
//!   E1 静态各塞一份(数空间) → E2 动态:名字引用+空GOT → E3 ld.so 按 key 解析回填 GOT
//!   → E4 一份共享+私有GOT 的空间对比 → E5 lazy vs now 绑定 → E6 dlopen + 缺符号失败
//!
//! 学生只填 `// TODO` 的函数体；`═ harness ═` 以下勿改。
#![allow(dead_code)] // own 等字段是示意模型的一部分，未被 harness 读取属正常

use std::collections::HashMap;

const LIB_CODE: u64 = 1000; // 一份库的代码大小（字节，建模用）
const OWN_CODE: u64 = 200; // 一个程序自身代码大小
const GOT_SLOT: u64 = 8; // 一个 GOT 槽 = 一个指针 = 8 字节

/// 共享库：一份，按符号名存「库内相对偏移」。加载到 `base` 后，符号真实地址 = base + 偏移。
/// 对应真实 `.so` 的 `.dynsym`（符号名→库内偏移）。
#[derive(Clone)]
struct Library {
    name: String,
    base: u64,                   // 本库被加载到的基址（真实世界里每库一个，受 ASLR 影响）
    symbols: Vec<(String, u64)>, // (符号名, 库内偏移)
}

impl Library {
    /// E3①：按【符号名 key】查表 → 命中返回真实地址 `base + 偏移`，未命中 `None`。
    /// 这就是「匹配 key 就能全局找到那一份公共实现」的核心（对应 ld.so 的 `find_sym`）。
    fn lookup(&self, sym: &str) -> Option<u64> {
        // TODO: 遍历 self.symbols，找 name==sym，命中返回 Some(self.base + 偏移)，否则 None。
        for (name, off) in &self.symbols {
            if name == sym {
                return Some(self.base + off);
            }
        }
        None
    }
}

/// 一个待解析的外部符号槽（GOT entry）。`resolved=None` = 还没回填真实地址。
struct GotSlot {
    sym: String,
    resolved: Option<u64>,
}

/// 静态链接的程序：把整份库**拷进自己肚子里**（各塞一份）。
struct StaticProgram {
    own: u64,
    embedded: Library, // 自己私有的一份库拷贝
}

/// 动态链接的程序：不塞库，只记「需要哪些库」(DT_NEEDED) + 一张 GOT（每个外部符号一个空槽）。
struct DynProgram {
    own: u64,
    needed: Vec<String>,
    got: Vec<GotSlot>,
}

/// 运行时已加载的共享库注册表（建模进程地址空间里「装了哪些 .so」）。
type Registry = HashMap<String, Library>;

// ═════════════════════ 学生填空区（六段，意境→现实）═════════════════════

/// E1：静态链接——把一份库的拷贝塞进程序。返回 `n` 个静态程序的**总空间**。
/// 体会：各塞一份 → n 份库拷贝，空间随 n 线性膨胀。
fn static_total_space(n: u64) -> u64 {
    // TODO: 每个静态程序 = 自身 OWN_CODE + 一整份 LIB_CODE 拷贝；n 个即 n*(OWN_CODE+LIB_CODE)。
    n * (OWN_CODE + LIB_CODE)
}

/// E1：造一个静态程序——把库 `clone` 一份塞进去（自包含，不依赖外部）。
fn make_static(lib: &Library) -> StaticProgram {
    // TODO: own=OWN_CODE；embedded = lib.clone()（各塞一份拷贝）。
    StaticProgram {
        own: OWN_CODE,
        embedded: lib.clone(),
    }
}

/// E2：造一个动态程序——只登记 needed + 给每个外部符号建一个【空】GOT 槽（resolved=None）。
fn make_dyn(needed_lib: &str, syms: &[&str]) -> DynProgram {
    // TODO: own=OWN_CODE；needed=[needed_lib]；对每个 sym 建 GotSlot{sym, resolved:None}。
    DynProgram {
        own: OWN_CODE,
        needed: vec![needed_lib.to_string()],
        got: syms
            .iter()
            .map(|s| GotSlot {
                sym: s.to_string(),
                resolved: None,
            })
            .collect(),
    }
}

/// E3②：ld.so 重定位——遍历 prog 的每个 GOT 空槽，拿符号名去**唯一共享库**按 key 查、回填真实地址。
/// （对应 NOW binding：加载时一次性把所有 JUMP_SLOT 填好。）
fn resolve(prog: &mut DynProgram, lib: &Library) {
    // TODO: 对 prog.got 每个槽：slot.resolved = lib.lookup(&slot.sym)。
    for slot in prog.got.iter_mut() {
        slot.resolved = lib.lookup(&slot.sym);
    }
}

/// E4：n 个动态程序【共享一份】库 + 各自一张小 GOT；返回**总空间**。
/// 体会：库只算一份，n 个程序只多 n 张小 GOT → 空间远小于静态。
fn dyn_total_space(n: u64, n_syms: u64) -> u64 {
    // TODO: 一份共享 LIB_CODE + n*(OWN_CODE + n_syms*GOT_SLOT)。
    LIB_CODE + n * (OWN_CODE + n_syms * GOT_SLOT)
}

/// E5：NOW 绑定——加载时一次性解析**所有** GOT 槽，返回解析次数（= 槽数）。
fn now_bind(prog: &mut DynProgram, lib: &Library) -> u64 {
    // TODO: 解析全部槽，返回解析的槽数。
    let mut cnt = 0;
    for slot in prog.got.iter_mut() {
        slot.resolved = lib.lookup(&slot.sym);
        cnt += 1;
    }
    cnt
}

/// E5：LAZY 绑定——只在**被调用**时才解析对应符号；返回本轮触发的解析次数。
/// 只有 `used` 里真正调用到的符号才会被解析（首次调用触发回填）。
fn lazy_bind(prog: &mut DynProgram, lib: &Library, used: &[&str]) -> u64 {
    // TODO: 对 used 里每个符号，找到其槽；若还没解析(None)→lib.lookup 回填、计数+1。
    let mut cnt = 0;
    for u in used {
        if let Some(slot) = prog.got.iter_mut().find(|s| s.sym == *u) {
            if slot.resolved.is_none() {
                slot.resolved = lib.lookup(u);
                cnt += 1;
            }
        }
    }
    cnt
}

/// E6：dlopen——运行时把一份库装进注册表（运行中按需加载，不是启动时 DT_NEEDED）。
fn dlopen(reg: &mut Registry, lib: Library) {
    // TODO: reg.insert(lib.name.clone(), lib)。
    reg.insert(lib.name.clone(), lib);
}

/// E6：dlsym——按 库名+符号名 查真实地址；**缺库或缺符号 → Err（绝不 panic）**。
/// 对应「没有这个符号，调用该行为就会失败」。
fn dlsym(reg: &Registry, libname: &str, sym: &str) -> Result<u64, String> {
    // TODO: 取 reg[libname]；命中再 lib.lookup(sym)；任一缺失返回 Err("...")。
    match reg.get(libname) {
        Some(lib) => lib
            .lookup(sym)
            .ok_or_else(|| format!("undefined symbol: {sym}")),
        None => Err(format!("cannot open shared object: {libname}")),
    }
}

// ═════════════════════════════ harness（勿改）═════════════════════════════

/// 造一份示例库 libc：导出 puts/printf/malloc 三个符号，加载在 0x4000_0000。
fn make_libc() -> Library {
    Library {
        name: "libc.so".into(),
        base: 0x4000_0000,
        symbols: vec![
            ("puts".into(), 0x100),
            ("printf".into(), 0x240),
            ("malloc".into(), 0x380),
        ],
    }
}

fn check_static() -> bool {
    let libc = make_libc();
    let n = 3u64;
    // 每个静态程序都得自包含：从自己那份拷贝里能查到 puts。
    let progs: Vec<StaticProgram> = (0..n).map(|_| make_static(&libc)).collect();
    let mut ok = true;
    for p in &progs {
        if p.embedded.lookup("puts") != Some(0x4000_0100) {
            println!("STATIC_FAIL 静态程序的内嵌库查不到 puts（应自包含）");
            ok = false;
        }
    }
    let space = static_total_space(n);
    let want = n * (OWN_CODE + LIB_CODE);
    if space != want {
        println!("STATIC_FAIL 总空间={space} 应={want}（各塞一份→n*(own+lib)）");
        ok = false;
    }
    if ok {
        println!("STATIC: n={n} 各塞一份库 → 总空间={space} 字节");
        println!("STATIC_PASS");
    }
    ok
}

fn check_dynsym() -> bool {
    let p = make_dyn("libc.so", &["puts", "printf"]);
    let mut ok = true;
    if p.needed != vec!["libc.so".to_string()] {
        println!("DYNSYM_FAIL needed={:?} 应=[\"libc.so\"]", p.needed);
        ok = false;
    }
    if p.got.len() != 2 {
        println!("DYNSYM_FAIL GOT 槽数={} 应=2", p.got.len());
        ok = false;
    }
    if p.got.iter().any(|s| s.resolved.is_some()) {
        println!("DYNSYM_FAIL 刚建的 GOT 槽必须全未解析(None)");
        ok = false;
    }
    if p.got.first().map(|s| s.sym.as_str()) != Some("puts") {
        println!("DYNSYM_FAIL GOT[0] 符号名应=puts");
        ok = false;
    }
    if ok {
        println!("DYNSYM: 动态程序只存 needed=[libc.so] + 2 个空 GOT 槽（未塞库）");
        println!("DYNSYM_PASS");
    }
    ok
}

fn check_resolve() -> bool {
    let libc = make_libc();
    let mut p = make_dyn("libc.so", &["puts", "printf", "malloc"]);
    resolve(&mut p, &libc);
    let want = [
        ("puts", 0x4000_0100u64),
        ("printf", 0x4000_0240),
        ("malloc", 0x4000_0380),
    ];
    let mut ok = true;
    for (sym, addr) in want {
        match p.got.iter().find(|s| s.sym == sym).and_then(|s| s.resolved) {
            Some(a) if a == addr => {}
            other => {
                println!("RESOLVE_FAIL {sym} 解析={other:?} 应=Some({addr:#x})");
                ok = false;
            }
        }
    }
    if ok {
        println!("RESOLVE: ld.so 按 key 把 puts/printf/malloc 回填进私有 GOT（base+偏移）");
        println!("RESOLVE_PASS");
    }
    ok
}

fn check_share() -> bool {
    let n = 100u64;
    let n_syms = 3u64;
    let stat = static_total_space(n);
    let dynamic = dyn_total_space(n, n_syms);
    let mut ok = true;
    let want_dyn = LIB_CODE + n * (OWN_CODE + n_syms * GOT_SLOT);
    if dynamic != want_dyn {
        println!("SHARE_FAIL 动态总空间={dynamic} 应={want_dyn}（一份共享库+各自小 GOT）");
        ok = false;
    }
    if !(dynamic < stat) {
        println!("SHARE_FAIL 动态({dynamic}) 应远小于 静态({stat})");
        ok = false;
    }
    if ok {
        println!(
            "SHARE: n={n} → 静态={stat}B（各塞一份） vs 动态={dynamic}B（一份共享+各自GOT），省 {}B",
            stat - dynamic
        );
        println!("SHARE_PASS");
    }
    ok
}

fn check_bind() -> bool {
    let libc = make_libc();
    // 程序声明依赖 3 个符号，但实际只调用其中 1 个（puts）。
    let mut p_now = make_dyn("libc.so", &["puts", "printf", "malloc"]);
    let mut p_lazy = make_dyn("libc.so", &["puts", "printf", "malloc"]);
    let now_cnt = now_bind(&mut p_now, &libc); // 加载即全解析
    let lazy_cnt = lazy_bind(&mut p_lazy, &libc, &["puts"]); // 只解析被调用的 puts
    let mut ok = true;
    if now_cnt != 3 {
        println!("BIND_FAIL now 绑定解析次数={now_cnt} 应=3（全部 GOT 槽）");
        ok = false;
    }
    if lazy_cnt != 1 {
        println!("BIND_FAIL lazy 绑定解析次数={lazy_cnt} 应=1（只解析被调用的 puts）");
        ok = false;
    }
    // lazy 下未被调用的 printf/malloc 仍应是 None。
    let unused_resolved = p_lazy
        .got
        .iter()
        .filter(|s| s.sym != "puts")
        .any(|s| s.resolved.is_some());
    if unused_resolved {
        println!("BIND_FAIL lazy 下未调用的符号不应被解析");
        ok = false;
    }
    if ok {
        println!("BIND: now 解析 {now_cnt} 个；lazy 只解析 {lazy_cnt} 个（按需，省解析开销）");
        println!("BIND_PASS");
    }
    ok
}

fn check_dlopen() -> bool {
    let mut reg: Registry = Registry::new();
    dlopen(&mut reg, make_libc());
    // 运行时再插件式装入一个 libplugin（导出 greet）。
    dlopen(
        &mut reg,
        Library {
            name: "libplugin.so".into(),
            base: 0x5000_0000,
            symbols: vec![("greet".into(), 0x10)],
        },
    );
    let mut ok = true;
    match dlsym(&reg, "libplugin.so", "greet") {
        Ok(a) if a == 0x5000_0010 => {}
        other => {
            println!("DLOPEN_FAIL dlsym(greet)={other:?} 应=Ok(0x50000010)");
            ok = false;
        }
    }
    // 缺符号 → Err（不 panic）。
    match dlsym(&reg, "libplugin.so", "nonexist") {
        Err(_) => {}
        Ok(a) => {
            println!("DLOPEN_FAIL dlsym(缺符号) 应 Err，却得 Ok({a:#x})");
            ok = false;
        }
    }
    // 缺库 → Err。
    if dlsym(&reg, "libmissing.so", "x").is_ok() {
        println!("DLOPEN_FAIL dlsym(缺库) 应 Err");
        ok = false;
    }
    if ok {
        println!("DLOPEN: 运行时装入 libplugin，dlsym(greet)=Ok；缺符号/缺库=Err（不崩）");
        println!("DLOPEN_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_static();
    all &= check_dynsym();
    all &= check_resolve();
    all &= check_share();
    all &= check_bind();
    all &= check_dlopen();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
