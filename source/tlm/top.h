/**
 * @file top.h
 * @brief SystemC wrapper defining the Top-Level module of the design.
 * 
 * Instantiates the actual hardware calculation module (`delta`) alongside 
 * the verification module (`tb`) and binds all of their ports together 
 * via standard `sc_signal` channels. 
 * 
 * Also generates the global 10ns clock used to drive both modules synchronously.
 */

#ifndef TOP_H
#define TOP_H

#include <systemc.h>
#include "delta.h"
#include "tb.h"

SC_MODULE(top) {
    // ---------------- Global Signals ----------------
    sc_clock clk;              ///< Global clock, period = 10ns
    sc_signal<bool> rst;       ///< Global active-high reset signal

    // ---------------- Interconnect Signals ----------------
    sc_signal<bool>     fifo_in_rdy;  ///< Connects tb to dut: DUT is ready for data
    sc_signal<bool>     fifo_in_vld;  ///< Connects tb to dut: TB provides valid data
    sc_signal<data53_t> data_in;      ///< Connects tb to dut: 53-bit data payload

    sc_signal<data53_t> data_out;     ///< Connects dut to tb: 53-bit calculated delta
    sc_signal<bool>     fifo_out_rdy; ///< Connects dut to tb: TB is ready to check data
    sc_signal<bool>     fifo_out_vld; ///< Connects dut to tb: DUT output is valid

    // ---------------- Component Instances ----------------
    delta *dut;      ///< Pointer to the Design Under Test (DUT)
    tb    *tb_inst;  ///< Pointer to the Testbench environment

    // ---------------- Constructor ----------------
    SC_CTOR(top) : clk("clk", 10, SC_NS) {
        // Instantiate and bind DUT ports
        dut = new delta("dut");
        dut->clk(clk);
        dut->rst(rst);
        dut->fifo_in_rdy(fifo_in_rdy);
        dut->fifo_in_vld(fifo_in_vld);
        dut->data_in(data_in);
        dut->data_out(data_out);
        dut->fifo_out_rdy(fifo_out_rdy);
        dut->fifo_out_vld(fifo_out_vld);

        // Instantiate and bind Testbench ports
        tb_inst = new tb("tb_inst");
        tb_inst->clk(clk);
        tb_inst->rst(rst);
        tb_inst->fifo_in_rdy(fifo_in_rdy);
        tb_inst->fifo_in_vld(fifo_in_vld);
        tb_inst->data_in(data_in);
        tb_inst->data_out(data_out);
        tb_inst->fifo_out_rdy(fifo_out_rdy);
        tb_inst->fifo_out_vld(fifo_out_vld);
    }
    
    // ---------------- Destructor ----------------
    ~top() {
        // Free dynamically allocated modules
        delete dut;
        delete tb_inst;
    }
};

#endif // TOP_H