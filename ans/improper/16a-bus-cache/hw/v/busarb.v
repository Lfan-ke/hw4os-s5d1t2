// 16a · 总线与缓存 - 仲裁器（Verilog，参考解）。
// “总线/互连/仲裁器”祛魅 = 地址区间译码：addr 落 §2.2 哪段区间 → 选哪个设备（单热 sel），全落空 = 总线错误。
//   regdev   0x4000_0000..0x4000_1000
//   sensor   0x4001_0000..0x4001_0010
//   switchdev 0x4002_0000..0x4002_0010
//   else     BUS_ERR
`default_nettype none
`timescale 1ns/1ps
module busarb (
    input  wire [31:0] addr,
    output wire        sel_reg,
    output wire        sel_sen,
    output wire        sel_sw,
    output wire        bus_err,
    output wire [1:0]  dev
);
    localparam [31:0] REG_BASE = 32'h4000_0000, REG_END = 32'h4000_1000;
    localparam [31:0] SEN_BASE = 32'h4001_0000, SEN_END = 32'h4001_0010;
    localparam [31:0] SW_BASE  = 32'h4002_0000, SW_END  = 32'h4002_0010;

    // ── 学生填：仲裁区间判定（addr 落区间 → 单热 sel）──
    assign sel_reg = (addr >= REG_BASE) && (addr < REG_END);
    assign sel_sen = (addr >= SEN_BASE) && (addr < SEN_END);
    assign sel_sw  = (addr >= SW_BASE)  && (addr < SW_END);
    assign bus_err = ~(sel_reg | sel_sen | sel_sw);

    assign dev = sel_reg ? 2'd0 : sel_sen ? 2'd1 : sel_sw ? 2'd2 : 2'd3;
endmodule
`default_nettype wire
