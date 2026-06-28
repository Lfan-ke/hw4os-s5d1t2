# python/lang — CPython 3.14 语言级 carpet 测试交付

#764 列表项 `python <!-- 3.14 - pyπ - lts - venv -->` 的**语言级**(解释器 + 语法 +
标准库 + CLI)工业级地毯式测试。**不测**第三方/原生包(numpy/pyarrow 等是独立用例)。

## 是什么
22 个自包含模块(`python/`),逐 API/逐选项覆盖 CPython 3.14:
`t01` 全语法/运算符/推导/match/装饰器 · `t02` 数据模型全 dunder · `t03` OOP(MRO/super/
metaclass/abc/dataclass/enum/slots/property)· `t04` 每内置类型每方法 · `t05` 每内置函数 ·
`t06` 生成器+itertools · `t07` functools+operator · `t08` asyncio/协程/async-gen/TaskGroup ·
`t09` threading/queue/ThreadPoolExecutor · `t10` multiprocessing/ProcessPoolExecutor ·
`t11` inspect/ast/dis/gc/weakref/sys 自省反射 · `t12` re/struct/textwrap/unicodedata ·
`t13` json/csv/pickle/base64/hashlib/zlib/bz2/lzma · `t14` math/decimal/fractions/statistics/
random/collections/heapq · `t15` os/pathlib/io/tempfile/shutil/subprocess · `t16` datetime/
contextlib/signal · `t17` typing/argparse/logging/uuid/ipaddress · `t18` 3.14 特性(t-strings /
annotationlib / concurrent.interpreters / zstd / except 无括号 / finally 控制流 / from_number /
map(strict) / super 可拷贝 / memoryview 泛型 / 自由线程 / compression 命名空间,版本门控)·
`t19` 解释器 CLI(`python3 --help` 各选项 + `-m` 标准库 + stdin/脚本 + REPL 交互(`-i` 自动回显/
多行/错误恢复/exit) + 多入口 + `PYTHON*` env + 退出码)· `t20` 每个 `python3 -m` 标准库 CLI 工具 ·
`t21` 文档库索引每个模块 import 可达性 + 子包入口 · `test_lang` 综合 + venv。`run_all.py` 逐模块
跑(子进程 `-u` 无缓冲),全过才打印 `TEST PASSED`。
3.14-only 语法用 `exec()` 版本门控,旧解释器 graceful skip。

## 验证结果(qemu-10 单核 starry,据实)
| arch | 结果 |
|:--:|:--|
| aarch64 | √ 20/20 `TEST PASSED` + SUCCESS PATTERN |
| riscv64 | √ 20/20 `TEST PASSED` + SUCCESS PATTERN |
| loongarch64 | √ 20/20(需内核 #239 FDT-RAM + `-m 2048M`)|
| x86_64 | 经上游 CI(本地 app-qemu `-kernel` 无 PVH note,受 OVMF/UEFI 限制)|

## 跑法(维护者)
把本目录放到 tgoskits `apps/starry/python-lang/`,然后:
```
cargo xtask starry app qemu -t python-lang --arch aarch64   # / riscv64 / loongarch64 / x86_64
```
`prebuild.sh` 自动把 Alpine base 工作副本升级为 CPython 3.14(注入 `./apks/<arch>` 闭包)
并经 overlay 注入测试模块;成功判据 `success_regex = (?m)^TEST PASSED\s*$`。

## 依赖 / 来源
见 `SOURCES.md`。loongarch64 需内核 **#239**(FDT 检测真实 RAM,honor `-m`);3.14 闭包来自
Alpine edge(`./apks/`,LFS)。
