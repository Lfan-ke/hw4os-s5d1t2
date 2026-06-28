// 16c · core-local interrupt - device side (BlueSpec SV, student fill-in).
// CLINT RTL = the source the software CLINT model mirrors:
//   timer = free-running mtime + mtimecmp comparator; msip = software-interrupt register.
// Fill in two combinational outputs: mtip (comparator) and msip (register read); write methods / tb: do not edit.
package Clint;

typedef Bit#(64) Time64;

interface Clint;
   method Action  setCmp(Time64 v);
   method Action  setMsip(Bit#(1) v);
   method Action  tick;
   method Time64  mtime;
   method Bool    mtip;     // timer interrupt pending (mtime >= mtimecmp)
   method Bit#(1) msip;     // software interrupt pending
endinterface

(* synthesize *)
module mkClint (Clint);
   Reg#(Time64)  mtimeR    <- mkReg(0);
   Reg#(Time64)  mtimecmpR <- mkReg(0);
   Reg#(Bit#(1)) msipR     <- mkReg(0);

   method Action  setCmp(Time64 v);   mtimecmpR <= v; endmethod
   method Action  setMsip(Bit#(1) v); msipR <= v;     endmethod
   method Action  tick;               mtimeR <= mtimeR + 1; endmethod
   method Time64  mtime = mtimeR;
   // ── student fill ── comparator + msip register read
   method Bool    mtip  = False;   // TODO: (mtimeR >= mtimecmpR)
   method Bit#(1) msip  = 0;       // TODO: msipR
endmodule

// ── test harness (given, do not edit): tb drives the CPU side, same scenario as the four sw variants ──
(* synthesize *)
module mkTbClint (Empty);
   Clint          dev       <- mkClint;
   Reg#(Bit#(8))  phase     <- mkReg(0);
   Reg#(Bit#(8))  tcnt      <- mkReg(0);
   Reg#(Bit#(8))  fires     <- mkReg(0);
   Reg#(Bit#(8))  ipis      <- mkReg(0);
   Reg#(Bit#(16)) errors    <- mkReg(0);
   Reg#(Time64)   cmpMirror <- mkReg(0);

   Integer  nTick    = 16;
   Integer  expTimer = 3;
   Integer  expSoft  = 2;
   Time64   period   = 5;

   rule setup (phase == 0);
      dev.setCmp(period);
      cmpMirror <= period;
      phase <= 1;
   endrule

   rule timer (phase == 1);
      if (dev.mtip) begin
         fires <= fires + 1;
         cmpMirror <= cmpMirror + period;
         dev.setCmp(cmpMirror + period);   // handler reload (also clears this MTIP)
      end
      dev.tick;
      if (tcnt == fromInteger(nTick - 1)) phase <= 2;
      tcnt <= tcnt + 1;
   endrule

   rule ipi_raise1 (phase == 2);
      dev.setMsip(1);
      phase <= 3;
   endrule
   rule ipi_handle1 (phase == 3);
      if (dev.msip == 1) ipis <= ipis + 1;
      else begin $display("DEV_FAIL msip not pending"); errors <= errors + 1; end
      dev.setMsip(0);
      phase <= 4;
   endrule
   rule ipi_check1 (phase == 4);
      if (dev.msip != 0) begin $display("DEV_FAIL msip not cleared"); errors <= errors + 1; end
      dev.setMsip(1);
      phase <= 5;
   endrule
   rule ipi_handle2 (phase == 5);
      if (dev.msip == 1) ipis <= ipis + 1;
      else begin $display("DEV_FAIL msip not pending"); errors <= errors + 1; end
      dev.setMsip(0);
      phase <= 6;
   endrule
   rule ipi_check2 (phase == 6);
      if (dev.msip != 0) begin $display("DEV_FAIL msip not cleared"); errors <= errors + 1; end
      phase <= 7;
   endrule

   rule report (phase == 7);
      Bit#(16) e = errors;
      if (!(fires == fromInteger(expTimer) && dev.mtime == fromInteger(nTick))) begin
         $display("DEV_FAIL timer fires=%0d mtime=%0d", fires, dev.mtime); e = e + 1;
      end
      if (ipis != fromInteger(expSoft)) begin
         $display("DEV_FAIL ipi=%0d", ipis); e = e + 1;
      end
      if (e == 0) begin
         $display("TIMER_PASS fires=%0d mtime=%0d", fires, dev.mtime);
         $display("SOFT_PASS  ipi=%0d", ipis);
         $display("DEV_PASS   mtimecmp comparator + msip register RTL ok");
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", e);
      $finish(0);
   endrule
endmodule

endpackage
