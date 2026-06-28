// tb.v — self-contained Verilog testbench for dut.v. Drives a fixed, DETERMINISTIC
// stimulus and prints a trace with $display, so the SAME source runs under BOTH
// simulators on StarryOS:
//   * iverilog:  iverilog -g2012 -o dut.vvp tb.v dut.v ; vvp dut.vvp
//   * verilator: verilator --binary --timing -j 0 --top tb tb.v dut.v ; ./Vtb
// The two must print byte-identical output (matched against a host golden).
`timescale 1ns/1ps
module tb;
    localparam WIDTH = 8;
    reg               clk = 1'b0;
    reg               rst;
    reg  [2:0]        op;
    reg  [WIDTH-1:0]  a, b;
    wire [WIDTH-1:0]  y, count;
    wire              carry;
    wire [1:0]        state;
    integer           i;

    dut #(.WIDTH(WIDTH)) u_dut (
        .clk(clk), .rst(rst), .op(op), .a(a), .b(b),
        .y(y), .carry(carry), .count(count), .state(state)
    );

    // 10ns clock
    always #5 clk = ~clk;

    task step;  // advance one full clock, sampling on the negedge for stability
        begin @(negedge clk); end
    endtask

    initial begin
        // ---- reset 2 cycles ----
        rst = 1'b1; op = 3'd0; a = 8'd0; b = 8'd0;
        step; step;
        rst = 1'b0;

        // ---- ALU sweep (combinational; print after settle) ----
        sweep(3'd0, 8'd200, 8'd100);
        sweep(3'd1, 8'd50,  8'd20);
        sweep(3'd2, 8'hF0,  8'h0F);
        sweep(3'd3, 8'hF0,  8'h0F);
        sweep(3'd4, 8'hAA,  8'hFF);
        sweep(3'd5, 8'h81,  8'd0);
        sweep(3'd6, 8'h81,  8'd0);
        sweep(3'd7, 8'h0F,  8'd0);

        // ---- counter: reset then count 7 cycles ----
        rst = 1'b1; step; rst = 1'b0;
        for (i = 0; i < 7; i = i + 1) step;
        $display("COUNT=%0d", count);

        // ---- FSM: from reset, observe 5 states ----
        rst = 1'b1; step; rst = 1'b0;
        for (i = 0; i < 5; i = i + 1) begin
            $display("FSM cycle=%0d state=%0d", i, state);
            step;
        end

        $display("VERILOG_SIM_OK");
        $finish;
    end

    task sweep(input [2:0] o, input [7:0] av, input [7:0] bv);
        begin
            op = o; a = av; b = bv;
            #1; // let combinational logic settle
            $display("ALU op=%0d a=%0d b=%0d y=%0d carry=%0d", o, av, bv, y, carry);
        end
    endtask
endmodule
