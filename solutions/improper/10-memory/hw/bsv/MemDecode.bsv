// 平坦大内存地址译码器 —— 硬件路径（BlueSpec SV 参考解）。
// 与软件 addr_route() / Verilog mem_decode 逐位等价。
//   la <  fast_size → cs_fast=1, cs_slow=0, off = la
//   la >= fast_size → cs_fast=0, cs_slow=1, off = la - fast_size
// 输出打包成 Bit#(10) = { cs_fast[9], cs_slow[8], local_off[7:0] }。
package MemDecode;

// ── 核心逻辑：学生只需填这个函数体 ────────────────────────────────
function Bit#(10) decode(Bit#(8) la, Bit#(8) fast_size);
   Bit#(1) cs_fast;
   Bit#(1) cs_slow;
   Bit#(8) off;
   if (la < fast_size) begin
      cs_fast = 1'b1;
      cs_slow = 1'b0;
      off     = la;
   end else begin
      cs_fast = 1'b0;
      cs_slow = 1'b1;
      off     = la - fast_size;
   end
   return { cs_fast, cs_slow, off };
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function ActionValue#(Bit#(32)) chk(Bit#(8) fast_size, Bit#(8) la);
   actionvalue
      Bit#(10) got = decode(la, fast_size);
      Bit#(1)  e_fast = (la < fast_size) ? 1'b1 : 1'b0;
      Bit#(1)  e_slow = (la < fast_size) ? 1'b0 : 1'b1;
      Bit#(8)  e_off  = (la < fast_size) ? la : (la - fast_size);
      Bit#(10) exp = { e_fast, e_slow, e_off };
      if (got != exp) begin
         $display("FAIL la=%0d exp=0x%03h got=0x%03h", la, exp, got);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbMem (Empty);
   Reg#(Bool)     done <- mkReg(False);
   Reg#(Bit#(8))  i    <- mkReg(0);
   Reg#(Bit#(32)) errs <- mkReg(0);

   Bit#(8) fast_size = 8'd8;
   Bit#(8) total     = 8'd24; // FAST_SIZE + SLOW_SIZE

   rule step (!done && i < total);
      let e <- chk(fast_size, i);
      errs <= errs + e;
      i <= i + 1;
   endrule

   rule finish (!done && i >= total);
      if (errs == 0) $display("DECODE_PASS");
      if (errs == 0) $display("ALL_PASS");
      else           $display("SOME_FAIL errors=%0d", errs);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
