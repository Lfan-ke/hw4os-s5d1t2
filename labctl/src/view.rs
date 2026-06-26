//! 解析每实验的 view.toml（可视化声明）：拓扑节点/连线、数据流场景、接口、波形信号。
//! 驱动 TUI 伴侣面板与（后续）diagram 导出。

use anyhow::{Context, Result};
use serde::Deserialize;
use std::collections::{HashMap, HashSet};
use std::path::Path;

#[derive(Debug, Deserialize, Default)]
pub struct View {
    #[serde(default, rename = "node")]
    pub nodes: Vec<Node>,
    #[serde(default, rename = "edge")]
    pub edges: Vec<Edge>,
    #[serde(default, rename = "flow")]
    pub flows: Vec<Flow>,
    #[serde(default, rename = "iface")]
    pub ifaces: Vec<Iface>,
    #[serde(default)]
    pub wave: Wave,
}

#[derive(Debug, Deserialize)]
pub struct Node {
    pub id: String,
    pub label: String,
}

#[derive(Debug, Deserialize)]
pub struct Edge {
    pub from: String,
    pub to: String,
}

#[derive(Debug, Deserialize)]
pub struct Flow {
    pub name: String,
    #[serde(default)]
    pub path: Vec<String>,
    #[serde(default)]
    pub note: String,
}

#[derive(Debug, Deserialize)]
pub struct Iface {
    pub name: String,
    #[serde(default)]
    pub bits: String,
    #[serde(default)]
    pub note: String,
}

#[derive(Debug, Deserialize, Default)]
pub struct Wave {
    #[serde(default)]
    pub signals: Vec<String>,
}

impl View {
    /// 若实验目录下有 view.toml 则解析，否则 None。
    pub fn load(dir: &Path) -> Result<Option<View>> {
        let p = dir.join("view.toml");
        if !p.is_file() {
            return Ok(None);
        }
        let raw = std::fs::read_to_string(&p).with_context(|| format!("读取 {} 失败", p.display()))?;
        let v: View = toml::from_str(&raw).with_context(|| format!("解析 {} 失败", p.display()))?;
        Ok(Some(v))
    }

    fn label_of(&self, id: &str) -> String {
        self.nodes
            .iter()
            .find(|n| n.id == id)
            .map(|n| n.label.clone())
            .unwrap_or_else(|| id.to_string())
    }

    /// 把 nodes 按 edges 串成主链（入度 0 起点沿首条出边走），返回 label 序列。
    pub fn main_chain(&self) -> Vec<String> {
        let mut next: HashMap<&str, &str> = HashMap::new();
        let mut indeg: HashMap<&str, usize> = self.nodes.iter().map(|n| (n.id.as_str(), 0)).collect();
        for e in &self.edges {
            next.entry(e.from.as_str()).or_insert(e.to.as_str());
            *indeg.entry(e.to.as_str()).or_insert(0) += 1;
        }
        let start = self
            .nodes
            .iter()
            .map(|n| n.id.as_str())
            .find(|id| indeg.get(id).copied().unwrap_or(0) == 0);

        let mut chain = Vec::new();
        let mut seen = HashSet::new();
        let mut cur = start;
        while let Some(id) = cur {
            if !seen.insert(id) {
                break;
            }
            chain.push(self.label_of(id));
            cur = next.get(id).copied();
        }
        chain
    }

    /// 不在主链上的“侧输入”连线（如 mode→op），返回 (from_label, to_label)。
    pub fn side_edges(&self) -> Vec<(String, String)> {
        // 主链上的节点 id 集合
        let mut next: HashMap<&str, &str> = HashMap::new();
        let mut indeg: HashMap<&str, usize> = self.nodes.iter().map(|n| (n.id.as_str(), 0)).collect();
        for e in &self.edges {
            next.entry(e.from.as_str()).or_insert(e.to.as_str());
            *indeg.entry(e.to.as_str()).or_insert(0) += 1;
        }
        let start = self
            .nodes
            .iter()
            .map(|n| n.id.as_str())
            .find(|id| indeg.get(id).copied().unwrap_or(0) == 0);
        let mut on_chain = HashSet::new();
        let mut cur = start;
        while let Some(id) = cur {
            if !on_chain.insert(id) {
                break;
            }
            cur = next.get(id).copied();
        }
        // 主链首条出边集合（用于排除）
        let chain_edges: HashSet<(&str, &str)> = on_chain
            .iter()
            .filter_map(|id| next.get(id).map(|t| (*id, *t)))
            .collect();

        self.edges
            .iter()
            .filter(|e| !chain_edges.contains(&(e.from.as_str(), e.to.as_str())))
            .map(|e| (self.label_of(&e.from), self.label_of(&e.to)))
            .collect()
    }
}
