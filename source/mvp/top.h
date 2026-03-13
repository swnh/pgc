/**
 * @file top.h
 * @brief SystemC wrapper defining the Top-Level module of the design.
 * 
 * Instantiates the Delta Calculation Unit and the Binarizer, 
 * alongside the verification module (tb).
 */

#ifndef TOP_H
#define TOP_H

#include <systemc.h>
#include "delta.h"
#include "binarizer.h"
#include "context_modeler.h"
#include "arith_encoder.h"
#include "tb.h"

SC_MODULE(top) {
    // ---------------- Global Signals ----------------
    sc_clock clk;
    sc_signal<bool> rst;

    // ---------------- Interconnect Signals ----------------
    // TB -> Delta
    sc_signal<bool>     tb_to_delta_vld;
    sc_signal<bool>     delta_in_rdy;
    sc_signal<data53_t> tb_to_delta_data;

    // Delta -> Binarizer
    sc_signal<data53_t> delta_to_bin_data;
    sc_signal<bool>     delta_out_vld;
    sc_signal<bool>     bin_in_rdy;

    // Binarizer -> Context Modeler (Bin FIFO)
    sc_signal<bool>        bin_fifo_vld;
    sc_signal<bool>        bin_fifo_rdy;
    sc_signal<sc_uint<32>> bin_data_out;

    // Context Modeler -> Arithmetic Encoder
    sc_signal<bool>        ae_vld;
    sc_signal<bool>        ae_rdy;
    sc_signal<bool>        ae_bit;
    sc_signal<sc_uint<16>> ae_prob;

    // Arithmetic Encoder -> Context Modeler (Update)
    sc_signal<bool>        ae_update_vld;
    sc_signal<sc_uint<16>> ae_update_prob;

    // Arithmetic Encoder -> TB
    sc_signal<bool>        byte_out_vld;
    sc_signal<bool>        byte_out_rdy;
    sc_signal<sc_uint<8>>  byte_out;

    // Binarizer -> TB (Bypass Buffer)
    sc_signal<bool>        bypass_fifo_vld;
    sc_signal<bool>        bypass_fifo_rdy;
    sc_signal<sc_uint<45>> bypass_data_out;

    // ---------------- Component Instances ----------------
    delta           *dut_delta;
    binarizer       *dut_bin;
    context_modeler *dut_cm;
    arith_encoder   *dut_ae;
    tb              *tb_inst;

    //SC_HAS_PROCESS(top);

    // ---------------- Constructor ----------------
    top(sc_module_name name, std::string in_file, std::string res_file) 
        : sc_module(name), clk("clk", 10, SC_NS) {
        // Instantiate Delta
        dut_delta = new delta("dut_delta");
        dut_delta->clk(clk);
        dut_delta->rst(rst);
        dut_delta->fifo_in_vld(tb_to_delta_vld);
        dut_delta->fifo_in_rdy(delta_in_rdy);
        dut_delta->data_in(tb_to_delta_data);
        dut_delta->data_out(delta_to_bin_data);
        dut_delta->fifo_out_vld(delta_out_vld);
        dut_delta->fifo_out_rdy(bin_in_rdy);

        // Instantiate Binarizer
        dut_bin = new binarizer("dut_bin");
        dut_bin->clk(clk);
        dut_bin->rst(rst);
        dut_bin->fifo_in_vld(delta_out_vld);
        dut_bin->fifo_in_rdy(bin_in_rdy);
        dut_bin->data_in(delta_to_bin_data);
        dut_bin->bin_fifo_vld(bin_fifo_vld);
        dut_bin->bin_fifo_rdy(bin_fifo_rdy);
        dut_bin->bin_data_out(bin_data_out);
        dut_bin->bypass_fifo_vld(bypass_fifo_vld);
        dut_bin->bypass_fifo_rdy(bypass_fifo_rdy);
        dut_bin->bypass_data_out(bypass_data_out);

        // Instantiate Context Modeler
        dut_cm = new context_modeler("dut_cm");
        dut_cm->clk(clk);
        dut_cm->rst(rst);
        dut_cm->bin_fifo_vld(bin_fifo_vld);
        dut_cm->bin_fifo_rdy(bin_fifo_rdy);
        dut_cm->bin_data_in(bin_data_out);
        dut_cm->ae_vld(ae_vld);
        dut_cm->ae_rdy(ae_rdy);
        dut_cm->ae_bit(ae_bit);
        dut_cm->ae_prob(ae_prob);
        dut_cm->ae_update_vld(ae_update_vld);
        dut_cm->ae_update_prob(ae_update_prob);

        // Instantiate Arithmetic Encoder
        dut_ae = new arith_encoder("dut_ae");
        dut_ae->clk(clk);
        dut_ae->rst(rst);
        dut_ae->ae_vld(ae_vld);
        dut_ae->ae_rdy(ae_rdy);
        dut_ae->ae_bit(ae_bit);
        dut_ae->ae_prob(ae_prob);
        dut_ae->ae_update_vld(ae_update_vld);
        dut_ae->ae_update_prob(ae_update_prob);
        dut_ae->byte_out_vld(byte_out_vld);
        dut_ae->byte_out_rdy(byte_out_rdy);
        dut_ae->byte_out(byte_out);

        // Instantiate Testbench
        tb_inst = new tb("tb_inst", in_file, res_file);
        tb_inst->clk(clk);
        tb_inst->rst(rst);
        // TB -> Delta
        tb_inst->fifo_in_vld(tb_to_delta_vld);
        tb_inst->fifo_in_rdy(delta_in_rdy);
        tb_inst->data_in(tb_to_delta_data);
        // AE -> TB
        tb_inst->byte_out_vld(byte_out_vld);
        tb_inst->byte_out_rdy(byte_out_rdy);
        tb_inst->byte_out(byte_out);
        // Binarizer -> TB
        tb_inst->bypass_fifo_vld(bypass_fifo_vld);
        tb_inst->bypass_fifo_rdy(bypass_fifo_rdy);
        tb_inst->bypass_data_out(bypass_data_out);
    }
    
    ~top() {
        delete dut_delta;
        delete dut_bin;
        delete dut_cm;
        delete dut_ae;
        delete tb_inst;
    }
};

#endif // TOP_H
