# F3-exokernel 思考题（essay 变体）

> 在每题下作答；写完后删除文末含 LABCTL_ESSAY_TODO 的那一行即视为完成。

## 1. 「内核当裁判，不当独裁者」是什么意思？外核为什么这么设计、代价是什么？

（联系宏内核「强加 fs/vm/sched 抽象」vs 外核「只发裸资源 + 保护检查」；
本 demo 里 `exo_alloc` 只校验越界/重叠、从不关心块拿来干嘛。代价：抽象复杂度搬到应用、
资源回收难、生态阻力——但思想活在 Unikernel/DPDK/eBPF。）

TODO: 在此作答。

## 2. Aegis / Xok / jos 怎么把 OS 抽象交给 libOS？用本 demo 举例。

（联系 jos `sys_page_alloc` 给页不给 malloc/堆、fs 在用户态 libOS；
本 demo 里同一段块发给 libB 后，libB 用 `libb_place` 铺成 `[11,10]`，
libA 用 `liba_place` 铺成 `[0,1]`——同样裸块、不同抽象。机制在内核、策略在 libOS。）

TODO: 在此作答。

## 3. 外核与 SASOS（单地址空间 OS）有何本质区别？

（联系：外核=多地址空间(MMU 隔离多 libOS)+软硬结合保护+追求灵活；
SASOS=单地址空间(无切换无 TLB flush)+几乎不隔离+追求极致性能；
现代影响 Unikernel/Serverless/DPDK/eBPF。）

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
