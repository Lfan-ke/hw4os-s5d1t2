//! 计分：M 选 N（必修）+ 辅助分（独立账本）。

use crate::manifest::Exercise;
use crate::variant::{Status, VariantResult};

pub struct ExerciseScore {
    pub rel: String,
    pub title: String,
    pub require: u32,
    pub weight: u32,
    pub results: Vec<VariantResult>,
    pub passed: usize,
    pub required_done: bool,
    pub bonus: usize,
}

/// 对一个实验的全部变体结果做计分。
pub fn score_exercise(ex: &Exercise, results: Vec<VariantResult>) -> ExerciseScore {
    let passed = results
        .iter()
        .filter(|r| r.status == Status::Pass)
        .count();
    let required_done = passed as u32 >= ex.require;
    // 辅助分按「轴」计：跨轴（软件 / 硬件 / essay）才是「多角度 / what-if」成就；
    // 同轴二选一（rust↔c、verilog↔bsv）不重复奖励。统计通过的**不同 axis** 数。
    let mut passing_axes: std::collections::HashSet<&str> = std::collections::HashSet::new();
    for r in &results {
        if r.status == Status::Pass {
            if let Some(v) = ex.variants.iter().find(|v| v.id == r.id) {
                passing_axes.insert(v.axis.as_str());
            }
        }
    }
    // 必修达成后，覆盖的轴每多一个 +1（首个轴满足 require、不计辅助分）。
    let bonus = if required_done {
        passing_axes.len().saturating_sub(ex.require as usize)
    } else {
        0
    };
    ExerciseScore {
        rel: ex.rel.clone(),
        title: ex.title.clone(),
        require: ex.require,
        weight: ex.weight,
        results,
        passed,
        required_done,
        bonus,
    }
}

/// 全课程汇总：必修总分 + 辅助分总分（两本独立的账）。
pub struct Board {
    pub items: Vec<ExerciseScore>,
}

impl Board {
    pub fn required_total(&self) -> u32 {
        self.items
            .iter()
            .filter(|s| s.required_done)
            .map(|s| s.weight)
            .sum()
    }
    pub fn required_max(&self) -> u32 {
        self.items.iter().map(|s| s.weight).sum()
    }
    pub fn bonus_total(&self) -> usize {
        self.items.iter().map(|s| s.bonus).sum()
    }
}
