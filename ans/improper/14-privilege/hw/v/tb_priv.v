// 14-privilege Verilog testbench（给定，勿改）。
// 驱动与软件相同的向量，逐位比对 (csr_o, trap)，打印 *_PASS / FAIL。
`default_nettype none
`timescale 1ns/1ps
module tb_priv;
    reg  [4:0] csr;
    reg  [2:0] kind;
    reg  [1:0] arg_priv;
    reg        arg_en;
    wire [4:0] csr_o;
    wire       trap;

    integer errors;
    integer gerr;

    reg clk;
    initial clk = 1'b0;
    always #5 clk = ~clk; // 仅为产生波形

    priv_gate dut (
        .csr(csr), .kind(kind), .arg_priv(arg_priv), .arg_en(arg_en),
        .csr_o(csr_o), .trap(trap)
    );

    localparam [1:0] A = 2'd2;
    localparam [1:0] B = 2'd1;
    localparam [1:0] C = 2'd0;

    localparam [2:0] NORMAL  = 3'd0;
    localparam [2:0] DROP    = 3'd1;
    localparam [2:0] ECALL   = 3'd2;
    localparam [2:0] XRET    = 3'd3;
    localparam [2:0] SETFEAT = 3'd4;
    localparam [2:0] USEFEAT = 3'd5;

    function [4:0] mkcsr(input [1:0] cur, input fe, input [1:0] sp);
        mkcsr = {sp, fe, cur};
    endfunction

    // 普通向量：固定 csr 输入，比对一次。
    task chk(input [4:0] icsr, input [2:0] k, input [1:0] ap, input ae,
             input [4:0] exp_csr, input exp_trap);
        begin
            csr = icsr; kind = k; arg_priv = ap; arg_en = ae;
            #10;
            if (csr_o !== exp_csr || trap !== exp_trap) begin
                $display("FAIL csr=0x%02x kind=%0d ap=%0d ae=%0d | exp(csr=0x%02x,trap=%0d) got(csr=0x%02x,trap=%0d)",
                         icsr, k, ap, ae, exp_csr, exp_trap, csr_o, trap);
                gerr   = gerr + 1;
                errors = errors + 1;
            end
        end
    endtask

    // 串行向量：用上一拍的 csr_o 当本拍输入（capstone 轨迹）。
    reg [4:0] chain;
    task chk_chain(input [2:0] k, input [1:0] ap, input ae,
                   input [4:0] exp_csr, input exp_trap);
        begin
            csr = chain; kind = k; arg_priv = ap; arg_en = ae;
            #10;
            if (csr_o !== exp_csr || trap !== exp_trap) begin
                $display("CAPSTONE_FAIL csr=0x%02x kind=%0d | exp(csr=0x%02x,trap=%0d) got(csr=0x%02x,trap=%0d)",
                         chain, k, exp_csr, exp_trap, csr_o, trap);
                gerr   = gerr + 1;
                errors = errors + 1;
            end
            chain = csr_o;
        end
    endtask

    initial begin
        $dumpfile("tb_priv.vcd");
        $dumpvars(0, tb_priv);
        errors = 0;

        // 子实验 1：CMP
        gerr = 0;
        chk(mkcsr(A,1'b0,2'd0), NORMAL, A, 1'b0, mkcsr(A,1'b0,2'd0), 1'b0);
        chk(mkcsr(C,1'b0,2'd0), NORMAL, A, 1'b0, mkcsr(C,1'b0,2'd0), 1'b1);
        chk(mkcsr(B,1'b0,2'd0), NORMAL, B, 1'b0, mkcsr(B,1'b0,2'd0), 1'b0);
        chk(mkcsr(B,1'b0,2'd0), NORMAL, A, 1'b0, mkcsr(B,1'b0,2'd0), 1'b1);
        chk(mkcsr(C,1'b0,2'd0), NORMAL, C, 1'b0, mkcsr(C,1'b0,2'd0), 1'b0);
        chk(mkcsr(A,1'b0,2'd0), NORMAL, C, 1'b0, mkcsr(A,1'b0,2'd0), 1'b0);
        if (gerr == 0) $display("CMP_PASS");

        // 子实验 2：DROP
        gerr = 0;
        chk(mkcsr(A,1'b0,2'd0), DROP, B, 1'b0, mkcsr(B,1'b0,2'd0), 1'b0);
        chk(mkcsr(A,1'b0,2'd0), DROP, C, 1'b0, mkcsr(C,1'b0,2'd0), 1'b0);
        chk(mkcsr(B,1'b0,2'd0), DROP, C, 1'b0, mkcsr(C,1'b0,2'd0), 1'b0);
        chk(mkcsr(C,1'b0,2'd0), DROP, A, 1'b0, mkcsr(C,1'b0,2'd0), 1'b1);
        chk(mkcsr(B,1'b0,2'd0), DROP, A, 1'b0, mkcsr(B,1'b0,2'd0), 1'b1);
        chk(mkcsr(B,1'b0,2'd0), DROP, B, 1'b0, mkcsr(B,1'b0,2'd0), 1'b0);
        if (gerr == 0) $display("DROP_PASS");

        // 子实验 3：TRAP（ECALL/XRET）
        gerr = 0;
        chk(mkcsr(C,1'b0,2'd0), ECALL, 2'd0, 1'b0, mkcsr(A,1'b0,C), 1'b0);
        chk(mkcsr(B,1'b0,2'd0), ECALL, 2'd0, 1'b0, mkcsr(A,1'b0,B), 1'b0);
        chk(mkcsr(A,1'b0,B),    XRET,  2'd0, 1'b0, mkcsr(B,1'b0,B), 1'b0);
        chk(mkcsr(C,1'b0,2'd0), XRET,  2'd0, 1'b0, mkcsr(C,1'b0,2'd0), 1'b1);
        chk(mkcsr(B,1'b0,2'd0), XRET,  2'd0, 1'b0, mkcsr(B,1'b0,2'd0), 1'b1);
        chk(mkcsr(C,1'b1,2'd0), ECALL, 2'd0, 1'b0, mkcsr(A,1'b1,C), 1'b0);
        if (gerr == 0) $display("TRAP_PASS");

        // 子实验 4：FEAT（SETFEAT/USEFEAT）
        gerr = 0;
        chk(mkcsr(A,1'b0,2'd0), SETFEAT, 2'd0, 1'b1, mkcsr(A,1'b1,2'd0), 1'b0);
        chk(mkcsr(B,1'b0,2'd0), SETFEAT, 2'd0, 1'b1, mkcsr(B,1'b1,2'd0), 1'b0);
        chk(mkcsr(C,1'b0,2'd0), SETFEAT, 2'd0, 1'b1, mkcsr(C,1'b0,2'd0), 1'b1);
        chk(mkcsr(A,1'b1,2'd0), SETFEAT, 2'd0, 1'b0, mkcsr(A,1'b0,2'd0), 1'b0);
        chk(mkcsr(C,1'b1,2'd0), USEFEAT, C, 1'b0, mkcsr(C,1'b1,2'd0), 1'b0);
        chk(mkcsr(C,1'b0,2'd0), USEFEAT, C, 1'b0, mkcsr(C,1'b0,2'd0), 1'b1);
        chk(mkcsr(A,1'b1,2'd0), USEFEAT, B, 1'b0, mkcsr(A,1'b1,2'd0), 1'b0);
        chk(mkcsr(C,1'b1,2'd0), USEFEAT, B, 1'b0, mkcsr(C,1'b1,2'd0), 1'b1);
        chk(mkcsr(B,1'b1,2'd0), USEFEAT, B, 1'b0, mkcsr(B,1'b1,2'd0), 1'b0);
        if (gerr == 0) $display("FEAT_PASS");

        // 子实验 5：CAPSTONE 轨迹（csr 串行）
        gerr = 0;
        chain = mkcsr(A,1'b0,2'd0); // A 启动
        chk_chain(DROP,    B, 1'b0, mkcsr(B,1'b0,2'd0), 1'b0);
        chk_chain(SETFEAT, 2'd0, 1'b1, mkcsr(B,1'b1,2'd0), 1'b0);
        chk_chain(DROP,    C, 1'b0, mkcsr(C,1'b1,2'd0), 1'b0);
        chk_chain(USEFEAT, B, 1'b0, mkcsr(C,1'b1,2'd0), 1'b1);
        chk_chain(ECALL,   2'd0, 1'b0, mkcsr(A,1'b1,C), 1'b0);
        chk_chain(XRET,    2'd0, 1'b0, mkcsr(C,1'b1,2'd0), 1'b0);
        if (gerr == 0) $display("CAPSTONE_PASS");

        if (errors == 0) $display("ALL_PASS");
        else             $display("SOME_FAIL errors=%0d", errors);
        $finish;
    end
endmodule
`default_nettype wire
