#pragma once
#include <systemc.h>
#include "delta.h"
#include "binarizer.h"

SC_MODULE(pgc_dut) {

    sc_in<bool>             clk;
    sc_in<bool>             rst;

    sc_in<sc_uint<53>>      data_in;
    sc_in<bool>             tb_out_vld;
    sc_out<bool>            delta_in_rdy;

    sc_in<bool>             ctx_rdy;

    sc_out<sc_uint<60>>     ctx_bin_dx;
    sc_out<sc_uint<60>>     ctx_bin_dy;
    sc_out<sc_uint<60>>     ctx_bin_dz;
    sc_out<bool>            ctx_bin_vld;

    sc_out<sc_uint<54>>     bypass_bin_out;
    sc_out<bool>            bypass_vld;

    // delta tap — forwarded by SC_METHOD, not .bind()
    sc_out<sc_uint<53>>     delta_tap;
    sc_out<bool>            delta_tap_vld;

    delta       dut_delta;
    binarizer   dut_binarizer;

    sc_signal<sc_uint<53>>  sig_delta_data;
    sc_signal<bool>         sig_delta_vld;
    sc_signal<bool>         sig_binarizer_in_rdy;

    // forward internal signals to tap ports — avoids double-driver
    void tap_fwd() {
        delta_tap.write(sig_delta_data.read());
        delta_tap_vld.write(sig_delta_vld.read());
    }

    SC_CTOR(pgc_dut)
    : dut_delta("dut_delta"), dut_binarizer("dut_binarizer")
    {
        // ── delta ──────────────────────────────────────────────────
        dut_delta.clk(clk);
        dut_delta.rst(rst);
        dut_delta.data_in(data_in);
        dut_delta.tb_out_vld(tb_out_vld);
        dut_delta.delta_in_rdy(delta_in_rdy);
        dut_delta.data_out(sig_delta_data);
        dut_delta.delta_out_vld(sig_delta_vld);
        dut_delta.binarizer_in_rdy(sig_binarizer_in_rdy);

        // ── binarizer ──────────────────────────────────────────────
        dut_binarizer.clk(clk);
        dut_binarizer.rst(rst);
        dut_binarizer.delta_data_in(sig_delta_data);
        dut_binarizer.delta_out_vld(sig_delta_vld);
        dut_binarizer.ctx_rdy(ctx_rdy);
        dut_binarizer.ctx_bin_dx(ctx_bin_dx);
        dut_binarizer.ctx_bin_dy(ctx_bin_dy);
        dut_binarizer.ctx_bin_dz(ctx_bin_dz);
        dut_binarizer.ctx_bin_vld(ctx_bin_vld);
        dut_binarizer.bypass_bin_out(bypass_bin_out);
        dut_binarizer.bypass_vld(bypass_vld);
        dut_binarizer.binarizer_in_rdy(sig_binarizer_in_rdy);

        // ── tap forwarder ──────────────────────────────────────────
        SC_METHOD(tap_fwd);
        sensitive << sig_delta_data << sig_delta_vld;
    }
};