//! 编译链接（软件建模）—— Rust。
//! 母题：编译器只产出"带名字的碎片(section)"，是手写链接脚本决定
//!   「哪段落在哪个地址、谁先谁后、要不要塞进同一颗镜像」。
//! 本课用纯软件数组/结构体把链接器的工作建模出来，逐题递进：
//!   E1 段布局与边界符号  →  E2 自定义段 .config 落到对齐地址
//!   E3 ELF 头解析 vs 纯二进制入口偏移  →  E4 A→B→C 串接执行（app 表）
//! 学生只填带 // TODO 的函数体；下方测试 harness（向量+不变量+PASS 打印）勿改。
#![allow(unused_variables, dead_code)]

// ───────────────────────── 公共常量/模型 ─────────────────────────

/// 裸机加载基址（对应 rcore linker.ld 的 BASE_ADDRESS = 0x80200000）。
const BASE: u64 = 0x8020_0000;
/// 段顺序：0=.text 1=.rodata 2=.data 3=.bss（与真实 linker.ld 同序）。
const N_SEC: usize = 4;
/// .config 段魔数与字段（学生需把它们正确"编进"段里）。
const CONFIG_MAGIC: u32 = 0x00C0_FFEE;
const PAGE: u64 = 0x1000; // 4K 对齐

#[derive(Clone, Copy, PartialEq, Debug)]
struct Config {
    magic: u32,
    version: u32,
    nslots: u32,
    flags: u32,
}

/// 向上对齐到 a（a 为 2 的幂）。链接脚本里的 `. = ALIGN(a);`。
fn align_up(x: u64, a: u64) -> u64 {
    (x + a - 1) & !(a - 1)
}

// ═════════════════════ 学生填空区（四段核心逻辑）═════════════════════

/// E1：段布局。给定基址与各段长度，按 .text→.rodata→.data→.bss 顺序
///     **连续**摆放，返回每段起始地址。对应 linker.ld 里
///     `.text : { *(.text*) }  .rodata : { *(.rodata*) } ...` 的地址推进。
fn layout(base: u64, sizes: &[u64; N_SEC]) -> [u64; N_SEC] {
    // TODO: 从 base 开始，依次累加上一段长度得到下一段起点。
    // HINT: starts[0]=base; starts[i]=starts[i-1]+sizes[i-1];
    [base; N_SEC] // ← 占位：四段全堆在 base，地址不递增，判 FAIL
}

/// E1：`.bss` 在文件里不占体积（零填充），由启动代码清零。返回长 len 的全 0 镜像。
fn clear_bss(len: usize) -> Vec<u8> {
    // TODO: 返回 len 字节、全为 0 的缓冲（模拟 _start 里 memset(__bss_start..__bss_end, 0)）。
    Vec::new() // ← 占位：长度不对、未清零，判 FAIL
}

/// E2：把自定义段 `.config` 落到 4K 对齐的地址（PROVIDE(__config_start = ALIGN(0x1000))）。
fn place_config(prev_end: u64) -> u64 {
    // TODO: 把 prev_end 向上对齐到 PAGE。HINT: align_up(prev_end, PAGE)。
    prev_end // ← 占位：未对齐到 4K，判 FAIL
}

/// E2：把带 `#[link_section=".config"]` 的结构体"编进"段里——返回其内容。
fn make_config() -> Config {
    // TODO: 填入约定的魔数与字段（magic=CONFIG_MAGIC, version=1, nslots=8, flags=0b101）。
    Config { magic: 0, version: 0, nslots: 0, flags: 0 } // ← 占位：魔数为 0，判 FAIL
}

/// E3(a)：解析自身 ELF64 头部前 64 字节，返回 (magic_ok, e_entry, e_phoff)。
///   magic = 7F 45 4C 46；e_entry 在偏移 24（8 字节小端）；e_phoff 在偏移 32。
fn parse_elf(buf: &[u8]) -> (bool, u64, u64) {
    // TODO: 校验前 4 字节魔数；用小端从偏移 24/32 各取 8 字节。
    // HINT: u64::from_le_bytes(buf[24..32].try_into().unwrap())
    (false, 0, 0) // ← 占位：未解析，判 FAIL
}

/// E3(b)：纯二进制（objcopy -O binary）要求 link 地址 == load 地址。
///   返回链接基址：必须与裸机加载地址一致，入口才能落在 .bin 偏移 0。
fn bin_base() -> u64 {
    // TODO: 返回与 BASE 一致的链接基址（0x80200000），否则 .bin 入口偏移非 0。
    0 // ← 占位：基址错误（!= load 地址），判 FAIL
}

/// E3(b)：纯二进制里入口相对镜像起点的偏移 = entry - base。
fn bin_entry_offset(base: u64, entry: u64) -> u64 {
    // TODO: 返回 entry 相对 base 的偏移。HINT: entry - base。
    1 // ← 占位：偏移非 0，判 FAIL
}

/// E4：runner——按 `.apps` 段内顺序逐个取出 app 入口并调用，收集其打印的标签。
///   对应 rcore ch2：__apps_start..__apps_end 之间一张表，循环拉起。
///   // TODO[a] 函数指针表（本实现）：每个 app 是个函数，指针被收进 .apps。
///   // ELSE[b] .incbin 原始二进制：app 先 objcopy 成 .bin 再嵌入段，runner 顺序跳转。
fn run_apps(table: &[fn() -> &'static str]) -> Vec<&'static str> {
    // TODO: 按 table 顺序调用每个 app，把返回标签依次 push 进 order。
    Vec::new() // ← 占位：未拉起任何 app，判 FAIL
}

// ─────────────── E4：三个 app（给定，勿改）——被收进 .apps 表 ───────────────
fn app_a() -> &'static str { "APP_A" }
fn app_b() -> &'static str { "APP_B" }
fn app_c() -> &'static str { "APP_C" }

// ═════════════════════ 测试 harness（勿改）═════════════════════

fn check_layout() -> bool {
    // .text=0x180, .rodata=0x40, .data=0x80, .bss=0x200（字节）
    let sizes: [u64; N_SEC] = [0x180, 0x40, 0x80, 0x200];
    let starts = layout(BASE, &sizes);
    let mut ok = true;
    // 1) 基址正确、地址严格递增 text<rodata<data<bss
    if starts[0] != BASE {
        println!("LAYOUT_FAIL .text 应钉在 {:#x}，实为 {:#x}", BASE, starts[0]);
        ok = false;
    }
    for i in 1..N_SEC {
        if starts[i] <= starts[i - 1] {
            println!("LAYOUT_FAIL 段地址非递增: sec{}={:#x} <= sec{}={:#x}", i, starts[i], i - 1, starts[i - 1]);
            ok = false;
        }
        if starts[i] != starts[i - 1] + sizes[i - 1] {
            println!("LAYOUT_FAIL 段未连续摆放: sec{}={:#x} 应={:#x}", i, starts[i], starts[i - 1] + sizes[i - 1]);
            ok = false;
        }
    }
    // 2) 放在 .rodata 的常量地址应落在只读区 [rodata_start, data_start)
    let rodata_const = starts[1] + 0x10;
    if !(starts[1] <= rodata_const && rodata_const < starts[2]) {
        println!("LAYOUT_FAIL 只读常量 {:#x} 未落在 .rodata 区间", rodata_const);
        ok = false;
    }
    // 3) .bss 零填充、长度正确
    let bss = clear_bss(sizes[3] as usize);
    if bss.len() != sizes[3] as usize {
        println!("LAYOUT_FAIL .bss 长度={} 应={}", bss.len(), sizes[3]);
        ok = false;
    } else if bss.iter().any(|&b| b != 0) {
        println!("LAYOUT_FAIL .bss 未清零");
        ok = false;
    }
    if ok {
        println!("LAYOUT_PASS");
    }
    ok
}

fn check_section() -> bool {
    // 上一段（.text+.rodata+.data+.bss）末尾，故意取一个非 4K 对齐的值。
    let prev_end = BASE + 0x4A0;
    let cstart = place_config(prev_end);
    let cfg = make_config();
    let mut ok = true;
    if cstart % PAGE != 0 {
        println!("SECTION_FAIL __config_start={:#x} 未 4K 对齐", cstart);
        ok = false;
    }
    if cstart < prev_end {
        println!("SECTION_FAIL __config_start={:#x} 不应回退到 prev_end={:#x} 之前", cstart, prev_end);
        ok = false;
    }
    let want = Config { magic: CONFIG_MAGIC, version: 1, nslots: 8, flags: 0b101 };
    if cfg != want {
        println!("SECTION_FAIL .config 内容不符: got={:?} want={:?}", cfg, want);
        ok = false;
    }
    if ok {
        println!("SECTION_PASS");
    }
    ok
}

/// 造一个 64 字节 ELF64 头：magic + e_entry@24 + e_phoff@32。
fn fake_elf_header(entry: u64, phoff: u64) -> Vec<u8> {
    let mut h = vec![0u8; 64];
    h[0..4].copy_from_slice(&[0x7F, 0x45, 0x4C, 0x46]);
    h[4] = 2; // EI_CLASS = ELFCLASS64
    h[5] = 1; // EI_DATA  = little-endian
    h[24..32].copy_from_slice(&entry.to_le_bytes());
    h[32..40].copy_from_slice(&phoff.to_le_bytes());
    h
}

fn check_elf() -> bool {
    let want_entry = BASE;
    let want_phoff: u64 = 64;
    let hdr = fake_elf_header(want_entry, want_phoff);
    let (magic_ok, entry, phoff) = parse_elf(&hdr);
    let mut ok = true;
    if !magic_ok {
        println!("ELF_FAIL 魔数校验失败（应 7F 45 4C 46）");
        ok = false;
    }
    if entry != want_entry {
        println!("ELF_FAIL e_entry={:#x} 应={:#x}", entry, want_entry);
        ok = false;
    }
    if phoff != want_phoff {
        println!("ELF_FAIL e_phoff={} 应={}", phoff, want_phoff);
        ok = false;
    }
    if ok {
        println!("ELF_PASS");
    }
    ok
}

fn check_bin() -> bool {
    // 纯二进制裸机加载地址。link 地址必须 == load 地址，入口才在偏移 0。
    let load_addr = BASE;
    let entry = BASE; // _start 钉在基址
    let base = bin_base();
    let off = bin_entry_offset(base, entry);
    let mut ok = true;
    if base != load_addr {
        println!("BIN_FAIL link 基址={:#x} 必须 == load 地址={:#x}", base, load_addr);
        ok = false;
    }
    if off != 0 {
        println!("BIN_FAIL .bin 入口偏移={:#x} 应=0（link==load 才成立）", off);
        ok = false;
    }
    if ok {
        println!("BIN_PASS");
    }
    ok
}

fn check_chain() -> bool {
    // .apps 段内顺序：A → B → C。runner 必须按此顺序拉起。
    let table: [fn() -> &'static str; 3] = [app_a, app_b, app_c];
    let order = run_apps(&table);
    let want = ["APP_A", "APP_B", "APP_C"];
    let mut ok = true;
    if order.len() != want.len() {
        println!("CHAIN_FAIL app 数={} 应={}", order.len(), want.len());
        ok = false;
    } else {
        for (i, (g, w)) in order.iter().zip(want.iter()).enumerate() {
            if g != w {
                println!("CHAIN_FAIL 第{}个 app={} 应={}（出现顺序须 == 段内顺序）", i, g, w);
                ok = false;
            }
        }
    }
    if ok {
        for a in &order {
            println!("{}", a);
        }
        println!("CHAIN_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_layout();
    all &= check_section();
    all &= check_elf();
    all &= check_bin();
    all &= check_chain();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
