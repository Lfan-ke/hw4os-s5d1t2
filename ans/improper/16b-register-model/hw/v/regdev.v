// 16b · 寄存器模型 - 设备侧（Verilog，参考解）。
// regdev 的 RTL：软件四语抄的“同一张布局表”的源头 - 字段就是 wire 切片。
// 寄存器（addr=字节偏移，§2.1）：
//   0x0 CTRL   RW  EN[0] IE[1] MODE[3:2] RST[8]
//   0x4 STATUS RO  READY[0] BUSY[1] IRQ[2]（READY=EN，IRQ=EN&IE）
//   0x8 DATA   WO  BYTE[7:0]（需就绪写入；读返回 0）
//   0xC ID     RO  magic 0x52454744 ("REGD")
`default_nettype none
`timescale 1ns/1ps
module regdev (
    input  wire        clk,
    input  wire        rst,
    input  wire        sel,
    input  wire        we,
    input  wire [3:0]  addr,
    input  wire [31:0] wdata,
    output reg  [31:0] rdata,
    output wire        f_en,
    output wire        f_ie,
    output wire [1:0]  f_mode,
    output wire        f_rst,
    output wire        f_ready,
    output wire        f_busy,
    output wire        f_irq,
    output wire [7:0]  f_byte
);
    localparam [31:0] MAGIC = 32'h5245_4744;
    localparam [3:0]  A_CTRL = 4'h0, A_STATUS = 4'h4, A_DATA = 4'h8, A_ID = 4'hC;

    reg [31:0] ctrl_r;
    reg [7:0]  data_r;

    // ── 学生填①：字段位选（硬件字段 = wire 切片）──
    assign f_en    = ctrl_r[0];
    assign f_ie    = ctrl_r[1];
    assign f_mode  = ctrl_r[3:2];
    assign f_rst   = ctrl_r[8];
    assign f_ready = f_en;
    assign f_busy  = 1'b0;
    assign f_irq   = f_en & f_ie;
    assign f_byte  = data_r;

    wire [31:0] status_word = {29'b0, f_irq, f_busy, f_ready};

    always @(*) begin
        case (addr)
            A_CTRL:   rdata = ctrl_r;
            A_STATUS: rdata = status_word;
            A_DATA:   rdata = 32'h0;
            A_ID:     rdata = MAGIC;
            default:  rdata = 32'h0;
        endcase
    end

    // ── 学生填②：时序写/更新 - CTRL 可写；DATA(需就绪)捕获低字节；STATUS/ID 只读 ──
    always @(posedge clk) begin
        if (rst) begin
            ctrl_r <= 32'h0;
            data_r <= 8'h0;
        end else if (sel && we) begin
            case (addr)
                A_CTRL:  ctrl_r <= wdata;
                A_DATA:  if (f_ready) data_r <= wdata[7:0];
                default: ;
            endcase
        end
    end
endmodule
`default_nettype wire
