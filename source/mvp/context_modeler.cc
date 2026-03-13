/**
 * @file context_modeler.cc
 * @brief SystemC implementation of the Context Modeler Unit.
 */

#include "context_modeler.h"
#include <iostream>

void context_modeler::process_context() {
    // ----------------------------------------------------
    // Reset Phase
    // ----------------------------------------------------
    bin_fifo_rdy.write(false);
    ae_vld.write(false);
    ae_bit.write(false);
    ae_prob.write(0);
    
    state = IDLE;
    current_k = 0;
    current_ctxIdx = 0;
    tree_n = 0;
    tree_idx = 1;

    // Initialize Context Memory to p=0.5 (0x8000)
    for (int i = 0; i < 512; i++) {
        ctx_mem[i] = 0x8000;
    }

    wait();

    // ----------------------------------------------------
    // Main FSM
    // ----------------------------------------------------
    while (true) {
        switch (state) {
            case IDLE:
                bin_fifo_rdy.write(true);
                ae_vld.write(false);
                
                if (bin_fifo_vld.read() && bin_fifo_rdy.read()) {
                    sc_uint<32> din = bin_data_in.read();
                    bin_fifo_rdy.write(false);

                    // Extract fields
                    // X flags
                    zero_flag[0] = din.range(5, 5);
                    num_bits[0]  = din.range(4, 1);
                    sign_flag[0] = din.range(0, 0);
                    // Y flags
                    zero_flag[1] = din.range(11, 11);
                    num_bits[1]  = din.range(10, 7);
                    sign_flag[1] = din.range(6, 6);
                    // Z flags
                    zero_flag[2] = din.range(17, 17);
                    num_bits[2]  = din.range(16, 13);
                    sign_flag[2] = din.range(12, 12);

                    current_k = 0;
                    current_ctxIdx = 0; // Starts at 0 for X
                    state = READ_GT0;
                }
                break;

            case READ_GT0:
                {
                    int addr = current_k; // _ctxResGt0
                    sc_uint<16> prob = ctx_mem[addr];
                    bool bit = !zero_flag[current_k]; // encode(res != 0)

                    ae_vld.write(true);
                    ae_bit.write(bit);
                    ae_prob.write(prob);

                    if (ae_vld.read() && ae_rdy.read()) {
                        state = WAIT_GT0_UPDATE;
                    }
                }
                break;

            case WAIT_GT0_UPDATE:
                ae_vld.write(false);
                if (ae_update_vld.read()) {
                    int addr = current_k;
                    ctx_mem[addr] = ae_update_prob.read();

                    if (zero_flag[current_k]) {
                        // Res == 0, skip NumBits and Sign
                        state = NEXT_COMP;
                    } else {
                        // Setup for NumBits tree
                        tree_n = 3; // _pgeom_resid_abs_log2_bits[k] - 1 (assumed 4 bits max)
                        tree_idx = 1;
                        state = NUM_BITS;
                    }
                }
                break;

            case NUM_BITS:
                {
                    bool bit = (num_bits[current_k] >> tree_n) & 1;
                    // Address formula: 6 + ctxIdx * 3 * 15 + k * 15 + (tree_idx - 1)
                    int addr = 6 + (current_ctxIdx * 45) + (current_k * 15) + (tree_idx - 1);
                    sc_uint<16> prob = ctx_mem[addr];

                    ae_vld.write(true);
                    ae_bit.write(bit);
                    ae_prob.write(prob);

                    if (ae_vld.read() && ae_rdy.read()) {
                        state = WAIT_NUM_BITS_UPDATE;
                    }
                }
                break;

            case WAIT_NUM_BITS_UPDATE:
                ae_vld.write(false);
                if (ae_update_vld.read()) {
                    bool bit = (num_bits[current_k] >> tree_n) & 1;
                    int addr = 6 + (current_ctxIdx * 45) + (current_k * 15) + (tree_idx - 1);
                    ctx_mem[addr] = ae_update_prob.read();

                    tree_idx = (tree_idx << 1) | bit;
                    tree_n--;

                    if (tree_n >= 0) {
                        state = NUM_BITS;
                    } else {
                        state = SIGN;
                    }
                }
                break;

            case SIGN:
                {
                    int addr = 3 + current_k; // _ctxSign
                    sc_uint<16> prob = ctx_mem[addr];
                    bool bit = sign_flag[current_k];

                    ae_vld.write(true);
                    ae_bit.write(bit);
                    ae_prob.write(prob);

                    if (ae_vld.read() && ae_rdy.read()) {
                        state = WAIT_SIGN_UPDATE;
                    }
                }
                break;

            case WAIT_SIGN_UPDATE:
                ae_vld.write(false);
                if (ae_update_vld.read()) {
                    int addr = 3 + current_k;
                    ctx_mem[addr] = ae_update_prob.read();
                    state = NEXT_COMP;
                }
                break;

            case NEXT_COMP:
                if (current_k == 0) {
                    // Update ctxIdx for Y and Z based on X's numBits
                    sc_uint<4> nb_x = num_bits[0];
                    current_ctxIdx = (nb_x + 1) >> 1;
                    if (current_ctxIdx > 4) current_ctxIdx = 4;
                }

                current_k++;
                if (current_k < 3) {
                    state = READ_GT0;
                } else {
                    state = IDLE; // Done with X, Y, Z
                }
                break;
        }

        wait();
    }
}
