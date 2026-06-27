// 平坦大内存地址译码器 —— 硬件路径（Verilog 学生填空版）。
// 与软件 addr_route() 逐位等价：线性地址 la 落到 (cs_fast/cs_slow, local_off)。
//   la <  FAST_SIZE → cs_fast=1, cs_slow=0, off = la
//   la >= FAST_SIZE → cs_fast=0, cs_slow=1, off = la - FAST_SIZE
// 纯组合逻辑（一拍），无锁存。占位实现 0 warning 可编译，但 DECODE 判 FAIL。
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
        // TODO: 用 la 与 FAST_SIZE 比较，给三个输出赋值（与软件 addr_route 等价）：
        //   la < FAST_SIZE : cs_fast=1'b1; cs_slow=1'b0; local_off=la;
        //   否则           : cs_fast=1'b0; cs_slow=1'b1; local_off=la-FAST_SIZE;
        // 注意：每条分支都要给全部三个输出赋值（防 latch），用带位宽字面量。
        // ↓ 占位：读了 la 但恒置 0，DECODE 会判 FAIL。删掉并写出正确逻辑。
        cs_fast   = 1'b0;
        cs_slow   = 1'b0;
        local_off = la & 8'b0;
    end
endmodule
`default_nettype wire
