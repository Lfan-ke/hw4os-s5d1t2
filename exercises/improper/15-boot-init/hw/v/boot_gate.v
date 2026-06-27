// 引导入门 · 启动握手门 —— 硬件路径（Verilog）。
// 纯组合译码：给定已锁存的配置（unlocked/clkdiv/en/data_raw），按 addr 译出 rdata。
// 与软件 mmio_read 完全同构：未就绪读 DATA 吐 0x0BADB007，就绪后给变换值。
// 你只需填 always 块。
`default_nettype none
`timescale 1ns/1ps
module boot_gate (
    input  wire        unlocked,  // UNLOCK==MAGIC 已写入
    input  wire [3:0]  clkdiv,    // 时钟分频
    input  wire        en,        // CTRL.EN
    input  wire [15:0] data_raw,  // 最近写入 DATA
    input  wire [2:0]  addr,      // 要读的寄存器
    output reg  [31:0] rdata
);
    localparam [2:0] A_STATUS = 3'd3;
    localparam [2:0] A_DATA   = 3'd4;

    localparam [31:0] ST_READY  = 32'h0000_0001;
    localparam [31:0] ST_LOCKED = 32'h0000_0002;
    localparam [31:0] ST_BADCLK = 32'h0000_0004;
    localparam [31:0] ST_NOTEN  = 32'h0000_0008;
    localparam [31:0] BADBOOT   = 32'h0BAD_B007;

    // 给定：合法 CLKDIV 与就绪条件（直接用）
    wire clkdiv_valid = (clkdiv != 4'd0);             // 1..15 合法，0 非法
    wire ready        = unlocked & en & clkdiv_valid; // 三者齐备才就绪

    wire [31:0] data_xform = {16'b0, data_raw ^ 16'hCAFE};

    always @(*) begin
        // TODO: 按 addr 译出 rdata（每条分支都给全部 32 位赋值，防 latch）：
        //   A_STATUS: 拼出状态字——ready?ST_READY · !unlocked?ST_LOCKED ·
        //             !clkdiv_valid?ST_BADCLK · !en?ST_NOTEN（按位或起来）
        //   A_DATA:   ready ? data_xform : BADBOOT
        //   default:  32'b0
        // 占位：读 addr 仅为给 @* 敏感信号；输出恒 0 → tb 判 LOCK_FAIL。
        rdata = {29'b0, addr} & 32'b0;
    end
endmodule
`default_nettype wire
