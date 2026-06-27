// 13-shared-memory Verilog testbench（给定，学生勿改）。
// 喂与软件同构的向量，验证共享环 FIFO/环绕/空满；两种语义框架：
//   RING 框架（结构化邮箱）→ RING_PASS
//   MMIO 框架（设备↔OS 共享区）→ MMIO_SHM_PASS
`default_nettype none
`timescale 1ns/1ps
module tb_shm;
    reg         clk;
    reg         rst;
    reg         push_en;
    reg  [31:0] push_data;
    reg         pop_en;
    wire [31:0] pop_data;
    wire        avail;
    wire        full;
    wire [3:0]  count;

    integer errors;
    integer gerr;

    initial clk = 1'b0;
    always #5 clk = ~clk;

    ring_mbox dut (
        .clk(clk), .rst(rst),
        .push_en(push_en), .push_data(push_data),
        .pop_en(pop_en), .pop_data(pop_data),
        .avail(avail), .full(full), .count(count)
    );

    // 入队一拍（在 negedge 摆好控制信号，posedge 生效）。
    task do_push(input [31:0] v);
        begin
            @(negedge clk);
            push_en   = 1'b1;
            push_data = v;
            pop_en    = 1'b0;
            @(negedge clk);
            push_en   = 1'b0;
        end
    endtask

    // 期望满时再入队应被拒绝：count 不变。
    task push_reject(input [31:0] v);
        reg [3:0] c0;
        begin
            @(negedge clk);
            c0        = count;
            push_en   = 1'b1;
            push_data = v;
            pop_en    = 1'b0;
            @(negedge clk);
            push_en   = 1'b0;
            if (count !== c0) begin
                $display("FAIL 满后竟接受入队 count %0d->%0d", c0, count);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask

    // 出队一拍，比对队首。
    task do_pop(input [31:0] exp);
        begin
            @(negedge clk);
            if (avail !== 1'b1) begin
                $display("FAIL 出队时 avail=0（空队）");
                gerr = gerr + 1; errors = errors + 1;
            end
            if (pop_data !== exp) begin
                $display("FAIL 出队 got=0x%08x exp=0x%08x", pop_data, exp);
                gerr = gerr + 1; errors = errors + 1;
            end
            pop_en  = 1'b1;
            push_en = 1'b0;
            @(negedge clk);
            pop_en  = 1'b0;
        end
    endtask

    // 期望空队：avail 必须为 0。
    task expect_empty;
        begin
            @(negedge clk);
            if (avail !== 1'b0) begin
                $display("FAIL 应为空队但 avail=1 count=%0d", count);
                gerr = gerr + 1; errors = errors + 1;
            end
        end
    endtask

    task reset_dut;
        begin
            @(negedge clk);
            rst = 1'b1; push_en = 1'b0; pop_en = 1'b0;
            @(negedge clk);
            rst = 1'b0;
        end
    endtask

    initial begin
        $dumpfile("tb_shm.vcd");
        $dumpvars(0, tb_shm);
        errors    = 0;
        rst       = 1'b0;
        push_en   = 1'b0;
        push_data = 32'd0;
        pop_en    = 1'b0;

        // ── RING 框架：填满 / 满拒绝 / FIFO 排空 / 空 / 环绕 ──
        reset_dut;
        gerr = 0;
        do_push(32'd11); do_push(32'd22); do_push(32'd33); do_push(32'd44);
        push_reject(32'd55);                       // 满后入队应被拒
        do_pop(32'd11); do_pop(32'd22); do_pop(32'd33); do_pop(32'd44);
        expect_empty;                              // 排空
        do_push(32'd61); do_push(32'd62); do_push(32'd63); // head/tail 已推进 → 环绕
        do_pop(32'd61); do_pop(32'd62); do_pop(32'd63);
        if (gerr == 0) $display("RING_PASS");

        // ── MMIO 框架：设备 doorbell 连写、OS 轮询 avail 读取（同一 DUT）──
        reset_dut;
        gerr = 0;
        do_push(32'hD0); do_push(32'hD1); do_push(32'hD2);
        do_pop(32'hD0); do_pop(32'hD1);            // OS 边读边让设备补写 → 环绕
        do_push(32'hD3); do_push(32'hD4);
        do_pop(32'hD2); do_pop(32'hD3); do_pop(32'hD4);
        expect_empty;
        if (gerr == 0) $display("MMIO_SHM_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
