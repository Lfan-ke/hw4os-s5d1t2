// 简化 VLAN Tag 处理 —— 硬件路径（Verilog 参考解）。
// 包字: [31]VALID [30]HAS_TAG [29]DROP [28]DIR(in) [21:16]VID(6b) [15:0]PAYLOAD
// 纯组合逻辑：与软件 process() 完全同构，输出逐位一致。
`default_nettype none
`timescale 1ns/1ps
module vlan_proc (
    input  wire [1:0]  mode,    // 0=Access 1=Trunk 2=Hybrid
    input  wire [5:0]  pvid,
    input  wire [63:0] allow,
    input  wire [63:0] untag,
    input  wire [31:0] in_pkt,
    output reg  [31:0] out_pkt
);
    localparam [31:0] VALID   = 32'h8000_0000;
    localparam [31:0] HAS_TAG = 32'h4000_0000;
    localparam [31:0] DROP    = 32'h2000_0000;

    wire        has_tag = in_pkt[30];
    wire        egress  = in_pkt[28];
    wire [5:0]  vid     = in_pkt[21:16];
    wire [15:0] payload = in_pkt[15:0];

    // 四个基本操作（与软件辅助函数一一对应）
    wire [31:0] op_strip  = VALID | {16'b0, payload};
    wire [31:0] op_insert = VALID | HAS_TAG | ({26'b0, pvid} << 16) | {16'b0, payload};
    wire [31:0] op_keep   = VALID | (in_pkt & (HAS_TAG | (32'h3F << 16) | 32'h0000_FFFF));
    wire [31:0] op_drop   = VALID | DROP;

    wire allowed  = allow[vid];
    wire do_untag = untag[vid];

    always @(*) begin
        if (!egress) begin
            // 收包 ingress
            case (mode)
                2'd0:    out_pkt = has_tag ? op_strip : op_insert;            // Access
                default: out_pkt = (!has_tag) ? op_drop :
                                   (!allowed) ? op_drop : op_keep;             // Trunk/Hybrid
            endcase
        end else begin
            // 发包 egress
            case (mode)
                2'd0:    out_pkt = op_strip;                                   // Access
                2'd1:    out_pkt = op_keep;                                    // Trunk
                default: out_pkt = (has_tag && do_untag) ? op_strip : op_keep; // Hybrid
            endcase
        end
    end
endmodule
`default_nettype wire
