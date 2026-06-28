// 16c · 核内中断 - testbench（给定，学生勿改）。
// tb 当『CPU/参考驱动』跑同一场景：装填 mtimecmp，逐 tick 走 mtime，采样 mtip 计 timer 触发；
// 写 msip 拉起软件中断、handler 清零。与软件四语逐位一致：timer 触发 3 次、软件中断 2 次。
`default_nettype none
`timescale 1ns/1ps
module tb_clint;
    localparam [63:0] PERIOD = 64'd5;
    localparam integer NTICK = 16;
    localparam integer EXP_TIMER = 3;
    localparam integer EXP_SOFT  = 2;
    localparam [4:0] A_MSIP = 5'h00, A_CMP = 5'h08;

    reg         clk, rst, tick, we;
    reg  [4:0]  waddr;
    reg  [63:0] wdata;
    wire [63:0] mtime_o;
    wire        mtip, msip;

    integer errors, fires, ipis, i;
    reg [63:0] cmp;

    clint dut (
        .clk(clk), .rst(rst), .tick(tick), .we(we), .waddr(waddr), .wdata(wdata),
        .mtime_o(mtime_o), .mtip(mtip), .msip(msip)
    );

    initial clk = 1'b0;
    always #5 clk = ~clk;

    task ipi;  // 一次软件中断：拉起 → 校验挂起 → handler 清零 → 校验已清
        begin
            @(negedge clk); we = 1'b1; waddr = A_MSIP; wdata = 64'd1;
            @(negedge clk); we = 1'b0;
            if (msip !== 1'b1) begin $display("DEV_FAIL msip 未挂起"); errors = errors + 1; end
            else ipis = ipis + 1;
            @(negedge clk); we = 1'b1; waddr = A_MSIP; wdata = 64'd0;
            @(negedge clk); we = 1'b0;
            if (msip !== 1'b0) begin $display("DEV_FAIL msip 未清零"); errors = errors + 1; end
        end
    endtask

    initial begin
        $dumpfile("tb_clint.vcd");
        $dumpvars(0, tb_clint);
        errors = 0; fires = 0; ipis = 0;
        tick = 1'b0; we = 1'b0; waddr = 5'h0; wdata = 64'd0;
        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // ── timer 相：装填 mtimecmp=PERIOD，逐 tick 采样比较器 ──
        @(negedge clk); we = 1'b1; waddr = A_CMP; wdata = PERIOD;
        @(negedge clk); we = 1'b0;
        cmp = PERIOD;
        for (i = 0; i < NTICK; i = i + 1) begin
            if (mtip) begin                     // mtime >= mtimecmp → timer 中断
                fires = fires + 1;
                cmp = cmp + PERIOD;             // handler 重装填（同时清本次 MTIP）
                @(negedge clk); we = 1'b1; waddr = A_CMP; wdata = cmp; tick = 1'b1;
                @(negedge clk); we = 1'b0; tick = 1'b0;
            end else begin
                @(negedge clk); tick = 1'b1;
                @(negedge clk); tick = 1'b0;
            end
        end

        // ── 软件中断相：拉起 2 次 IPI，handler 计数并清零 ──
        ipi;
        ipi;

        // ── 判级（与软件逐位一致：fires=3 mtime=16 ipi=2）──
        if (fires == EXP_TIMER && mtime_o == NTICK)
            $display("TIMER_PASS fires=%0d mtime=%0d", fires, mtime_o);
        else begin $display("DEV_FAIL timer fires=%0d mtime=%0d", fires, mtime_o); errors = errors + 1; end
        if (ipis == EXP_SOFT)
            $display("SOFT_PASS  ipi=%0d", ipis);
        else begin $display("DEV_FAIL ipis=%0d", ipis); errors = errors + 1; end

        if (errors == 0) begin
            $display("DEV_PASS   mtimecmp comparator + msip register RTL ok");
            $display("ALL_PASS");
        end else
            $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
