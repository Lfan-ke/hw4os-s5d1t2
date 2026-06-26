//! 工具链探测：决定某 build 关键字所需工具是否齐备（缺则变体降级为 Unavailable）。

use std::path::PathBuf;

/// PATH 中是否存在可执行文件 `cmd`。
pub fn which(cmd: &str) -> bool {
    if let Ok(path) = std::env::var("PATH") {
        for dir in std::env::split_paths(&path) {
            let p: PathBuf = dir.join(cmd);
            if p.is_file() {
                return true;
            }
        }
    }
    false
}

/// 返回 `cmds` 中第一个可用的命令名。
pub fn first_available<'a>(cmds: &[&'a str]) -> Option<&'a str> {
    cmds.iter().copied().find(|c| which(c))
}

/// 某 build 关键字所需工具链是否齐备。
pub fn build_available(build: &str) -> bool {
    match build {
        "cargo" => which("cargo"),
        "gcc-host" => which("gcc") || which("cc"),
        "gcc-rv64" => {
            (which("riscv64-unknown-elf-gcc") || which("riscv64-linux-gnu-gcc"))
                && which("qemu-riscv64")
        }
        "iverilog" => which("iverilog") && which("vvp"),
        "verilator" => which("verilator"),
        "bsc" => which("bsc"),
        _ => false,
    }
}
