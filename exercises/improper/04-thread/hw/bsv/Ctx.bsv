// 双上下文寄存器文件 —— 硬件路径（BlueSpec SV）。
// 两套上下文 = 两组复制的寄存器（影子寄存器组）；vcpu_read/vcpu_sum 是共享后端：
// 由 active 一拍选中一套上下文。你只需填这两个函数。
package Ctx;

import Vector::*;

typedef 4 NReg;
typedef Vector#(NReg, Bit#(32)) RegFile; // 一套上下文 = 一组寄存器

// ── 学生填：从两套上下文 + active 选当前上下文的读口与共享加法器 ──

// 读当前上下文的 raddr 寄存器。
function Bit#(32) vcpu_read(RegFile c0, RegFile c1, Bit#(1) active, Bit#(2) raddr);
   // TODO: RegFile cur = (active == 0) ? c0 : c1; return cur[raddr];
   let touch = c0[raddr] ^ c1[raddr] ^ zeroExtend(active);
   return touch & 32'b0; // 占位 → 判 FAIL
endfunction

// 共享加法器：当前上下文 ra 字 + sa 字（唯一一个加法器，两套上下文分时复用）。
function Bit#(32) vcpu_sum(RegFile c0, RegFile c1, Bit#(1) active, Bit#(2) ra, Bit#(2) sa);
   // TODO: RegFile cur = (active == 0) ? c0 : c1; return cur[ra] + cur[sa];
   let touch = c0[ra] ^ c1[sa] ^ zeroExtend(active);
   return touch & 32'b0; // 占位 → 判 FAIL
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function ActionValue#(Bit#(32)) chkEq(String tag, Bit#(32) got, Bit#(32) exp);
   actionvalue
      if (got != exp) begin
         $display("%s_FAIL got=0x%08h exp=0x%08h", tag, got, exp);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbCtx (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      RegFile c0 = replicate(0);
      RegFile c1 = replicate(0);
      c0[0] = 100; c0[1] = 200; c0[2] = 7;
      c1[0] = 11;  c1[1] = 22;  c1[2] = 3;

      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      g = 0;
      let s0 <- chkEq("CTX_SWAP", vcpu_read(c0, c1, 0, 0), 100); g = g + s0;
      let s1 <- chkEq("CTX_SWAP", vcpu_read(c0, c1, 0, 1), 200); g = g + s1;
      let s2 <- chkEq("CTX_SWAP", vcpu_read(c0, c1, 1, 0), 11);  g = g + s2;
      let s3 <- chkEq("CTX_SWAP", vcpu_read(c0, c1, 1, 1), 22);  g = g + s3;
      if (g == 0) $display("CTX_SWAP_PASS");
      errors = errors + g;

      g = 0;
      let h0 <- chkEq("SHARE", vcpu_sum(c0, c1, 0, 0, 1), 300); g = g + h0;
      let h1 <- chkEq("SHARE", vcpu_sum(c0, c1, 1, 0, 1), 33);  g = g + h1;
      if (g == 0) $display("SHARE_PASS");
      errors = errors + g;

      g = 0;
      let d0 <- chkEq("SCHED", vcpu_read(c0, c1, 0, 2), 7); g = g + d0;
      let d1 <- chkEq("SCHED", vcpu_read(c0, c1, 1, 2), 3); g = g + d1;
      let d2 <- chkEq("SCHED", vcpu_read(c0, c1, 0, 2), 7); g = g + d2;
      let d3 <- chkEq("SCHED", vcpu_read(c0, c1, 1, 2), 3); g = g + d3;
      if (g == 0) $display("SCHED_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
