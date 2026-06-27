// 平坦大内存地址译码器 —— 硬件路径（Verilog 参考解）。
// 与软件 addr_route() 逐位等价：线性地址 la 落到 (cs_fast/cs_slow, local_off)。
//   la <  FAST_SIZE → cs_fast=1, cs_slow=0, off = la
//   la >= FAST_SIZE → cs_fast=0, cs_slow=1, off = la - FAST_SIZE
// 纯组合逻辑（一拍），无锁存。
`default_nettype none
`timescale 1ns/1ps
module mem_decode #(
    parameter [7:0] FAST_SIZE = 8'd8
) (
    input  wire [7:0] la,        // 线性地址
    output reg        cs_fast,   // 选中 FAST 设备
    output reg        cs_slow,   // 选中 SLOW 设备
    output reg  [7:0] local_off  // 设备内偏移
);
    always @(*) begin
        if (la < FAST_SIZE) begin
            cs_fast   = 1'b1;
            cs_slow   = 1'b0;
            local_off = la;
        end else begin
            cs_fast   = 1'b0;
            cs_slow   = 1'b1;
            local_off = la - FAST_SIZE;
        end
    end
endmodule
`default_nettype wire
