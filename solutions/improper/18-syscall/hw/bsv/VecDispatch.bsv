// S1 硬件向量分发器（MCU 中断向量表模型）—— BlueSpec SV 参考解。
// 纯组合逻辑函数：与软件 dispatch() / Verilog vec_dispatch 完全同构。
//   mode=1 向量化 → handler_pc = base + (cause<<2)
//   mode=0 直接   → handler_pc = base
//   accept = trap_req
package VecDispatch;

// ── 核心逻辑：学生只需填这个函数体 ────────────────────────────────
// 返回 Tuple2#(handler_pc, accept)
function Tuple2#(Bit#(32), Bit#(1)) vec_dispatch(Bit#(1) mode, Bit#(32) base,
                                                 Bit#(4) cause, Bit#(1) trap_req);
   Bit#(32) offset = zeroExtend(cause) << 2;     // cause*4：向量化间隔 4 字节
   Bit#(32) handler_pc = (mode == 1) ? (base + offset) : base;
   Bit#(1)  accept = trap_req;
   return tuple2(handler_pc, accept);
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function ActionValue#(Bit#(32)) chk(String grp, Bit#(1) mode, Bit#(32) base,
                                    Bit#(4) cause, Bit#(1) trap_req,
                                    Bit#(32) exp_pc, Bit#(1) exp_acc);
   actionvalue
      match {.pc, .acc} = vec_dispatch(mode, base, cause, trap_req);
      if (pc != exp_pc || acc != exp_acc) begin
         $display("%s_FAIL mode=%0d cause=%0d exp=(0x%08h,%0d) got=(0x%08h,%0d)",
                  grp, mode, cause, exp_pc, exp_acc, pc, acc);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbVec (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) base = 32'h8000_0000;
      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      // ── DIRECT：handler_pc 恒为 base ──
      g = 0;
      let d0 <- chk("DIRECT", 1'b0, base, 4'd0, 1'b1, base, 1'b1); g = g + d0;
      let d1 <- chk("DIRECT", 1'b0, base, 4'd3, 1'b1, base, 1'b1); g = g + d1;
      let d2 <- chk("DIRECT", 1'b0, base, 4'd8, 1'b1, base, 1'b1); g = g + d2;
      if (g == 0) $display("DIRECT_PASS");
      errors = errors + g;

      // ── VECTORED：handler_pc = base + 4*cause ──
      g = 0;
      let v0 <- chk("VECTORED", 1'b1, base, 4'd0,  1'b1, base,             1'b1); g = g + v0;
      let v1 <- chk("VECTORED", 1'b1, base, 4'd1,  1'b1, base + 32'h4,     1'b1); g = g + v1;
      let v2 <- chk("VECTORED", 1'b1, base, 4'd8,  1'b1, base + 32'h20,    1'b1); g = g + v2;
      let v3 <- chk("VECTORED", 1'b1, base, 4'd15, 1'b1, base + 32'h3C,    1'b1); g = g + v3;
      if (g == 0) $display("VECTORED_PASS");
      errors = errors + g;

      // ── DISPATCH：混合流；accept 跟随 trap_req ──
      g = 0;
      let p0 <- chk("DISPATCH", 1'b1, base, 4'd2, 1'b1, base + 32'h08, 1'b1); g = g + p0;
      let p1 <- chk("DISPATCH", 1'b1, base, 4'd5, 1'b0, base + 32'h14, 1'b0); g = g + p1;
      let p2 <- chk("DISPATCH", 1'b0, base, 4'd9, 1'b1, base,          1'b1); g = g + p2;
      if (g == 0) $display("DISPATCH_PASS");
      errors = errors + g;

      if (errors == 0) begin
         $display("S1_PASS");
         $display("ALL_PASS");
      end
      else $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
