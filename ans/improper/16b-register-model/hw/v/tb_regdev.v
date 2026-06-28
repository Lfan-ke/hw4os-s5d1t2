// 16b · 寄存器模型 - testbench（给定，学生勿改）。
// tb 当“参考驱动”跑同一段 trace；并对“字段抽头”做逐位镜像校验，与软件四语逐位一致。
`default_nettype none
`timescale 1ns/1ps
module tb_regdev;
    reg         clk, rst, sel, we;
    reg  [3:0]  addr;
    reg  [31:0] wdata;
    wire [31:0] rdata;
    wire        f_en, f_ie, f_rst, f_ready, f_busy, f_irq;
    wire [1:0]  f_mode;
    wire [7:0]  f_byte;

    integer errors;

    regdev dut (
        .clk(clk), .rst(rst), .sel(sel), .we(we), .addr(addr), .wdata(wdata), .rdata(rdata),
        .f_en(f_en), .f_ie(f_ie), .f_mode(f_mode), .f_rst(f_rst),
        .f_ready(f_ready), .f_busy(f_busy), .f_irq(f_irq), .f_byte(f_byte)
    );

    localparam [31:0] MAGIC = 32'h5245_4744;
    localparam [3:0]  A_CTRL = 4'h0, A_STATUS = 4'h4, A_DATA = 4'h8, A_ID = 4'hC;
    localparam [31:0] TRACE_CTRL = 32'h0000_000B, TRACE_STATUS = 32'h0000_0005;

    initial clk = 1'b0;
    always #5 clk = ~clk;

    task wr(input [3:0] a, input [31:0] d);
        begin
            @(negedge clk); sel = 1'b1; we = 1'b1; addr = a; wdata = d;
            @(negedge clk); sel = 1'b0; we = 1'b0; wdata = 32'h0;
        end
    endtask
    task rd(input [3:0] a, output [31:0] d);
        begin
            addr = a; #1; d = rdata;
        end
    endtask

    reg [31:0] v, ctrl_recon, status_recon;
    initial begin
        $dumpfile("tb_regdev.vcd");
        $dumpvars(0, tb_regdev);
        errors = 0;
        sel = 1'b0; we = 1'b0; addr = 4'h0; wdata = 32'h0;
        rst = 1'b1; @(negedge clk); @(negedge clk); rst = 1'b0;

        // RAW：探 ID
        rd(A_ID, v);
        if (v !== MAGIC) begin $display("DEV_FAIL ID=0x%08x", v); errors = errors + 1; end
        // RO 写应被忽略（写 ID 后仍是 magic）
        wr(A_ID, 32'hDEAD_BEEF);
        rd(A_ID, v);
        if (v !== MAGIC) begin $display("DEV_FAIL ID 被 RO 写改动=0x%08x", v); errors = errors + 1; end

        // 写 CTRL = 0x0B，读回（RW）
        wr(A_CTRL, TRACE_CTRL);
        rd(A_CTRL, v);
        if (v !== TRACE_CTRL) begin $display("DEV_FAIL CTRL 回读=0x%08x", v); errors = errors + 1; end
        // 读 STATUS（RO，由 CTRL 推导）
        rd(A_STATUS, v);
        if (v !== TRACE_STATUS) begin $display("DEV_FAIL STATUS=0x%08x exp=0x%08x", v, TRACE_STATUS); errors = errors + 1; end

        // 写 DATA（WO，需就绪），设备捕获低字节；读 DATA 返回 0
        wr(A_DATA, 32'h0000_00A5);
        if (f_byte !== 8'hA5) begin $display("DEV_FAIL DATA 捕获=0x%02x", f_byte); errors = errors + 1; end
        rd(A_DATA, v);
        if (v !== 32'h0) begin $display("DEV_FAIL WO 读非 0=0x%08x", v); errors = errors + 1; end

        // 逐位镜像：从字段抽头重建整字，须等于原始 raw
        ctrl_recon   = {23'b0, f_rst, 4'b0, f_mode, f_ie, f_en};
        status_recon = {29'b0, f_irq, f_busy, f_ready};
        if (ctrl_recon !== TRACE_CTRL) begin $display("DEV_FAIL CTRL 镜像=0x%08x", ctrl_recon); errors = errors + 1; end
        if (status_recon !== TRACE_STATUS) begin $display("DEV_FAIL STATUS 镜像=0x%08x", status_recon); errors = errors + 1; end

        if (errors == 0) begin
            $display("RAW_PASS  寄存器读写 trace：ID/CTRL/STATUS 一致");
            $display("DEV_PASS  设备字段响应正确（RO 拒写 / WO 读 0 / 捕获字节）");
            $display("MIRROR_PASS raw↔字段抽头逐位一致：CTRL=0x%08x STATUS=0x%08x", ctrl_recon, status_recon);
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL errors=%0d", errors);
        end
        $finish;
    end
endmodule
`default_nettype wire
