// 16b · 寄存器模型 - 设备侧（BlueSpec SV，参考解）。
// 与软件四语 / Verilog regdev 抄的“同一张布局表”的源头：字段即 Bit 切片。
// 寄存器（§2.1）：0x0 CTRL(RW EN/IE/MODE/RST) 0x4 STATUS(RO READY/BUSY/IRQ) 0x8 DATA(WO BYTE) 0xC ID(RO magic)。
package RegDev;

typedef Bit#(4)  Addr;
typedef Bit#(32) Word;

Word magic = 32'h5245_4744; // "REGD"

interface RegDev;
   method Action  write(Addr addr, Word wdata);
   method Word    read(Addr addr);
   method Bit#(8) captured; // 设备捕获的 DATA 字节（调试抽头）
endinterface

(* synthesize *)
module mkRegDev (RegDev);
   Reg#(Word)     ctrl  <- mkReg(0);
   Reg#(Bit#(8))  dataR <- mkReg(0);

   // ── 学生填②：时序写/更新 - CTRL 可写；DATA(需就绪)捕获低字节；STATUS/ID 只读 ──
   method Action write(Addr addr, Word wdata);
      case (addr)
         4'h0: ctrl <= wdata;
         4'h8: if (ctrl[0] == 1) dataR <= wdata[7:0]; // 需 EN（就绪）
         default: noAction;                            // STATUS/ID 只读
      endcase
   endmethod

   // ── 学生填①：字段位选 + 组合读多路器 ──
   method Word read(Addr addr);
      Bit#(1) en = ctrl[0], ie = ctrl[1];
      Bit#(1) ready = en, busy = 0, irq = en & ie;
      Word statusWord = zeroExtend({irq, busy, ready});
      case (addr)
         4'h0: return ctrl;
         4'h4: return statusWord;
         4'h8: return 0;       // WO：读返回 0
         4'hC: return magic;
         default: return 0;
      endcase
   endmethod

   method Bit#(8) captured = dataR;
endmodule

// ── 测试 harness（给定，勿改）：tb 当参考驱动跑同一段 trace + 逐位镜像校验 ──
// 注：BSV 值方法 read(addr) 每周期对每个不同实参各占一个端口；故每条 rule 只读一个地址，
//     把需要的字存进 tb 侧寄存器，留到镜像 rule 统一重建（与 Verilog 的 #1 重摆 addr 等价）。
(* synthesize *)
module mkTbRegDev (Empty);
   RegDev         dev       <- mkRegDev;
   Reg#(Bit#(8))  step      <- mkReg(0);
   Reg#(Bit#(16)) errors    <- mkReg(0);
   Reg#(Word)     ctrlSeen  <- mkReg(0);
   Reg#(Word)     statusSeen <- mkReg(0);

   Word traceCtrl   = 32'h0000_000B; // EN=1 IE=1 MODE=2(solid)
   Word traceStatus = 32'h0000_0005; // READY=1 IRQ=1

   rule s0 (step == 0); // 探 ID；RO 写应被忽略
      if (dev.read(4'hC) != magic) begin
         $display("DEV_FAIL ID=0x%08h", dev.read(4'hC)); errors <= errors + 1;
      end
      dev.write(4'hC, 32'hDEAD_BEEF);
      step <= 1;
   endrule

   rule s1 (step == 1); // RO 写未改 ID；写 CTRL
      if (dev.read(4'hC) != magic) begin
         $display("DEV_FAIL ID changed by RO write"); errors <= errors + 1;
      end
      dev.write(4'h0, traceCtrl);
      step <= 2;
   endrule

   rule s2 (step == 2); // 读回 CTRL(RW)，存入 tb 寄存器
      if (dev.read(4'h0) != traceCtrl) begin
         $display("DEV_FAIL CTRL readback=0x%08h", dev.read(4'h0)); errors <= errors + 1;
      end
      ctrlSeen <= dev.read(4'h0);
      step <= 3;
   endrule

   rule s3 (step == 3); // 读 STATUS(RO)，存入 tb 寄存器；写 DATA(WO)
      if (dev.read(4'h4) != traceStatus) begin
         $display("DEV_FAIL STATUS=0x%08h", dev.read(4'h4)); errors <= errors + 1;
      end
      statusSeen <= dev.read(4'h4);
      dev.write(4'h8, 32'h0000_00A5);
      step <= 4;
   endrule

   rule s4 (step == 4); // 设备捕获字节；WO 读 0
      Bit#(16) e = errors;
      if (dev.captured != 8'hA5) begin
         $display("DEV_FAIL DATA captured=0x%02h", dev.captured); e = e + 1;
      end
      if (dev.read(4'h8) != 0) begin
         $display("DEV_FAIL WO read nonzero"); e = e + 1;
      end
      errors <= e;
      step <= 5;
   endrule

   rule s5 (step == 5); // 逐位镜像：从字段切片重建整字
      Word cw = ctrlSeen, sw = statusSeen;
      Word crecon = {23'b0, cw[8], 4'b0, cw[3:2], cw[1], cw[0]};
      Word srecon = {29'b0, sw[2], sw[1], sw[0]};
      if (crecon != traceCtrl || srecon != traceStatus) begin
         $display("DEV_FAIL mirror CTRL=0x%08h STATUS=0x%08h", crecon, srecon); errors <= errors + 1;
      end
      step <= 6;
   endrule

   rule s_done (step == 6);
      if (errors == 0) begin
         $display("RAW_PASS  register trace: ID/CTRL/STATUS consistent");
         $display("DEV_PASS  device fields: RO rejects write / WO reads 0 / byte captured");
         $display("MIRROR_PASS raw<->field-slice bit-exact: CTRL=0x%08h STATUS=0x%08h", traceCtrl, traceStatus);
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", errors);
      $finish(0);
   endrule
endmodule

endpackage
