// 三态特权机 —— 硬件路径（Verilog）。
// 状态字 csr[4:0] = { saved_priv[4:3], feat_en[2], cur_priv[1:0] }
// 纯组合逻辑：与软件 step() 完全同构。一拍把 (csr, op) 映射到 (csr_o, trap)。
// 你只需填 always 块里的 case (kind)；tb_priv.v（向量 + PASS 打印）勿改。
//
// 特权级：A(最高)=2'd2，B=2'd1，C(最低)=2'd0。比较器 cur<arg 就是“没权限”那根线。
`default_nettype none
`timescale 1ns/1ps
module priv_gate (
    input  wire [4:0] csr,       // {saved_priv[1:0], feat_en, cur_priv[1:0]}
    input  wire [2:0] kind,      // 0 NORMAL 1 DROP 2 ECALL 3 XRET 4 SETFEAT 5 USEFEAT
    input  wire [1:0] arg_priv,
    input  wire       arg_en,
    output reg  [4:0] csr_o,
    output reg        trap
);
    localparam [1:0] A = 2'd2; // 最高
    localparam [1:0] B = 2'd1;
    localparam [1:0] C = 2'd0; // 最低

    localparam [2:0] NORMAL  = 3'd0;
    localparam [2:0] DROP    = 3'd1;
    localparam [2:0] ECALL   = 3'd2;
    localparam [2:0] XRET    = 3'd3;
    localparam [2:0] SETFEAT = 3'd4;
    localparam [2:0] USEFEAT = 3'd5;

    // csr 取位（这些线就是触发器读出来的当前态）
    wire [1:0] cur = csr[1:0];
    wire       fe  = csr[2];
    wire [1:0] sp  = csr[4:3];

    reg [1:0] ncur;
    reg       nfe;
    reg [1:0] nsp;

    always @(*) begin
        // 默认：csr 不变、不陷入。各分支只改需要改的位（防 latch：所有输出都先赋默认）。
        ncur = cur;
        nfe  = fe;
        nsp  = sp;
        trap = 1'b0;
        case (kind)
            NORMAL: begin
                // TODO[a]（推荐）：一根比较器 —— trap = (cur < arg_priv);
                // ELSE[b]：把 3×3 等级关系展开成显式真值表（(C,B)/(C,A)/(B,A) 时 trap=1'b1）。
            end
            DROP: begin
                // TODO: (arg_priv > cur) → trap=1'b1（不许直接提权）；否则 ncur = arg_priv。
            end
            ECALL: begin
                // TODO: 合法陷入 —— nsp = cur（保存前态）；ncur = A（进最高态）。
            end
            XRET: begin
                // TODO[a]：用 2 位 saved_priv 存完整前态 ——
                //   (cur != A) → trap=1'b1（非最高态不得 xret）；否则 ncur = sp。
                // ELSE[b]：仿真实 sstatus.SPP 只用 1 位，并在 THINKING.md 讨论信息为何不足。
            end
            SETFEAT: begin
                // TODO: (cur < B) → trap=1'b1（配置使能位至少要 B 态）；否则 nfe = arg_en。
            end
            USEFEAT: begin
                // TODO: 能力 = 特权够 且 使能位亮，缺一不可 ——
                //   trap = (cur < arg_priv) || (fe == 1'b0);
            end
            default: begin
                // 其余 kind：保持默认（无操作）。
                ncur = cur;
            end
        endcase
        csr_o = {nsp, nfe, ncur};
    end
endmodule
`default_nettype wire
