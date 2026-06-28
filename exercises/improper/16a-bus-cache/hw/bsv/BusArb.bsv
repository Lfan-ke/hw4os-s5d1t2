// 16a · 总线与缓存 - 仲裁器（BlueSpec SV，学生填空版）。
// 与 Verilog/软件四语抄的“同一台译码器”：仲裁 = 地址区间译码（§2.2）。
// addr 落区间 → Dev（DevReg/DevSensor/DevSwitch），全落空 = DevErr（总线错误）。
// 你只需在 route 方法里填“仲裁区间判定”（下方 ── 学生填 ──）；tb/期望路由表勿改。
// 注：$display 仅 ASCII（中文只放注释）。
package BusArb;

typedef Bit#(32) Addr;
typedef enum { DevReg, DevSensor, DevSwitch, DevErr } Dev deriving (Bits, Eq);

interface BusArb;
   method Dev route(Addr a);
endinterface

(* synthesize *)
module mkBusArb (BusArb);
   method Dev route(Addr a);
      // ── 学生填：仲裁区间判定（addr 落区间 → Dev）──
      // TODO: 按 §2.2 补全 sensor/switchdev 区间，及各段上界（`< END`）判定。
      if (a >= 32'h4000_0000) return DevReg; // ← 占位：缺上界与其它区间 → 路由不符
      else return DevErr;
   endmethod
endmodule

// 同一组 8 个地址 + 同一张期望路由（与软件四语逐位一致的 ARB_PASS 场景）。
function Addr addrOf(Bit#(4) i);
   case (i)
      0: return 32'h4000_0000; 1: return 32'h4000_0FFC;
      2: return 32'h4001_0000; 3: return 32'h4001_000C;
      4: return 32'h4002_0000; 5: return 32'h4002_0008;
      6: return 32'h4003_0000; default: return 32'h3FFF_FFFC;
   endcase
endfunction
function Dev expOf(Bit#(4) i);
   case (i)
      0, 1: return DevReg; 2, 3: return DevSensor; 4, 5: return DevSwitch;
      default: return DevErr;
   endcase
endfunction

// ── 测试 harness（给定，勿改）：tb 当参考驱动。值方法 route(a) 每周期对每个实参占一端口，
//    故每条 rule 只 route 一个地址（与 Verilog 的 #1 重摆 addr 等价）。──
(* synthesize *)
module mkTbBusArb (Empty);
   BusArb         dut    <- mkBusArb;
   Reg#(Bit#(4))  idx    <- mkReg(0);
   Reg#(Bit#(16)) errors <- mkReg(0);

   rule step (idx < 8);
      Dev got = dut.route(addrOf(idx));
      if (got != expOf(idx)) begin
         $display("ARB_FAIL idx=%0d", idx);
         errors <= errors + 1;
      end
      idx <= idx + 1;
   endrule

   rule done (idx == 8);
      if (errors == 0) begin
         $display("ARB_PASS bus arbitration = address-range decode (8/8 routed)");
         $display("DEV_PASS one-hot select / bus_err on out-of-range");
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", errors);
      $finish(0);
   endrule
endmodule

endpackage
