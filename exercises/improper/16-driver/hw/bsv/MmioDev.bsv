// 16.1 裸机 MMIO —— 设备侧（BlueSpec SV，学生填空版）。
// 与软件驱动 / Verilog mmio_dev 同一寄存器契约的两面：tb 当“参考驱动”握手，你写“设备”。
// 你只需填两处：write 时序更新 + read 组合多路器。占位能编译、0 warning、运行判 DEV_FAIL。
// 寄存器：0x0 ID(magic 只读) 0x4 CTRL(bit0 使能) 0x8 STATUS(bit0 ready) 0xC DATA(写需就绪/读回显)。
package MmioDev;

typedef Bit#(4)  Addr;
typedef Bit#(32) Word;

Word magic = 32'h426C_6E6B; // "Blnk"

interface MmioDev;
   method Action write(Addr addr, Word wdata);
   method Word   read(Addr addr);
endinterface

(* synthesize *)
module mkMmioDev (MmioDev);
   Reg#(Bool)     enabled <- mkReg(False);
   Reg#(Bit#(8))  last    <- mkReg(0);

   // ── 学生填②：时序写/更新 —— CTRL 改使能；DATA(需就绪)捕获低字节 ──
   method Action write(Addr addr, Word wdata);
      // TODO: case(addr) 4'h4: enabled<=unpack(wdata[0]); 4'hC: if(enabled) last<=wdata[7:0]; default: noAction;
      Word ignore = wdata | zeroExtend(addr);          // 占位：引用 addr/wdata 消除未用告警
      enabled <= (ignore == 0) ? False : enabled;       // 恒不真正使能 → ready 不来 → DEV_FAIL
      last    <= last;
   endmethod

   // ── 学生填①：组合读多路器 —— 按 addr 选寄存器 ──
   method Word read(Addr addr);
      // TODO: 使能即就绪 ready=pack(enabled)；case(addr) 4'h0→magic 4'h4→enabled 4'h8→ready 4'hC→last default→0
      return (zeroExtend(addr) ^ zeroExtend(last)) & 32'h0; // 占位：读输入但恒 0 → ID 读不到 magic → DEV_FAIL
   endmethod
endmodule

// ── 测试 harness（给定，勿改）：tb 扮演参考驱动，用 step 把握手摊到逐拍 ──
(* synthesize *)
module mkTbMmio (Empty);
   MmioDev        dev    <- mkMmioDev;
   Reg#(Bit#(8))  step   <- mkReg(0);
   Reg#(Bit#(16)) errors <- mkReg(0);

   // ① probe：读 ID 比对 magic；并尝试未使能写 DATA（应被忽略）
   rule s0 (step == 0);
      if (dev.read(4'h0) != magic) begin
         $display("DEV_FAIL ID=0x%08h exp=0x%08h", dev.read(4'h0), magic);
         errors <= errors + 1;
      end
      dev.write(4'hC, 32'h0000_0099); // 未使能：忽略
      step <= 1;
   endrule

   // ② 使能
   rule s1 (step == 1);
      dev.write(4'h4, 32'h0000_0001);
      step <= 2;
   endrule

   // ③ 轮询 ready 并突发写第一个字节
   rule s2 (step == 2);
      if (dev.read(4'h8)[0] != 1'b1) begin
         $display("DEV_FAIL ready not set STATUS=0x%08h", dev.read(4'h8));
         errors <= errors + 1;
      end
      dev.write(4'hC, 32'h0000_00AA);
      step <= 3;
   endrule

   // ④ 回显校验 0xAA，写第二个字节 0x55
   rule s3 (step == 3);
      if (dev.read(4'hC)[7:0] != 8'hAA) begin
         $display("DEV_FAIL DATA echo=0x%02h exp=0xAA", dev.read(4'hC)[7:0]);
         errors <= errors + 1;
      end
      dev.write(4'hC, 32'h0000_0055);
      step <= 4;
   endrule

   // ⑤ 回显校验 0x55
   rule s4 (step == 4);
      if (dev.read(4'hC)[7:0] != 8'h55) begin
         $display("DEV_FAIL DATA echo=0x%02h exp=0x55", dev.read(4'hC)[7:0]);
         errors <= errors + 1;
      end
      step <= 5;
   endrule

   rule s_done (step == 5);
      if (errors == 0) begin
         $display("DEV_PASS");
         $display("MMIO_PASS");
         $display("ALL_PASS");
      end else
         $display("SOME_FAIL errors=%0d", errors);
      $finish(0);
   endrule
endmodule

endpackage
