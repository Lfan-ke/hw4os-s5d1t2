// 09-abstract-file Verilog testbench（给定，学生勿改）。
// 驱动与软件相同的序列，逐拍比对，打印 FILELIKE_PASS / RING_PASS / ALL_PASS。
`default_nettype none
`timescale 1ns/1ps
module tb_abstract;
    reg         clk, rst, we;
    reg  [15:0] wdata;
    wire [15:0] rdata, const_read;

    integer errors;
    integer ring_err;

    abstract_dev dut (
        .clk(clk), .rst(rst), .we(we), .wdata(wdata),
        .rdata(rdata), .const_read(const_read)
    );

    initial clk = 1'b0;
    always #5 clk = ~clk;

    // 写一个值：拉高 we 一个时钟周期再放下
    task wr(input [15:0] x);
        begin
            @(negedge clk); we = 1'b1; wdata = x;
            @(negedge clk); we = 1'b0; wdata = 16'd0;
        end
    endtask

    initial begin
        $dumpfile("tb_abstract.vcd");
        $dumpvars(0, tb_abstract);
        errors = 0; ring_err = 0;
        we = 1'b0; wdata = 16'd0;

        // 复位
        rst = 1'b1;
        @(negedge clk); @(negedge clk);
        rst = 1'b0;

        // ── 子实验 1：常量设备 read 恒 1 ──
        if (const_read !== 16'd1) begin
            $display("FILELIKE_FAIL const_read=%0d exp=1", const_read);
            errors = errors + 1;
        end else begin
            $display("FILELIKE_PASS");
        end

        // ── 子实验 2：RingSum 灌序列 666/111/222/233 → 读 666/777/333/0 ──
        wr(16'd666); if (rdata !== 16'd666) begin $display("RING_FAIL 666 -> %0d exp 666", rdata); ring_err = ring_err + 1; end
        wr(16'd111); if (rdata !== 16'd777) begin $display("RING_FAIL 111 -> %0d exp 777", rdata); ring_err = ring_err + 1; end
        wr(16'd222); if (rdata !== 16'd333) begin $display("RING_FAIL 222 -> %0d exp 333", rdata); ring_err = ring_err + 1; end
        wr(16'd233); if (rdata !== 16'd0)   begin $display("RING_FAIL 233 -> %0d exp 0",   rdata); ring_err = ring_err + 1; end
        if (ring_err == 0) $display("RING_PASS");
        errors = errors + ring_err;

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
