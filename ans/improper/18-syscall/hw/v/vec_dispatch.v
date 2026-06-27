// S1 硬件向量分发器（MCU 中断向量表模型）—— Verilog 参考解。
// 纯组合逻辑：按号在「表」里查地址，直接跳过去——这就是中断 = 硬件驱动的间接跳转。
//   mode=1 向量化(vectored) → handler_pc = base + (cause<<2)
//   mode=0 直接(direct)     → handler_pc = base
//   accept = trap_req（无陷入请求则不接受）
`default_nettype none
`timescale 1ns/1ps
module vec_dispatch (
    input  wire        mode,      // 0=direct 1=vectored
    input  wire [31:0] base,      // 向量表基址（= mtvec/stvec BASE）
    input  wire [3:0]  cause,     // 异常号（4-bit，压成 16 项）
    input  wire        trap_req,  // 是否有陷入请求
    output reg  [31:0] handler_pc,
    output reg         accept
);
    // cause*4 的 32 位零扩展（向量化间隔 4 字节，对应 RISC-V vectored 模式）
    wire [31:0] offset = {26'b0, cause, 2'b00};

    always @(*) begin
        if (mode) handler_pc = base + offset; // vectored：查表得 base+4*cause
        else      handler_pc = base;          // direct：所有号共用一个入口
        accept = trap_req;
    end
endmodule
`default_nettype wire
