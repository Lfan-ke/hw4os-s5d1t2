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
mod variant;

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use std::path::{Path, PathBuf};

use manifest::{Course, Exercise};
use score::{score_exercise, Board};
use variant::{run_variant, Status};

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
        /// 用 solutions/ 参考解而非 exercises/
        #[arg(long)]
        solutions: bool,
    },
    /// 全量跑出记分板
    Verify {
        /// 用 solutions/ 参考解自测（应全过）
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
    root.join(if solutions { "solutions" } else { "exercises" })
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

fn print_exercise_result(s: &score::ExerciseScore) {
    println!("\n▌ {}  — {}", s.rel, s.title);
    for r in &s.results {
        let detail = match &r.status {
            Status::Pass => "通过".to_string(),
            Status::Fail(why) => format!("失败：{}", why),
            Status::Unavailable => "工具链缺失（跳过，不计分）".to_string(),
            Status::Error(e) => format!("错误：{}", e.lines().next().unwrap_or("")),
        };
        let warn = if r.warnings > 0 {
            format!(" ({}w)", r.warnings)
        } else {
            String::new()
        };
        println!("   {} {:<8}{} {}", r.status.symbol(), r.id, warn, detail);
        // 失败/错误时展示日志尾部，便于定位
        if matches!(r.status, Status::Fail(_) | Status::Error(_)) && !r.log.trim().is_empty() {
            for line in r.log.trim_end().lines().rev().take(6).collect::<Vec<_>>().iter().rev() {
                println!("       │ {}", line);
            }
        }
    }
    let mark = if s.required_done { "✓ 完成" } else { "✗ 未达成" };
    println!(
        "   ── 必修 {} (通过 {}/{} 条，require={})  辅助分 +{}",
        mark, s.passed, s.results.len(), s.require, s.bonus
    );
}

fn cmd_run(course: &Course, root: &Path, id: &Option<String>, solutions: bool) -> Result<()> {
    let ex = resolve(course, id)?;
    let s = run_one(ex, root, solutions);
    print_exercise_result(&s);
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
        print_exercise_result(&s);
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
