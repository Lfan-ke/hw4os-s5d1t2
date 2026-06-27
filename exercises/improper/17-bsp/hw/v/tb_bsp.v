// 17-bsp Verilog testbench（给定，勿改）。
// 两个译码器实例 = 板 A（BASE=0x1000_0000）与板 B（BASE=0x1002_0000）的 UART 窗口。
// 校验：设备恰在 BASE 窗口内应答、窗口外（含另一块板的基址）不应答。
`default_nettype none
`timescale 1ns/1ps
module tb_bsp;
    reg  [31:0] addr;
    wire        sel_a, sel_b;
    wire [31:0] rdata_a, rdata_b;

    integer errors, gerr;
    reg clk; initial clk = 1'b0; always #5 clk = ~clk; // 仅为产生波形

    bsp_decode #(.BASE(32'h1000_0000), .SIZE(32'h0000_1000)) dec_a (
        .addr(addr), .sel(sel_a), .rdata(rdata_a));
    bsp_decode #(.BASE(32'h1002_0000), .SIZE(32'h0000_1000)) dec_b (
        .addr(addr), .sel(sel_b), .rdata(rdata_b));

    localparam [31:0] MAGIC = 32'hDEC0_0000;

    task chk_a(input [31:0] a, input exp_sel, input [31:0] exp_rdata);
        begin
            addr = a; #1;
            if (sel_a !== exp_sel || (exp_sel && rdata_a !== exp_rdata)) begin
                $display("FAIL A addr=0x%08x sel=%b(exp %b) rdata=0x%08x(exp 0x%08x)",
                         a, sel_a, exp_sel, rdata_a, exp_rdata);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask
    task chk_b(input [31:0] a, input exp_sel, input [31:0] exp_rdata);
        begin
            addr = a; #1;
            if (sel_b !== exp_sel || (exp_sel && rdata_b !== exp_rdata)) begin
                $display("FAIL B addr=0x%08x sel=%b(exp %b) rdata=0x%08x(exp 0x%08x)",
                         a, sel_b, exp_sel, rdata_b, exp_rdata);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask

    initial begin
        $dumpfile("tb_bsp.vcd");
        $dumpvars(0, tb_bsp);
        errors = 0;

        // 板 A：窗口内应答、窗口外/板 B 基址不应答
        gerr = 0;
        chk_a(32'h1000_0000, 1'b1, MAGIC | 32'h0000_0000); // 窗口起点
        chk_a(32'h1000_0FFF, 1'b1, MAGIC | 32'h0000_0FFF); // 窗口末尾
        chk_a(32'h1000_1000, 1'b0, 32'h0000_0000);         // 刚出窗口
        chk_a(32'h0FFF_FFFF, 1'b0, 32'h0000_0000);         // 窗口下方
        chk_a(32'h1002_0000, 1'b0, 32'h0000_0000);         // 板 B 基址，A 不应答
        if (gerr == 0) $display("DECODE_A_PASS");

        // 板 B：窗口内应答、板 A 基址不应答
        gerr = 0;
        chk_b(32'h1002_0000, 1'b1, MAGIC | 32'h0000_0000);
        chk_b(32'h1002_0FFF, 1'b1, MAGIC | 32'h0000_0FFF);
        chk_b(32'h1002_1000, 1'b0, 32'h0000_0000);
        chk_b(32'h1000_0000, 1'b0, 32'h0000_0000);         // 板 A 基址，B 不应答
        if (gerr == 0) $display("DECODE_B_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
