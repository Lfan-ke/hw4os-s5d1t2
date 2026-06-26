// 简化 VLAN Tag 处理 —— 硬件路径（BlueSpec SV 参考解）。
// 包字: [31]VALID [30]HAS_TAG [29]DROP [28]DIR(in) [21:16]VID(6b) [15:0]PAYLOAD
// 纯组合逻辑函数：与软件 process() / Verilog vlan_proc 完全同构。
package VlanProc;

// ── 核心逻辑：学生只需填这个函数体 ────────────────────────────────
function Bit#(32) vlan_process(Bit#(2) mode, Bit#(6) pvid,
                               Bit#(64) allow, Bit#(64) untag, Bit#(32) inp);
   Bit#(1)  has_tag = inp[30];
   Bit#(1)  egress  = inp[28];
   Bit#(6)  vid     = inp[21:16];
   Bit#(16) payload = inp[15:0];

   Bit#(32) op_strip  = 32'h8000_0000 | zeroExtend(payload);
   Bit#(32) op_insert = 32'h8000_0000 | 32'h4000_0000 | (zeroExtend(pvid) << 16) | zeroExtend(payload);
   Bit#(32) op_keep   = 32'h8000_0000 | (inp & (32'h4000_0000 | (32'h3F << 16) | 32'h0000_FFFF));
   Bit#(32) op_drop   = 32'h8000_0000 | 32'h2000_0000;

   Bit#(1) allowed  = allow[vid];
   Bit#(1) do_untag = untag[vid];

   Bit#(32) outp;
   if (egress == 0) begin
      // 收包 ingress
      if (mode == 2'd0)
         outp = (has_tag == 1) ? op_strip : op_insert;                 // Access
      else
         outp = (has_tag == 0) ? op_drop : ((allowed == 0) ? op_drop : op_keep); // Trunk/Hybrid
   end else begin
      // 发包 egress
      if (mode == 2'd0)
         outp = op_strip;                                              // Access
      else if (mode == 2'd1)
         outp = op_keep;                                               // Trunk
      else
         outp = ((has_tag == 1) && (do_untag == 1)) ? op_strip : op_keep; // Hybrid
   end
   return outp;
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function Bit#(32) mk(Bit#(1) dir, Bit#(1) tag, Bit#(6) vid, Bit#(16) pl);
   return 32'h8000_0000
        | ((dir == 1) ? 32'h1000_0000 : 32'h0)
        | ((tag == 1) ? 32'h4000_0000 : 32'h0)
        | (zeroExtend(vid) << 16)
        | zeroExtend(pl);
endfunction

function ActionValue#(Bit#(32)) chk(String tag, Bit#(2) mode, Bit#(6) pvid,
                                    Bit#(64) allow, Bit#(64) untag,
                                    Bit#(32) inp, Bit#(32) exp);
   actionvalue
      Bit#(32) got = vlan_process(mode, pvid, allow, untag, inp);
      if (got != exp) begin
         $display("%s_FAIL in=0x%08h exp=0x%08h got=0x%08h", tag, inp, exp, got);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbVlan (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(64) allow = (64'd1 << 10) | (64'd1 << 20);
      Bit#(64) untag = (64'd1 << 10);
      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      // ── ACCESS, pvid=5 ──
      g = 0;
      let a0 <- chk("ACCESS", 2'd0, 6'd5, 0, 0, mk(0, 1, 10, 16'h1234), 32'h8000_1234); g = g + a0;
      let a1 <- chk("ACCESS", 2'd0, 6'd5, 0, 0, mk(0, 0, 0,  16'h1234), 32'hC005_1234); g = g + a1;
      let a2 <- chk("ACCESS", 2'd0, 6'd5, 0, 0, mk(1, 1, 10, 16'h1234), 32'h8000_1234); g = g + a2;
      if (g == 0) $display("ACCESS_PASS");
      errors = errors + g;

      // ── TRUNK, allow={10,20} ──
      g = 0;
      let t0 <- chk("TRUNK", 2'd1, 6'd0, allow, 0, mk(0, 1, 10, 16'hABCD), 32'hC00A_ABCD); g = g + t0;
      let t1 <- chk("TRUNK", 2'd1, 6'd0, allow, 0, mk(0, 1, 30, 16'h1111), 32'hA000_0000); g = g + t1;
      let t2 <- chk("TRUNK", 2'd1, 6'd0, allow, 0, mk(0, 0, 0,  16'h2222), 32'hA000_0000); g = g + t2;
      let t3 <- chk("TRUNK", 2'd1, 6'd0, allow, 0, mk(1, 1, 10, 16'hABCD), 32'hC00A_ABCD); g = g + t3;
      if (g == 0) $display("TRUNK_PASS");
      errors = errors + g;

      // ── HYBRID, allow={10,20}, untag={10} ──
      g = 0;
      let h0 <- chk("HYBRID", 2'd2, 6'd0, allow, untag, mk(0, 1, 20, 16'h0F0F), 32'hC014_0F0F); g = g + h0;
      let h1 <- chk("HYBRID", 2'd2, 6'd0, allow, untag, mk(0, 1, 30, 16'h3333), 32'hA000_0000); g = g + h1;
      let h2 <- chk("HYBRID", 2'd2, 6'd0, allow, untag, mk(1, 1, 10, 16'h0F0F), 32'h8000_0F0F); g = g + h2;
      let h3 <- chk("HYBRID", 2'd2, 6'd0, allow, untag, mk(1, 1, 20, 16'h0F0F), 32'hC014_0F0F); g = g + h3;
      if (g == 0) $display("HYBRID_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
