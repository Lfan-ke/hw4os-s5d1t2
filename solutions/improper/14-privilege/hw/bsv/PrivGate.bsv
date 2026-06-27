// 三态特权机 —— 硬件路径（BlueSpec SV，参考解）。
// 状态字 csr[4:0] = { saved_priv[4:3], feat_en[2], cur_priv[1:0] }
// 纯组合逻辑函数：与软件 step() / Verilog priv_gate 完全同构。
// 返回 Bit#(6) = { trap, csr_o[4:0] }。
//
// 特权级：A(最高)=2'd2，B=2'd1，C(最低)=2'd0。
package PrivGate;

// ── 核心逻辑：填这个函数体 ────────────────────────────────────────
function Bit#(6) priv_step(Bit#(5) csr, Bit#(3) kind, Bit#(2) arg_priv, Bit#(1) arg_en);
   Bit#(2) cur = csr[1:0];      // 当前态（触发器读出）
   Bit#(1) fe  = csr[2];        // 功能使能位
   Bit#(2) sp  = csr[4:3];      // saved_priv

   // 默认：csr 不变、不陷入。各分支只改需要改的。
   Bit#(2) ncur = cur;
   Bit#(1) nfe  = fe;
   Bit#(2) nsp  = sp;
   Bit#(1) trap = 0;

   case (kind)
      3'd0: begin // NORMAL：一根比较器 cur < 需要的等级 → 没权限
         trap = (cur < arg_priv) ? 1 : 0;
      end
      3'd1: begin // DROP：向下放权 = 写低位；上行非法
         if (arg_priv > cur) trap = 1;
         else                ncur = arg_priv;
      end
      3'd2: begin // ECALL：合法陷入，保存前态、进最高态
         nsp  = cur;
         ncur = 2'd2; // A
      end
      3'd3: begin // XRET：恢复 saved_priv；非最高态不得 xret
         if (cur != 2'd2) trap = 1;
         else             ncur = sp;
      end
      3'd4: begin // SETFEAT：置/清使能位，至少 B 态
         if (cur < 2'd1) trap = 1;
         else            nfe = arg_en;
      end
      3'd5: begin // USEFEAT：特权够 且 使能位亮，缺一不可
         trap = ((cur < arg_priv) || (fe == 0)) ? 1 : 0;
      end
      default: begin
         ncur = cur;
      end
   endcase

   Bit#(5) ocsr = {nsp, nfe, ncur};
   return {trap, ocsr};
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
function Bit#(5) mkcsr(Bit#(2) cur, Bit#(1) fe, Bit#(2) sp);
   return {sp, fe, cur};
endfunction

function ActionValue#(Bit#(32)) chk(String tag, Bit#(5) csr, Bit#(3) kind,
                                    Bit#(2) arg_priv, Bit#(1) arg_en,
                                    Bit#(5) exp_csr, Bit#(1) exp_trap);
   actionvalue
      Bit#(6) r = priv_step(csr, kind, arg_priv, arg_en);
      Bit#(1) gt = r[5];
      Bit#(5) gc = r[4:0];
      if (gc != exp_csr || gt != exp_trap) begin
         $display("%s_FAIL csr=0x%02h kind=%0d ap=%0d ae=%0d | exp(csr=0x%02h,trap=%0d) got(csr=0x%02h,trap=%0d)",
                  tag, csr, kind, arg_priv, arg_en, exp_csr, exp_trap, gc, gt);
         return 1;
      end
      else begin
         return 0;
      end
   endactionvalue
endfunction

(* synthesize *)
module mkTbPriv (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) errors = 0;
      Bit#(32) g = 0;

      // 子实验 1：CMP
      g = 0;
      let c0 <- chk("CMP", mkcsr(2'd2,0,2'd0), 3'd0, 2'd2, 0, mkcsr(2'd2,0,2'd0), 0); g = g + c0;
      let c1 <- chk("CMP", mkcsr(2'd0,0,2'd0), 3'd0, 2'd2, 0, mkcsr(2'd0,0,2'd0), 1); g = g + c1;
      let c2 <- chk("CMP", mkcsr(2'd1,0,2'd0), 3'd0, 2'd1, 0, mkcsr(2'd1,0,2'd0), 0); g = g + c2;
      let c3 <- chk("CMP", mkcsr(2'd1,0,2'd0), 3'd0, 2'd2, 0, mkcsr(2'd1,0,2'd0), 1); g = g + c3;
      let c4 <- chk("CMP", mkcsr(2'd0,0,2'd0), 3'd0, 2'd0, 0, mkcsr(2'd0,0,2'd0), 0); g = g + c4;
      let c5 <- chk("CMP", mkcsr(2'd2,0,2'd0), 3'd0, 2'd0, 0, mkcsr(2'd2,0,2'd0), 0); g = g + c5;
      if (g == 0) $display("CMP_PASS");
      errors = errors + g;

      // 子实验 2：DROP
      g = 0;
      let d0 <- chk("DROP", mkcsr(2'd2,0,2'd0), 3'd1, 2'd1, 0, mkcsr(2'd1,0,2'd0), 0); g = g + d0;
      let d1 <- chk("DROP", mkcsr(2'd2,0,2'd0), 3'd1, 2'd0, 0, mkcsr(2'd0,0,2'd0), 0); g = g + d1;
      let d2 <- chk("DROP", mkcsr(2'd1,0,2'd0), 3'd1, 2'd0, 0, mkcsr(2'd0,0,2'd0), 0); g = g + d2;
      let d3 <- chk("DROP", mkcsr(2'd0,0,2'd0), 3'd1, 2'd2, 0, mkcsr(2'd0,0,2'd0), 1); g = g + d3;
      let d4 <- chk("DROP", mkcsr(2'd1,0,2'd0), 3'd1, 2'd2, 0, mkcsr(2'd1,0,2'd0), 1); g = g + d4;
      let d5 <- chk("DROP", mkcsr(2'd1,0,2'd0), 3'd1, 2'd1, 0, mkcsr(2'd1,0,2'd0), 0); g = g + d5;
      if (g == 0) $display("DROP_PASS");
      errors = errors + g;

      // 子实验 3：TRAP（ECALL/XRET）
      g = 0;
      let t0 <- chk("TRAP", mkcsr(2'd0,0,2'd0), 3'd2, 2'd0, 0, mkcsr(2'd2,0,2'd0), 0); g = g + t0;
      let t1 <- chk("TRAP", mkcsr(2'd1,0,2'd0), 3'd2, 2'd0, 0, mkcsr(2'd2,0,2'd1), 0); g = g + t1;
      let t2 <- chk("TRAP", mkcsr(2'd2,0,2'd1), 3'd3, 2'd0, 0, mkcsr(2'd1,0,2'd1), 0); g = g + t2;
      let t3 <- chk("TRAP", mkcsr(2'd0,0,2'd0), 3'd3, 2'd0, 0, mkcsr(2'd0,0,2'd0), 1); g = g + t3;
      let t4 <- chk("TRAP", mkcsr(2'd1,0,2'd0), 3'd3, 2'd0, 0, mkcsr(2'd1,0,2'd0), 1); g = g + t4;
      let t5 <- chk("TRAP", mkcsr(2'd0,1,2'd0), 3'd2, 2'd0, 0, mkcsr(2'd2,1,2'd0), 0); g = g + t5;
      if (g == 0) $display("TRAP_PASS");
      errors = errors + g;

      // 子实验 4：FEAT（SETFEAT/USEFEAT）
      g = 0;
      let f0 <- chk("FEAT", mkcsr(2'd2,0,2'd0), 3'd4, 2'd0, 1, mkcsr(2'd2,1,2'd0), 0); g = g + f0;
      let f1 <- chk("FEAT", mkcsr(2'd1,0,2'd0), 3'd4, 2'd0, 1, mkcsr(2'd1,1,2'd0), 0); g = g + f1;
      let f2 <- chk("FEAT", mkcsr(2'd0,0,2'd0), 3'd4, 2'd0, 1, mkcsr(2'd0,0,2'd0), 1); g = g + f2;
      let f3 <- chk("FEAT", mkcsr(2'd2,1,2'd0), 3'd4, 2'd0, 0, mkcsr(2'd2,0,2'd0), 0); g = g + f3;
      let f4 <- chk("FEAT", mkcsr(2'd0,1,2'd0), 3'd5, 2'd0, 0, mkcsr(2'd0,1,2'd0), 0); g = g + f4;
      let f5 <- chk("FEAT", mkcsr(2'd0,0,2'd0), 3'd5, 2'd0, 0, mkcsr(2'd0,0,2'd0), 1); g = g + f5;
      let f6 <- chk("FEAT", mkcsr(2'd2,1,2'd0), 3'd5, 2'd1, 0, mkcsr(2'd2,1,2'd0), 0); g = g + f6;
      let f7 <- chk("FEAT", mkcsr(2'd0,1,2'd0), 3'd5, 2'd1, 0, mkcsr(2'd0,1,2'd0), 1); g = g + f7;
      let f8 <- chk("FEAT", mkcsr(2'd1,1,2'd0), 3'd5, 2'd1, 0, mkcsr(2'd1,1,2'd0), 0); g = g + f8;
      if (g == 0) $display("FEAT_PASS");
      errors = errors + g;

      // 子实验 5：CAPSTONE 轨迹（csr 串行喂入）
      g = 0;
      Bit#(5) ch = mkcsr(2'd2,0,2'd0); // A 启动
      let p0 <- chk("CAPSTONE", ch, 3'd1, 2'd1, 0, mkcsr(2'd1,0,2'd0), 0); g = g + p0; ch = mkcsr(2'd1,0,2'd0);
      let p1 <- chk("CAPSTONE", ch, 3'd4, 2'd0, 1, mkcsr(2'd1,1,2'd0), 0); g = g + p1; ch = mkcsr(2'd1,1,2'd0);
      let p2 <- chk("CAPSTONE", ch, 3'd1, 2'd0, 0, mkcsr(2'd0,1,2'd0), 0); g = g + p2; ch = mkcsr(2'd0,1,2'd0);
      let p3 <- chk("CAPSTONE", ch, 3'd5, 2'd1, 0, mkcsr(2'd0,1,2'd0), 1); g = g + p3; ch = mkcsr(2'd0,1,2'd0);
      let p4 <- chk("CAPSTONE", ch, 3'd2, 2'd0, 0, mkcsr(2'd2,1,2'd0), 0); g = g + p4; ch = mkcsr(2'd2,1,2'd0);
      let p5 <- chk("CAPSTONE", ch, 3'd3, 2'd0, 0, mkcsr(2'd0,1,2'd0), 0); g = g + p5;
      if (g == 0) $display("CAPSTONE_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
