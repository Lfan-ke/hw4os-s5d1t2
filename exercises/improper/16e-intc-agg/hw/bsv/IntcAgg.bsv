// 16e - interrupt aggregation + multi-core arbitration -- device side (BlueSpec SV, student fill-in).
// PLIC-style aggregator: one shared IRQ source vs N=4 hart-contexts.
//   claim   : atomically take the gateway -- only the first claiming hart gets IRQ_ID, others read 0.
//   complete: return the gateway -- inflight clears, source can fire again (missing it stalls forever).
//   IPI     : set target hart's MSIP (software interrupt) for cross-core coordination.
// Fill two spots: the claim grant (TODO1) and the IPI MSIP set (TODO2); the read mux / tb are given.
// NOTE: $display is ASCII only (bsc constraint); Chinese stays in comments.
package IntcAgg;

typedef Bit#(2)  Hart;
typedef Bit#(32) Word;

Word irqId = 32'd7;

interface IntcAgg;
   method Action               assertIrq;
   method ActionValue#(Word)   claim(Hart hart);
   method Action               complete(Hart hart, Word id);
   method Action               ipiSend(Hart target);
   method Action               ipiClear(Hart hart);
   method Bit#(4)              msip;
   method Hart                 winner;
   method Bool                 won;
endinterface

(* synthesize *)
module mkIntcAgg (IntcAgg);
   Reg#(Bool)     pending  <- mkReg(False);
   Reg#(Bool)     inflight <- mkReg(False);
   Reg#(Bool)     wonR     <- mkReg(False);
   Reg#(Hart)     winnerR  <- mkReg(0);
   Reg#(Bit#(4))  msipR    <- mkReg(0);

   method Action assertIrq;
      pending <= True;
   endmethod

   // ── claim = atomic gateway take ──
   method ActionValue#(Word) claim(Hart hart);
      Word id = 0;
      // TODO1: only when pending and the gateway is free (!inflight) does this hart win:
      //   set id = irqId, lock the gateway (inflight <= True), record winner (wonR <= True; winnerR <= hart).
      if (pending && !inflight) begin
         winnerR <= hart; // placeholder: recorded the hart but never granted irqId / locked gateway -> no winner
      end
      return id;
   endmethod

   // complete = return gateway: re-arm so source can fire again.
   method Action complete(Hart hart, Word id);
      if (id == irqId && hart == winnerR)
         inflight <= False;
   endmethod

   method Action ipiSend(Hart target);
      // TODO2: set target hart's MSIP bit (ring the software interrupt).
      //   HINT: msipR <= msipR | (4'b1 << target);
      msipR <= msipR & ~(4'b1 << target); // placeholder: clears instead of sets -> follower never sees IPI
   endmethod
   method Action ipiClear(Hart hart);
      msipR <= msipR & ~(4'b1 << hart);
   endmethod

   method Bit#(4) msip   = msipR;
   method Hart    winner = winnerR;
   method Bool    won    = wonR;
endmodule

// ── test harness (given, do not edit): same aggregation scenario as the software variants ──
// One action(value) method per rule (BSV: a register written once per rule; ActionValue called once).
(* synthesize *)
module mkTbIntcAgg (Empty);
   IntcAgg        dut    <- mkIntcAgg;
   Reg#(Bit#(8))  step   <- mkReg(0);
   Reg#(Bit#(16)) errors <- mkReg(0);

   rule s0 (step == 0);
      dut.assertIrq;
      step <= 1;
   endrule

   rule s1 (step == 1);             // hart0 claims first -> IRQ_ID
      let id <- dut.claim(0);
      if (id != irqId) begin $display("ARB_FAIL hart0 claim=0x%08h", id); errors <= errors + 1; end
      step <= 2;
   endrule

   rule s2 (step == 2);             // hart1 -> gateway in-flight -> 0
      let id <- dut.claim(1);
      if (id == irqId) begin $display("ARB_FAIL hart1 also claimed"); errors <= errors + 1; end
      step <= 3;
   endrule

   rule s3 (step == 3);             // hart2 -> 0
      let id <- dut.claim(2);
      if (id == irqId) begin $display("ARB_FAIL hart2 also claimed"); errors <= errors + 1; end
      step <= 4;
   endrule

   rule s4 (step == 4);             // hart3 -> 0
      let id <- dut.claim(3);
      if (id == irqId) begin $display("ARB_FAIL hart3 also claimed"); errors <= errors + 1; end
      step <= 5;
   endrule

   rule s5 (step == 5);             // missing complete: re-claim still 0 (stalled)
      let id <- dut.claim(0);
      if (id != 0) begin $display("DEV_FAIL reclaim before complete=0x%08h", id); errors <= errors + 1; end
      step <= 6;
   endrule

   rule s6 (step == 6);             // complete -> re-arm gateway
      dut.complete(0, irqId);
      step <= 7;
   endrule

   rule s7 (step == 7);             // re-armed: claim again gets IRQ_ID
      let id <- dut.claim(0);
      if (id != irqId) begin $display("DEV_FAIL not re-armed after complete=0x%08h", id); errors <= errors + 1; end
      step <= 8;
   endrule

   rule s8 (step == 8);
      dut.complete(0, irqId);
      step <= 9;
   endrule

   rule s9  (step == 9);  dut.ipiSend(1); step <= 10; endrule
   rule s10 (step == 10); dut.ipiSend(2); step <= 11; endrule
   rule s11 (step == 11); dut.ipiSend(3); step <= 12; endrule

   rule s12 (step == 12);            // hart0 rang MSIP of 1/2/3, not its own
      if (dut.msip != 4'b1110) begin $display("IPI_FAIL msip=0x%01h", dut.msip); errors <= errors + 1; end
      step <= 13;
   endrule

   rule s13 (step == 13); dut.ipiClear(1); step <= 14; endrule
   rule s14 (step == 14); dut.ipiClear(2); step <= 15; endrule
   rule s15 (step == 15); dut.ipiClear(3); step <= 16; endrule

   rule s16 (step == 16);            // all acks cleared MSIP; arbitration unique
      Bit#(16) e = errors;
      if (dut.msip != 0) begin $display("IPI_FAIL msip after clear=0x%01h", dut.msip); e = e + 1; end
      if (!dut.won || dut.winner != 0) begin
         $display("ARB_FAIL won=%b winner=%0d", dut.won, dut.winner); e = e + 1;
      end
      errors <= e;
      step <= 17;
   endrule

   rule s_done (step == 17);
      if (errors == 0) begin
         $display("ARBITER_PASS one IRQ7 handled by hart0 only: 3 contenders claimed 0");
         $display("DEV_PASS  gateway re-armed after complete (missing complete stalls source)");
         $display("IPI_PASS  hart0 rang 3 MSIPs, followers acked and cleared");
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", errors);
      $finish(0);
   endrule
endmodule

endpackage
