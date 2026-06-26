# 自己设计课设

注意：

如果设计的题目与硬件相关 需要硬件辅助 则给出对应硬件代码来给学生参考

硬件代码要求：

- 可 iverilog、yosys、verilater、vivado 等工具识别 + 0 warning

- 可有对应的make命令便捷查看波形 + 硬件结构

- 使用 verilog + bluespec sv 双语法（类似于一些lab的c/rust双语法参考）

如果设计的课设有必要，则会给出对应的TODO给学生来补充（虽然我们偏向于是OSlab）

我们的实验仓库要求：

- 每次提交记得 commit -s -S gpg 密码 360123 不允许出现任何的agent的合作署名 以及 agent 的名字

- 先不push

- 对标rustlings的lab体验，逐题递进，我已经给出每个实验的顺序以及详细设计，你需要使用最优雅的代码实现它

- 其中，验证的时候，偶尔会出现硬件或者软件其中之一实现即可的，硬件部分因为是bsv/v两种，你给出两种分别的判题 + 任意一个过就算过

- 特殊情况我会直接标注！我们的课设暂时分为两大块：
 - `不正经的lab`用于给学生建立软硬件感知和功能的心智模型
 - `正经的lab`用于给学生一步步规范化建立一个常态内核，类似于rcore（ls tgln/stage1/2026s-tg-rcore-Lfan-ke/）

- 如果涉及到ISA，我们以RV64GC为基准

- 部分实验你需要按照你觉得正确/更加适合理解和入门的顺序进行调整，内容也是，可适当简化模型，（这里的简化是简化学生的作业/理解成本，而不是你我这设计课程的老师对lab功能性完整性实现）

# 不正经的课设

来对各个基础功能有感性认知，建立基本的心智模型

## 硬件管理
OS起源
给定一个硬件，类似于路由器三个口，你需要在软件或者硬件实现对应功能，来实现三个口的Tag/网络包过滤机制，了解：软件可做的，硬件也可做！
附加：硬件成本和软件时间要求，要综合考虑软硬件成本来完成最后指定功能
只打趣：一个大型web项目或者桌面软件可以完全可以解耦为数字逻辑，流片出一个ASIC，插电后就提供此Web项目的CRUD。但是为什么不这样子做？为什么？理解你对软件是硬件的配置文件以及软硬件逻辑一致，软件可做的硬件也可以做，硬件可以做的，软件在通用处理器上也可以模拟的看法，可联系：Qemu等等虚拟机监视器/模拟器，部分模拟物理效果的游戏，OpenCL等等。

实验设计：

deepseek建议，可参考：

```
明白了！那这个实验就清晰很多了——**本质上是一个VLAN Tag的插入/剥离/过滤的简单硬件/软件实现**，不涉及任何路由查表、MAC学习等复杂逻辑。

那么我把框架**大幅简化**，聚焦到核心需求：

---

## 一、实验核心：Tag的三种操作

| 端口模式 | 收包（Ingress） | 发包（Egress） |
| :--- | :--- | :--- |
| **Access** | 如果有Tag → **剥离**；如果无Tag → **插入PVID** | **剥离**所有Tag（发出去的是纯净帧） |
| **Trunk** | 必须带Tag，检查VID是否在允许列表 → **通过/丢弃** | **保留**Tag（原样发送） |
| **Hybrid** | 同Trunk（检查允许列表） | 根据配置决定**保留/剥离**Tag |

就这么简单，没有MAC表，没有转发决策。

---

## 二、硬件最小实现（Verilog空壳）

### 核心模块接口
module vlan_port (
    input  wire        clk, rst_n,
    // MMIO配置
    input  wire [31:0] cfg_addr,
    input  wire [31:0] cfg_wdata,
    input  wire        cfg_wen,
    output reg  [31:0] cfg_rdata,
    // 数据输入（从Testbench来）
    input  wire [31:0] rx_data,
    input  wire        rx_valid,
    input  wire        rx_last,
    output reg         rx_ready,
    // 数据输出（发给Testbench）
    output reg  [31:0] tx_data,
    output reg         tx_valid,
    output reg         tx_last,
    input  wire        tx_ready
);

### 寄存器定义（MMIO地址映射）
| 地址 | 寄存器 | 位域 | 说明 |
| :--- | :--- | :--- | :--- |
| `0x00` | `PORT_CTRL` | `[1:0]` = 模式（0:A, 1:T, 2:H）<br>`[15:8]` = PVID | 端口配置 |
| `0x04` | `VLAN_BITMAP_LO` | `[31:0]` | 允许的VLAN ID 0~31 |
| `0x08` | `VLAN_BITMAP_HI` | `[31:0]` | 允许的VLAN ID 32~63 |
| `0x0C` | `UNTAG_BITMAP_LO` | `[31:0]` | Hybrid口发包时剥离Tag的VID列表 |
| `0x10` | `UNTAG_BITMAP_HI` | `[31:0]` | Hybrid口发包时剥离Tag的VID列表 |
| `0x20` | `TX_DATA` | `[31:0]` | 软件写数据到发送FIFO |
| `0x24` | `TX_LEN` | `[15:0]` | 发送数据长度（字节） |
| `0x30` | `RX_DATA` | `[31:0]` | 软件读接收FIFO |
| `0x34` | `RX_LEN` | `[15:0]` | 接收数据长度（字节） |
| `0x38` | `RX_AVAIL` | `[0]` | 是否有待读取的帧 |

### ★ 核心TODO：学生需要填空的三个状态机

#### TODO 1: Ingress状态机（收包处理）
// 伪代码，学生需要转成Verilog
always @(posedge clk) begin
    if (rx_valid) begin
        // 检测帧头是否有VLAN Tag (EtherType == 0x8100)
        if (rx_data[31:16] == 16'h8100) begin
            vlan_id = rx_data[15:4];  // 提取VID
            has_tag = 1;
        end else begin
            has_tag = 0;
        end

        case (port_mode)
            MODE_ACCESS: begin
                if (has_tag) begin
                    // ★ 剥离Tag：跳过这4字节，只存后面数据
                    store_to_rx_fifo(rx_data, skip=1);  // 丢弃当前的Tag字
                end else begin
                    // ★ 插入PVID：先存EtherType=0x8100，再存PVID
                    store_to_rx_fifo(32'h8100_0000 | (pvid << 4));
                    store_to_rx_fifo(rx_data);  // 存原来的数据
                end
            end
            MODE_TRUNK: begin
                if (!has_tag) begin
                    // ★ 丢弃不带Tag的帧
                    drop_frame();
                end else if (!vlan_allowed(vlan_id)) begin
                    // ★ 丢弃不在允许列表的帧
                    drop_frame();
                end else begin
                    // ★ 原样通过（保留Tag）
                    store_to_rx_fifo(rx_data);
                end
            end
            MODE_HYBRID: begin
                // ★ 同Trunk的Ingress逻辑
            end
        endcase
    end
end

#### TODO 2: Egress状态机（发包处理）
// 软件通过MMIO写TX_FIFO触发发送
always @(posedge clk) begin
    if (tx_fifo_not_empty) begin
        data = pop_tx_fifo();

        case (port_mode)
            MODE_ACCESS: begin
                // ★ 剥离所有Tag（跳过0x8100开头的4字节）
                if (data[31:16] == 16'h8100) begin
                    // 丢掉这个字，读下一个
                    data = pop_tx_fifo();
                end
                send_to_phy(data);
            end
            MODE_TRUNK: begin
                // ★ 原样发送，不做任何修改
                send_to_phy(data);
            end
            MODE_HYBRID: begin
                // ★ 检查UNTAG_BITMAP，如果该VID需要剥离Tag则剥离，否则保留
                if (data[31:16] == 16'h8100) begin
                    vlan_id = data[15:4];
                    if (untag_bitmap[vlan_id]) begin
                        // 剥离Tag
                        data = pop_tx_fifo();
                    end
                    // 否则保留
                end
                send_to_phy(data);
            end
        endcase
    end
end

#### TODO 3: VLAN允许检查函数
function vlan_allowed(input [11:0] vid);
    if (vid < 32)
        vlan_allowed = vlan_bitmap_lo[vid];
    else
        vlan_allowed = vlan_bitmap_hi[vid-32];
endfunction

---

## 三、软件驱动（两种实现路径）

### 路径A：硬件做Tag处理（驱动极简）
// port_api.c
void port_config(int mode, int pvid) {
    WRITE_MMIO(PORT_CTRL, (mode & 0x3) | (pvid << 8));
}

void port_send(uint8_t *data, int len) {
    for (int i = 0; i < len; i += 4) {
        WRITE_MMIO(TX_DATA, *(uint32_t*)(data+i));
    }
    WRITE_MMIO(TX_LEN, len);
}

int port_recv(uint8_t *buf) {
    while (!(READ_MMIO(RX_AVAIL) & 1)) {}  // 轮询
    int len = READ_MMIO(RX_LEN);
    for (int i = 0; i < len; i += 4) {
        *(uint32_t*)(buf+i) = READ_MMIO(RX_DATA);
    }
    return len;
}

### 路径B：硬件当GPIO，软件做Tag处理
软件直接读写`TX_DATA`/`RX_DATA`，自己解析/构造帧头。

// 软件实现Tag插入
void port_send(uint8_t *data, int len, int vlan_id) {
    uint8_t frame[1024];
    int idx = 0;

    // 先拷贝MAC地址 (前12字节)
    memcpy(frame, data, 12); idx += 12;

    // 根据模式决定是否插入Tag
    if (port_mode == MODE_TRUNK || port_mode == MODE_HYBRID) {
        frame[idx++] = 0x81;
        frame[idx++] = 0x00;
        frame[idx++] = (vlan_id >> 8) & 0xFF;
        frame[idx++] = vlan_id & 0xFF;
    }

    // 拷贝剩余数据
    memcpy(frame+idx, data+12, len-12);

    // 通过GPIO发送
    for (int i = 0; i < len+4; i += 4) {
        WRITE_MMIO(TX_DATA, *(uint32_t*)(frame+i));
    }
}

// 软件实现Tag剥离
int port_recv(uint8_t *buf) {
    int len = READ_MMIO(RX_LEN);
    uint8_t raw[1024];
    for (int i = 0; i < len; i += 4) {
        *(uint32_t*)(raw+i) = READ_MMIO(RX_DATA);
    }

    // 检查是否有Tag
    if (raw[12] == 0x81 && raw[13] == 0x00) {
        // 剥离Tag
        memcpy(buf, raw, 12);           // MAC地址
        memcpy(buf+12, raw+16, len-16); // 跳过Tag
        return len - 4;
    } else {
        memcpy(buf, raw, len);
        return len;
    }
}

---

## 四、Testbench自动测试序列

// tb_top.v/bsv
task test_access_mode();
    // 1. 配置端口为Access, PVID=5
    mmio_write(PORT_CTRL, MODE_ACCESS | (5 << 8));

    // 2. 发送不带Tag的帧 (期望硬件/软件插入PVID=5)
    send_frame({dst_mac, src_mac, 0x0800, payload}); // EtherType=0x0800 (IP)
    // 软件断言: 收到的帧第13-14字节是0x8100，第15-16字节是0x0005

    // 3. 发送带Tag=10的帧 (期望硬件/软件剥离Tag)
    send_frame({dst_mac, src_mac, 0x8100, 0x000A, payload});
    // 软件断言: 收到的帧没有0x8100
endtask

task test_trunk_mode();
    // 1. 配置为Trunk，允许VLAN 10和20
    mmio_write(PORT_CTRL, MODE_TRUNK);
    mmio_write(VLAN_BITMAP_LO, (1<<10) | (1<<20));

    // 2. 发送带VLAN=10的帧 (期望通过)
    send_frame({dst_mac, src_mac, 0x8100, 0x000A, payload});
    // 软件断言: RX_LEN > 0

    // 3. 发送带VLAN=30的帧 (期望丢弃)
    send_frame({dst_mac, src_mac, 0x8100, 0x001E, payload});
    // 软件断言: RX_LEN == 0 (或者超时)

    // 4. 发送不带Tag的帧 (期望丢弃)
    send_frame({dst_mac, src_mac, 0x0800, payload});
    // 软件断言: RX_LEN == 0
endtask

task test_hybrid_mode();
    // 1. 配置为Hybrid，允许VLAN 10和20，其中VLAN 10发包时剥离Tag
    mmio_write(PORT_CTRL, MODE_HYBRID);
    mmio_write(VLAN_BITMAP_LO, (1<<10) | (1<<20));
    mmio_write(UNTAG_BITMAP_LO, 1<<10);

    // 2. 发送带VLAN=10的帧 (期望通过)
    // 3. 软件发包时，VLAN=10的帧应该被剥离Tag再发出去
    // 4. 软件发包时，VLAN=20的帧应该保留Tag
endtask

---

## 五、软件断言层（统一验证）

// test_port.c/rs(我们支持rust/c作为lab使用的语言 同样的 rust/c 过一个即可算过)
void test_access() {
    port_config(MODE_ACCESS, 5);

    // Testbench自动发了一个带Tag的帧，现在读回来
    len = port_recv(buf);
    // 断言: 应该没有Tag
    assert(!(buf[12] == 0x81 && buf[13] == 0x00));

    // Testbench又发了一个不带Tag的帧
    len = port_recv(buf);
    // 断言: 应该插入了PVID=5
    assert(buf[12] == 0x81 && buf[13] == 0x00);
    assert(((buf[14]<<8) | buf[15]) == 5);

    printf("Access test passed!\n");
}

---

1. **选择了哪种路径**（硬件做/软件做/混合做）
2. **核心代码**（Verilog/BSV状态机 或 C/Rust处理函数）

---

这样整个实验就**聚焦在Tag的插入/剥离/过滤**这个核心逻辑上，没有多余的路由、查表、MAC学习，非常干净。学生可以清晰对比硬件状态机和软件if-else实现同一个逻辑的差异，这正是你想要的**软硬件协同设计入门实验**！本来就是为了对比同样的硬件接线以及算法逻辑，软件实现和硬件实现的对应区别以及成本核算等（成本，之后的思考题里面出一个软件成本低的情况和硬件成本低的情况辅助思考！）

设置我们可以进一步抽象简化，即：将原本复杂的Tag处理变成单字节或者双字节的处理，之后路由器的完整tag处理作为引申思考
```

...

## 进程管理

实验1：某个特殊标识进程永远不启动，某个永远排在某个之后，剩下的随机
实验2：实现优先级调度，使用优先队列代替当前的Vec，优先级高的先出队接受调度...

就简单的使用软件来简易模拟进程调度的情况即可

## 编译链接

你需要将对应程序编译后的结果链接进入某个区段，保证执行完A直接执行B，以此类推
分为：ELF以及纯二进制两种，理解其区别
扩展：对应的内存布局以及区段的概念等等

需要用户自己手写link脚本，划分区块，将不同的数据编到对应的区块

## 线程管理

了解进程线程关系，了解标识执行流的究竟是哪一部分，了解有线程模型的OS，进程只是线程的资源容器
了解：硬件线程的超线程技术，是如何通过只扩展了少量寄存器，即使得一个物理核心可以当两个物理核心跑...

即：给出一个进程调度的模型，其中进程结构体PCB就是调度切换的核心，然后，给出提示：修改为线程调度模型

其中提示有：你可以在外独立一个结构体储存进程共享的资源：内存，文件描述符表，信号量等，而PCB -> TCB仅仅需要存储线程的上下文，比如寄存器值，栈指针等 外加指向 PCB 的指针即可

实验一：了解 CSR + GPRs （riscv） 就是一个上下文！

实验二：给出进程示例，实现线程调度模型

给出一个扩展思考：原本一个硬件核心只视为一个硬件线程，因为只有一套上下文，但是超线程技术可以将一个物理核心当两个物理程跑，那么，这里的核心有几套CSR+GPRs+取值译码等模块作为线程的上下文？此时，如果线程共享的是进程的fd等，那么硬件线程共享的是？（TLB，缓存等...你自己按照正确的酌情补充完善！）以及为什么共享这些（硬件的物理线程思考题+软件的进程线程思考题分别作答）可以加速？节省了哪些开销？

## 纤程相关

理解为什么纤程（有栈协程）被称为绿色线程，用户态切换开销小和内核态切换优化了哪些空间（ld st切换开销，用户态切换不需要等待tls加载切换）
以及，单线程场景下，为什么多异步无让出/阻塞任务会退化为批处理系统/单线程顺序执行（即：每个任务执行完，才会执行下一个任务，没有并行执行的情况，因为没有阻塞也就没有让出，除非主动yield）

一个/套rust/c的纤程实验，实现 + 体验常用类库里面的用法

## 协程相关

理解为什么协程（无栈，poll）被称为绿色线程，用户态切换开销小和内核态切换优化了哪些空间，对比有栈协程有什么优缺点

一个/套rust/c的协程实验，实现 + 体验常用类库里面的用法

协程的俩，我们还需要详细商议（上面俩）

## 进程通信

涉及原子操作，以及锁的一些知识
进程A控制进程B，在B结束后才进行某些操作（B会置某些位）

## 文件管理

给定块设备（v/bsv实现的ram），实现在此块设备内部的简易文件存储和区块规划
外加涉及：rootfs、分区等等的概念

即：我们设计的实验要分为几个小实验：

- 简易的在给定地址使用裸指针（访问MMIO）写一些数据，外加之后需要读出来

- 简易的规定小端序，以EMM233为开头 EMM666为结尾，中间是一个数据段，其中，EMM233开头后的三个字节存的为key，这样子存一堆东西，你要找出对应key内部存的所有数据，（思考key可视为较短的文件名字）

- 简易的目录/文件概念：参考 easyfs 的 inode 里面的 目录、目录项、文件、文件名等抽象来将块设备初始化为0 + 初始化文件目录 + 进行简易文件操作

- 简易的分区概念：之后设计实验，或者你自行看难度设计

- 简易的刷入概念：新建一个小于块设备大小的img，里面存了一些数据，且你要挂载后将特定目录下的某些文件自己使用linux命令移入后，你要刷img入对应的块设备，然后进行正确读取（考察linux挂载（块/img），初始化文件系统的img，刷入块设备，读取块设备，linux对应命令）

- 完整的一个大实验

## 抽象文件
你需要实现一个FileLike文件，要求：读永远是1，写永远是0
你需要实现一个简单硬件模拟，作用是两个寄存器，最后求和，寄存器是ringbuf，初始都是0，要求：写入数字到两个寄存器，最后读取到对应的和，写入233则将寄存器重置为0
比如：写入666 → 读出 0+666 = 666
写入111 → 读出 666+111=777
写入222 → 读出111+222=333 （666被覆盖，因为只有两个寄存器）
写入233 → 读出 0 此时两个写入寄存器均被0覆盖，所以 0+0=0
理解硬件的部分“设备文件”抽象，附加：
刘慈欣的人力计算机
MC的生电，石英晶振
当你发现某个物理规律且稳定出现的时候，比如：你做了事情A，就一定发生事件B，做了事情B就会反馈实践A，那么是否可以作为0和1来组建一个“计算机”？

思考：人类对规律在现实生活的应用：人类对自然规律的运用，水力驱动，气压驱动，电力驱动，人力驱动。
磁力控制，线缆控制，铜线光纤、石英晶振...

思考题一般情况下作为单独的题目，用户在对应文件里面输入一些字符作为回答即可视为通过。

## 内存管理
给定两个块设备，其中一个容量小，速度快，一个容量大，但是速度慢，且持久化，两个场景：
- 需要小内存即可运行，但是要求速度（小的作为内存，大的作为存储）
- 需要大内存即可运行，但是同时要求持久化（小的作为内存，建立swap空间，即：大的一部分需要作为内存进行同步的内存管理）
- 需要大内存即可运行，不要求速度，不要求持久化（两个都作为内存...）
外加涉及：swap知识

## 堆栈相关
给定两个块设备作为内存，你需要选定堆栈，且初始化对应的SP/Allocator（堆栈独立）
给定一个块设备作为内存，你需要在程序加载的之后，划分为堆栈，且保证互不侵犯（堆栈不独立，得手动控制向下生长的和向上生长的俩）

以及rust的一个global allocator的使用示例（因为软件侧我们是c/rust两条可能的路线，所以，C因为没有rust方便，所以我们自己提供一个global全局指针以及allocator，主要考察的是用户会把allocator注册到global，只不过c得手动控制，rust注册了就完事...）


## 地址空间
小实验1：一个程序实际上只需要一点点空间，但是他访问的范围大到离谱（即：访问一下0x0访问一下0x100000再访问/读写一下0x100000000000 本质只读写了几个字节 但是如果物理映射，则读写范围过大），物理机无法支持如此大范围的访问，所以，需要“偷梁换柱”，来屏蔽“抽查”，避免指针越界错误。（虚拟地址映射，科普：此时软件模拟的是硬件MMU的工作，但是在NoMMU的机子上，OS可通过软件垫片来“虚拟MMU”）（理解：用户软件眼里内存无限大...但是实际只是把用到的vpn映射到了实际的ppn...）
小实验2：你需要完成A地址空间直接映射，来作为公共的拍卖行来交换信息，其他的都是虚拟的
小实验3：如果多个拍卖行玩家，设置多个公共槽位来进行交换（SMP）
小实验4：两级地址映射，入门地址映射
小实验5：SV39，以及提一嘴反置页表（作为思考题）

## 共享内存
进程之间的共享空间（mmap）
设备和OS的共享空间，说到底，OS运行在的CPU也是物理设备的一种，所以也就是硬件共享MMIO通信区间还是软件内部共享部分空间...

得设计实验来让用户建立心智模型

## 三态转换
给定一个硬件，ABC三种特权级，层层限制可用操作，使得学生了解“所谓特权级硬件角度就是连了几根线而已”...
切换ABC三种特权级仅仅是将某些位置位，且开启某些功能也是，，，，其中硬件侧要把这部分逻辑使用笨代码易读的展示！辅助学生理解，仅仅简单示例即可，但是设计实验让学生参与，应该说，每个##下都要设计对应实验达成让学生亲手动手去理解某方面的东西建立心智模型即可

## 引导入门
某个硬件需要在启动的时候软件将某些位设置为某些相关内容才能继续使用某些功能，直接启动没有设置会报错，所以，你需要写一个前置程序，将某些寄存器设置为对应的位，且链接到软件程序之前（又是一道软件实验题，主要是软件将某些硬件寄存器置位，通过模拟的MMIO总线，其中，默认硬件仿真环境启动没有置为且执行错乱，直到置位）


## 驱动入门
给定内核，给定硬件，但是要求是，设计一款可插拔的驱动框架/系统，方便新的硬件过来可以和OS无缝集成，且，更加便捷的给用户态使用
外加附加：裸机驱动，设备树映射解析调用对应驱动（驱动表进行字符匹配来启用对应驱动）

设计几个实验来依次入门：裸机MMIO驱动手工手艺人，，，dts编写+编译fdt/dtb，以及对应的解析 + 驱动匹配，driver derve，衍生到现代平台总线的简化实验


## 板级入门
给定硬件A，OS B，硬件地址C，完成正确的配置
给定硬件B，OS B，硬件地址D，确保正确的配置
给定硬件A，OS升级到C，邮件地址C ...
思考，BSP在做什么，如果bootloader + 设备树不变，OS升级...理解设备树和OS驱动协同适配多种硬件平台和OS版本（驱动和...解耦）

实验要求编写简易的BSP，完成硬件初始化，以及设备树解析

## 系统调用

从MCU中断向量表到MPU系统调用

一个是单片机的中断向量表的实验，一个合理的组织化，GNU标准规范化的syscall

通过几个实验来过渡理解这块的内容


# 正经点的课设
相当于步入正轨，但是前置要求是完成rcore课设，在rcore的基础上，我们一步步引导逐步添加功能完成一个简单的OS，包括引导、驱动、系统调用等

从一个批处理系统开始

## 传统引导

从SBI开始直到内核启动，粗略介绍SBI
了解SBI做了什么，了解QEMU使用SBI作为前置来引导启动内核

之前了解到需要对硬件进行初始化，SBI就是对内核和基础外设初始化的一个通用固件

...

这里可以插入库核心的认知实验、外核的认知实验，以及实现一个简易的rtos以及其运行时/用户sdk/包，使得用户认识rtos，以及高级封装版本的loop start

以及实现一个类似于rust embassy异步运行时，视为rtos的aos衍生，辅助理解多道分时系统等的嵌入式变体等

...

之后的实验还没想好

最后是先完成一个单核心的OS

后面改造，完成后续实验课题，完成一个多核心的OS，有多槽多跳板，分析多槽的功能，为什么槽位要在直接映射区域来交换信息，而不是虚拟地址空间来交换信息

以及IPC相关的，异步通信机制

之后完成一个完整支持SMP的，使用RWLOCK（而非spin）锁的SMP OS

之后衍生支持大小核的AMP

之后衍生到H扩展 虚拟化，简易type1 vmm

以及一个tcg小实验，来告诉用户type2 vmm 部分是虚拟机 不同架构比如其他架构的qemu是模拟器（区分type1 2 1.5 模拟器），让用户自己匹配规则解析生成对应的指令，完成mini tcg，就简易的几条指令即可...

之后扩展到微内核、宏内核（基于之前smp os进行衍生+改造）的区分 + 简易的驱动系统实验收尾


如果实验有不妥的地方 可以一起协商！

正经的实验要覆盖：

简易sbi实现，以及之后到了后续章节我们除了必须的FID EID，我们可能还需要实现timer等SBI扩展作为实验的一部分（作为boot实验）

简易的调度系统（类似于tg-rcore，独立，抽离，我们实验需要实现多种调度系统，异步调度，队列调度，优先级的优先队列调度）

简易的文件系统，easyfs，ramfs，以及后面我们支持ext2，以及一些状态文件，比如 prof/** 下的 等，后续驱动我们也会有字符设备文件等等

主要的系统调用，就是gnu规定的常见的几个syscall，满足后续实验/测试用例要求即可，比如fork exec open 等，以及我们需要讲到win/unix/linux/mac不同的api，比如：pthread，epoll，io-uring，mac里面的poll实现等当作用户的实验内容

以及hal层，支持loongarch和riscv64的hal的设计与实现 - 简易实验，后续我们基于rv，所以这里是点到为止，参考polyhal但是简化为一些具有特点的实验辅助理解即可

以及libc，让用户支持rust的stdlib，或者c的libc，比如musl，glibc，newlibc，llvm等，这里采取简化原则，实现部分实验程序需要要的接口即可，可采取多个途径，一个途径过即可通过，但是也可以全过...

以及简易的net实验（参考xv6），以及简易的驱动实验（裸机，dd，平台总线）

以及编写简易的用户程序（基于自己的OS，自己的libc（部分，但是够用）写基础的一些算法 + 简易模板引擎 + 简易 TUI 框架（仅仅是一个MD组件，读取特定的MD格式，输出ANSI转义字符美化后的文本到终端...，甚至是丐版组件...））

以及基于qemu的虚拟GPU的简易GUI程序，以及GUI库的封装，以及解析html/css进行简易的GUI转译绘制的实验...
