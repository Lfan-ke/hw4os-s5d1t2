// 16d · 核外中断 PLIC - testbench（给定，学生勿改）。
// tb 当参考驱动跑同一场景：配置 prio/enable/threshold → raise → ROUTE 仲裁 → DEV 字段 → CLAIM 抽干 + 重触发。
`default_nettype none
`timescale 1ns/1ps
module tb_plic;
    reg         clk, rst, sel, we;
    reg  [7:0]  addr;
    reg  [31:0] wdata;
    wire [31:0] rdata;
    wire [2:0]  best_id;
    wire [1:0]  best_prio;

    integer errors;

    plic dut (
        .clk(clk), .rst(rst), .sel(sel), .we(we), .addr(addr), .wdata(wdata),
        .rdata(rdata), .best_id(best_id), .best_prio(best_prio)
    );

    localparam [7:0] A_PRIO1 = 8'h00, A_PRIO2 = 8'h04, A_PRIO3 = 8'h08, A_PRIO4 = 8'h0C,
                     A_PEND  = 8'h10, A_ENA   = 8'h14, A_THRESH = 8'h18,
                     A_CLAIM = 8'h1C, A_RAISE = 8'h20;

    initial clk = 1'b0;
    always #5 clk = ~clk;

    task wr(input [7:0] a, input [31:0] d);
        begin
            @(negedge clk); sel = 1'b1; we = 1'b1; addr = a; wdata = d;
            @(negedge clk); sel = 1'b0; we = 1'b0; wdata = 32'h0;
        end
    endtask
    task rd(input [7:0] a, output [31:0] d);
        begin
            addr = a; #1; d = rdata;
        end
    endtask
    // claim：读 CLAIM（sel=1,we=0）跨一个时钟沿，捕获顶源并触发设备清 pending。
    task claim(output [2:0] id);
        begin
            @(negedge clk); sel = 1'b1; we = 1'b0; addr = A_CLAIM; #1; id = rdata[2:0];
            @(negedge clk); sel = 1'b0;
        end
    endtask

    reg [31:0] v;
    reg [2:0]  c1, c2, c3, c4;
    initial begin
        $dumpfile("tb_plic.vcd");
        $dumpvars(0, tb_plic);
        errors = 0;
        sel = 1'b0; we = 1'b0; addr = 8'h0; wdata = 32'h0;
        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // 配置：prio 1/2/3/3、使能 {1,2,3}、阈值 1
        wr(A_PRIO1, 32'd1); wr(A_PRIO2, 32'd2); wr(A_PRIO3, 32'd3); wr(A_PRIO4, 32'd3);
        wr(A_ENA,    32'h0000_000E);
        wr(A_THRESH, 32'd1);
        // raise 全部源 {1,2,3,4}
        wr(A_RAISE,  32'h0000_001E);

        // ROUTE：满 pending 下取最高优先级源 = 3（prio 3）
        if (best_id !== 3'd3 || best_prio !== 2'd3) begin
            $display("ROUTE_FAIL best_id=%0d best_prio=%0d", best_id, best_prio);
            errors = errors + 1;
        end

        // DEV：寄存器字段回读 + RO 写忽略
        rd(A_PEND, v);
        if (v !== 32'h0000_001E) begin $display("DEV_FAIL PENDING=0x%08x", v); errors = errors + 1; end
        rd(A_ENA, v);
        if (v !== 32'h0000_000E) begin $display("DEV_FAIL ENABLE=0x%08x", v); errors = errors + 1; end
        rd(A_PRIO3, v);
        if (v !== 32'd3) begin $display("DEV_FAIL PRIO3=0x%08x", v); errors = errors + 1; end
        rd(A_THRESH, v);
        if (v !== 32'd1) begin $display("DEV_FAIL THRESHOLD=0x%08x", v); errors = errors + 1; end
        wr(A_PEND, 32'hFFFF_FFFF); // RO：写应被忽略
        rd(A_PEND, v);
        if (v !== 32'h0000_001E) begin $display("DEV_FAIL PENDING_RO=0x%08x", v); errors = errors + 1; end

        // CLAIM：按优先级抽干 → 3,2,0；余 {1,4}(=0x12) 被阈值/使能挡住
        claim(c1); claim(c2); claim(c3);
        if (c1 !== 3'd3 || c2 !== 3'd2 || c3 !== 3'd0) begin
            $display("CLAIM_FAIL seq=%0d,%0d,%0d", c1, c2, c3); errors = errors + 1;
        end
        rd(A_PEND, v);
        if (v !== 32'h0000_0012) begin $display("CLAIM_FAIL pending=0x%08x", v); errors = errors + 1; end
        // complete(EOI) + 重新 raise 源3 → 可再 claim
        wr(A_CLAIM, 32'd3); wr(A_CLAIM, 32'd2);
        wr(A_RAISE, 32'h0000_0008);
        claim(c4);
        if (c4 !== 3'd3) begin $display("CLAIM_FAIL refire=%0d", c4); errors = errors + 1; end

        if (errors == 0) begin
            $display("ROUTE_PASS top=3 prio=3 (max-priority arbitration)");
            $display("DEV_PASS  register fields: prio/enable/threshold readback, RO ignored");
            $display("CLAIM_PASS seq=3,2,0 refire=3");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
