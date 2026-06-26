//! 极简 VCD 解析 + 紧凑终端波形渲染。
//! 仅用于 labctl 内置预览；精确调试请用 `labctl wave --gui` 或 `make wave`（gtkwave）。

use std::path::Path;

pub struct Vcd {
    /// (短id, 信号名, 位宽)
    pub vars: Vec<(String, String, usize)>,
    /// (时间, 短id, 值串)
    pub changes: Vec<(u64, String, String)>,
}

pub fn parse(path: &Path) -> std::io::Result<Vcd> {
    let text = std::fs::read_to_string(path)?;
    let mut vars = Vec::new();
    let mut changes = Vec::new();
    let mut time: u64 = 0;
    for line in text.lines() {
        let l = line.trim();
        if l.is_empty() {
            continue;
        }
        if l.starts_with("$var") {
            // $var <type> <width> <id> <name> [range] $end
            let t: Vec<&str> = l.split_whitespace().collect();
            if t.len() >= 6 {
                let width: usize = t[2].parse().unwrap_or(1);
                vars.push((t[3].to_string(), t[4].to_string(), width));
            }
        } else if let Some(rest) = l.strip_prefix('#') {
            if let Ok(tt) = rest.parse::<u64>() {
                time = tt;
            }
        } else if l.starts_with('b') {
            // b<bits> <id>
            let t: Vec<&str> = l.split_whitespace().collect();
            if t.len() == 2 {
                changes.push((time, t[1].to_string(), t[0][1..].to_string()));
            }
        } else {
            let c = l.chars().next().unwrap();
            if matches!(c, '0' | '1' | 'x' | 'z') && l.len() >= 2 {
                changes.push((time, l[1..].to_string(), c.to_string()));
            }
        }
    }
    Ok(Vcd { vars, changes })
}

fn bin_to_hex(b: &str, width: usize) -> String {
    if b.chars().any(|c| c != '0' && c != '1') {
        return b.to_string(); // 含 x/z 原样
    }
    let val = u64::from_str_radix(b, 2).unwrap_or(0);
    let hexw = width.div_ceil(4);
    format!("{:0w$X}", val, w = hexw)
}

/// 紧凑渲染指定信号：1-bit 用 ▁▔，多 bit 列出依次取的（去重）十六进制值。
pub fn render(vcd: &Vcd, signals: &[String]) -> String {
    let mut o = String::new();
    for sig in signals {
        let Some((id, _, width)) = vcd.vars.iter().find(|(_, n, _)| n == sig) else {
            continue;
        };
        let mut series: Vec<(u64, &String)> = vcd
            .changes
            .iter()
            .filter(|(_, cid, _)| cid == id)
            .map(|(t, _, v)| (*t, v))
            .collect();
        series.sort_by_key(|(t, _)| *t);

        if *width == 1 {
            let wave: String = series
                .iter()
                .map(|(_, v)| match v.as_str() {
                    "1" => '▔',
                    "0" => '▁',
                    _ => '┄',
                })
                .collect();
            o.push_str(&format!("  {:<10} {}\n", sig, wave));
        } else {
            let mut vals = Vec::new();
            let mut prev = String::new();
            for (_, v) in &series {
                let hex = bin_to_hex(v, *width);
                if hex != prev {
                    vals.push(hex.clone());
                    prev = hex;
                }
            }
            o.push_str(&format!("  {:<10} {}\n", sig, vals.join(" ")));
        }
    }
    o
}
