// S1 向量分发器 Verilog testbench（给定，勿改）。
// 驱动与软件相同的向量，逐位比对 (handler_pc, accept)，打印 *_PASS / FAIL。
`default_nettype none
`timescale 1ns/1ps
module tb_vec;
    reg         mode;
    reg  [31:0] base;
    reg  [3:0]  cause;
    reg         trap_req;
    wire [31:0] handler_pc;
    wire        accept;

    integer errors;
    integer gerr;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk; // 仅产生波形

    vec_dispatch dut (
        .mode(mode), .base(base), .cause(cause), .trap_req(trap_req),
        .handler_pc(handler_pc), .accept(accept)
    );

    task chk(input m, input [31:0] b, input [3:0] c, input req,
             input [31:0] exp_pc, input exp_acc);
        begin
            mode = m; base = b; cause = c; trap_req = req;
            #10;
            if (handler_pc !== exp_pc || accept !== exp_acc) begin
                $display("FAIL mode=%0d cause=%0d exp=(0x%08x,%0d) got=(0x%08x,%0d)",
                         m, c, exp_pc, exp_acc, handler_pc, accept);
                gerr   = gerr + 1;
                errors = errors + 1;
            end
        end
    endtask

    localparam [31:0] BASE = 32'h8000_0000;

    initial begin
        $dumpfile("tb_vec.vcd");
        $dumpvars(0, tb_vec);
        errors = 0;

        // ── DIRECT：handler_pc 恒为 base，不随 cause 变 ──
        gerr = 0;
        chk(1'b0, BASE, 4'd0, 1'b1, BASE, 1'b1);
        chk(1'b0, BASE, 4'd3, 1'b1, BASE, 1'b1);
        chk(1'b0, BASE, 4'd8, 1'b1, BASE, 1'b1);
        if (gerr == 0) $display("DIRECT_PASS");

        // ── VECTORED：handler_pc = base + 4*cause（查表）──
        gerr = 0;
        chk(1'b1, BASE, 4'd0,  1'b1, BASE,             1'b1);
        chk(1'b1, BASE, 4'd1,  1'b1, BASE + 32'h4,     1'b1);
        chk(1'b1, BASE, 4'd8,  1'b1, BASE + 32'h20,    1'b1);
        chk(1'b1, BASE, 4'd15, 1'b1, BASE + 32'h3C,    1'b1);
        if (gerr == 0) $display("VECTORED_PASS");

        // ── DISPATCH：混合流；accept 跟随 trap_req ──
        gerr = 0;
        chk(1'b1, BASE, 4'd2, 1'b1, BASE + 32'h08, 1'b1);
        chk(1'b1, BASE, 4'd5, 1'b0, BASE + 32'h14, 1'b0); // 无请求 → accept=0
        chk(1'b0, BASE, 4'd9, 1'b1, BASE,          1'b1);
        if (gerr == 0) $display("DISPATCH_PASS");

        if (errors == 0) begin
            $display("S1_PASS");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
