// sc_main.cpp — deterministic SystemC testbench for the StarryOS bluesv "system c"
// track (#764). Exercises the SystemC kernel: sc_module, SC_CTOR, SC_METHOD + SC_THREAD,
// sc_clock, static sensitivity, sc_signal, wait(), sc_time_stamp, sc_stop — the core of
// the discrete-event simulation engine. Pure C++ (no Verilog); cross-compiled static
// musl + libsystemc.a, run on StarryOS. Output is deterministic for exact-match golden.
#include <systemc.h>

SC_MODULE(counter) {
    sc_in<bool>  clk;
    sc_in<bool>  rst;
    sc_out<int>  count;
    int cnt;
    void tick() {                       // SC_METHOD on rising clk
        if (rst.read()) cnt = 0;
        else            cnt = cnt + 1;
        count.write(cnt);
    }
    SC_CTOR(counter) : cnt(0) {
        SC_METHOD(tick);
        sensitive << clk.pos();
        dont_initialize();
    }
};

SC_MODULE(driver) {
    sc_out<bool> rst;
    sc_in<int>   count;
    sc_in<bool>  clk;
    void run() {                        // SC_THREAD paced by the clock only
        rst.write(true);
        wait();                         // cycle 1: hold reset
        rst.write(false);
        for (int i = 0; i < 8; i++) {
            wait();                     // advance exactly one clock
            std::cout << "SC count=" << count.read() << std::endl;
        }
        std::cout << "SYSTEMC_OK" << std::endl;
        sc_stop();
    }
    SC_CTOR(driver) {
        SC_THREAD(run);
        sensitive << clk.pos();         // wait() resumes on next rising edge
    }
};

int sc_main(int /*argc*/, char* /*argv*/[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst;
    sc_signal<int>  count;

    counter u_cnt("u_cnt");
    u_cnt.clk(clk); u_cnt.rst(rst); u_cnt.count(count);

    driver u_drv("u_drv");
    u_drv.rst(rst); u_drv.count(count); u_drv.clk(clk);

    sc_start();                          // run until sc_stop()
    return 0;
}
