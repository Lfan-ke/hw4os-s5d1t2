// Tb.bsv — minimal Bluespec SystemVerilog design + testbench for the StarryOS
// bluesv case (#764). bsc compiles this to Verilog; the generated Verilog is then
// simulated by iverilog (vvp) and verilator on StarryOS. Deterministic $display.
package Tb;

interface Counter;
   method Action inc();
   method Bit#(8) value();
endinterface

(* synthesize *)
module mkCounter(Counter);
   Reg#(Bit#(8)) cnt <- mkReg(0);
   method Action inc(); cnt <= cnt + 1; endmethod
   method Bit#(8) value(); return cnt; endmethod
endmodule

(* synthesize *)
module mkTb(Empty);
   Counter c <- mkCounter;
   Reg#(Bit#(8)) i <- mkReg(0);
   rule run;
      if (i >= 8) begin
         $display("BLUESV_COUNT=%0d", c.value());
         $display("BLUESV_SIM_OK");
         $finish(0);
      end else begin
         c.inc();
         i <= i + 1;
      end
   endrule
endmodule

endpackage
