// 16e · 中断聚合 + 多核仲裁 - testbench（给定，学生勿改）。
// tb 当参考驱动跑同一聚合场景：4 hart claim 竞争 → 唯一 winner → complete 重新武装 → IPI。
`default_nettype none
`timescale 1ns/1ps
module tb_intc_agg;
    reg         clk, rst;
    reg         irq_assert, claim_req, complete_req, ipi_send, ipi_clear;
    reg  [1:0]  claim_hart, complete_hart, ipi_target, ipi_clear_hart;
    reg  [31:0] complete_id;
    wire [31:0] claim_id;
    wire [3:0]  msip;
    wire [1:0]  winner;
    wire        won;

    integer errors, i, winners;

    intc_agg dut (
        .clk(clk), .rst(rst), .irq_assert(irq_assert),
        .claim_req(claim_req), .claim_hart(claim_hart), .claim_id(claim_id),
        .complete_req(complete_req), .complete_hart(complete_hart), .complete_id(complete_id),
        .ipi_send(ipi_send), .ipi_target(ipi_target),
        .ipi_clear(ipi_clear), .ipi_clear_hart(ipi_clear_hart),
        .msip(msip), .winner(winner), .won(won)
    );

    localparam [31:0] IRQ_ID = 32'd7;

    initial clk = 1'b0;
    always #5 clk = ~clk;

    // 一拍 claim：摆 hart + strobe，采样 claim_id（组合）。
    task do_claim(input [1:0] h, output [31:0] id);
        begin
            @(negedge clk); claim_req = 1'b1; claim_hart = h;
            #1; id = claim_id;
            @(negedge clk); claim_req = 1'b0;
        end
    endtask
    task do_complete(input [1:0] h);
        begin
            @(negedge clk); complete_req = 1'b1; complete_hart = h; complete_id = IRQ_ID;
            @(negedge clk); complete_req = 1'b0;
        end
    endtask
    task do_ipi(input [1:0] t);
        begin
            @(negedge clk); ipi_send = 1'b1; ipi_target = t;
            @(negedge clk); ipi_send = 1'b0;
        end
    endtask
    task do_clear(input [1:0] h);
        begin
            @(negedge clk); ipi_clear = 1'b1; ipi_clear_hart = h;
            @(negedge clk); ipi_clear = 1'b0;
        end
    endtask

    reg [31:0] id;
    initial begin
        $dumpfile("tb_intc_agg.vcd");
        $dumpvars(0, tb_intc_agg);
        errors = 0; winners = 0;
        irq_assert = 0; claim_req = 0; claim_hart = 0;
        complete_req = 0; complete_hart = 0; complete_id = 0;
        ipi_send = 0; ipi_target = 0; ipi_clear = 0; ipi_clear_hart = 0;
        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // 设备拉起 IRQ
        @(negedge clk); irq_assert = 1'b1; @(negedge clk); irq_assert = 1'b0;

        // hart0 先 claim → 应拿到 IRQ_ID
        do_claim(2'd0, id);
        if (id !== IRQ_ID) begin $display("ARB_FAIL hart0 claim=0x%08x", id); errors = errors + 1; end
        else winners = winners + 1;
        // hart1/2/3 再 claim → gateway in-flight，应读 0
        for (i = 1; i < 4; i = i + 1) begin
            do_claim(i[1:0], id);
            if (id === IRQ_ID) begin
                $display("ARB_FAIL hart%0d 也 claim 到 IRQ", i); winners = winners + 1; errors = errors + 1;
            end
        end
        if (winners !== 1 || won !== 1'b1 || winner !== 2'd0) begin
            $display("ARB_FAIL winners=%0d won=%b winner=%0d", winners, won, winner);
            errors = errors + 1;
        end

        // 漏 complete：再 claim 仍读 0（卡住）；complete 后重新武装能再 claim 到 IRQ_ID
        do_claim(2'd0, id);
        if (id !== 32'd0) begin $display("DEV_FAIL 未 complete 却能再 claim=0x%08x", id); errors = errors + 1; end
        do_complete(2'd0);
        do_claim(2'd0, id);
        if (id !== IRQ_ID) begin $display("DEV_FAIL complete 后未重新武装=0x%08x", id); errors = errors + 1; end
        do_complete(2'd0);

        // IPI：hart0 向 1/2/3 敲 MSIP；从核确认清位
        do_ipi(2'd1); do_ipi(2'd2); do_ipi(2'd3);
        if (msip[3:1] !== 3'b111 || msip[0] !== 1'b0) begin
            $display("IPI_FAIL msip=0x%01x", msip); errors = errors + 1;
        end
        do_clear(2'd1); do_clear(2'd2); do_clear(2'd3);
        if (msip !== 4'd0) begin $display("IPI_FAIL 清位后 msip=0x%01x", msip); errors = errors + 1; end

        if (errors == 0) begin
            $display("ARBITER_PASS 同一 IRQ%0d 仅 hart0 处理：3 个竞争者 claim 到 0", IRQ_ID);
            $display("DEV_PASS  gateway complete 后重新武装（漏 complete 则源永久卡住）");
            $display("IPI_PASS    hart0 敲 3 路 MSIP，从核确认清位");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
