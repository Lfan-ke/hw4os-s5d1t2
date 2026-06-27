// S1 硬件向量分发器（MCU 中断向量表模型）—— Verilog。
// 纯组合逻辑：按号在「表」里查地址，直接跳过去——中断 = 硬件驱动的间接跳转。
//   mode=1 向量化(vectored) → handler_pc = base + (cause<<2)
//   mode=0 直接(direct)     → handler_pc = base
//   accept = trap_req
// 你只需填 always 块。
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
    // cause*4 的 32 位零扩展（向量化间隔 4 字节）—— 已给好，直接用。
    wire [31:0] offset = {26'b0, cause, 2'b00};

    always @(*) begin
        // TODO: 用 mode/base/offset/trap_req 组合出 handler_pc 与 accept：
        //   if (mode) handler_pc = base + offset;  // vectored：查表得 base+4*cause
        //   else      handler_pc = base;           // direct：所有号共用一个入口
        //   accept = trap_req;
        handler_pc = (base ^ offset) & 32'b0; // ← 占位：读 base/offset(含 cause)，恒 0 → FAIL
        accept     = (mode | trap_req) & 1'b0; // ← 占位：读 mode/trap_req，恒 0
    end
endmodule
`default_nettype wire
