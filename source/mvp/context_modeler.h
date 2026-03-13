/**
 * @file context_modeler.h
 * @brief SystemC header for the Context Modeler Unit.
 * 
 * This module fetches binarized flags from the Binarizer and provides
 * context modeling for the Arithmetic Encoder. It stores probabilities
 * in a local SRAM and sequentially outputs (bit, prob) to the AE,
 * updating the probabilities based on feedback from the AE.
 */

#ifndef CONTEXT_MODELER_H
#define CONTEXT_MODELER_H

#include <systemc.h>

SC_MODULE(context_modeler) {
    // ---------------- Ports ----------------
    sc_in<bool> clk;
    sc_in<bool> rst;

    // Input from Binarizer (Bin FIFO)
    sc_in<bool>         bin_fifo_vld;
    sc_out<bool>        bin_fifo_rdy;
    sc_in<sc_uint<32>>  bin_data_in;

    // To Arithmetic Encoder
    sc_out<bool>        ae_vld;
    sc_in<bool>         ae_rdy;
    sc_out<bool>        ae_bit;
    sc_out<sc_uint<16>> ae_prob;

    // From Arithmetic Encoder (Probability Update)
    sc_in<bool>         ae_update_vld;
    sc_in<sc_uint<16>>  ae_update_prob;

    // ---------------- Internal State ----------------
    // Context Memory: 512 entries, 16 bits each
    // Addr 0-2: _ctxResGt0 (X, Y, Z)
    // Addr 3-5: _ctxSign (X, Y, Z)
    // Addr 6-230: _ctxNumBits (5 ctxIdx * 3 components * 15 treeIdx)
    sc_uint<16> ctx_mem[512];

    enum state_t { IDLE, READ_GT0, WAIT_GT0_UPDATE, NUM_BITS, WAIT_NUM_BITS_UPDATE, SIGN, WAIT_SIGN_UPDATE, NEXT_COMP };
    state_t state;
    
    int current_k;        // 0=X, 1=Y, 2=Z
    int current_ctxIdx;   // Context index for NumBits (0 to 4)
    int tree_n;           // current tree level (3 down to 0)
    int tree_idx;         // current tree node index (1 to 15)

    // Current point variables
    sc_uint<1> zero_flag[3];
    sc_uint<4> num_bits[3];
    sc_uint<1> sign_flag[3];

    // ---------------- Process ----------------
    void process_context();

    // ---------------- Constructor ----------------
    SC_CTOR(context_modeler) {
        SC_CTHREAD(process_context, clk.pos());
        reset_signal_is(rst, true);
    }
};

#endif // CONTEXT_MODELER_H
