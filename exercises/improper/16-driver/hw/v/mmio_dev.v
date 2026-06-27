// 16.1 裸机 MMIO —— 设备侧（Verilog，学生填空版）。
// 与软件驱动是同一寄存器契约的两面：tb 当“参考驱动”来回握手，你写“设备”。
// 你只需填两处：组合读多路器 + 时序写更新。占位能编译、0 warning、运行判 DEV_FAIL。
// 寄存器映射（addr 为字节偏移）：
//   0x0 ID     只读：恒返回 magic 0x426C6E6B ("Blnk")
//   0x4 CTRL   写：bit0=使能；读回使能位
//   0x8 STATUS 只读：bit0=ready（= 已使能）
//   0xC DATA   写(需已使能)：捕获低字节；读：回显最近一次写入的字节
`default_nettype none
`timescale 1ns/1ps
module mmio_dev (
    input  wire        clk,
    input  wire        rst,
    input  wire        sel,        // 片选
    input  wire        we,         // 写使能
    input  wire [3:0]  addr,       // 寄存器字节偏移
    input  wire [31:0] wdata,
    output reg  [31:0] rdata
);
    localparam [31:0] MAGIC = 32'h426C_6E6B;
    localparam [3:0]  R_ID = 4'h0, R_CTRL = 4'h4, R_STATUS = 4'h8, R_DATA = 4'hC;

    reg        enabled;
    reg [7:0]  last;
    wire       ready = enabled;   // 本简化设备使能即就绪

    // ── 学生填①：组合读多路器 —— 按 addr 选寄存器输出到 rdata ──
    always @(*) begin
        // TODO: case(addr) R_ID→MAGIC; R_CTRL→{31'b0,enabled}; R_STATUS→{31'b0,ready};
        //                  R_DATA→{24'b0,last}; default→32'h0。每个分支都要给 rdata 全宽赋值。
        rdata = ({28'b0, addr} ^ {31'b0, ready} ^ {24'b0, last}) & 32'h0; // ← 占位：读输入但恒 0 → ID 读不到 magic → DEV_FAIL
    end

    // ── 学生填②：时序写/更新 —— CTRL 改使能；DATA(需 ready)捕获字节 ──
    always @(posedge clk) begin
        if (rst) begin
            enabled <= 1'b0;
            last    <= 8'h00;
        end else if (sel && we) begin
            // TODO: case(addr) R_CTRL: enabled<=wdata[0]; R_DATA: if(ready) last<=wdata[7:0]; default: ;
            enabled <= enabled & wdata[0] & 1'b0;   // ← 占位：读 wdata 但恒不使能 → ready 不来 → DEV_FAIL
            last    <= last & wdata[7:0] & 8'h00;    // ← 占位
        end
    end
endmodule
`default_nettype wire
