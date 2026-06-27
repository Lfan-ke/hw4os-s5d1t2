// 进程通信原语 —— 硬件路径（Verilog 参考解）。
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
`default_nettype none
`timescale 1ns/1ps
module ipc_proc (
    input  wire [3:0]  op,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [63:0] y
);
    // 控制字位: BUSY=1<<31, LOCK=1<<29 在本 ALU 不直接参与组合（仅文档），
    // 用到的是 DONE 与 START。
    localparam [31:0] DONE  = 32'h4000_0000;
    localparam [31:0] START = 32'h1000_0000;

    // b 仅用低 16 位(job)与第 0 位(phase)；显式吸收高位以保持 0-warning(含 verilator)。
    wire _unused_b = &{1'b0, b[31:16]};

    localparam [3:0] OP_BFINISH = 4'd0;
    localparam [3:0] OP_APOLL   = 4'd1;
    localparam [3:0] OP_TAS     = 4'd2;
    localparam [3:0] OP_UNLOCK  = 4'd3;
    localparam [3:0] OP_DOWN    = 4'd4;
    localparam [3:0] OP_UP      = 4'd5;
    localparam [3:0] OP_ASTEP   = 4'd6;
    localparam [3:0] OP_BSTEP   = 4'd7;

    // 基本组合原语（与软件辅助函数一一对应，已给好直接用）
    wire [31:0] bf     = DONE | {16'b0, a[15:0]};                       // b_finish
    wire        ready  = a[30];                                          // a_poll: DONE 位
    wire        got    = (a[0] == 1'b0);                                 // tas: 旧值为 0 即抢到
    wire [7:0]  cdec   = a[7:0] - 8'd1;                                  // down: count-1
    wire [7:0]  cinc   = a[7:0] + 8'd1;                                  // up:   count+1
    wire [15:0] post16 = {a[14:0], 1'b0};                                // a_step: result*2
    wire [31:0] bstep  = (a & START) != 32'b0 ? (DONE | {16'b0, b[15:0]}) : a;

    always @(*) begin
        y = 64'b0;
        case (op)
            OP_BFINISH: y[31:0] = bf;
            OP_APOLL:   begin y[15:0] = a[15:0]; y[32] = ready; end
            OP_TAS:     begin y[0] = 1'b1;       y[1]  = got;   end
            OP_UNLOCK:  y = 64'b0;
            OP_DOWN:    begin y[7:0] = cdec;      y[8]  = ~cdec[7]; end   // ok = (count'>=0)
            OP_UP:      y[7:0] = cinc;
            OP_ASTEP: begin
                if (b[0] == 1'b0) begin
                    y[31:0] = a | START;          // phase0: 按门铃
                    y[48]   = 1'b1;               // phase'=1
                end else if (ready) begin
                    y[31:0] = a & ~DONE;          // 见 DONE: 取数算后续、清 DONE
                    y[47:32] = post16;
                    y[48]    = 1'b0;              // phase'=0
                end else begin
                    y[31:0] = a;                  // B 没干完: 死等
                    y[48]   = 1'b1;
                end
            end
            OP_BSTEP: y[31:0] = bstep;
            default:  y = 64'b0;
        endcase
    end
endmodule
`default_nettype wire
