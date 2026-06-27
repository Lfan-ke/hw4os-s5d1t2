// 设备文件搬进硬件 —— BlueSpec SV。
// ① 常量设备：constRead 恒 1（写吞掉、不改状态）。
// ② RingSum：深度 2 环形寄存器；wr(x)=x==233 复位，否则移位 r1<=r0;r0<=x；rd=r0+r1。
// 与软件 FileLike / Verilog abstract_dev 完全同构。你填 wr 与 constRead。
package AbstractDev;

// 「一切皆文件」= 一切皆 read/write 接口。
interface RingDev;
   method Action     wr(Bit#(16) x);   // write
   method Bit#(16)   rd;               // RingSum read = r0 + r1
   method Bit#(16)   constRead;        // 常量设备 read 恒 1
endinterface

(* synthesize *)
module mkRingDev (RingDev);
   Reg#(Bit#(16)) r0 <- mkReg(0);
   Reg#(Bit#(16)) r1 <- mkReg(0);

   // ── 学生填：RingSum 写入（233 复位，否则移位）──
   method Action wr(Bit#(16) x);
      // TODO: x==16'd233 → r0<=0;r1<=0（清零）；否则 r1<=r0; r0<=x。
      //   // TODO[a] 移位写法（如上）  // ELSE[b] head 指针写法
      // 占位：状态不更新（(x==x) 恒真→保持原值；读 x 仅为消除未用告警）→ 环和恒 0 判 RING_FAIL。
      r0 <= (x == x) ? r0 : x; // ← 占位：删掉，写出正确逻辑
      r1 <= r1;
   endmethod

   // RingSum read = r0 + r1（组合求和，给定）
   method Bit#(16) rd;
      return r0 + r1;
   endmethod

   // ── 学生填：常量设备 read 恒 1 ──
   method Bit#(16) constRead;
      // TODO: 改成 16'd1。
      return 16'd0; // ← 占位（读出 0 会判 FILELIKE_FAIL）
   endmethod
endmodule

// ── 测试 harness（给定，勿改）──────────────────────────────────────
// 用 step 计数把序列摊到逐拍：每拍发一个 wr，下一拍读上一拍结果。
(* synthesize *)
module mkTbAbstract (Empty);
   RingDev        dev     <- mkRingDev;
   Reg#(Bit#(8))  step    <- mkReg(0);
   Reg#(Bit#(16)) errors  <- mkReg(0);
   Reg#(Bit#(16)) ringErr <- mkReg(0);

   // 子实验 1：常量设备恒 1，并发首个写入 666
   rule r_const (step == 0);
      if (dev.constRead != 16'd1) begin
         $display("FILELIKE_FAIL const_read=%0d exp=1", dev.constRead);
         errors <= errors + 1;
      end else begin
         $display("FILELIKE_PASS");
      end
      dev.wr(16'd666);
      step <= 1;
   endrule

   rule r_w1 (step == 1);
      if (dev.rd != 16'd666) begin $display("RING_FAIL 666 -> %0d exp 666", dev.rd); ringErr <= ringErr + 1; end
      dev.wr(16'd111);
      step <= 2;
   endrule

   rule r_w2 (step == 2);
      if (dev.rd != 16'd777) begin $display("RING_FAIL 111 -> %0d exp 777", dev.rd); ringErr <= ringErr + 1; end
      dev.wr(16'd222);
      step <= 3;
   endrule

   rule r_w3 (step == 3);
      if (dev.rd != 16'd333) begin $display("RING_FAIL 222 -> %0d exp 333", dev.rd); ringErr <= ringErr + 1; end
      dev.wr(16'd233);
      step <= 4;
   endrule

   rule r_w4 (step == 4);
      if (dev.rd != 16'd0) begin $display("RING_FAIL 233 -> %0d exp 0", dev.rd); ringErr <= ringErr + 1; end
      step <= 5;
   endrule

   rule r_done (step == 5);
      if (ringErr == 0) $display("RING_PASS");
      if ((errors + ringErr) == 0) $display("ALL_PASS");
      else $display("SOME_FAIL errors=%0d", errors + ringErr);
      $finish(0);
   endrule
endmodule

endpackage
