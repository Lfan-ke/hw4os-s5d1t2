// sim_main.cpp — Verilator C++ testbench for top.sv. Drives the design with a
// fixed input sequence and prints a DETERMINISTIC trace (exact-matched against a
// host-verilated golden on StarryOS). No wall clock / no randomness.
#include "Vtop.h"
#include "verilated.h"
#include <cstdio>

static Vtop* dut = nullptr;

static void tick() {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vtop;

    // ---- reset for 2 cycles ----
    dut->rst = 1; dut->op = 0; dut->a = 0; dut->b = 0; dut->we = 0; dut->waddr = 0;
    tick(); tick();
    dut->rst = 0;

    // ---- ALU sweep: combinational, sample after eval (no clock needed). ----
    struct { int op; int a; int b; } cases[] = {
        {0, 200, 100}, {1, 50, 20}, {2, 0xF0, 0x0F}, {3, 0xF0, 0x0F},
        {4, 0xAA, 0xFF}, {5, 0x81, 0}, {6, 0x81, 0}, {7, 0x0F, 0},
    };
    for (auto& c : cases) {
        dut->op = c.op; dut->a = c.a; dut->b = c.b; dut->eval();
        printf("ALU op=%d a=%3d b=%3d y=%3d carry=%d\n", c.op, c.a, c.b,
               (int)dut->alu_y, (int)dut->alu_carry);
    }

    // ---- register file: write alu_y(=a+b) to 4 slots, then read summed output ----
    int vals[4] = {10, 20, 30, 40};
    for (int i = 0; i < 4; i++) {
        dut->op = 0; dut->a = vals[i]; dut->b = 0; // alu_y = a
        dut->we = 1; dut->waddr = i; dut->eval();
        tick();
    }
    dut->we = 0; dut->eval();
    printf("RF_SUM=%d\n", (int)dut->rf_sum); // 10+20+30+40 = 100

    // ---- sequential counter: count cycles from a known reset ----
    dut->rst = 1; tick(); dut->rst = 0;
    for (int i = 0; i < 7; i++) tick();
    printf("COUNT=%d\n", (int)dut->count); // 7

    // ---- FSM: from reset, observe the state sequence over 5 cycles ----
    dut->rst = 1; tick(); dut->rst = 0;
    const char* names[4] = {"IDLE", "RUN", "HOLD", "DONE"};
    for (int i = 0; i < 5; i++) {
        printf("FSM cycle=%d state=%s\n", i, names[dut->state & 3]);
        tick();
    }

    printf("VERILOG_SIM_OK\n");
    dut->final();
    delete dut;
    return 0;
}
