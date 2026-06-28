// 16e · 中断聚合 + 多核仲裁 - 设备侧（Verilog，参考解）。
// PLIC 风格的聚合器：一个共享 IRQ 源对 N=4 个 hart-context。
//   claim  : 原子拿 gateway - 只有第一个 claim 的 hart 拿到 IRQ_ID，其余读 0（仲裁）。
//   complete: 还 gateway - inflight 归 0，源可再次投递（漏 complete 则永久卡住）。
//   IPI    : 写目标 hart 的 MSIP（软件中断），跨核协调。
`default_nettype none
`timescale 1ns/1ps
module intc_agg (
    input  wire        clk,
    input  wire        rst,
    input  wire        irq_assert,     // 设备拉起 IRQ（pending<=1）
    input  wire        claim_req,      // 某 hart 发起 claim（一拍一个）
    input  wire [1:0]  claim_hart,
    output wire [31:0] claim_id,       // claim 返回值：IRQ_ID 或 0
    input  wire        complete_req,   // complete 选通
    input  wire [1:0]  complete_hart,
    input  wire [31:0] complete_id,
    input  wire        ipi_send,       // 写目标 MSIP
    input  wire [1:0]  ipi_target,
    input  wire        ipi_clear,      // 从核确认：清自己的 MSIP
    input  wire [1:0]  ipi_clear_hart,
    output wire [3:0]  msip,
    output wire [1:0]  winner,
    output wire        won
);
    localparam [31:0] IRQ_ID = 32'd7;

    reg        pending_r, inflight_r, won_r;
    reg [1:0]  winner_r;
    reg [3:0]  msip_r;

    // ── 学生填①：仲裁授予 - 仅当有 pending 且 gateway 空闲时，本次 claim 拿到 IRQ_ID ──
    wire claim_ok = claim_req & pending_r & ~inflight_r;
    assign claim_id = claim_ok ? IRQ_ID : 32'd0;

    assign msip   = msip_r;
    assign winner = winner_r;
    assign won    = won_r;

    always @(posedge clk) begin
        if (rst) begin
            pending_r  <= 1'b0;
            inflight_r <= 1'b0;
            won_r      <= 1'b0;
            winner_r   <= 2'd0;
            msip_r     <= 4'd0;
        end else begin
            if (irq_assert) pending_r <= 1'b1;
            if (claim_ok) begin
                inflight_r <= 1'b1;       // gateway 进入 in-flight
                won_r      <= 1'b1;
                winner_r   <= claim_hart; // 记录唯一处理者
            end
            if (complete_req && complete_id == IRQ_ID && complete_hart == winner_r)
                inflight_r <= 1'b0;       // 还 gateway，重新武装
            // ── 学生填②：IPI - 置目标 hart 的 MSIP；从核确认时清位 ──
            if (ipi_send)  msip_r[ipi_target]     <= 1'b1;
            if (ipi_clear) msip_r[ipi_clear_hart] <= 1'b0;
        end
    end
endmodule
`default_nettype wire
