//! 统一判题：输出需包含全部 `expect` 子串，且不含任何 `forbid` 子串。

use crate::manifest::Judge;

/// 判定运行输出。Ok(()) = 通过；Err(原因) = 失败。
pub fn judge_output(out: &str, j: &Judge) -> Result<(), String> {
    for f in &j.forbid {
        if out.contains(f.as_str()) {
            return Err(format!("命中禁止串 \"{}\"", f));
        }
    }
    for e in &j.expect {
        if !out.contains(e.as_str()) {
            return Err(format!("缺少期望串 \"{}\"", e));
        }
    }
    Ok(())
}
