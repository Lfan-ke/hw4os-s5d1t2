//! 变体 runner：探测工具链 → 构建（捕获 warning）→ 运行（捕获输出，带超时）→ 判题。
//! 判题不依赖 Makefile，labctl 直接调 cargo/gcc/iverilog/bsc。

use crate::judge;
use crate::manifest::{Exercise, Variant};
use crate::toolchain;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;

/// 单变体判定结果四态。
#[derive(Debug, Clone, PartialEq)]
pub enum Status {
    Pass,
    Fail(String),
    Unavailable,
    Error(String),
}

impl Status {
    pub fn symbol(&self) -> &'static str {
        match self {
            Status::Pass => "✓",
            Status::Fail(_) => "✗",
            Status::Unavailable => "⊘",
            Status::Error(_) => "!",
        }
    }
}

pub struct VariantResult {
    pub id: String,
    pub status: Status,
    pub warnings: usize,
    /// 全过程日志（失败时展示尾部，便于学生调试）
    pub log: String,
}

struct RunOutput {
    output: String,
    warnings: usize,
    log: String,
}

/// 跑一个变体：base_dir 是 exercises/ 或 solutions/ 下的实验目录。
pub fn run_variant(ex: &Exercise, base_dir: &Path, v: &Variant, build_root: &Path) -> VariantResult {
    if !toolchain::build_available(&v.build) {
        return VariantResult {
            id: v.id.clone(),
            status: Status::Unavailable,
            warnings: 0,
            log: format!("缺少 build=\"{}\" 所需工具链", v.build),
        };
    }

    let vdir = base_dir.join(&v.dir);
    if !vdir.is_dir() {
        return VariantResult {
            id: v.id.clone(),
            status: Status::Error(format!("变体目录不存在: {}", vdir.display())),
            warnings: 0,
            log: String::new(),
        };
    }

    let bdir = build_root.join(ex.rel.replace('/', "_")).join(&v.id);
    let _ = std::fs::create_dir_all(&bdir);
    let timeout_s = ex.judge.timeout_s;

    let res = match v.build.as_str() {
        "essay" => run_essay(&vdir),
        "cargo" => run_cargo(&vdir, timeout_s),
        "gcc-host" => run_gcc_host(&vdir, &bdir, timeout_s),
        "gcc-rv64" => run_gcc_rv64(&vdir, &bdir, timeout_s),
        "iverilog" => run_iverilog(&vdir, &bdir, timeout_s),
        "bsc" => run_bsc(&vdir, &bdir, v, timeout_s),
        "qemu-virt" => run_qemu_virt(&vdir, timeout_s),
        other => Err(format!("未知 build 关键字: {}", other)),
    };

    match res {
        Ok(run) => {
            if v.warn_gate && run.warnings > 0 {
                return VariantResult {
                    id: v.id.clone(),
                    status: Status::Fail(format!("{} 个 warning（未过 0-warning 门）", run.warnings)),
                    warnings: run.warnings,
                    log: run.log,
                };
            }
            let status = if v.build == "essay" {
                // 思考题：rustlings 式——答案非空且删掉未作答哨兵即过
                if run.output.trim().is_empty() {
                    Status::Fail("答案为空".into())
                } else if run.output.contains("LABCTL_ESSAY_TODO") {
                    Status::Fail("仍含未作答哨兵 LABCTL_ESSAY_TODO（作答后删除该行）".into())
                } else {
                    Status::Pass
                }
            } else {
                match judge::judge_output(&run.output, &ex.judge) {
                    Ok(()) => Status::Pass,
                    Err(why) => Status::Fail(why),
                }
            };
            VariantResult {
                id: v.id.clone(),
                status,
                warnings: run.warnings,
                log: run.log,
            }
        }
        Err(e) => VariantResult {
            id: v.id.clone(),
            status: Status::Error(e.clone()),
            warnings: 0,
            log: e,
        },
    }
}

fn combine(out: &std::process::Output) -> String {
    let mut s = String::from_utf8_lossy(&out.stdout).into_owned();
    s.push_str(&String::from_utf8_lossy(&out.stderr));
    s
}

fn files_with_ext(dir: &Path, ext: &str) -> Vec<PathBuf> {
    let mut v = Vec::new();
    if let Ok(rd) = std::fs::read_dir(dir) {
        for e in rd.flatten() {
            let p = e.path();
            if p.extension().and_then(|x| x.to_str()) == Some(ext) {
                v.push(p);
            }
        }
    }
    v.sort();
    v
}

/// 运行阶段统一入口：若系统有 GNU `timeout` 则包一层防死循环。
fn exec_run(prog: &str, args: &[OsString], cwd: &Path, timeout_s: u64) -> Result<std::process::Output, String> {
    let mut cmd;
    if toolchain::which("timeout") {
        // GNU timeout：选项须在 DURATION 之前 —— timeout --kill-after=5s 30s PROG …
        cmd = Command::new("timeout");
        cmd.arg("--kill-after=5s");
        cmd.arg(format!("{timeout_s}s"));
        cmd.arg(prog);
    } else {
        cmd = Command::new(prog);
    }
    cmd.args(args).current_dir(cwd);
    cmd.output().map_err(|e| format!("运行 {prog} 失败: {e}"))
}

// ── cargo（host）──────────────────────────────────────────────────
fn run_cargo(dir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    let out = exec_run("cargo", &[OsString::from("run"), OsString::from("-q")], dir, timeout_s)?;
    let s = combine(&out);
    let warnings = s.matches("warning:").count();
    Ok(RunOutput { output: s.clone(), warnings, log: s })
}

// ── gcc（host，纯逻辑实验）─────────────────────────────────────────
fn run_gcc_host(dir: &Path, bdir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    let cs = files_with_ext(dir, "c");
    if cs.is_empty() {
        return Err("无 .c 源文件".into());
    }
    let cc = toolchain::first_available(&["gcc", "cc"]).ok_or("找不到 gcc/cc")?;
    let bin = bdir.join("a.out");
    let mut cmd = Command::new(cc);
    cmd.args(["-Wall", "-Wextra", "-O2", "-o"]).arg(&bin);
    for c in &cs {
        cmd.arg(c);
    }
    let comp = cmd.output().map_err(|e| format!("{} 执行失败: {}", cc, e))?;
    let cerr = String::from_utf8_lossy(&comp.stderr).into_owned();
    if !comp.status.success() {
        return Err(format!("编译失败:\n{}", cerr));
    }
    let warnings = cerr.matches("warning:").count();
    let run = exec_run(&bin.to_string_lossy(), &[], bdir, timeout_s)?;
    let s = combine(&run);
    Ok(RunOutput { output: s.clone(), warnings, log: format!("{cerr}{s}") })
}

// ── qemu-virt（正经赛道：make kernel.elf → qemu-system-riscv64 S 态内核）──
fn run_qemu_virt(dir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    let mk = Command::new("make")
        .arg("kernel.elf")
        .current_dir(dir)
        .output()
        .map_err(|e| format!("make 执行失败: {}", e))?;
    let mkerr = combine(&mk);
    if !mk.status.success() {
        return Err(format!("内核构建失败:\n{}", mkerr));
    }
    let warnings = mkerr.matches("warning:").count();
    let elf = dir.join("kernel.elf");
    if !elf.is_file() {
        return Err("make 未产出 kernel.elf".into());
    }
    let args = [
        OsString::from("-machine"),
        OsString::from("virt"),
        OsString::from("-nographic"),
        OsString::from("-bios"),
        OsString::from("default"),
        OsString::from("-kernel"),
        elf.into_os_string(),
    ];
    let run = exec_run("qemu-system-riscv64", &args, dir, timeout_s)?;
    let s = combine(&run);
    Ok(RunOutput {
        output: s.clone(),
        warnings,
        log: format!("{mkerr}{s}"),
    })
}

// ── essay（思考题：读答案文件，judge 用 expect 关键字）────────────
fn run_essay(dir: &Path) -> Result<RunOutput, String> {
    let mut files = files_with_ext(dir, "md");
    files.extend(files_with_ext(dir, "txt"));
    let f = files
        .into_iter()
        .next()
        .ok_or("essay 变体目录需有 .md/.txt 答案文件")?;
    let content = std::fs::read_to_string(&f).map_err(|e| e.to_string())?;
    Ok(RunOutput {
        output: content.clone(),
        warnings: 0,
        log: content,
    })
}

// ── gcc-rv64（qemu-user：riscv64 静态 ELF → qemu-riscv64）─────────
fn run_gcc_rv64(dir: &Path, bdir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    let cs = files_with_ext(dir, "c");
    if cs.is_empty() {
        return Err("无 .c 源文件".into());
    }
    let cc = toolchain::first_available(&["riscv64-linux-gnu-gcc", "riscv64-unknown-elf-gcc"])
        .ok_or("找不到 riscv64 gcc")?;
    let bin = bdir.join("a.rv64");
    let mut cmd = Command::new(cc);
    cmd.args(["-Wall", "-Wextra", "-O2", "-static", "-o"]).arg(&bin);
    for c in &cs {
        cmd.arg(c);
    }
    let comp = cmd.output().map_err(|e| format!("{} 执行失败: {}", cc, e))?;
    let cerr = String::from_utf8_lossy(&comp.stderr).into_owned();
    if !comp.status.success() {
        return Err(format!("编译失败:\n{}", cerr));
    }
    let warnings = cerr.matches("warning:").count();
    let run = exec_run("qemu-riscv64", &[bin.into_os_string()], bdir, timeout_s)?;
    let s = combine(&run);
    Ok(RunOutput {
        output: s.clone(),
        warnings,
        log: format!("{cerr}{s}"),
    })
}

// ── iverilog ─────────────────────────────────────────────────────
fn run_iverilog(dir: &Path, bdir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    let vs = files_with_ext(dir, "v");
    if vs.is_empty() {
        return Err("无 .v 源文件".into());
    }
    let sim = bdir.join("sim.vvp");
    let mut cmd = Command::new("iverilog");
    cmd.args(["-g2012", "-Wall", "-o"]).arg(&sim);
    for f in &vs {
        cmd.arg(f);
    }
    let comp = cmd.current_dir(dir).output().map_err(|e| format!("iverilog 执行失败: {}", e))?;
    let cerr = String::from_utf8_lossy(&comp.stderr).into_owned();
    if !comp.status.success() {
        return Err(format!("iverilog 编译失败:\n{}", cerr));
    }
    let warnings = cerr.lines().filter(|l| l.to_lowercase().contains("warning")).count();
    let run = exec_run("vvp", &[sim.clone().into_os_string()], bdir, timeout_s)?;
    let s = combine(&run);
    Ok(RunOutput { output: s.clone(), warnings, log: format!("{cerr}{s}") })
}

// ── bsc（Bluesim）────────────────────────────────────────────────
fn run_bsc(dir: &Path, bdir: &Path, v: &Variant, timeout_s: u64) -> Result<RunOutput, String> {
    let top = v.top.clone().ok_or("hw-bsv 需在 meta.toml 指定 top（仿真顶层模块名）")?;
    let entry = match &v.entry {
        Some(e) => e.clone(),
        None => {
            let bs = files_with_ext(dir, "bsv");
            if bs.len() != 1 {
                return Err("请在 meta.toml 指定 entry（目录内 .bsv 不唯一）".into());
            }
            bs[0].file_name().and_then(|s| s.to_str()).unwrap_or("").to_string()
        }
    };
    let bd = bdir.to_string_lossy().into_owned();

    let comp = Command::new("bsc")
        .args(["-sim", "-bdir", &bd, "-simdir", &bd, "-info-dir", &bd, "-u", "-g", &top, &entry])
        .current_dir(dir)
        .output()
        .map_err(|e| format!("bsc 执行失败: {}", e))?;
    let comp_err = combine(&comp);
    if !comp.status.success() {
        return Err(format!("bsc 编译失败:\n{}", comp_err));
    }

    let simbin = bdir.join("sim_bsv");
    let link = Command::new("bsc")
        .args(["-sim", "-bdir", &bd, "-simdir", &bd, "-e", &top, "-o"])
        .arg(&simbin)
        .current_dir(dir)
        .output()
        .map_err(|e| format!("bsc 链接失败: {}", e))?;
    let link_err = combine(&link);
    if !link.status.success() {
        return Err(format!("bsc 链接失败:\n{}", link_err));
    }

    let warnings = (comp_err.clone() + &link_err)
        .lines()
        .filter(|l| l.contains("Warning:"))
        .count();

    let run = exec_run(&simbin.to_string_lossy(), &[], bdir, timeout_s)?;
    let s = combine(&run);
    Ok(RunOutput { output: s.clone(), warnings, log: format!("{comp_err}{link_err}{s}") })
}
