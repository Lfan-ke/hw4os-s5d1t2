//! 解析 `info.toml`（总清单）与各实验 `meta.toml`（元数据）。
//! `view.toml`（可视化声明）在 TUI 里程碑接入，这里先不解析。

use anyhow::{Context, Result};
use serde::Deserialize;
use std::path::{Path, PathBuf};

/// info.toml 顶层（`[course]` 下含 name/isa/order）
#[derive(Debug, Deserialize)]
struct InfoFile {
    course: InfoCourse,
}

#[derive(Debug, Deserialize)]
struct InfoCourse {
    name: String,
    isa: String,
    #[serde(default)]
    order: Vec<String>,
}

/// 加载后的课程（清单 + 各实验元数据）
#[derive(Debug)]
pub struct Course {
    pub name: String,
    pub isa: String,
    pub exercises: Vec<Exercise>,
}

/// 一个实验的元数据（exercises/<rel>/meta.toml）
#[derive(Debug, Deserialize)]
pub struct Exercise {
    pub id: String,
    pub title: String,
    #[allow(dead_code)] // track/env：qemu 与 TUI 里程碑消费
    pub track: String,
    /// 必修阈值：通过变体数 ≥ require 即视为完成（默认 1）
    #[serde(default = "default_one")]
    pub require: u32,
    /// 软件变体运行环境：host | qemu-user | qemu-virt
    #[allow(dead_code)] // qemu 里程碑消费
    #[serde(default)]
    pub env: Option<String>,
    /// 必修分权重
    #[serde(default = "default_one")]
    pub weight: u32,
    #[serde(default, rename = "variant")]
    pub variants: Vec<Variant>,
    #[serde(default)]
    pub judge: Judge,
    #[serde(default, rename = "hint")]
    pub hints: Vec<Hint>,

    /// 运行期填充：该实验目录的绝对路径
    #[serde(skip)]
    pub dir: PathBuf,
    /// 运行期填充：相对清单路径，如 "improper/01-hw-vlan"
    #[serde(skip)]
    pub rel: String,
}

fn default_one() -> u32 {
    1
}

/// 一条实现路径（变体）
#[derive(Debug, Deserialize)]
pub struct Variant {
    pub id: String,
    /// software | hardware
    #[allow(dead_code)] // axis/lang：TUI 与按轴加权辅助分里程碑消费
    pub axis: String,
    /// rust | c | verilog | bsv
    #[allow(dead_code)]
    pub lang: String,
    /// 相对实验目录的子目录，如 "sw/rust"
    pub dir: String,
    /// labctl 内置构建器关键字：cargo | gcc-host | gcc-rv64 | iverilog | bsc ...
    pub build: String,
    /// 为真时编译输出含 warning 即判失败（硬件 0-warning 门）
    #[serde(default)]
    pub warn_gate: bool,
    /// 仿真顶层模块名（bsc 必需；iverilog 可省，自动识别）
    #[serde(default)]
    pub top: Option<String>,
    /// 入口源文件（bsc 的顶层 .bsv；省略则取目录内唯一 .bsv）
    #[serde(default)]
    pub entry: Option<String>,
}

/// 统一判题口径（所有变体对外行为一致）
#[derive(Debug, Deserialize)]
pub struct Judge {
    #[serde(default)]
    pub expect: Vec<String>,
    #[serde(default)]
    pub forbid: Vec<String>,
    #[serde(default = "default_timeout")]
    pub timeout_s: u64,
}

fn default_timeout() -> u64 {
    30
}

impl Default for Judge {
    fn default() -> Self {
        Judge {
            expect: Vec::new(),
            forbid: Vec::new(),
            timeout_s: default_timeout(),
        }
    }
}

/// 渐进提示
#[derive(Debug, Deserialize)]
pub struct Hint {
    pub text: String,
}

impl Course {
    /// 从仓库根加载 info.toml 及其 order 中每个实验的 meta.toml。
    pub fn load(root: &Path) -> Result<Course> {
        let info_path = root.join("info.toml");
        let raw = std::fs::read_to_string(&info_path)
            .with_context(|| format!("读取 {} 失败", info_path.display()))?;
        let info: InfoFile = toml::from_str(&raw).context("解析 info.toml 失败")?;

        let mut exercises = Vec::new();
        for rel in &info.course.order {
            let dir = root.join("exercises").join(rel);
            let meta_path = dir.join("meta.toml");
            let mraw = std::fs::read_to_string(&meta_path)
                .with_context(|| format!("读取 {} 失败", meta_path.display()))?;
            let mut ex: Exercise = toml::from_str(&mraw)
                .with_context(|| format!("解析 {} 失败", meta_path.display()))?;
            ex.dir = dir;
            ex.rel = rel.clone();
            exercises.push(ex);
        }

        Ok(Course {
            name: info.course.name,
            isa: info.course.isa,
            exercises,
        })
    }
}
