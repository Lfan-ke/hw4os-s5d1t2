// 板级地址译码器 —— 硬件路径（Verilog）。
// sel  = 地址落在 [BASE, BASE+SIZE) 窗口内（"设备在哪"的硬件真身）。
// rdata= 命中时设备应答 = MAGIC | offset；窗口外恒 0（不应答）。
// 与软件 bsp_probe / MMIO 总线软硬同构：参数 BASE 综合成板 A 或板 B 的 UART 基址。
// 你只需填 always 块；tb 勿改。
`default_nettype none
`timescale 1ns/1ps
module bsp_decode #(
    parameter [31:0] BASE = 32'h1000_0000,
    parameter [31:0] SIZE = 32'h0000_1000
) (
    input  wire [31:0] addr,
    output reg         sel,
    output reg  [31:0] rdata
);
    localparam [31:0] MAGIC = 32'hDEC0_0000; // 设备应答魔数（叠加窗口内偏移）

    always @(*) begin
        // TODO: 写出地址译码逻辑（每条分支都给 sel 与 rdata 全赋值，防 latch）：
        //   命中: (addr >= BASE) && (addr < BASE + SIZE)
        //     → sel = 1'b1; rdata = MAGIC | (addr - BASE);
        //   否则: sel = 1'b0; rdata = 32'b0;
        sel   = (addr & 32'b0) != 32'b0;  // ← 占位：读 addr 给 @* 敏感信号，恒 0 → 判 FAIL
        rdata = addr & 32'b0;             // ← 占位
    end
endmodule
`default_nettype wire
