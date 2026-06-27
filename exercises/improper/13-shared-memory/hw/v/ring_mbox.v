// 设备↔OS 的 MMIO 共享环 mailbox —— 硬件路径（Verilog）。
// 定深=4 的 ring：设备侧 doorbell(push) 抬 tail，OS 侧 MMIO 读(pop) 抬 head。
// count 法判空/满；与软件 Ring 完全同构。你只需填 always 块。
`default_nettype none
`timescale 1ns/1ps
module ring_mbox (
    input  wire        clk,
    input  wire        rst,        // 同步复位（高有效）
    input  wire        push_en,
    input  wire [31:0] push_data,
    input  wire        pop_en,
    output wire [31:0] pop_data,
    output wire        avail,      // count>0
    output wire        full,       // count==CAP
    output wire [3:0]  count
);
    // 深度 CAP=4：head/tail 取 2 bit 自然按 %4 环绕；cnt 取 3 bit（0..4）。
    reg [31:0] mem [0:3];
    reg [1:0]  head, tail;
    reg [2:0]  cnt;

    wire do_push = push_en & ~full;     // 满则拒绝（已给）
    wire do_pop  = pop_en  & avail;     // 空则忽略（已给）

    // TODO: 时序更新 head/tail/cnt 与写入 mem（见 README §3）：
    //   if (do_push) begin mem[tail] <= push_data; tail <= tail + 2'd1; end
    //   if (do_pop)  begin head <= head + 2'd1; end
    //   case ({do_push, do_pop}) 2'b10: cnt<=cnt+1; 2'b01: cnt<=cnt-1; default: cnt<=cnt; endcase
    always @(posedge clk) begin
        if (rst) begin
            head <= 2'd0;
            tail <= 2'd0;
            cnt  <= 3'd0;
        end else begin
            // ← 占位：什么都不更新（读 do_push/do_pop 仅为消除未用告警）。
            head <= head;
            tail <= tail;
            cnt  <= cnt & {2'b11, ~(do_push & do_pop & 1'b0)};
        end
    end

    // 组合输出（已给）。
    assign avail    = (cnt != 3'd0);
    assign full     = (cnt == 3'd4);
    assign count    = {1'b0, cnt};
    assign pop_data = mem[head];
endmodule
`default_nettype wire
