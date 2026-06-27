// 设备文件搬进硬件 —— Verilog。
// ① 常量设备：组合逻辑，读端口恒 1，写无副作用（不改状态）。
// ② RingSum：深度 2 环形寄存器时序状态机。
//    write(we) 时：wdata==233 → 清零；否则移位 r1<=r0; r0<=wdata。
//    read（组合）：rdata = r0 + r1（已给好）。
// 与软件 FileLike 完全同构，输出逐位一致。你填 const_read 与 always 块。
`default_nettype none
`timescale 1ns/1ps
module abstract_dev (
    input  wire        clk,
    input  wire        rst,
    input  wire        we,         // 写使能
    input  wire [15:0] wdata,      // 写入数据
    output wire [15:0] rdata,      // RingSum read = r0 + r1
    output wire [15:0] const_read  // 常量设备 read 恒 1
);
    reg [15:0] r0, r1;

    // 常量设备：read 端口恒 1（写吞掉、不改状态）
    // TODO: 改成 16'd1（像 /dev/zero 的 read 恒得固定值）。
    assign const_read = 16'd0; // ← 占位（读出 0 会判 FILELIKE_FAIL）

    // RingSum read：组合求和（已给好）
    assign rdata = r0 + r1;

    // RingSum 时序状态机
    always @(posedge clk) begin
        if (rst) begin
            r0 <= 16'd0;
            r1 <= 16'd0;
        end else if (we) begin
            // TODO: wdata==16'd233 → r0<=0;r1<=0（清零）；
            //       否则移位：r1<=r0; r0<=wdata。
            //   // TODO[a] 移位写法（如上）  // ELSE[b] head 指针写法
            // 占位：状态不更新（读 wdata 仅为表明这里该用到它）→ 环和恒 0 判 RING_FAIL。
            r0 <= r0 & (wdata | ~wdata) & 16'd0; // ← 占位：删掉，写出正确逻辑
            r1 <= r1 & 16'd0;
        end
    end
endmodule
`default_nettype wire
