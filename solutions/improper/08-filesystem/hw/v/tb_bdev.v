// RAM 块设备 testbench（给定，勿改）。
// 向全部 64 个块写入确定性图案，再逐块读回比对 —— 写进去读出来逐字节相等。
`default_nettype none
`timescale 1ns/1ps
module tb_bdev;
    reg         clk;
    reg         we;
    reg  [5:0]  addr;
    reg  [31:0] wdata;
    wire [31:0] rdata;

    integer i;
    integer errors;

    ram_bdev #(.AW(6), .DW(32)) dut (
        .clk(clk), .we(we), .addr(addr), .wdata(wdata), .rdata(rdata)
    );

    initial clk = 1'b0;
    always #5 clk = ~clk;

    // 每个块的确定性特征图案
    function [31:0] pat(input [5:0] a);
        pat = (({26'b0, a}) * 32'h0101_0101) ^ 32'h0000_BEEF;
    endfunction

    initial begin
        $dumpfile("tb_bdev.vcd");
        $dumpvars(0, tb_bdev);
        errors = 0;
        we     = 1'b0;
        addr   = 6'd0;
        wdata  = 32'd0;

        // 写阶段：逐块写入
        for (i = 0; i < 64; i = i + 1) begin
            @(negedge clk);
            we    = 1'b1;
            addr  = i[5:0];
            wdata = pat(i[5:0]);
        end
        @(negedge clk);
        we = 1'b0;

        // 读阶段：逐块读回比对（同步读，posedge 后采样）
        for (i = 0; i < 64; i = i + 1) begin
            @(negedge clk);
            addr = i[5:0];
            @(posedge clk);
            #1;
            if (rdata !== pat(i[5:0])) begin
                $display("BDEV_FAIL addr=%0d exp=0x%08x got=0x%08x", i, pat(i[5:0]), rdata);
                errors = errors + 1;
            end
        end

        if (errors == 0) begin
            $display("BDEV_PASS");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
