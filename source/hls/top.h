#pragma once
#include <systemc.h>
#include "pgc_dut.h"
#include "tb.h"

SC_MODULE(Top) {

    pgc_dut dut;
    Tb      tb;

    sc_clock                clk;
    sc_signal<bool>         rst;

    sc_signal<sc_uint<53>>  data_in;
    sc_signal<bool>         tb_out_vld;
    sc_signal<bool>         ctx_rdy;
    sc_signal<bool>         delta_in_rdy;
    sc_signal<sc_uint<60>>  ctx_bin_dx;
    sc_signal<sc_uint<60>>  ctx_bin_dy;
    sc_signal<sc_uint<60>>  ctx_bin_dz;
    sc_signal<bool>         ctx_bin_vld;
    sc_signal<sc_uint<54>>  bypass_bin_out;
    sc_signal<bool>         bypass_vld;
    sc_signal<sc_uint<53>>  delta_tap;
    sc_signal<bool>         delta_tap_vld;

    SC_CTOR(Top)
    : dut("dut"), tb("tb"), clk("clk", 10, SC_NS)
    {
        dut.clk(clk);                   dut.rst(rst);
        dut.data_in(data_in);           dut.tb_out_vld(tb_out_vld);
        dut.delta_in_rdy(delta_in_rdy); dut.ctx_rdy(ctx_rdy);
        dut.ctx_bin_dx(ctx_bin_dx);     dut.ctx_bin_dy(ctx_bin_dy);
        dut.ctx_bin_dz(ctx_bin_dz);     dut.ctx_bin_vld(ctx_bin_vld);
        dut.bypass_bin_out(bypass_bin_out); dut.bypass_vld(bypass_vld);
        dut.delta_tap(delta_tap);       dut.delta_tap_vld(delta_tap_vld);

        tb.clk(clk);                    tb.rst(rst);
        tb.data_in(data_in);            tb.tb_out_vld(tb_out_vld);
        tb.delta_in_rdy(delta_in_rdy);  tb.ctx_rdy(ctx_rdy);
        tb.ctx_bin_dx(ctx_bin_dx);      tb.ctx_bin_dy(ctx_bin_dy);
        tb.ctx_bin_dz(ctx_bin_dz);      tb.ctx_bin_vld(ctx_bin_vld);
        tb.bypass_bin_out(bypass_bin_out); tb.bypass_vld(bypass_vld);
        tb.delta_tap(delta_tap);        tb.delta_tap_vld(delta_tap_vld);
    }
};