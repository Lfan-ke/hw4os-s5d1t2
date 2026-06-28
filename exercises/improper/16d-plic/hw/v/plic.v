// 16d · 核外中断 PLIC - 设备侧（Verilog，学生填空版）。
// 平台级共享外设中断路由器：priority/enable/threshold → claim/complete。
// 寄存器（addr=字节偏移）：
//   0x00..0x0C PRIO1..PRIO4 RW（每源 2 位优先级，0=屏蔽）
//   0x10 PENDING   RO  bitmap（bit i = 源 i 挂起）
//   0x14 ENABLE    RW  bitmap（上下文使能）
//   0x18 THRESHOLD RW（仅 prio>threshold 的源可见）
//   0x1C CLAIM     R=取顶源(读副作用：清其 pending) / W=complete(EOI)
//   0x20 RAISE     WO  bitmap（OR 进 pending，模拟外设 IRQ 线）
// 你只需补全“每源 eligibility”（下方 ── 学生填① ──）；仲裁链/读多路/写时序勿改。
`default_nettype none
`timescale 1ns/1ps
module plic (
    input  wire        clk,
    input  wire        rst,
    input  wire        sel,
    input  wire        we,
    input  wire [7:0]  addr,
    input  wire [31:0] wdata,
    output reg  [31:0] rdata,
    output wire [2:0]  best_id,
    output wire [1:0]  best_prio
);
    localparam [7:0] A_PRIO1 = 8'h00, A_PRIO2 = 8'h04, A_PRIO3 = 8'h08, A_PRIO4 = 8'h0C,
                     A_PEND  = 8'h10, A_ENA   = 8'h14, A_THRESH = 8'h18,
                     A_CLAIM = 8'h1C, A_RAISE = 8'h20;

    reg [1:0] prio1, prio2, prio3, prio4, threshold;
    reg [4:0] pending, enable;

    // ── 学生填①：每源 eligibility = pending & enable & (prio > threshold) ──
    wire elig1 = 1'b0; // TODO: pending[1] & enable[1] & (prio1 > threshold)
    wire elig2 = 1'b0; // TODO: pending[2] & enable[2] & (prio2 > threshold)
    wire elig3 = 1'b0; // TODO: pending[3] & enable[3] & (prio3 > threshold)
    wire elig4 = 1'b0; // TODO: pending[4] & enable[4] & (prio4 > threshold)

    // 优先级仲裁 - 取最高 prio；同 prio 取最小 id（自高 id 向低用 >= 让低 id 胜）。勿改。
    reg [2:0] bid;
    reg [1:0] bp;
    always @(*) begin
        bid = 3'd0; bp = 2'd0;
        if (elig4 && prio4 >= bp) begin bp = prio4; bid = 3'd4; end
        if (elig3 && prio3 >= bp) begin bp = prio3; bid = 3'd3; end
        if (elig2 && prio2 >= bp) begin bp = prio2; bid = 3'd2; end
        if (elig1 && prio1 >= bp) begin bp = prio1; bid = 3'd1; end
    end
    assign best_id   = bid;
    assign best_prio = bp;

    always @(*) begin
        case (addr)
            A_PRIO1:  rdata = {30'b0, prio1};
            A_PRIO2:  rdata = {30'b0, prio2};
            A_PRIO3:  rdata = {30'b0, prio3};
            A_PRIO4:  rdata = {30'b0, prio4};
            A_PEND:   rdata = {27'b0, pending};
            A_ENA:    rdata = {27'b0, enable};
            A_THRESH: rdata = {30'b0, threshold};
            A_CLAIM:  rdata = {29'b0, best_id};
            default:  rdata = 32'h0;
        endcase
    end

    always @(posedge clk) begin
        if (rst) begin
            prio1 <= 2'd0; prio2 <= 2'd0; prio3 <= 2'd0; prio4 <= 2'd0; threshold <= 2'd0;
            pending <= 5'd0; enable <= 5'd0;
        end else if (sel) begin
            if (we) begin
                case (addr)
                    A_PRIO1:  prio1 <= wdata[1:0];
                    A_PRIO2:  prio2 <= wdata[1:0];
                    A_PRIO3:  prio3 <= wdata[1:0];
                    A_PRIO4:  prio4 <= wdata[1:0];
                    A_ENA:    enable <= wdata[4:0];
                    A_THRESH: threshold <= wdata[1:0];
                    A_RAISE:  pending <= pending | wdata[4:0];
                    default:  ; // PENDING(RO) / CLAIM(EOI)：pending 不变
                endcase
            end else if (addr == A_CLAIM) begin
                pending <= pending & ~(5'b1 << best_id); // claim 读副作用：清 in-service 源
            end
        end
    end
endmodule
`default_nettype wire
