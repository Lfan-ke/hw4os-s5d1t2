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
    // 辅助分：必修达成后，超出 require 的每条通过路径 +1（独立于总分）
    let bonus = if required_done {
        passed.saturating_sub(ex.require as usize)
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
