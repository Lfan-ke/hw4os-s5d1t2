// 16d · 核外中断 PLIC - 设备侧（BlueSpec SV，参考解）。
// 平台级共享外设中断路由器：priority/enable/threshold → claim/complete。
// 寄存器（addr=字节偏移）：0x00..0x0C PRIO1..4(RW) 0x10 PENDING(RO bitmap)
//   0x14 ENABLE(RW bitmap) 0x18 THRESHOLD(RW) 0x1C CLAIM(R=取顶源并清 pending / W=EOI) 0x20 RAISE(WO)。
package Plic;

typedef Bit#(8)  Addr;
typedef Bit#(32) Word;

// ── 学生填：组合仲裁器 - 合格源 = pend & ena & (prio>thr)，取最高 prio（同 prio 取最小 id）。──
// 返回 (best_id, best_prio)；无合格源时 best_id=0。
function Tuple2#(Bit#(3), Bit#(2))
   arbitrate(Bit#(5) pend, Bit#(5) ena,
             Bit#(2) p1, Bit#(2) p2, Bit#(2) p3, Bit#(2) p4, Bit#(2) thr);
   Bit#(3) bid = 0;
   Bit#(2) bp  = 0;
   Bool e1 = (pend[1] == 1) && (ena[1] == 1) && (p1 > thr);
   Bool e2 = (pend[2] == 1) && (ena[2] == 1) && (p2 > thr);
   Bool e3 = (pend[3] == 1) && (ena[3] == 1) && (p3 > thr);
   Bool e4 = (pend[4] == 1) && (ena[4] == 1) && (p4 > thr);
   if (e4 && (p4 >= bp)) begin bp = p4; bid = 4; end
   if (e3 && (p3 >= bp)) begin bp = p3; bid = 3; end
   if (e2 && (p2 >= bp)) begin bp = p2; bid = 2; end
   if (e1 && (p1 >= bp)) begin bp = p1; bid = 1; end
   return tuple2(bid, bp);
endfunction

interface Plic;
   method Action  write(Addr a, Word d);
   method Word    read(Addr a);
   method Bit#(3) bestId;
   method Bit#(2) bestPrio;
   method Action  claim; // 读 CLAIM 的副作用：清顶源的 pending 位
endinterface

(* synthesize *)
module mkPlic (Plic);
   Reg#(Bit#(2)) prio1     <- mkReg(0);
   Reg#(Bit#(2)) prio2     <- mkReg(0);
   Reg#(Bit#(2)) prio3     <- mkReg(0);
   Reg#(Bit#(2)) prio4     <- mkReg(0);
   Reg#(Bit#(5)) pending   <- mkReg(0);
   Reg#(Bit#(5)) enable    <- mkReg(0);
   Reg#(Bit#(2)) threshold <- mkReg(0);

   function Tuple2#(Bit#(3), Bit#(2)) arb();
      return arbitrate(pending, enable, prio1, prio2, prio3, prio4, threshold);
   endfunction

   method Action write(Addr a, Word d);
      case (a)
         8'h00: prio1 <= d[1:0];
         8'h04: prio2 <= d[1:0];
         8'h08: prio3 <= d[1:0];
         8'h0C: prio4 <= d[1:0];
         8'h14: enable <= d[4:0];
         8'h18: threshold <= d[1:0];
         8'h20: pending <= pending | d[4:0];
         default: noAction; // PENDING(RO) / CLAIM(EOI)：pending 不变
      endcase
   endmethod

   method Word read(Addr a);
      case (a)
         8'h00: return zeroExtend(prio1);
         8'h04: return zeroExtend(prio2);
         8'h08: return zeroExtend(prio3);
         8'h0C: return zeroExtend(prio4);
         8'h10: return zeroExtend(pending);
         8'h14: return zeroExtend(enable);
         8'h18: return zeroExtend(threshold);
         8'h1C: return zeroExtend(tpl_1(arb()));
         default: return 0;
      endcase
   endmethod

   method Bit#(3) bestId   = tpl_1(arb());
   method Bit#(2) bestPrio = tpl_2(arb());

   method Action claim;
      Bit#(3) id = tpl_1(arb());
      if (id != 0) pending <= pending & ~(5'b1 << id);
   endmethod
endmodule

// ── 测试 harness（给定，勿改）：tb 当参考驱动跑同一场景 ──
// 注：BSV 值方法 read(addr) 每周期对每个不同实参各占一个端口；故每条 rule 只读一个地址，
//     把需要的字存进 tb 侧寄存器，分步推进（与 Verilog 的 #1 重摆 addr 等价）。
(* synthesize *)
module mkTbPlic (Empty);
   Plic          dev    <- mkPlic;
   Reg#(Bit#(6)) step   <- mkReg(0);
   Reg#(Bit#(8)) errors <- mkReg(0);
   Reg#(Bit#(3)) c1     <- mkReg(0);
   Reg#(Bit#(3)) c2     <- mkReg(0);
   Reg#(Bit#(3)) c3     <- mkReg(0);
   Reg#(Bit#(3)) c4     <- mkReg(0);

   rule cfg0 (step == 0); dev.write(8'h00, 32'd1); step <= 1; endrule
   rule cfg1 (step == 1); dev.write(8'h04, 32'd2); step <= 2; endrule
   rule cfg2 (step == 2); dev.write(8'h08, 32'd3); step <= 3; endrule
   rule cfg3 (step == 3); dev.write(8'h0C, 32'd3); step <= 4; endrule
   rule cfg4 (step == 4); dev.write(8'h14, 32'h0000_000E); step <= 5; endrule // 使能 {1,2,3}
   rule cfg5 (step == 5); dev.write(8'h18, 32'd1); step <= 6; endrule          // 阈值 1
   rule cfg6 (step == 6); dev.write(8'h20, 32'h0000_001E); step <= 7; endrule  // raise {1,2,3,4}

   rule route (step == 7); // ROUTE：最高优先级源 = 3（prio 3）
      if (dev.bestId != 3 || dev.bestPrio != 3) begin
         $display("ROUTE_FAIL best_id=%0d best_prio=%0d", dev.bestId, dev.bestPrio);
         errors <= errors + 1;
      end
      step <= 8;
   endrule

   rule dev0 (step == 8);  let v = dev.read(8'h08); if (v != 3)            begin $display("DEV_FAIL PRIO3=0x%08h", v); errors <= errors + 1; end step <= 9;  endrule
   rule dev1 (step == 9);  let v = dev.read(8'h14); if (v != 32'h0E)       begin $display("DEV_FAIL ENABLE=0x%08h", v); errors <= errors + 1; end step <= 10; endrule
   rule dev2 (step == 10); let v = dev.read(8'h18); if (v != 1)            begin $display("DEV_FAIL THRESHOLD=0x%08h", v); errors <= errors + 1; end step <= 11; endrule
   rule dev3 (step == 11); let v = dev.read(8'h10); if (v != 32'h1E)       begin $display("DEV_FAIL PENDING=0x%08h", v); errors <= errors + 1; end step <= 12; endrule
   rule dev4 (step == 12); dev.write(8'h10, 32'hFFFF_FFFF); step <= 13; endrule // RO：写应被忽略
   rule dev5 (step == 13); let v = dev.read(8'h10); if (v != 32'h1E)       begin $display("DEV_FAIL PENDING_RO=0x%08h", v); errors <= errors + 1; end step <= 14; endrule

   rule clm0 (step == 14); c1 <= dev.bestId; dev.claim; step <= 15; endrule
   rule clm1 (step == 15); c2 <= dev.bestId; dev.claim; step <= 16; endrule
   rule clm2 (step == 16); c3 <= dev.bestId; dev.claim; step <= 17; endrule
   rule clm3 (step == 17); // 检查抽干序列 + 余 pending {1,4}=0x12
      let v = dev.read(8'h10);
      Bit#(8) e = errors;
      if (c1 != 3 || c2 != 2 || c3 != 0) begin $display("CLAIM_FAIL seq=%0d,%0d,%0d", c1, c2, c3); e = e + 1; end
      if (v != 32'h12) begin $display("CLAIM_FAIL pending=0x%08h", v); e = e + 1; end
      errors <= e;
      step <= 18;
   endrule
   rule eoi0 (step == 18); dev.write(8'h1C, 32'd3); step <= 19; endrule // complete(3)
   rule eoi1 (step == 19); dev.write(8'h1C, 32'd2); step <= 20; endrule // complete(2)
   rule refr (step == 20); dev.write(8'h20, 32'h0000_0008); step <= 21; endrule // 重新 raise 源3
   rule clm4 (step == 21); c4 <= dev.bestId; dev.claim; step <= 22; endrule
   rule clm5 (step == 22);
      if (c4 != 3) begin $display("CLAIM_FAIL refire=%0d", c4); errors <= errors + 1; end
      step <= 23;
   endrule

   rule done (step == 23);
      if (errors == 0) begin
         $display("ROUTE_PASS top=3 prio=3 (max-priority arbitration)");
         $display("DEV_PASS  register fields: prio/enable/threshold readback, RO ignored");
         $display("CLAIM_PASS seq=3,2,0 refire=3");
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", errors);
      $finish(0);
   endrule
endmodule

endpackage
