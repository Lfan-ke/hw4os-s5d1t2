# S04 思考题（学生作答）

> 在每题下写出你的理解；非空且命中关键字即过。写完删掉最后一行的 TODO 标记。

## 1. 无栈协程「无」的是哪根栈？poll 的状态存在哪里？

（提示：对比纤程「每任务一根栈、换栈指针」；无栈协程把跨让出点存活的局部塞进
状态结构体 `struct Task` 的 `state/n/wake_tick`——「换的是状态号」。）

你的回答：

## 2. waker / reactor 解决了什么？为什么不在 executor 里 busy-poll？

（提示：busy-poll 空转烧 CPU；Pending 时登记唤醒条件、executor 就绪队列空就 wfi 睡，
靠时钟中断 + reactor 把到期任务重新入队，只重 poll 被唤醒者。）

你的回答：

## 3. 「无让出即退化为顺序执行」——给个例子，并说怎样才真交错。

（提示：poll 从不返回 Pending → executor 退化为顺序批处理 AABBCC；至少要一个让出点
返回 Pending 才能交错成 ABCABC。单核协作式是并发不并行。）

你的回答：

<!-- LABCTL_ESSAY_TODO: 请把上面三题作答完整后删除本行 -->
