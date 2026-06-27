// 07-ipc Verilog testbench（给定，学生勿改）。
// 用纯组合 ipc_proc「IPC ALU」驱动四段序列（握手/锁/信号量/编排），
// 逐位比对，打印 *_PASS / *_FAIL。判题口径与软件四变体一致。
`default_nettype none
`timescale 1ns/1ps
module tb_ipc;
    reg  [3:0]  op;
    reg  [31:0] a;
    reg  [31:0] b;
    wire [63:0] y;

    integer errors;
    integer gerr;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk;   // 仅为产生波形

    ipc_proc dut (.op(op), .a(a), .b(b), .y(y));

    localparam [31:0] BUSY  = 32'h8000_0000;
    localparam [31:0] DONE  = 32'h4000_0000;
    localparam [31:0] START = 32'h1000_0000;

    localparam [3:0] OP_BFINISH = 4'd0;
    localparam [3:0] OP_APOLL   = 4'd1;
    localparam [3:0] OP_TAS     = 4'd2;
    localparam [3:0] OP_UNLOCK  = 4'd3;
    localparam [3:0] OP_DOWN    = 4'd4;
    localparam [3:0] OP_UP      = 4'd5;
    localparam [3:0] OP_ASTEP   = 4'd6;
    localparam [3:0] OP_BSTEP   = 4'd7;

    // 驱动一次组合运算，结果落在 y。
    task apply(input [3:0] o, input [31:0] aa, input [31:0] bb);
        begin
            op = o; a = aa; b = bb;
            #10;
        end
    endtask

    // 局部变量
    reg [31:0] donec, ctrl, ctrl2, job, lock;
    reg        phase, got;
    reg [7:0]  count;
    integer    in_cs, grants, r;
    reg [31:0] jobs [0:3];
    reg [31:0] sh, r0v, r1v, nv;

    initial begin
        $dumpfile("tb_ipc.vcd");
        $dumpvars(0, tb_ipc);
        errors = 0;
        op = 0; a = 0; b = 0;

        // ───────── 1. done-bit 握手 ─────────
        gerr = 0;
        // B 运行中: DONE=0 + 垃圾 RESULT —— A 绝不能 ready
        apply(OP_APOLL, BUSY | 32'h0000_DEAD, 32'b0);
        if (y[32] !== 1'b0) begin
            $display("EARLY_FAIL A 在 DONE=0 时就绪了"); gerr = gerr + 1;
        end
        // B 完成: DONE=1,RESULT=0x1234
        apply(OP_BFINISH, 32'h0000_1234, 32'b0); donec = y[31:0];
        apply(OP_APOLL, donec, 32'b0);
        if (y[32] !== 1'b1 || y[15:0] !== 16'h1234) begin
            $display("HANDSHAKE_FAIL ready=%b result=0x%04x 应=(1,0x1234)", y[32], y[15:0]);
            gerr = gerr + 1;
        end
        if (gerr == 0) $display("HANDSHAKE_PASS");
        errors = errors + gerr;

        // ───────── 2. test_and_set 锁 ─────────
        gerr = 0;
        apply(OP_TAS, 32'd0, 32'b0);
        if (y[0] !== 1'b1 || y[1] !== 1'b1) begin
            $display("TAS_FAIL tas(0)=(new=%b,got=%b) 应=(1,1)", y[0], y[1]); gerr = gerr + 1;
        end
        apply(OP_TAS, 32'd1, 32'b0);
        if (y[0] !== 1'b1 || y[1] !== 1'b0) begin
            $display("TAS_FAIL tas(1)=(new=%b,got=%b) 应=(1,0)", y[0], y[1]); gerr = gerr + 1;
        end
        apply(OP_UNLOCK, 32'b0, 32'b0);
        if (y[31:0] !== 32'b0) begin
            $display("TAS_FAIL unlock 应=0 got=0x%08x", y[31:0]); gerr = gerr + 1;
        end
        if (gerr == 0) $display("TAS_PASS");
        errors = errors + gerr;

        // 给定交错调度跑两个 proc 抢锁/放锁，断言临界区内 <= 1
        gerr = 0; lock = 32'b0; in_cs = 0;
        // proc0 抢锁
        apply(OP_TAS, lock, 32'b0); lock = {31'b0, y[0]}; got = y[1];
        if (got) in_cs = in_cs + 1;
        else begin $display("MUTEX_FAIL proc0 抢空锁却失败"); gerr = gerr + 1; end
        if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL 同时 %0d 个", in_cs); gerr = gerr + 1; end
        // proc1 持锁期间抢锁，必须失败
        apply(OP_TAS, lock, 32'b0); lock = {31'b0, y[0]}; got = y[1];
        if (got) begin
            in_cs = in_cs + 1;
            if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL proc1 闯入，同时 %0d 个", in_cs); gerr = gerr + 1; end
        end
        // proc0 放锁
        apply(OP_UNLOCK, 32'b0, 32'b0); lock = y[31:0]; in_cs = in_cs - 1;
        // proc1 重试，这次抢到
        apply(OP_TAS, lock, 32'b0); lock = {31'b0, y[0]}; got = y[1];
        if (got) in_cs = in_cs + 1;
        else begin $display("MUTEX_FAIL proc1 在锁释放后仍抢不到"); gerr = gerr + 1; end
        if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL 同时 %0d 个", in_cs); gerr = gerr + 1; end
        apply(OP_UNLOCK, 32'b0, 32'b0); lock = y[31:0]; in_cs = in_cs - 1;
        if (in_cs != 0) begin $display("MUTEX_FAIL 收尾计数=%0d 应=0", in_cs); gerr = gerr + 1; end
        // 对照：非原子读改写丢更新（信息行）
        sh = 0; r0v = sh; r1v = sh; nv = r1v + 1;
        $display("NAIVE_RACE 非原子读改写丢更新: got=%0d expected=2", nv);
        if (gerr == 0) $display("MUTEX_PASS");
        errors = errors + gerr;

        // ───────── 3. 计数信号量 ─────────
        gerr = 0;
        apply(OP_DOWN, 32'd2, 32'b0);
        if (y[8] !== 1'b1 || y[7:0] !== 8'd1) begin $display("SEM_FAIL down(2) 应=(1,ok)"); gerr = gerr + 1; end
        apply(OP_DOWN, 32'd1, 32'b0);
        if (y[8] !== 1'b1 || y[7:0] !== 8'd0) begin $display("SEM_FAIL down(1) 应=(0,ok)"); gerr = gerr + 1; end
        apply(OP_DOWN, 32'd0, 32'b0);
        if (y[8] !== 1'b0) begin $display("SEM_FAIL down(0) 空仓应 ok=0"); gerr = gerr + 1; end
        apply(OP_UP, 32'd0, 32'b0);
        if (y[7:0] !== 8'd1) begin $display("SEM_FAIL up(0) 应=1"); gerr = gerr + 1; end
        apply(OP_UP, 32'd2, 32'b0);
        if (y[7:0] !== 8'd3) begin $display("SEM_FAIL up(2) 应=3"); gerr = gerr + 1; end
        // 不变式：2 个资源恰好发放 2 次（仅 ok 才更新 count）
        count = 8'd2; grants = 0;
        for (r = 0; r < 3; r = r + 1) begin
            apply(OP_DOWN, {24'b0, count}, 32'b0);
            if (y[8]) begin count = y[7:0]; grants = grants + 1; end
        end
        if (grants != 2) begin $display("SEM_FAIL 2 个资源却发放 %0d 次", grants); gerr = gerr + 1; end
        apply(OP_UP, {24'b0, count}, 32'b0); count = y[7:0];
        apply(OP_DOWN, {24'b0, count}, 32'b0);
        if (y[8] !== 1'b1) begin $display("SEM_FAIL up 后队首拿不到资源"); gerr = gerr + 1; end
        else count = y[7:0];
        if (gerr == 0) $display("SEM_PASS");
        errors = errors + gerr;

        // ───────── 4. 编排 capstone ─────────
        gerr = 0;
        jobs[0] = 32'd7; jobs[1] = 32'd21; jobs[2] = 32'd100; jobs[3] = 32'd3;
        ctrl = 32'b0; phase = 1'b0;
        for (r = 0; r < 4; r = r + 1) begin
            job = jobs[r];
            // A 按门铃
            apply(OP_ASTEP, ctrl, {31'b0, phase}); ctrl = y[31:0]; phase = y[48];
            if ((ctrl & START) === 32'b0 || phase !== 1'b1) begin
                $display("ORCH_FAIL r%0d A 没按门铃", r); gerr = gerr + 1;
            end
            // A 提前轮询：B 还没干完，不得推进
            apply(OP_ASTEP, ctrl, {31'b0, phase}); ctrl2 = y[31:0];
            if (ctrl2 !== ctrl || y[48] !== 1'b1 || y[47:32] !== 16'b0) begin
                $display("ORCH_FAIL r%0d A 在 B 完成前推进了(乱序)", r); gerr = gerr + 1;
            end
            // B 干活置位
            apply(OP_BSTEP, ctrl, job); ctrl = y[31:0];
            if ((ctrl & DONE) === 32'b0 || (ctrl & START) !== 32'b0 || ctrl[15:0] !== job[15:0]) begin
                $display("ORCH_FAIL r%0d B 未正确置位 ctrl=0x%08x", r, ctrl); gerr = gerr + 1;
            end
            // A 检测 DONE，做后续 post=result*2，清 DONE
            apply(OP_ASTEP, ctrl, {31'b0, phase}); ctrl = y[31:0]; phase = y[48];
            if (y[47:32] !== (job[15:0] << 1) || (ctrl & DONE) !== 32'b0 || phase !== 1'b0) begin
                $display("ORCH_FAIL r%0d 后续值/收尾错 post=%0d 应=%0d", r, y[47:32], job << 1); gerr = gerr + 1;
            end
        end
        if (gerr == 0) $display("ORCH_PASS");
        errors = errors + gerr;

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
