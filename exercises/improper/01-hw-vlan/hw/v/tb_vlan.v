// 01-hw-vlan Verilog testbench（给定，勿改）。
// 驱动与软件相同的测试向量，逐位比对 out_pkt，打印 *_PASS / FAIL。
`default_nettype none
`timescale 1ns/1ps
module tb_vlan;
    reg  [1:0]  mode;
    reg  [5:0]  pvid;
    reg  [63:0] allow;
    reg  [63:0] untag;
    reg  [31:0] in_pkt;
    wire [31:0] out_pkt;

    integer errors;
    integer gerr;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk;   // 仅为产生波形

    vlan_proc dut (
        .mode(mode), .pvid(pvid), .allow(allow), .untag(untag),
        .in_pkt(in_pkt), .out_pkt(out_pkt)
    );

    localparam [31:0] VALID   = 32'h8000_0000;
    localparam [31:0] HAS_TAG = 32'h4000_0000;
    localparam [31:0] DROP    = 32'h2000_0000;
    localparam [31:0] DIR     = 32'h1000_0000;

    function [31:0] mk(input dir, input tag, input [5:0] vid, input [15:0] pl);
        mk = VALID
           | (dir ? DIR : 32'b0)
           | (tag ? HAS_TAG : 32'b0)
           | ({26'b0, vid} << 16)
           | {16'b0, pl};
    endfunction

    task chk(input [31:0] inp, input [31:0] exp);
        begin
            in_pkt = inp;
            #10;
            if (out_pkt !== exp) begin
                $display("FAIL in=0x%08x exp=0x%08x got=0x%08x", inp, exp, out_pkt);
                gerr   = gerr + 1;
                errors = errors + 1;
            end
        end
    endtask

    initial begin
        $dumpfile("tb_vlan.vcd");
        $dumpvars(0, tb_vlan);
        errors = 0;

        mode = 2'd0; pvid = 6'd5; allow = 64'd0; untag = 64'd0; gerr = 0;
        chk(mk(1'b0, 1'b1, 6'd10, 16'h1234), 32'h8000_1234);
        chk(mk(1'b0, 1'b0, 6'd0,  16'h1234), 32'hC005_1234);
        chk(mk(1'b1, 1'b1, 6'd10, 16'h1234), 32'h8000_1234);
        if (gerr == 0) $display("ACCESS_PASS");

        mode = 2'd1; pvid = 6'd0; allow = (64'd1 << 10) | (64'd1 << 20); untag = 64'd0; gerr = 0;
        chk(mk(1'b0, 1'b1, 6'd10, 16'hABCD), 32'hC00A_ABCD);
        chk(mk(1'b0, 1'b1, 6'd30, 16'h1111), 32'hA000_0000);
        chk(mk(1'b0, 1'b0, 6'd0,  16'h2222), 32'hA000_0000);
        chk(mk(1'b1, 1'b1, 6'd10, 16'hABCD), 32'hC00A_ABCD);
        if (gerr == 0) $display("TRUNK_PASS");

        mode = 2'd2; pvid = 6'd0; allow = (64'd1 << 10) | (64'd1 << 20); untag = (64'd1 << 10); gerr = 0;
        chk(mk(1'b0, 1'b1, 6'd20, 16'h0F0F), 32'hC014_0F0F);
        chk(mk(1'b0, 1'b1, 6'd30, 16'h3333), 32'hA000_0000);
        chk(mk(1'b1, 1'b1, 6'd10, 16'h0F0F), 32'h8000_0F0F);
        chk(mk(1'b1, 1'b1, 6'd20, 16'h0F0F), 32'hC014_0F0F);
        if (gerr == 0) $display("HYBRID_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
