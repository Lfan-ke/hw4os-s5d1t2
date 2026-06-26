//! TUI 伴侣面板：把 view.toml 的拓扑/数据流/接口 + 变体判定状态渲染成一屏。
//! 用左边框样式（不画右边框），规避 CJK 等宽对齐问题，终端友好。

use crate::manifest::Exercise;
use crate::variant::VariantResult;
use crate::view::View;

fn push(o: &mut String, s: &str) {
    o.push_str("│ ");
    o.push_str(s);
    o.push('\n');
}

/// 渲染伴侣面板。status 为 None 时只显示结构，Some 时附变体判定。
pub fn render(ex: &Exercise, view: Option<&View>, status: Option<&[VariantResult]>) -> String {
    let mut o = String::new();
    o.push_str(&format!("╭─ labctl ▸ {} {}\n", ex.rel, "─".repeat(16)));
    push(&mut o, &ex.title);

    if let Some(v) = view {
        push(&mut o, "");
        push(&mut o, "拓扑 / 数据流");
        let chain = v.main_chain();
        if !chain.is_empty() {
            push(&mut o, &format!("  {}", chain.join("  ▶  ")));
        }
        for (f, t) in v.side_edges() {
            push(&mut o, &format!("     ▲ {}  →  {}", f, t));
        }

        if !v.flows.is_empty() {
            push(&mut o, "");
            push(&mut o, "数据流场景");
            for fl in &v.flows {
                let trail = if fl.path.is_empty() {
                    String::new()
                } else {
                    format!("[{}] ", fl.path.join("▸"))
                };
                push(&mut o, &format!("  • {:<20} {}{}", fl.name, trail, fl.note));
            }
        }

        if !v.ifaces.is_empty() {
            push(&mut o, "");
            push(&mut o, "接口");
            for i in &v.ifaces {
                push(&mut o, &format!("  {:<16}{:<10}{}", i.name, i.bits, i.note));
            }
        }

        if !v.wave.signals.is_empty() {
            push(&mut o, "");
            push(&mut o, &format!("波形信号  {}", v.wave.signals.join("  ")));
        }
    }

    push(&mut o, "");
    push(&mut o, "变体");
    match status {
        Some(rs) => {
            let line = rs
                .iter()
                .map(|r| format!("{} {}", r.status.symbol(), r.id))
                .collect::<Vec<_>>()
                .join("    ");
            push(&mut o, &format!("  {}", line));
        }
        None => {
            let ids = ex
                .variants
                .iter()
                .map(|v| v.id.clone())
                .collect::<Vec<_>>()
                .join("  ");
            push(&mut o, &format!("  {}", ids));
            push(&mut o, "  （运行 labctl run / watch 查看判定）");
        }
    }

    o.push_str(&format!("╰{}\n", "─".repeat(42)));
    o
}
