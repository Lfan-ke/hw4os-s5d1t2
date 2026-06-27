//! labctl —— AI4OSE OSLAB 实验引导 / 编译 / 判题 / 计分 runner。
//!
//! 里程碑进度：
//!   [x] 清单解析 + list
//!   [x] 变体 runner（sw-rust/sw-c/hw-v/hw-bsv）+ judge + score（M 选 N / 辅助分）
//!   [ ] TUI（拓扑/数据流/波形/接口）+ watch + hint 渐进
//!   [ ] 逃生舱 wave --gui / diagram

mod judge;
mod manifest;
mod score;
mod toolchain;
mod tui;
mod variant;
mod vcd;
mod view;

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{Duration, SystemTime};

use manifest::{Course, Exercise};
use score::{score_exercise, Board};
use variant::{run_variant, Status};
use view::View;

#[derive(Parser)]
#[command(name = "labctl", version, about = "AI4OSE OSLAB 实验引导 / 判题 runner")]
struct Cli {
    /// 仓库根（含 info.toml）；默认从当前目录向上查找
    #[arg(long, global = true)]
    root: Option<PathBuf>,
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// 列出全部实验与状态
    List,
    /// 跑当前/指定实验的所有可用变体
    Run {
        /// 实验相对路径，如 improper/01-hw-vlan（省略则跑第一个）
        id: Option<String>,
        /// 用 ans/ 参考答案而非 exercises/
        #[arg(long)]
        solutions: bool,
    },
    /// 全量跑出记分板
    Verify {
        /// 用 ans/ 参考答案自测（应全过）
        #[arg(long)]
        solutions: bool,
    },
    /// 揭示提示
    Hint { id: Option<String> },
    /// 分别打印必修分与辅助分（两本独立的账）
    Score {
        #[arg(long)]
        solutions: bool,
    },
    /// 显示下一个实验
    Next,
    /// 渲染实验的 TUI 伴侣面板（拓扑/数据流/接口，静态）
    View { id: Option<String> },
    /// 监视文件，保存即自动重跑并刷新伴侣面板
    Watch { id: Option<String> },
    /// 看波形：终端紧凑渲染，或 --gui 用 gtkwave
    Wave {
        id: Option<String>,
        #[arg(long)]
        gui: bool,
    },
    /// 导出 Mermaid 拓扑/数据流图（可入 README/浏览器预览）
    Diagram { id: Option<String> },
}

fn find_root(explicit: Option<PathBuf>) -> Result<PathBuf> {
    if let Some(p) = explicit {
        return Ok(p);
    }
    let mut dir = std::env::current_dir()?;
    loop {
        if dir.join("info.toml").is_file() {
            return Ok(dir);
        }
        if !dir.pop() {
            anyhow::bail!("找不到 info.toml（请在仓库内运行，或用 --root 指定）");
        }
    }
}

fn base_dir(root: &Path, solutions: bool) -> PathBuf {
    // 参考答案统一放根目录 ans/；题面在 exercises/
    root.join(if solutions { "ans" } else { "exercises" })
}

fn build_root(root: &Path) -> PathBuf {
    root.join(".labctl").join("build")
}

/// 按 id 解析实验（匹配 rel 全名或末段 id）；省略则取第一个。
fn resolve<'a>(course: &'a Course, id: &Option<String>) -> Result<&'a Exercise> {
    match id {
        None => course
            .exercises
            .first()
            .context("课程中没有任何实验"),
        Some(want) => course
            .exercises
            .iter()
            .find(|e| &e.rel == want || e.id == *want)
            .with_context(|| format!("找不到实验: {}", want)),
    }
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let root = find_root(cli.root)?;
    let course = Course::load(&root).context("加载课程清单失败")?;

    match cli.cmd {
        Cmd::List => cmd_list(&course),
        Cmd::Run { id, solutions } => cmd_run(&course, &root, &id, solutions),
        Cmd::Verify { solutions } => cmd_verify(&course, &root, solutions).map(|_| ()),
        Cmd::Hint { id } => cmd_hint(&course, &id),
        Cmd::Score { solutions } => cmd_score(&course, &root, solutions),
        Cmd::Next => cmd_next(&course),
        Cmd::View { id } => cmd_view(&course, &root, &id),
        Cmd::Watch { id } => cmd_watch(&course, &root, &id),
        Cmd::Wave { id, gui } => cmd_wave(&course, &root, &id, gui),
        Cmd::Diagram { id } => cmd_diagram(&course, &root, &id),
    }
}

fn cmd_list(course: &Course) -> Result<()> {
    println!("课程：{}  （ISA {}）", course.name, course.isa);
    println!("共 {} 个实验：", course.exercises.len());
    for ex in &course.exercises {
        let variants: Vec<String> = ex
            .variants
            .iter()
            .map(|v| format!("{}{}", v.id, if v.warn_gate { "*" } else { "" }))
            .collect();
        println!(
            "  • {:<22} require={}  [{}]  — {}",
            ex.rel,
            ex.require,
            variants.join(", "),
            ex.title
        );
    }
    println!("\n(* = 0-warning 门；require=N 即“M 选 N”必修阈值)");
    Ok(())
}

/// 跑一个实验的全部变体并打印结果。返回该实验计分。
fn run_one(ex: &Exercise, root: &Path, solutions: bool) -> score::ExerciseScore {
    let base = base_dir(root, solutions).join(&ex.rel);
    // 按 solutions/exercises 分隔构建目录，避免 bsc/iverilog 增量编译复用对方的产物
    let broot = build_root(root).join(if solutions { "sol" } else { "ex" });
    let mut results = Vec::new();
    for v in &ex.variants {
        let r = run_variant(ex, &base, v, &broot);
        results.push(r);
    }
    score_exercise(ex, results)
}

// ANSI 颜色（仅 labctl 人类面板用；与被判程序输出无关，不影响判题）
const C_GRN: &str = "\x1b[32m";
const C_YEL: &str = "\x1b[33m";
const C_RED: &str = "\x1b[31m";
const C_DIM: &str = "\x1b[2m";
const C_RST: &str = "\x1b[0m";

fn print_exercise_result(
    s: &score::ExerciseScore,
    variants: &[crate::manifest::Variant],
    solutions: bool,
) {
    let axis_of = |id: &str| -> &str {
        variants
            .iter()
            .find(|v| v.id == id)
            .map(|v| v.axis.as_str())
            .unwrap_or("")
    };
    println!("\n▌ {}  — {}", s.rel, s.title);
    // 已被「通过」覆盖的轴（软件 / 硬件 / essay）。
    let mut covered: std::collections::HashSet<&str> = std::collections::HashSet::new();
    for r in &s.results {
        if r.status == Status::Pass {
            covered.insert(axis_of(&r.id));
        }
    }
    for r in &s.results {
        let warn = if r.warnings > 0 {
            format!(" ({}w)", r.warnings)
        } else {
            String::new()
        };
        // 学生练习(非 --solutions)中必修已达成时：同轴已过的备选→灰「无需重复」（rust/c、v/bsv 二选一）；
        // 尚未覆盖的轴→黄「可挑战」（跨轴/综合才是真正的 what-if，鼓励多手抓拿辅助分）。
        let soft = !solutions && s.required_done && matches!(r.status, Status::Fail(_));
        let axis = axis_of(&r.id);
        let (color, sym, detail, show_log) = match &r.status {
            Status::Pass => (C_GRN, "✓", "通过".to_string(), false),
            Status::Fail(_) if soft && covered.contains(axis) => (
                C_DIM,
                "○",
                "备选途径（同轴 rust/c 或 v/bsv 二选一，已过无需重复）".to_string(),
                false,
            ),
            Status::Fail(_) if soft => (
                C_YEL,
                "◌",
                "可挑战：换个角度做这一轴（辅助分；必修已达成，不计失败）".to_string(),
                false,
            ),
            Status::Fail(why) => (C_RED, "✗", format!("失败：{}", why), true),
            Status::Unavailable => (C_DIM, "⊘", "工具链缺失（跳过，不计分）".to_string(), false),
            Status::Error(e) => (C_RED, "!", format!("错误：{}", e.lines().next().unwrap_or("")), true),
        };
        println!("   {}{} {:<8}{}{} {}", color, sym, r.id, warn, C_RST, detail);
        if show_log && !r.log.trim().is_empty() {
            for line in r.log.trim_end().lines().rev().take(6).collect::<Vec<_>>().iter().rev() {
                println!("       {}│ {}{}", C_DIM, line, C_RST);
            }
        }
    }
    // 可挑战的「轴」数：有变体但尚未被通过覆盖的轴（软/硬/essay 各算一个角度）。
    let mut all_axes: std::collections::HashSet<&str> = std::collections::HashSet::new();
    for r in &s.results {
        all_axes.insert(axis_of(&r.id));
    }
    let challenge_axes = all_axes.iter().filter(|a| !covered.contains(*a)).count();
    let (mc, mark) = if s.required_done {
        (C_GRN, "✓ 完成")
    } else {
        (C_RED, "✗ 未达成")
    };
    let extra = if !solutions && s.required_done && challenge_axes > 0 {
        format!(
            "  {}（还有 {} 个角度可挑战拿辅助分）{}",
            C_YEL, challenge_axes, C_RST
        )
    } else {
        String::new()
    };
    println!(
        "   ── 必修 {}{}{} (通过 {}/{} 条，require={})  辅助分 +{}{}",
        mc, mark, C_RST, s.passed, s.results.len(), s.require, s.bonus, extra
    );
}

fn cmd_run(course: &Course, root: &Path, id: &Option<String>, solutions: bool) -> Result<()> {
    let ex = resolve(course, id)?;
    let s = run_one(ex, root, solutions);
    print_exercise_result(&s, &ex.variants, solutions);
    if !s.required_done {
        std::process::exit(1);
    }
    Ok(())
}

fn cmd_verify(course: &Course, root: &Path, solutions: bool) -> Result<Board> {
    println!(
        "labctl verify{}  —— 逐题判定中…",
        if solutions { " --solutions" } else { "" }
    );
    let mut items = Vec::new();
    for ex in &course.exercises {
        let s = run_one(ex, root, solutions);
        print_exercise_result(&s, &ex.variants, solutions);
        items.push(s);
    }
    let board = Board { items };
    println!(
        "\n══ 记分板 ══  必修 {}/{}    辅助分 +{}（独立账）",
        board.required_total(),
        board.required_max(),
        board.bonus_total()
    );
    Ok(board)
}

fn cmd_score(course: &Course, root: &Path, solutions: bool) -> Result<()> {
    let mut items = Vec::new();
    for ex in &course.exercises {
        items.push(run_one(ex, root, solutions));
    }
    let board = Board { items };
    println!("必修分：{}/{}", board.required_total(), board.required_max());
    println!("辅助分：+{}（与总分独立计算）", board.bonus_total());
    Ok(())
}

fn cmd_hint(course: &Course, id: &Option<String>) -> Result<()> {
    let ex = resolve(course, id)?;
    if ex.hints.is_empty() {
        println!("{} 暂无提示。", ex.rel);
        return Ok(());
    }
    println!("{} 的提示：", ex.rel);
    for (i, h) in ex.hints.iter().enumerate() {
        println!("  [{}] {}", i + 1, h.text);
    }
    Ok(())
}

fn cmd_next(course: &Course) -> Result<()> {
    match course.exercises.first() {
        Some(ex) => println!("下一个实验：{}  — {}", ex.rel, ex.title),
        None => println!("课程中没有任何实验。"),
    }
    Ok(())
}

fn cmd_view(course: &Course, root: &Path, id: &Option<String>) -> Result<()> {
    let ex = resolve(course, id)?;
    let dir = base_dir(root, false).join(&ex.rel);
    let view = View::load(&dir)?;
    print!("{}", tui::render(ex, view.as_ref(), None));
    Ok(())
}

/// 收集实验目录下源文件的最新修改时间（跳过构建产物目录）。
fn latest_mtime(dir: &Path) -> SystemTime {
    fn walk(dir: &Path, newest: &mut SystemTime) {
        let skip = ["sim_build", "target", ".labctl", ".git", "build"];
        if let Ok(rd) = std::fs::read_dir(dir) {
            for e in rd.flatten() {
                let p = e.path();
                let name = e.file_name();
                let name = name.to_string_lossy();
                if p.is_dir() {
                    if !skip.iter().any(|s| *s == name) {
                        walk(&p, newest);
                    }
                } else if let Ok(m) = e.metadata().and_then(|m| m.modified()) {
                    if m > *newest {
                        *newest = m;
                    }
                }
            }
        }
    }
    let mut newest = SystemTime::UNIX_EPOCH;
    walk(dir, &mut newest);
    newest
}

fn cmd_watch(course: &Course, root: &Path, id: &Option<String>) -> Result<()> {
    let ex = resolve(course, id)?;
    let dir = base_dir(root, false).join(&ex.rel);
    let view = View::load(&dir)?;
    let mut last = SystemTime::UNIX_EPOCH;
    loop {
        let now = latest_mtime(&dir);
        if now != last {
            last = now;
            let s = run_one(ex, root, false);
            print!("\x1b[2J\x1b[H"); // 清屏 + 光标归位
            print!("{}", tui::render(ex, view.as_ref(), Some(&s.results)));
            let mark = if s.required_done { "✓ 完成" } else { "✗ 未达成" };
            println!(
                "  必修 {}（通过 {}/{}，require={}）  辅助分 +{}",
                mark, s.passed, s.results.len(), ex.require, s.bonus
            );
            println!("  〔保存任意源文件自动重跑 · Ctrl-C 退出〕");
            use std::io::Write;
            std::io::stdout().flush().ok();
        }
        std::thread::sleep(Duration::from_millis(500));
    }
}

fn find_vcd(dir: &Path) -> Option<PathBuf> {
    std::fs::read_dir(dir).ok()?.flatten().find_map(|e| {
        let p = e.path();
        (p.extension().and_then(|x| x.to_str()) == Some("vcd")).then_some(p)
    })
}

fn cmd_wave(course: &Course, root: &Path, id: &Option<String>, gui: bool) -> Result<()> {
    let ex = resolve(course, id)?;
    // 优先 iverilog 硬件变体（tb 的 $dumpfile 自动产 vcd）
    let hw = ex
        .variants
        .iter()
        .find(|v| v.axis == "hardware" && v.build == "iverilog")
        .or_else(|| ex.variants.iter().find(|v| v.axis == "hardware"))
        .context("本实验无硬件变体，无法看波形")?;

    let base = base_dir(root, false).join(&ex.rel);
    let broot = build_root(root).join("ex");
    let _ = run_variant(ex, &base, hw, &broot); // 跑一次产 vcd
    let bdir = broot.join(ex.rel.replace('/', "_")).join(&hw.id);
    let vcd_path = find_vcd(&bdir).context("未找到 .vcd（确认 testbench 有 $dumpfile）")?;

    if gui {
        if !toolchain::which("gtkwave") {
            anyhow::bail!("未安装 gtkwave");
        }
        Command::new("gtkwave")
            .arg(&vcd_path)
            .spawn()
            .context("启动 gtkwave 失败")?;
        println!("已用 gtkwave 打开 {}", vcd_path.display());
        return Ok(());
    }

    let parsed = vcd::parse(&vcd_path)?;
    let signals = match View::load(&base)? {
        Some(v) if !v.wave.signals.is_empty() => v.wave.signals,
        _ => parsed.vars.iter().map(|(_, n, _)| n.clone()).collect(),
    };
    println!("波形（{}，1-bit:▁▔，多bit:依次取值）", hw.id);
    print!("{}", vcd::render(&parsed, &signals));
    println!("（精确波形：labctl wave --gui  或  make -C {}/hw/v wave）", ex.rel);
    Ok(())
}

fn mermaid_id(s: &str) -> String {
    s.chars()
        .map(|c| if c.is_alphanumeric() { c } else { '_' })
        .collect()
}

fn cmd_diagram(course: &Course, root: &Path, id: &Option<String>) -> Result<()> {
    let ex = resolve(course, id)?;
    let dir = base_dir(root, false).join(&ex.rel);
    let view = View::load(&dir)?.context("本实验无 view.toml")?;
    println!("```mermaid");
    println!("flowchart LR");
    for n in &view.nodes {
        println!("  {}[\"{}\"]", mermaid_id(&n.id), n.label);
    }
    for e in &view.edges {
        println!("  {} --> {}", mermaid_id(&e.from), mermaid_id(&e.to));
    }
    println!("```");
    if !view.flows.is_empty() {
        println!("\n数据流场景：");
        for f in &view.flows {
            println!("- **{}**: {}", f.name, f.note);
        }
    }
    Ok(())
}
