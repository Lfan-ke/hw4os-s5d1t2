// 设备↔OS 的 MMIO 共享环 mailbox —— 硬件路径（Verilog 参考解）。
// 定深=4 的 ring：设备侧 doorbell(push) 抬 tail，OS 侧 MMIO 读(pop) 抬 head。
// count 法判空/满；与软件 Ring 完全同构。
`default_nettype none
`timescale 1ns/1ps
module ring_mbox (
    input  wire        clk,
    input  wire        rst,        // 同步复位（高有效）
    input  wire        push_en,    // 设备写一拍
    input  wire [31:0] push_data,
    input  wire        pop_en,     // OS 读一拍
    output wire [31:0] pop_data,   // 当前队首（组合读）
    output wire        avail,      // count>0
    output wire        full,       // count==CAP
    output wire [3:0]  count
);
    // 深度 CAP=4：head/tail 取 2 bit 自然按 %4 环绕；cnt 取 3 bit（0..4）。
    reg [31:0] mem [0:3];
    reg [1:0]  head, tail;
    reg [2:0]  cnt;

    wire do_push = push_en & ~full;     // 满则拒绝
    wire do_pop  = pop_en  & avail;     // 空则忽略

    // 学生填：时序更新 head/tail/cnt 与写入 mem。
    always @(posedge clk) begin
        if (rst) begin
            head <= 2'd0;
            tail <= 2'd0;
            cnt  <= 3'd0;
        end else begin
            if (do_push) begin
                mem[tail] <= push_data;
                tail      <= tail + 2'd1;   // 2bit 自然环绕
            end
            if (do_pop) begin
                head <= head + 2'd1;
            end
            case ({do_push, do_pop})
                2'b10:   cnt <= cnt + 3'd1; // 只入
                2'b01:   cnt <= cnt - 3'd1; // 只出
                default: cnt <= cnt;        // 不变 / 同进同出
            endcase
        end
    end

    // 组合输出（已给）。
    assign avail    = (cnt != 3'd0);
    assign full     = (cnt == 3'd4);
    assign count    = {1'b0, cnt};
    assign pop_data = mem[head];
endmodule
`default_nettype wire
