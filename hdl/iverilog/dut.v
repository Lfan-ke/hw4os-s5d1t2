// dut.v — a small, portable Verilog design for the StarryOS verilog stress case
// (#764 "verilog <!-- verilator, iverilog -->"). Deliberately SIMPLE and written
// in plain Verilog-2001 so BOTH iverilog (→vvp) and verilator (→binary) accept it
// unchanged. Exercises: parameter, combinational ALU (case), a synchronous counter,
// and a small 4-state FSM (IDLE→RUN→HOLD→DONE→IDLE).
module dut #(
    parameter WIDTH = 8
) (
    input  wire              clk,
    input  wire              rst,    // sync active-high
    input  wire [2:0]        op,
    input  wire [WIDTH-1:0]  a,
    input  wire [WIDTH-1:0]  b,
    output reg  [WIDTH-1:0]  y,      // ALU result
    output reg               carry,
    output reg  [WIDTH-1:0]  count,  // free-running counter
    output reg  [1:0]        state   // FSM state
);
    // ---- combinational ALU ----
    reg [WIDTH:0] ext;
    always @(*) begin
        case (op)
            3'd0: ext = a + b;          // add (with carry)
            3'd1: ext = {1'b0, a - b};  // sub
            3'd2: ext = {1'b0, a & b};  // and
            3'd3: ext = {1'b0, a | b};  // or
            3'd4: ext = {1'b0, a ^ b};  // xor
            3'd5: ext = {1'b0, a << 1}; // shl
            3'd6: ext = {1'b0, a >> 1}; // shr
            default: ext = {1'b0, ~a};  // not
        endcase
    end
    always @(*) begin
        y     = ext[WIDTH-1:0];
        carry = ext[WIDTH];
    end

    // ---- synchronous counter ----
    always @(posedge clk) begin
        if (rst) count <= {WIDTH{1'b0}};
        else     count <= count + 1'b1;
    end

    // ---- 4-state FSM: IDLE(0)->RUN(1)->HOLD(2)->DONE(3)->IDLE ----
    localparam S_IDLE = 2'd0, S_RUN = 2'd1, S_HOLD = 2'd2, S_DONE = 2'd3;
    reg [1:0] cur, nxt;
    always @(*) begin
        case (cur)
            S_IDLE: nxt = rst ? S_IDLE : S_RUN;
            S_RUN:  nxt = S_HOLD;
            S_HOLD: nxt = S_DONE;
            S_DONE: nxt = S_IDLE;
            default: nxt = S_IDLE;
        endcase
    end
    always @(posedge clk) begin
        if (rst) cur <= S_IDLE;
        else     cur <= nxt;
    end
    always @(*) state = cur;
endmodule
