// RAM 块设备模型 —— 硬件路径（BlueSpec SV 参考解）。
// 每个地址 = 一个「块」，data = 块内容（简化为 1 字/块）。
// 软件 BlockDev::write_block/read_block 的硬件同构：wr 写、rd 读。
package Bdev;

import Vector::*;

interface Bdev_ifc;
   method Action      wr(Bit#(6) addr, Bit#(32) data);
   method Bit#(32)    rd(Bit#(6) addr);
endinterface

(* synthesize *)
module mkBdev (Bdev_ifc);
   Vector#(64, Reg#(Bit#(32))) mem <- replicateM(mkReg(0));

   method Action wr(Bit#(6) addr, Bit#(32) data);
      mem[addr] <= data;     // 写使能下把 data 落到该块
   endmethod

   method Bit#(32) rd(Bit#(6) addr);
      return mem[addr];       // 读出该块内容
   endmethod
endmodule

// 每个块的确定性特征图案（与 Verilog tb 的 pat 同构）
function Bit#(32) pat(Bit#(6) a);
   return (zeroExtend(a) * 32'h0101_0101) ^ 32'h0000_BEEF;
endfunction

// ── 测试 harness（给定，勿改）─────────────────────────────────────
(* synthesize *)
module mkTbBdev (Empty);
   Bdev_ifc       dev    <- mkBdev;
   Reg#(Bit#(2))  phase  <- mkReg(0);
   Reg#(Bit#(7))  idx    <- mkReg(0);
   Reg#(Bit#(32)) errors <- mkReg(0);

   // 写阶段：逐块写入图案
   rule wphase (phase == 0);
      dev.wr(truncate(idx), pat(truncate(idx)));
      if (idx == 63) begin idx <= 0; phase <= 1; end
      else idx <= idx + 1;
   endrule

   // 读阶段：逐块读回比对
   rule rphase (phase == 1);
      Bit#(32) got = dev.rd(truncate(idx));
      if (got != pat(truncate(idx))) begin
         $display("BDEV_FAIL addr=%0d exp=0x%08h got=0x%08h", idx, pat(truncate(idx)), got);
         errors <= errors + 1;
      end
      if (idx == 63) begin idx <= 0; phase <= 2; end
      else idx <= idx + 1;
   endrule

   rule donephase (phase == 2);
      if (errors == 0) begin
         $display("BDEV_PASS");
         $display("ALL_PASS");
      end else begin
         $display("SOME_FAIL errors=%0d", errors);
      end
      $finish(0);
   endrule
endmodule

endpackage
