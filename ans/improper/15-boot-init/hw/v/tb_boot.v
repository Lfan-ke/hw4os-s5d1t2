// 15-boot-init Verilog testbench（给定，学生勿改）。
// 驱动“先误用 → 再正确握手 → 顺序检查”的规范序列，逐项打印 *_PASS / FAIL。
`default_nettype none
`timescale 1ns/1ps
module tb_boot;
    reg         unlocked;
    reg  [3:0]  clkdiv;
    reg         en;
    reg  [15:0] data_raw;
    reg  [2:0]  addr;
    wire [31:0] rdata;

    integer errors;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk;   // 仅为产生波形

    boot_gate dut (
        .unlocked(unlocked), .clkdiv(clkdiv), .en(en),
        .data_raw(data_raw), .addr(addr), .rdata(rdata)
    );

    localparam [2:0] A_STATUS = 3'd3;
    localparam [2:0] A_DATA   = 3'd4;

    localparam [31:0] ST_READY  = 32'h0000_0001;
    localparam [31:0] ST_LOCKED = 32'h0000_0002;
    localparam [31:0] BADBOOT   = 32'h0BAD_B007;

    // 组合读：设置 addr，等组合稳定后采样 rdata
    task rd(input [2:0] a);
        begin
            addr = a;
            #2;
        end
    endtask

    initial begin
        $dumpfile("tb_boot.vcd");
        $dumpvars(0, tb_boot);
        errors   = 0;
        unlocked = 1'b0; clkdiv = 4'd0; en = 1'b0; data_raw = 16'd0; addr = 3'd0;

        // ── 15.1 LOCK：未握手（全默认）直接用 DATA 应被拒 ──
        rd(A_DATA);
        if (rdata !== BADBOOT) begin
            $display("LOCK_FAIL 未握手读 DATA 应得 0x%08h got 0x%08h", BADBOOT, rdata);
            errors = errors + 1;
        end else begin
            rd(A_STATUS);
            if ((rdata & ST_LOCKED) == 32'b0 || (rdata & ST_READY) != 32'b0) begin
                $display("LOCK_FAIL 未握手 STATUS 应 LOCKED 且 !READY got 0x%08h", rdata);
                errors = errors + 1;
            end else begin
                $display("LOCK_PASS");
            end
        end

        // ── 15.2 BOOT：四步握手后应就绪 ──
        unlocked = 1'b1; clkdiv = 4'd5; en = 1'b1; // magic + 合法 CLKDIV + EN
        rd(A_STATUS);
        if ((rdata & ST_READY) == 32'b0) begin
            $display("BOOT_FAIL 握手后 STATUS.READY 未置位 got 0x%08h", rdata);
            errors = errors + 1;
        end else begin
            $display("BOOT_PASS");
        end

        // ── 15.2 USE：写 DATA 读回变换值 ──
        data_raw = 16'h1234;
        rd(A_DATA);
        if (rdata !== 32'h0000_D8CA) begin // 0x1234 ^ 0xCAFE
            $display("USE_FAIL DATA 变换错 exp 0x0000D8CA got 0x%08h", rdata);
            errors = errors + 1;
        end else begin
            $display("USE_PASS");
        end

        // ── 15.3 ORDER：先用后置位会被拒、置位后才正常 ──
        // 先回到未握手态用 DATA（抢跑）→ 应被拒
        unlocked = 1'b0; en = 1'b0; clkdiv = 4'd0; data_raw = 16'h00AA;
        rd(A_DATA);
        if (rdata !== BADBOOT) begin
            $display("ORDER_FAIL 抢跑用 DATA 应被拒 0x%08h got 0x%08h", BADBOOT, rdata);
            errors = errors + 1;
        end else begin
            // 再正确握手后用 DATA → 应正常
            unlocked = 1'b1; clkdiv = 4'd7; en = 1'b1; data_raw = 16'h00AA;
            rd(A_DATA);
            if (rdata !== 32'h0000_CA54) begin // 0x00AA ^ 0xCAFE
                $display("ORDER_FAIL 握手后用 DATA 应正常 exp 0x0000CA54 got 0x%08h", rdata);
                errors = errors + 1;
            end else begin
                $display("ORDER_PASS");
            end
        end

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
