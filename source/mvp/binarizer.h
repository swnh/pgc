/**
 * @file binarizer.h
 * @brief SystemC header for the Entropy Encoder Binarizer.
 * 
 * This module fetches residuals (dx, dy, dz) from the delta module and 
 * performs binarization. It splits the data into:
 * 1. Bin FIFO: Contains flags (zero, numBits, sign) for context modeling.
 * 2. Bypass Buffer: Contains the actual residual values (abs(res)-1) for 
 *    direct arithmetic encoding.
 * 
 * Binarization Logic:
 * - zero_flag: 1 if abs(res) == 0, else 0.
 * - value: abs(res) - 1 (only if abs(res) > 0).
 * - numBits: 1 + floor(log2(value)) (only if value > 0, else 0).
 * - sign_flag: 1 if res < 0, else 0.
 */

#ifndef BINARIZER_H
#define BINARIZER_H

#include <systemc.h>
#include "delta.h"

SC_MODULE(binarizer) {
    // ---------------- Ports ----------------
    sc_in<bool> clk;
    sc_in<bool> rst;

    // Input from Delta Module
    sc_in<bool>         fifo_in_vld;
    sc_out<bool>        fifo_in_rdy;
    sc_in<data53_t>     data_in;

    // Output to Context Modeler (Bin FIFO)
    sc_out<bool>        bin_fifo_vld;
    sc_in<bool>         bin_fifo_rdy;
    sc_out<sc_uint<32>> bin_data_out;

    // Output to Arithmetic Encoder (Bypass Buffer)
    sc_out<bool>        bypass_fifo_vld;
    sc_in<bool>         bypass_fifo_rdy;
    sc_out<sc_uint<45>> bypass_data_out;

    // ---------------- Internal State ----------------
    sc_uint<9> point_idx; ///< Tracks point position in the 384-point ring

    // ---------------- Process ----------------
    void process_binarize();

    // ---------------- Constructor ----------------
    SC_CTOR(binarizer) {
        SC_CTHREAD(process_binarize, clk.pos());
        reset_signal_is(rst, true);
        point_idx = 0;
    }
};

#endif // BINARIZER_H
