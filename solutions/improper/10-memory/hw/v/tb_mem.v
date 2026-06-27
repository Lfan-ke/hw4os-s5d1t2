// 10-memory Verilog testbench（给定，学生勿改）。
// 扫描整条平坦大内存的线性地址，逐位比对译码输出 {cs_fast, cs_slow, local_off}。
// 全对打印 DECODE_PASS 与 ALL_PASS；任一错打印含 FAIL 的行。
`default_nettype none
`timescale 1ns/1ps
module tb_mem;
    localparam [7:0] FAST_SIZE = 8'd8;
    localparam [7:0] SLOW_SIZE = 8'd16;
    localparam integer TOTAL = 24; // FAST_SIZE + SLOW_SIZE

    reg  [7:0] la;
    wire       cs_fast;
    wire       cs_slow;
    wire [7:0] local_off;

    integer errors;
    integer i;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk; // 仅为产生波形

    mem_decode #(.FAST_SIZE(FAST_SIZE)) dut (
        .la(la), .cs_fast(cs_fast), .cs_slow(cs_slow), .local_off(local_off)
    );

    // 参考模型：与软件 addr_route 同构
    task chk(input [7:0] a);
        reg        e_fast, e_slow;
        reg [7:0]  e_off;
        begin
            la = a;
            #10;
            if (a < FAST_SIZE) begin
                e_fast = 1'b1; e_slow = 1'b0; e_off = a;
            end else begin
                e_fast = 1'b0; e_slow = 1'b1; e_off = a - FAST_SIZE;
            end
            if (cs_fast !== e_fast || cs_slow !== e_slow || local_off !== e_off) begin
                $display("FAIL la=%0d exp{f=%b s=%b off=%0d} got{f=%b s=%b off=%0d}",
                         a, e_fast, e_slow, e_off, cs_fast, cs_slow, local_off);
                errors = errors + 1;
            end
        end
    endtask

    initial begin
        $dumpfile("tb_mem.vcd");
        $dumpvars(0, tb_mem);
        errors = 0;

        for (i = 0; i < TOTAL; i = i + 1)
            chk(i[7:0]);

        if (errors == 0) $display("DECODE_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
