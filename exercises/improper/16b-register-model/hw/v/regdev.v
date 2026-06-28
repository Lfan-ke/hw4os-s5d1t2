// 16b · 寄存器模型 - 设备侧（Verilog，学生填空版）。
// regdev 的 RTL：软件四语抄的“同一张布局表”的源头 - 字段就是 wire 切片。
// 寄存器（addr=字节偏移，§2.1）：
//   0x0 CTRL   RW  EN[0] IE[1] MODE[3:2] RST[8]
//   0x4 STATUS RO  READY[0] BUSY[1] IRQ[2]（READY=EN，IRQ=EN&IE）
//   0x8 DATA   WO  BYTE[7:0]（需就绪写入；读返回 0）
//   0xC ID     RO  magic 0x52454744 ("REGD")
// 你只需把“字段位选”从 ctrl_r 选出来（下方 ── 学生填① ──）；读多路器/写时序勿改。
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

    // ── 学生填①：字段位选（硬件字段 = wire 切片） - 按 §2.1 从 ctrl_r 选出各字段 ──
    assign f_en    = 1'b0;   // TODO: ctrl_r[0]
    assign f_ie    = 1'b0;   // TODO: ctrl_r[1]
    assign f_mode  = 2'b0;   // TODO: ctrl_r[3:2]
    assign f_rst   = 1'b0;   // TODO: ctrl_r[8]
    // 以下由字段派生（勿改）：
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
