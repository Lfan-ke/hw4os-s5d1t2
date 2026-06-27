// 进程通信原语 —— 硬件路径（Verilog）。
// 控制字: [31]BUSY [30]DONE [29]LOCK [28]START [15:0]RESULT
// 一个纯组合「IPC ALU」：按 op 把 (a,b) 映射到 64-bit 结果 y，与软件 8 个纯函数逐位同构。
//   op 0 BFINISH : y[31:0]=b_finish(a[15:0])
//   op 1 APOLL   : y[15:0]=result, y[32]=ready
//   op 2 TAS     : y[0]=new(=1), y[1]=got
//   op 3 UNLOCK  : y=0
//   op 4 DOWN    : y[7:0]=count', y[8]=ok
//   op 5 UP      : y[7:0]=count+1
//   op 6 ASTEP   : y[31:0]=ctrl', y[47:32]=post, y[48]=phase'   (b[0]=phase)
//   op 7 BSTEP   : y[31:0]=b_step(ctrl=a, job=b)
// 你只需填下面那个 always 块（helper wire 已给好，直接用）。
`default_nettype none
`timescale 1ns/1ps
module ipc_proc (
    input  wire [3:0]  op,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [63:0] y
);
    localparam [31:0] DONE  = 32'h4000_0000;
    localparam [31:0] START = 32'h1000_0000;

    wire _unused_b = &{1'b0, b[31:16]};

    localparam [3:0] OP_BFINISH = 4'd0;
    localparam [3:0] OP_APOLL   = 4'd1;
    localparam [3:0] OP_TAS     = 4'd2;
    localparam [3:0] OP_UNLOCK  = 4'd3;
    localparam [3:0] OP_DOWN    = 4'd4;
    localparam [3:0] OP_UP      = 4'd5;
    localparam [3:0] OP_ASTEP   = 4'd6;
    localparam [3:0] OP_BSTEP   = 4'd7;

    // 基本组合原语（已给好直接用）
    wire [31:0] bf     = DONE | {16'b0, a[15:0]};                       // b_finish
    wire        ready  = a[30];                                          // a_poll: DONE 位
    wire        got    = (a[0] == 1'b0);                                 // tas: 旧值为 0 即抢到
    wire [7:0]  cdec   = a[7:0] - 8'd1;                                  // down: count-1
    wire [7:0]  cinc   = a[7:0] + 8'd1;                                  // up:   count+1
    wire [15:0] post16 = {a[14:0], 1'b0};                                // a_step: result*2
    wire [31:0] bstep  = (a & START) != 32'b0 ? (DONE | {16'b0, b[15:0]}) : a;

    always @(*) begin
        // TODO: 用 case(op) + 上面的 helper wire 组合出 y（先 y=0 再按位写）。
        //   OP_BFINISH: y[31:0]=bf
        //   OP_APOLL:   y[15:0]=a[15:0]; y[32]=ready
        //   OP_TAS:     y[0]=1'b1; y[1]=got
        //   OP_UNLOCK:  y=0
        //   OP_DOWN:    y[7:0]=cdec; y[8]=~cdec[7]
        //   OP_UP:      y[7:0]=cinc
        //   OP_ASTEP:   if (b[0]==0) {y[31:0]=a|START; y[48]=1}
        //               else if (ready) {y[31:0]=a&~DONE; y[47:32]=post16; y[48]=0}
        //               else {y[31:0]=a; y[48]=1}
        //   OP_BSTEP:   y[31:0]=bstep
        y = {a, b} & 64'b0; // ← 占位：读 a,b 给 @* 敏感信号；删掉写出正确逻辑
    end
endmodule
`default_nettype wire
