// 16a · 总线与缓存 - testbench（给定，学生勿改）。
// tb 当“参考驱动”：发同一组 8 个地址，对照同一张期望路由表，并校验单热/越界 bus_err。
// 期望路由：0=regdev 1=sensor 2=switchdev 3=bus_err（与软件四语逐位一致的 ARB_PASS 场景）。
`default_nettype none
`timescale 1ns/1ps
module tb_busarb;
    reg  [31:0] addr;
    wire        sel_reg, sel_sen, sel_sw, bus_err;
    wire [1:0]  dev;
    integer errors, onehot_err;
    reg  [2:0]  hot;

    busarb dut (
        .addr(addr), .sel_reg(sel_reg), .sel_sen(sel_sen),
        .sel_sw(sel_sw), .bus_err(bus_err), .dev(dev)
    );

    task check(input [31:0] a, input [1:0] exp);
        begin
            addr = a; #1;
            if (dev !== exp) begin
                $display("ARB_FAIL addr=0x%08x dev=%0d exp=%0d", a, dev, exp);
                errors = errors + 1;
            end
            hot = sel_reg + sel_sen + sel_sw + bus_err; // 恰好单热（含 bus_err）
            if (hot !== 3'd1) begin
                $display("DEV_FAIL addr=0x%08x not one-hot (hot=%0d)", a, hot);
                onehot_err = onehot_err + 1;
            end
        end
    endtask

    initial begin
        $dumpfile("tb_busarb.vcd");
        $dumpvars(0, tb_busarb);
        errors = 0; onehot_err = 0;
        check(32'h4000_0000, 2'd0);
        check(32'h4000_0FFC, 2'd0);
        check(32'h4001_0000, 2'd1);
        check(32'h4001_000C, 2'd1);
        check(32'h4002_0000, 2'd2);
        check(32'h4002_0008, 2'd2);
        check(32'h4003_0000, 2'd3);
        check(32'h3FFF_FFFC, 2'd3);
        if (errors == 0 && onehot_err == 0) begin
            $display("ARB_PASS bus arbitration = address-range decode (8/8 routed)");
            $display("DEV_PASS one-hot select / bus_err on out-of-range");
            $display("ALL_PASS");
        end else begin
            $display("SOME_FAIL arb=%0d onehot=%0d", errors, onehot_err);
        end
        $finish;
    end
endmodule
`default_nettype wire
