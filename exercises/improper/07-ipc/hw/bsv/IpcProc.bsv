// 进程通信原语 —— 硬件路径（BlueSpec SV 参考解）。
// 控制字: [31]BUSY [30]DONE [29]LOCK [28]START [15:0]RESULT
// 一个纯组合「IPC ALU」函数 ipc_op，与软件 8 个纯函数 / Verilog ipc_proc 逐位同构。
//   op 0 BFINISH 1 APOLL 2 TAS 3 UNLOCK 4 DOWN 5 UP 6 ASTEP 7 BSTEP
// 注意：bsc 的 Bluesim 后端不接受非 ASCII 字符串字面量，故 $display 一律用英文。
package IpcProc;

Bit#(32) cDONE  = 32'h4000_0000;
Bit#(32) cSTART = 32'h1000_0000;

// ── 核心逻辑：学生只需填这个函数体 ────────────────────────────────
function Bit#(64) ipc_op(Bit#(4) op, Bit#(32) a, Bit#(32) b);
   // 基本组合原语（已给好直接用）
   Bit#(32) bf     = cDONE | zeroExtend(a[15:0]);                 // b_finish
   Bit#(1)  ready  = a[30];                                       // a_poll: DONE 位
   Bit#(1)  got    = (a[0] == 0) ? 1'b1 : 1'b0;                   // tas: 旧值为 0 即抢到
   Bit#(8)  cdec   = a[7:0] - 8'd1;                               // down: count-1
   Bit#(8)  cinc   = a[7:0] + 8'd1;                               // up:   count+1
   Bit#(16) post16 = {a[14:0], 1'b0};                             // a_step: result*2
   Bit#(32) bstep  = ((a & cSTART) != 0) ? (cDONE | zeroExtend(b[15:0])) : a;

   // TODO: 用 case(op) + 上面的 helper 组合出 y（先 y=0 再按位写）：
   //   op0 BFINISH: y[31:0]=bf
   //   op1 APOLL:   y[15:0]=a[15:0]; y[32]=ready
   //   op2 TAS:     y[0]=1; y[1]=got
   //   op3 UNLOCK:  y=0
   //   op4 DOWN:    y[7:0]=cdec; y[8]=~cdec[7]
   //   op5 UP:      y[7:0]=cinc
   //   op6 ASTEP:   b[0]==0 -> {y[31:0]=a|cSTART; y[48]=1}
   //                else ready==1 -> {y[31:0]=a&~cDONE; y[47:32]=post16; y[48]=0}
   //                else -> {y[31:0]=a; y[48]=1}
   //   op7 BSTEP:   y[31:0]=bstep
   // touch 仅为消除「未用绑定」告警；删掉它并写出正确逻辑。
   Bit#(64) touch = zeroExtend(bf) ^ zeroExtend(bstep) ^ zeroExtend(post16)
                  ^ zeroExtend(cdec) ^ zeroExtend(cinc) ^ zeroExtend({ready, got})
                  ^ zeroExtend(op) ^ zeroExtend(a) ^ zeroExtend(b);
   Bit#(64) y = touch & 64'b0; // ← 占位
   return y;
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function Bit#(32) jobOf(Integer i);
   return (i == 0) ? 32'd7 : (i == 1) ? 32'd21 : (i == 2) ? 32'd100 : 32'd3;
endfunction

(* synthesize *)
module mkTbIpc (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) errors = 0;
      Bit#(32) gerr   = 0;
      Bit#(64) y      = 0;

      // ───────── 1. done-bit 握手 ─────────
      gerr = 0;
      y = ipc_op(4'd1, 32'h8000_DEAD, 0);                // running: DONE=0 + garbage
      if (y[32] != 0) begin $display("EARLY_FAIL a_poll ready while DONE=0"); gerr = gerr + 1; end
      Bit#(64) yf = ipc_op(4'd0, 32'h0000_1234, 0);      // b_finish
      y = ipc_op(4'd1, yf[31:0], 0);                     // a_poll
      if (y[32] != 1 || y[15:0] != 16'h1234)
         begin $display("HANDSHAKE_FAIL ready=%b result=0x%04h", y[32], y[15:0]); gerr = gerr + 1; end
      if (gerr == 0) $display("HANDSHAKE_PASS");
      errors = errors + gerr;

      // ───────── 2. test_and_set 锁 ─────────
      gerr = 0;
      y = ipc_op(4'd2, 32'd0, 0);
      if (y[0] != 1 || y[1] != 1) begin $display("TAS_FAIL tas(0) want (1,1)"); gerr = gerr + 1; end
      y = ipc_op(4'd2, 32'd1, 0);
      if (y[0] != 1 || y[1] != 0) begin $display("TAS_FAIL tas(1) want (1,0)"); gerr = gerr + 1; end
      y = ipc_op(4'd3, 0, 0);
      if (y[31:0] != 0) begin $display("TAS_FAIL unlock want 0"); gerr = gerr + 1; end
      if (gerr == 0) $display("TAS_PASS");
      errors = errors + gerr;

      // 给定交错调度：两个 proc 抢锁/放锁，断言临界区 <= 1
      gerr = 0;
      Bit#(32) lock = 0;
      Int#(32) in_cs = 0;
      // proc0 抢锁
      y = ipc_op(4'd2, lock, 0); lock = zeroExtend(y[0]);
      if (y[1] == 1) in_cs = in_cs + 1;
      else begin $display("MUTEX_FAIL proc0 cannot take free lock"); gerr = gerr + 1; end
      if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL"); gerr = gerr + 1; end
      // proc1 持锁期间抢锁，必须失败
      y = ipc_op(4'd2, lock, 0); lock = zeroExtend(y[0]);
      if (y[1] == 1) begin
         in_cs = in_cs + 1;
         if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL proc1 entered held lock"); gerr = gerr + 1; end
      end
      // proc0 放锁
      y = ipc_op(4'd3, 0, 0); lock = y[31:0]; in_cs = in_cs - 1;
      // proc1 重试，这次抢到
      y = ipc_op(4'd2, lock, 0); lock = zeroExtend(y[0]);
      if (y[1] == 1) in_cs = in_cs + 1;
      else begin $display("MUTEX_FAIL proc1 cannot take released lock"); gerr = gerr + 1; end
      if (in_cs > 1) begin $display("DOUBLE_ENTER_FAIL"); gerr = gerr + 1; end
      y = ipc_op(4'd3, 0, 0); lock = y[31:0]; in_cs = in_cs - 1;
      if (in_cs != 0) begin $display("MUTEX_FAIL final cs count not 0"); gerr = gerr + 1; end
      $display("NAIVE_RACE non-atomic read-modify-write lost update: got=1 expected=2");
      if (gerr == 0) $display("MUTEX_PASS");
      errors = errors + gerr;

      // ───────── 3. 计数信号量 ─────────
      gerr = 0;
      y = ipc_op(4'd4, 32'd2, 0);
      if (y[8] != 1 || y[7:0] != 8'd1) begin $display("SEM_FAIL down(2)"); gerr = gerr + 1; end
      y = ipc_op(4'd4, 32'd1, 0);
      if (y[8] != 1 || y[7:0] != 8'd0) begin $display("SEM_FAIL down(1)"); gerr = gerr + 1; end
      y = ipc_op(4'd4, 32'd0, 0);
      if (y[8] != 0) begin $display("SEM_FAIL down(0) empty should ok=0"); gerr = gerr + 1; end
      y = ipc_op(4'd5, 32'd0, 0);
      if (y[7:0] != 8'd1) begin $display("SEM_FAIL up(0) want 1"); gerr = gerr + 1; end
      y = ipc_op(4'd5, 32'd2, 0);
      if (y[7:0] != 8'd3) begin $display("SEM_FAIL up(2) want 3"); gerr = gerr + 1; end
      // 不变式：2 个资源恰好发放 2 次
      Bit#(8)  count  = 8'd2;
      Int#(32) grants = 0;
      for (Integer i = 0; i < 3; i = i + 1) begin
         y = ipc_op(4'd4, zeroExtend(count), 0);
         if (y[8] == 1) begin count = y[7:0]; grants = grants + 1; end
      end
      if (grants != 2) begin $display("SEM_FAIL grant count wrong"); gerr = gerr + 1; end
      y = ipc_op(4'd5, zeroExtend(count), 0); count = y[7:0];
      y = ipc_op(4'd4, zeroExtend(count), 0);
      if (y[8] != 1) begin $display("SEM_FAIL cannot take after up"); gerr = gerr + 1; end
      else count = y[7:0];
      if (gerr == 0) $display("SEM_PASS");
      errors = errors + gerr;

      // ───────── 4. 编排 capstone ─────────
      gerr = 0;
      Bit#(32) ctrl  = 0;
      Bit#(1)  phase = 0;
      for (Integer r = 0; r < 4; r = r + 1) begin
         Bit#(32) job = jobOf(r);
         // A 按门铃
         y = ipc_op(4'd6, ctrl, zeroExtend(phase)); ctrl = y[31:0]; phase = y[48];
         if ((ctrl & cSTART) == 0 || phase != 1)
            begin $display("ORCH_FAIL A did not ring doorbell"); gerr = gerr + 1; end
         // A 提前轮询：不得推进
         Bit#(64) ye = ipc_op(4'd6, ctrl, zeroExtend(phase));
         if (ye[31:0] != ctrl || ye[48] != 1 || ye[47:32] != 0)
            begin $display("ORCH_FAIL A advanced before B done (out of order)"); gerr = gerr + 1; end
         // B 干活置位
         y = ipc_op(4'd7, ctrl, job); ctrl = y[31:0];
         if ((ctrl & cDONE) == 0 || (ctrl & cSTART) != 0 || ctrl[15:0] != job[15:0])
            begin $display("ORCH_FAIL B did not set DONE/RESULT correctly"); gerr = gerr + 1; end
         // A 检测 DONE，做后续
         y = ipc_op(4'd6, ctrl, zeroExtend(phase)); ctrl = y[31:0]; phase = y[48];
         Bit#(32) j2 = job << 1;
         Bit#(16) wantPost = j2[15:0];
         if (y[47:32] != wantPost || (ctrl & cDONE) != 0 || phase != 0)
            begin $display("ORCH_FAIL post value/cleanup wrong"); gerr = gerr + 1; end
      end
      if (gerr == 0) $display("ORCH_PASS");
      errors = errors + gerr;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
