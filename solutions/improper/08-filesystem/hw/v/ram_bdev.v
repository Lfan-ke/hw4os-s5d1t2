// RAM 块设备模型 —— 硬件路径（Verilog 参考解）。
// 一个单端口同步 RAM：每个地址 = 一个「块」，data = 块内容（简化为 1 字/块）。
// 软件 BlockDev::write_block/read_block 的硬件同构：写一拍、读一拍。
`default_nettype none
`timescale 1ns/1ps
module ram_bdev #(
    parameter integer AW = 6,
    parameter integer DW = 32
) (
    input  wire           clk,
    input  wire           we,
    input  wire [AW-1:0]  addr,
    input  wire [DW-1:0]  wdata,
    output reg  [DW-1:0]  rdata
);
    reg [DW-1:0] mem [0:(1<<AW)-1];

    always @(posedge clk) begin
        if (we) mem[addr] <= wdata; // 写使能下把 wdata 落到该块
        rdata <= mem[addr];         // 同步读：下一拍 rdata 出该块内容
    end
endmodule
`default_nettype wire
