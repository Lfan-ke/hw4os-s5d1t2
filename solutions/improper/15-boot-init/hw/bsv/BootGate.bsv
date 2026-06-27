// 引导入门 · 启动握手门 —— 硬件路径（BlueSpec SV 参考解）。
// 纯组合译码函数：与软件 mmio_read / Verilog boot_gate 完全同构。
// 未就绪读 DATA 吐 0x0BADB007，就绪后给变换值 data_raw^0xCAFE。
package BootGate;

// ── 核心逻辑：学生只需填这个函数体 ────────────────────────────────
function Bit#(32) boot_decode(Bool unlocked, Bit#(4) clkdiv, Bool en,
                              Bit#(16) data_raw, Bit#(3) addr);
   Bool clkdiv_valid = (clkdiv != 0);                 // 1..15 合法，0 非法
   Bool ready        = unlocked && en && clkdiv_valid; // 三者齐备才就绪

   Bit#(32) st_ready  = 32'h0000_0001;
   Bit#(32) st_locked = 32'h0000_0002;
   Bit#(32) st_badclk = 32'h0000_0004;
   Bit#(32) st_noten  = 32'h0000_0008;
   Bit#(32) badboot   = 32'h0BAD_B007;
   Bit#(32) data_xform = zeroExtend(data_raw ^ 16'hCAFE);

   Bit#(32) outp;
   if (addr == 3'd3) begin            // A_STATUS：拼状态字
      outp = (ready        ? st_ready  : 0)
           | (unlocked     ? 0 : st_locked)
           | (clkdiv_valid ? 0 : st_badclk)
           | (en           ? 0 : st_noten);
   end else if (addr == 3'd4) begin   // A_DATA
      outp = ready ? data_xform : badboot;
   end else begin
      outp = 0;
   end
   return outp;
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function ActionValue#(Bit#(32)) chk(String tag, Bit#(32) got, Bit#(32) exp);
   actionvalue
      if (got != exp) begin
         $display("%s_FAIL exp=0x%08h got=0x%08h", tag, exp, got);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbBoot (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      // ── 15.1 LOCK：未握手（全默认）直接用 DATA 应被拒、STATUS LOCKED ──
      g = 0;
      let l0 <- chk("LOCK", boot_decode(False, 0, False, 0, 3'd4), 32'h0BAD_B007); g = g + l0;
      let l1 <- chk("LOCK", boot_decode(False, 0, False, 0, 3'd3), 32'h0000_000E); g = g + l1; // LOCKED|BADCLK|NOTEN
      if (g == 0) $display("LOCK_PASS");
      errors = errors + g;

      // ── 15.2 BOOT：四步握手后 STATUS.READY=1 ──
      g = 0;
      let b0 <- chk("BOOT", boot_decode(True, 4'd5, True, 0, 3'd3), 32'h0000_0001); g = g + b0;
      if (g == 0) $display("BOOT_PASS");
      errors = errors + g;

      // ── 15.2 USE：写 DATA 读回变换值 0x1234^0xCAFE=0xD8CA ──
      g = 0;
      let u0 <- chk("USE", boot_decode(True, 4'd5, True, 16'h1234, 3'd4), 32'h0000_D8CA); g = g + u0;
      if (g == 0) $display("USE_PASS");
      errors = errors + g;

      // ── 15.3 ORDER：抢跑被拒、握手后正常 ──
      g = 0;
      let o0 <- chk("ORDER", boot_decode(False, 0, False, 16'h00AA, 3'd4), 32'h0BAD_B007); g = g + o0;
      let o1 <- chk("ORDER", boot_decode(True, 4'd7, True, 16'h00AA, 3'd4), 32'h0000_CA54); g = g + o1;
      if (g == 0) $display("ORDER_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
