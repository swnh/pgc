/**
 * @file top.h
 * @brief SystemC wrapper defining the Top-Level module of the design.
 */

#ifndef TOP_H
#define TOP_H

#include <systemc.h>
#include "delta.h"
#include "binarizer.h"
#include "context_modeler.h"
#include "arith_encoder.h"
#include "chunk_mux.h"
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

    // Binarizer -> ChunkMux (Bypass)
    sc_signal<bool>        bypass_vld;
    sc_signal<bool>        bypass_rdy;
    sc_signal<sc_uint<57>> bypass_data_in;

    // Context Modeler -> Arith Encoder
    sc_signal<bool>        ae_vld;
    sc_signal<bool>        ae_rdy;
    sc_signal<bool>        ae_bit;
    sc_signal<sc_uint<16>> ae_prob;

    // Arith Encoder -> Context Modeler (Update)
    sc_signal<bool>        ae_update_vld;
    sc_signal<sc_uint<16>> ae_update_prob;

    // Arith Encoder -> ChunkMux (AEC Bytes)
    sc_signal<bool>        aec_byte_vld;
    sc_signal<bool>        aec_byte_rdy;
    sc_signal<sc_uint<8>>  aec_byte_in;

    // ChunkMux -> TB
    sc_signal<bool>        chunk_out_vld;
    sc_signal<bool>        chunk_out_rdy;
    sc_signal<sc_uint<8>>  chunk_out_byte;

    // Flush signal for chunk mux
    sc_signal<bool>        flush_sig;

    // ---------------- Component Instances ----------------
    delta           *dut_delta;
    binarizer       *dut_bin;
    context_modeler *dut_cm;
    arith_encoder   *dut_ae;
    chunk_mux       *dut_mux;
    tb              *tb_inst;

    // ---------------- Constructor ----------------
    top(sc_module_name name, std::string in_file, std::string res_file) 
        : sc_module(name), clk("clk", 10, SC_NS) {
        
        dut_delta = new delta("dut_delta");
        dut_delta->clk(clk);
        dut_delta->rst(rst);
        dut_delta->fifo_in_vld(tb_to_delta_vld);
        dut_delta->fifo_in_rdy(delta_in_rdy);
        dut_delta->data_in(tb_to_delta_data);
        dut_delta->data_out(delta_to_bin_data);
        dut_delta->fifo_out_vld(delta_out_vld);
        dut_delta->fifo_out_rdy(bin_in_rdy);

        dut_bin = new binarizer("dut_bin");
        dut_bin->clk(clk);
        dut_bin->rst(rst);
        dut_bin->fifo_in_vld(delta_out_vld);
        dut_bin->fifo_in_rdy(bin_in_rdy);
        dut_bin->data_in(delta_to_bin_data);
        dut_bin->bin_fifo_vld(bin_fifo_vld);
        dut_bin->bin_fifo_rdy(bin_fifo_rdy);
        dut_bin->bin_data_out(bin_data_out);
        dut_bin->bypass_vld(bypass_vld);
        dut_bin->bypass_rdy(bypass_rdy);
        dut_bin->bypass_data_out(bypass_data_in);

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

        dut_ae = new arith_encoder("dut_ae");
        dut_ae->clk(clk);
        dut_ae->rst(rst);
        dut_ae->ae_vld(ae_vld);
        dut_ae->ae_rdy(ae_rdy);
        dut_ae->ae_bit(ae_bit);
        dut_ae->ae_prob(ae_prob);
        dut_ae->ae_update_vld(ae_update_vld);
        dut_ae->ae_update_prob(ae_update_prob);
        dut_ae->byte_out_vld(aec_byte_vld);
        dut_ae->byte_out_rdy(aec_byte_rdy);
        dut_ae->byte_out(aec_byte_in);

        dut_mux = new chunk_mux("dut_mux");
        dut_mux->clk(clk);
        dut_mux->rst(rst);
        dut_mux->aec_byte_vld(aec_byte_vld);
        dut_mux->aec_byte_rdy(aec_byte_rdy);
        dut_mux->aec_byte_in(aec_byte_in);
        dut_mux->bypass_vld(bypass_vld);
        dut_mux->bypass_rdy(bypass_rdy);
        dut_mux->bypass_data_in(bypass_data_in);
        dut_mux->chunk_out_vld(chunk_out_vld);
        dut_mux->chunk_out_rdy(chunk_out_rdy);
        dut_mux->chunk_out_byte(chunk_out_byte);
        dut_mux->flush(flush_sig);

        tb_inst = new tb("tb_inst", in_file, res_file);
        tb_inst->clk(clk);
        tb_inst->rst(rst);
        tb_inst->fifo_in_vld(tb_to_delta_vld);
        tb_inst->fifo_in_rdy(delta_in_rdy);
        tb_inst->data_in(tb_to_delta_data);
        tb_inst->chunk_out_vld(chunk_out_vld);
        tb_inst->chunk_out_rdy(chunk_out_rdy);
        tb_inst->chunk_out_byte(chunk_out_byte);
        tb_inst->mon_bypass_vld(bypass_vld);
        tb_inst->mon_bypass_rdy(bypass_rdy);
        tb_inst->mon_bypass_data(bypass_data_in);
        tb_inst->flush_out(flush_sig);
    }
    
    ~top() {
        delete dut_delta;
        delete dut_bin;
        delete dut_cm;
        delete dut_ae;
        delete dut_mux;
        delete tb_inst;
    }
};

#endif // TOP_H
