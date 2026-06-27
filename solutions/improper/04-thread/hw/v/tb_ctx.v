// 04-thread Verilog testbench（给定，学生勿改）。
// 写两套上下文 → 验证一拍换上下文(CTX_SWAP) / 共享加法器(SHARE) / 轮转(SCHED)。
// 输出与软件变体逐项一致：CTX_SWAP_PASS / SHARE_PASS / SCHED_PASS / ALL_PASS。
`default_nettype none
`timescale 1ns/1ps
module tb_ctx;
    reg         clk, rst, active, we;
    reg  [1:0]  waddr, raddr, saddr;
    reg  [31:0] wdata;
    wire [31:0] rdata, sum;
    integer errors, gerr;

    ctx_rf dut (
        .clk(clk), .rst(rst), .active(active), .we(we),
        .waddr(waddr), .wdata(wdata), .raddr(raddr), .saddr(saddr),
        .rdata(rdata), .sum(sum)
    );

    initial clk = 1'b0;
    always #5 clk = ~clk;

    // 同步写一个寄存器
    task wr(input a, input [1:0] addr, input [31:0] d);
        begin
            @(negedge clk);
            active = a; we = 1'b1; waddr = addr; wdata = d;
            @(negedge clk);
            we = 1'b0;
        end
    endtask

    // 读校验 rdata
    task rd_chk(input a, input [1:0] addr, input [31:0] exp);
        begin
            @(negedge clk);
            active = a; raddr = addr;
            #1;
            if (rdata !== exp) begin
                $display("FAIL ctx=%0d r[%0d]=0x%08x exp=0x%08x", a, addr, rdata, exp);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask

    // 共享加法器校验
    task sum_chk(input a, input [1:0] ra, input [1:0] sa, input [31:0] exp);
        begin
            @(negedge clk);
            active = a; raddr = ra; saddr = sa;
            #1;
            if (sum !== exp) begin
                $display("FAIL ctx=%0d sum=0x%08x exp=0x%08x", a, sum, exp);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask

    initial begin
        $dumpfile("tb_ctx.vcd");
        $dumpvars(0, tb_ctx);
        errors = 0;
        active = 1'b0; we = 1'b0; waddr = 2'd0; wdata = 32'd0; raddr = 2'd0; saddr = 2'd0;
        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // 写两套上下文（影子寄存器组）
        wr(1'b0, 2'd0, 32'd100); wr(1'b0, 2'd1, 32'd200); wr(1'b0, 2'd2, 32'd7);
        wr(1'b1, 2'd0, 32'd11);  wr(1'b1, 2'd1, 32'd22);  wr(1'b1, 2'd2, 32'd3);

        // CTX_SWAP：一拍换一套上下文，各自独立
        gerr = 0;
        rd_chk(1'b0, 2'd0, 32'd100); rd_chk(1'b0, 2'd1, 32'd200);
        rd_chk(1'b1, 2'd0, 32'd11);  rd_chk(1'b1, 2'd1, 32'd22);
        if (gerr == 0) $display("CTX_SWAP_PASS");

        // SHARE：同一个加法器被两套上下文分时复用
        gerr = 0;
        sum_chk(1'b0, 2'd0, 2'd1, 32'd300); // 100+200
        sum_chk(1'b1, 2'd0, 2'd1, 32'd33);  // 11+22
        if (gerr == 0) $display("SHARE_PASS");

        // SCHED：轮转——每拍切到另一套上下文读 reg2，交错出 7,3,7,3
        gerr = 0;
        rd_chk(1'b0, 2'd2, 32'd7); rd_chk(1'b1, 2'd2, 32'd3);
        rd_chk(1'b0, 2'd2, 32'd7); rd_chk(1'b1, 2'd2, 32'd3);
        if (gerr == 0) $display("SCHED_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
