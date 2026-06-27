// 板级地址译码器 —— 硬件路径（BlueSpec SV）。
// sel  = 地址落在 [base, base+size) 窗口内；rdata = 命中时设备应答 magic|offset。
// 与软件 bsp_probe / Verilog bsp_decode 完全同构。你只需填 bsp_decode 函数体；tb 勿改。
package BspDecode;

// ── 核心逻辑：填这个函数体 ────────────────────────────────────────
function Tuple2#(Bit#(1), Bit#(32)) bsp_decode(Bit#(32) base, Bit#(32) size, Bit#(32) addr);
   Bit#(32) magic = 32'hDEC0_0000;
   // TODO: 写出地址译码逻辑：
   //   sel   = ((addr >= base) && (addr < (base + size))) ? 1'b1 : 1'b0;
   //   rdata = (sel == 1) ? (magic | (addr - base)) : 32'h0;
   //   return tuple2(sel, rdata);
   Bit#(1)  sel   = ((addr & 32'b0) != 0) ? 1'b1 : 1'b0;                          // ← 占位：读 addr
   Bit#(32) rdata = (addr & 32'b0) | (magic & 32'b0) | (base & 32'b0) | (size & 32'b0); // ← 占位：读 magic/base/size，恒 0 → 判 FAIL
   return tuple2(sel, rdata);
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function ActionValue#(Bit#(32)) chk(String lab, Bit#(32) base, Bit#(32) addr,
                                    Bit#(1) exp_sel, Bit#(32) exp_rdata);
   actionvalue
      match {.sel, .rdata} = bsp_decode(base, 32'h0000_1000, addr);
      Bit#(1) bad = 0;
      if (sel != exp_sel) bad = 1;
      if ((exp_sel == 1) && (rdata != exp_rdata)) bad = 1;
      if (bad == 1) begin
         $display("%s_FAIL addr=0x%08h sel=%b(exp %b) rdata=0x%08h(exp 0x%08h)",
                  lab, addr, sel, exp_sel, rdata, exp_rdata);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbBsp (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) baseA = 32'h1000_0000;
      Bit#(32) baseB = 32'h1002_0000;
      Bit#(32) magic = 32'hDEC0_0000;
      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      // 板 A：窗口内应答、窗口外/板 B 基址不应答
      g = 0;
      let a0 <- chk("DECODE_A", baseA, 32'h1000_0000, 1, magic | 32'h0000_0000); g = g + a0;
      let a1 <- chk("DECODE_A", baseA, 32'h1000_0FFF, 1, magic | 32'h0000_0FFF); g = g + a1;
      let a2 <- chk("DECODE_A", baseA, 32'h1000_1000, 0, 0);                     g = g + a2;
      let a3 <- chk("DECODE_A", baseA, 32'h0FFF_FFFF, 0, 0);                     g = g + a3;
      let a4 <- chk("DECODE_A", baseA, 32'h1002_0000, 0, 0);                     g = g + a4;
      if (g == 0) $display("DECODE_A_PASS");
      errors = errors + g;

      // 板 B：窗口内应答、板 A 基址不应答
      g = 0;
      let b0 <- chk("DECODE_B", baseB, 32'h1002_0000, 1, magic | 32'h0000_0000); g = g + b0;
      let b1 <- chk("DECODE_B", baseB, 32'h1002_0FFF, 1, magic | 32'h0000_0FFF); g = g + b1;
      let b2 <- chk("DECODE_B", baseB, 32'h1002_1000, 0, 0);                     g = g + b2;
      let b3 <- chk("DECODE_B", baseB, 32'h1000_0000, 0, 0);                     g = g + b3;
      if (g == 0) $display("DECODE_B_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
