// 设备↔OS 的 MMIO 共享环 mailbox —— 硬件路径（BlueSpec SV 参考解）。
// 定深=4 的 ring 写成纯函数（状态 struct 经 let 串起），与软件 Ring / Verilog
// ring_mbox 完全同构。学生只填 ring_push / ring_pop。
package ShareMem;

import Vector::*;

typedef 4 Cap;

typedef struct {
   Vector#(Cap, Bit#(32)) buff;
   Bit#(2)                head;
   Bit#(2)                tail;
   Bit#(3)                cnt;   // 0..4
} Ring deriving (Bits, Eq);

function Ring newRing();
   return Ring { buff: replicate(0), head: 0, tail: 0, cnt: 0 };
endfunction

function Bool ring_avail(Ring r) = (r.cnt != 3'd0);
function Bool ring_full(Ring r)  = (r.cnt == 3'd4);

// ── 核心逻辑：学生填这两个函数 ────────────────────────────────────
// 入队：满则 (r, False)；否则写 tail、抬 tail、cnt+1，返回 (新环, True)。
function Tuple2#(Ring, Bool) ring_push(Ring r, Bit#(32) x);
   if (ring_full(r))
      return tuple2(r, False);
   else begin
      Ring n = r;
      n.buff = update(r.buff, r.tail, x);
      n.tail = r.tail + 2'd1;   // 2bit 自然环绕
      n.cnt  = r.cnt + 3'd1;
      return tuple2(n, True);
   end
endfunction

// 出队：空则 (r, False, 0)；否则读 head、抬 head、cnt-1，返回 (新环, True, 队首)。
function Tuple3#(Ring, Bool, Bit#(32)) ring_pop(Ring r);
   if (!ring_avail(r))
      return tuple3(r, False, 32'd0);
   else begin
      Ring n = r;
      Bit#(32) v = select(r.buff, r.head);
      n.head = r.head + 2'd1;
      n.cnt  = r.cnt - 3'd1;
      return tuple3(n, True, v);
   end
endfunction

// ── 测试 harness（给定，勿改）──────────────────────────────────────
(* synthesize *)
module mkTbShareMem (Empty);
   Reg#(Bool) done <- mkReg(False);

   rule run (!done);
      Bit#(32) errors = 0;
      Bit#(32) g = 0;
      Ring r = newRing();

      // ── RING 框架：填满 / 满拒绝 / FIFO 排空 / 空 / 环绕 ──
      g = 0;
      let p1 = ring_push(r, 32'd11); r = tpl_1(p1); if (!tpl_2(p1)) g = g + 1;
      let p2 = ring_push(r, 32'd22); r = tpl_1(p2); if (!tpl_2(p2)) g = g + 1;
      let p3 = ring_push(r, 32'd33); r = tpl_1(p3); if (!tpl_2(p3)) g = g + 1;
      let p4 = ring_push(r, 32'd44); r = tpl_1(p4); if (!tpl_2(p4)) g = g + 1;
      let p5 = ring_push(r, 32'd55); r = tpl_1(p5); if (tpl_2(p5))  g = g + 1; // 满应拒绝
      let o1 = ring_pop(r); r = tpl_1(o1); if (!tpl_2(o1) || tpl_3(o1) != 32'd11) g = g + 1;
      let o2 = ring_pop(r); r = tpl_1(o2); if (!tpl_2(o2) || tpl_3(o2) != 32'd22) g = g + 1;
      let o3 = ring_pop(r); r = tpl_1(o3); if (!tpl_2(o3) || tpl_3(o3) != 32'd33) g = g + 1;
      let o4 = ring_pop(r); r = tpl_1(o4); if (!tpl_2(o4) || tpl_3(o4) != 32'd44) g = g + 1;
      let o5 = ring_pop(r); r = tpl_1(o5); if (tpl_2(o5)) g = g + 1;            // 空应失败
      // 环绕（head/tail 已推进）
      let w1 = ring_push(r, 32'd61); r = tpl_1(w1); if (!tpl_2(w1)) g = g + 1;
      let w2 = ring_push(r, 32'd62); r = tpl_1(w2); if (!tpl_2(w2)) g = g + 1;
      let w3 = ring_push(r, 32'd63); r = tpl_1(w3); if (!tpl_2(w3)) g = g + 1;
      let q1 = ring_pop(r); r = tpl_1(q1); if (!tpl_2(q1) || tpl_3(q1) != 32'd61) g = g + 1;
      let q2 = ring_pop(r); r = tpl_1(q2); if (!tpl_2(q2) || tpl_3(q2) != 32'd62) g = g + 1;
      let q3 = ring_pop(r); r = tpl_1(q3); if (!tpl_2(q3) || tpl_3(q3) != 32'd63) g = g + 1;
      if (g == 0) $display("RING_PASS");
      errors = errors + g;

      // ── MMIO 框架：设备 doorbell 连写、OS 边读边补写（环绕）──
      g = 0;
      Ring m = newRing();
      let d0 = ring_push(m, 32'hD0); m = tpl_1(d0); if (!tpl_2(d0)) g = g + 1;
      let d1 = ring_push(m, 32'hD1); m = tpl_1(d1); if (!tpl_2(d1)) g = g + 1;
      let d2 = ring_push(m, 32'hD2); m = tpl_1(d2); if (!tpl_2(d2)) g = g + 1;
      let r0 = ring_pop(m); m = tpl_1(r0); if (!tpl_2(r0) || tpl_3(r0) != 32'hD0) g = g + 1;
      let r1 = ring_pop(m); m = tpl_1(r1); if (!tpl_2(r1) || tpl_3(r1) != 32'hD1) g = g + 1;
      let d3 = ring_push(m, 32'hD3); m = tpl_1(d3); if (!tpl_2(d3)) g = g + 1;
      let d4 = ring_push(m, 32'hD4); m = tpl_1(d4); if (!tpl_2(d4)) g = g + 1;
      let r2 = ring_pop(m); m = tpl_1(r2); if (!tpl_2(r2) || tpl_3(r2) != 32'hD2) g = g + 1;
      let r3 = ring_pop(m); m = tpl_1(r3); if (!tpl_2(r3) || tpl_3(r3) != 32'hD3) g = g + 1;
      let r4 = ring_pop(m); m = tpl_1(r4); if (!tpl_2(r4) || tpl_3(r4) != 32'hD4) g = g + 1;
      if (ring_avail(m)) g = g + 1;   // 应排空
      if (g == 0) $display("MMIO_SHM_PASS");
      errors = errors + g;

      if (errors == 0) $display("ALL_PASS");
      else             $display("SOME_FAIL errors=%0d", errors);
      done <= True;
      $finish(0);
   endrule
endmodule

endpackage
