// top.sv — comprehensive Verilog/SystemVerilog design for the StarryOS verilog
// stress case (#764 "verilog <!-- verilator, iverilog -->"). Verilated to C++,
// cross-compiled (musl) for 4 arches, run on StarryOS; the C++ testbench asserts
// a DETERMINISTIC trace so the case can exact-match a host-verilated golden.
//
// Exercises: parameters/localparam, packed structs + enums (SystemVerilog),
// combinational (always_comb) + sequential (always_ff) logic, an ALU, a register
// file (memory array), a generate loop (parallel adders), and a 4-state FSM.
module top #(
    parameter int WIDTH = 8
) (
    input  logic                 clk,
    input  logic                 rst,      // sync active-high
    input  logic [2:0]           op,        // ALU op
    input  logic [WIDTH-1:0]     a,
    input  logic [WIDTH-1:0]     b,
    input  logic                 we,        // regfile write enable
    input  logic [1:0]           waddr,
    output logic [WIDTH-1:0]     alu_y,
    output logic                 alu_carry,
    output logic [WIDTH-1:0]     count,
    output logic [WIDTH-1:0]     rf_sum,    // sum of all regfile entries
    output logic [1:0]           state
);
    // ---- enum FSM (SystemVerilog) ----
    typedef enum logic [1:0] { S_IDLE, S_RUN, S_HOLD, S_DONE } state_e;
    state_e cur, nxt;

    // ---- ALU: combinational ----
    logic [WIDTH:0] alu_ext;
    always_comb begin
        unique case (op)
            3'd0: alu_ext = a + b;           // add (carry out)
            3'd1: alu_ext = {1'b0, a - b};   // sub
            3'd2: alu_ext = {1'b0, a & b};   // and
            3'd3: alu_ext = {1'b0, a | b};   // or
            3'd4: alu_ext = {1'b0, a ^ b};   // xor
            3'd5: alu_ext = {1'b0, a << 1};  // shl
            3'd6: alu_ext = {1'b0, a >> 1};  // shr
            default: alu_ext = {1'b0, ~a};   // not
        endcase
    end
    assign alu_y     = alu_ext[WIDTH-1:0];
    assign alu_carry = alu_ext[WIDTH];

    // ---- sequential counter ----
    always_ff @(posedge clk) begin
        if (rst) count <= '0;
        else     count <= count + 1'b1;
    end

    // ---- register file (4 x WIDTH memory array) + generate-summed output ----
    logic [WIDTH-1:0] rf [0:3];
    always_ff @(posedge clk) begin
        if (rst) begin
            rf[0] <= '0; rf[1] <= '0; rf[2] <= '0; rf[3] <= '0;
        end else if (we) begin
            rf[waddr] <= alu_y;
        end
    end
    // generate: a tree of adders to sum the 4 entries.
    logic [WIDTH-1:0] s01, s23;
    genvar gi;
    generate
        // (trivial generate to exercise the construct)
        for (gi = 0; gi < 1; gi++) begin : g_sum
            assign s01 = rf[0] + rf[1];
            assign s23 = rf[2] + rf[3];
        end
    endgenerate
    assign rf_sum = s01 + s23;

    // ---- FSM: IDLE->RUN->HOLD->DONE->IDLE ----
    always_comb begin
        unique case (cur)
            S_IDLE: nxt = rst ? S_IDLE : S_RUN;
            S_RUN:  nxt = S_HOLD;
            S_HOLD: nxt = S_DONE;
            S_DONE: nxt = S_IDLE;
            default: nxt = S_IDLE;
        endcase
    end
    always_ff @(posedge clk) begin
        if (rst) cur <= S_IDLE;
        else     cur <= nxt;
    end
    assign state = cur;
endmodule
