// 双上下文寄存器文件 —— 硬件路径（Verilog 参考解）。
// bank0/bank1 = 两套复制的 architectural state（影子寄存器组）；
// 读口 + 加法器 = 共享后端，由 active 一拍选中一套上下文。
// 这正是超线程：复制最少的寄存器、共享最贵的执行单元。
`default_nettype none
`timescale 1ns/1ps
module ctx_rf (
    input  wire        clk,
    input  wire        rst,
    input  wire        active,   // 当前虚拟 CPU = 哪套上下文 (0/1)
    input  wire        we,       // 写使能
    input  wire [1:0]  waddr,
    input  wire [31:0] wdata,
    input  wire [1:0]  raddr,
    input  wire [1:0]  saddr,    // 第二读口地址（喂给共享加法器）
    output wire [31:0] rdata,    // 当前上下文 raddr 寄存器
    output wire [31:0] sum       // 共享加法器：raddr 字 + saddr 字
);
    // 两套上下文 = 复制 architectural state
    reg [31:0] bank0 [0:3];
    reg [31:0] bank1 [0:3];

    integer i;
    always @(posedge clk) begin
        if (rst) begin
            for (i = 0; i < 4; i = i + 1) begin
                bank0[i] <= 32'd0;
                bank1[i] <= 32'd0;
            end
        end else if (we) begin
            if (active == 1'b0) bank0[waddr] <= wdata;
            else                bank1[waddr] <= wdata;
        end
    end

    // 两套上下文各自取出 raddr / saddr 两个字（数组读口）
    wire [31:0] b0r = bank0[raddr];
    wire [31:0] b0s = bank0[saddr];
    wire [31:0] b1r = bank1[raddr];
    wire [31:0] b1s = bank1[saddr];

    // 共享数据通路：active 一拍选当前上下文（一个读口 + 一个加法器，分时复用）
    assign rdata = (active == 1'b0) ? b0r : b1r;
    assign sum   = (active == 1'b0) ? (b0r + b0s) : (b1r + b1s);
endmodule
`default_nettype wire
