// 16.1 裸机 MMIO —— testbench（给定，学生勿改）。
// tb 扮演“参考驱动”：probe(读 ID)→使能→轮询 ready→突发写/读 DATA，逐步校验设备响应。
`default_nettype none
`timescale 1ns/1ps
module tb_mmio;
    reg         clk, rst, sel, we;
    reg  [3:0]  addr;
    reg  [31:0] wdata;
    wire [31:0] rdata;

    integer errors;

    mmio_dev dut (
        .clk(clk), .rst(rst), .sel(sel), .we(we),
        .addr(addr), .wdata(wdata), .rdata(rdata)
    );

    localparam [31:0] MAGIC = 32'h426C_6E6B;
    localparam [3:0]  R_ID = 4'h0, R_CTRL = 4'h4, R_STATUS = 4'h8, R_DATA = 4'hC;

    initial clk = 1'b0;
    always #5 clk = ~clk;

    // 一次寄存器写：拉高 sel/we 一个时钟
    task wr(input [3:0] a, input [31:0] d);
        begin
            @(negedge clk); sel = 1'b1; we = 1'b1; addr = a; wdata = d;
            @(negedge clk); sel = 1'b0; we = 1'b0; wdata = 32'h0;
        end
    endtask

    // 组合读：摆好 addr，稳定后取 rdata
    task rd(input [3:0] a, output [31:0] d);
        begin
            addr = a; #1; d = rdata;
        end
    endtask

    reg [31:0] v;
    integer poll;
    initial begin
        $dumpfile("tb_mmio.vcd");
        $dumpvars(0, tb_mmio);
        errors = 0;
        sel = 1'b0; we = 1'b0; addr = 4'h0; wdata = 32'h0;

        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // ① probe：读 ID 比对 magic
        rd(R_ID, v);
        if (v !== MAGIC) begin $display("DEV_FAIL ID=0x%08x exp=0x%08x", v, MAGIC); errors = errors + 1; end

        // ② 未使能时写 DATA 应被忽略
        wr(R_DATA, 32'h0000_0099);

        // ③ 使能并轮询 STATUS.ready
        wr(R_CTRL, 32'h0000_0001);
        poll = 0;
        rd(R_STATUS, v);
        while ((v[0] !== 1'b1) && (poll < 16)) begin
            @(negedge clk); rd(R_STATUS, v); poll = poll + 1;
        end
        if (v[0] !== 1'b1) begin $display("DEV_FAIL ready 未置位 STATUS=0x%08x", v); errors = errors + 1; end

        // ④ 突发写/读 DATA，逐字节回显校验
        wr(R_DATA, 32'h0000_00AA);
        rd(R_DATA, v);
        if (v[7:0] !== 8'hAA) begin $display("DEV_FAIL DATA 回显=0x%02x exp=0xAA", v[7:0]); errors = errors + 1; end
        wr(R_DATA, 32'h0000_0055);
        rd(R_DATA, v);
        if (v[7:0] !== 8'h55) begin $display("DEV_FAIL DATA 回显=0x%02x exp=0x55", v[7:0]); errors = errors + 1; end

        if (errors == 0) begin
            $display("DEV_PASS");
            $display("MMIO_PASS");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
