// 16c · 核内中断 - 设备侧（Verilog，学生填空版）。
// CLINT 的 RTL：软件 CLINT 模型对位的源头 - timer = mtime 计数器 + mtimecmp 比较器；msip = 软件中断寄存器。
// toy 寄存器（waddr=字节偏移）：0x00 MSIP(RW bit0) 0x08 MTIMECMP(RW) ；mtime 由 tick 自走、软件只读。
// 你只需填两处组合输出：mtip（比较器）与 msip（寄存器读）；写时序/端口/tb 勿改。
`default_nettype none
`timescale 1ns/1ps
module clint (
    input  wire        clk,
    input  wire        rst,
    input  wire        tick,        // 拉高一拍 → mtime += 1（核内自走时钟）
    input  wire        we,          // 寄存器写选通
    input  wire [4:0]  waddr,       // 字节偏移
    input  wire [63:0] wdata,
    output wire [63:0] mtime_o,
    output wire        mtip,        // timer 中断挂起（核内）
    output wire        msip         // 软件中断挂起（核内）
);
    localparam [4:0] A_MSIP = 5'h00, A_CMP = 5'h08;

    reg [63:0] mtime_r, mtimecmp_r;
    reg        msip_r;

    // ── 学生填：比较器 + msip 寄存器读 ──
    assign mtip    = 1'b0;   // TODO: (mtime_r >= mtimecmp_r)
    assign msip    = 1'b0;   // TODO: msip_r
    assign mtime_o = mtime_r;

    always @(posedge clk) begin
        if (rst) begin
            mtime_r <= 64'd0; mtimecmp_r <= 64'd0; msip_r <= 1'b0;
        end else begin
            if (we && waddr == A_MSIP) msip_r     <= wdata[0];
            if (we && waddr == A_CMP)  mtimecmp_r <= wdata;
            if (tick)                  mtime_r    <= mtime_r + 64'd1;
        end
    end
endmodule
`default_nettype wire
