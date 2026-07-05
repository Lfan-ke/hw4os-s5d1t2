# hw4os-s5d1t2

操作系统课程设计交付仓库。本仓库以分支区分两类独立产出：面向 StarryOS 的 Linux 软件 / 编程语言 / 工具链地毯级测试交付（`osdst`），以及一套自行设计的 RISC-V 操作系统实验课程（`oslab`）。`main` 分支仅作索引与说明。

## 分支总览

| 分支 | 内容 | 说明 |
|:--|:--|:--|
| `main` | 仓库说明 | 本 README，索引各分支用途 |
| `osdst` | StarryOS 软件/语言/工具链交付 | 见下「osdst」 |
| `oslab` | 自设计 RISC-V 操作系统实验课程 | 见下「oslab」 |
| `osint` | 实习期间的软件与系统的适配与修复 | 见下「osint」 |

## osdst —— StarryOS 软件与工具链测试交付

在 StarryOS（基于 ArceOS 的 Linux 兼容宏内核）之上，对一批 Linux 软件、编程语言运行时与开发工具链进行四架构（x86_64 / aarch64 / riscv64 / loongarch64）测试与交付。每个子目录含测试用例与源码、四架构运行配置、资源获取脚本（`fetch-resources.sh`）与来源说明（`SOURCES.md`）、以及运行与验收指引。

具体覆盖项、构建与运行步骤、各项状态见 `osdst` 分支的 `README.md` 及各子目录文档。

## oslab —— 自设计 RISC-V 操作系统实验课程

一套面向操作系统原理教学的 RISC-V 实验课程，采用 rustlings 式的渐进判题流程（`labctl` 取题、判题、提示、记分）。三条赛道：

| 赛道 | 性质 | 内容 |
|:--|:--|:--|
| improper | 心智模型 | 以最朴素的软硬件模型呈现各 OS 子系统的本质（块设备、页表、特权级等） |
| proper | 工程落地 | 在 QEMU 上实现 S 态内核，循 rcore ch1–8 节奏至多核 / SMP / 虚拟化 / 微内核 |
| forms | 架构概览 | 五类内核形态（宏 / 微 / 外 / 库 / 框）及其混合权衡 |

硬件相关题目附 Verilog 与 Bluespec SystemVerilog 双语法参考代码（可经 iverilog / yosys / verilator 识别，含波形查看命令）。详见 `oslab` 分支的 `README.md` 与 `DESIGN.md`。

## 许可

见 `LICENSE`。
