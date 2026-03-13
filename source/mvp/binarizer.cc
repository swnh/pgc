/**
 * @file binarizer.cc
 * @brief SystemC implementation of the Entropy Encoder Binarizer.
 * 
 * This module performs binarization on the residuals (dx, dy, dz) 
 * received from the delta module. It splits the data into two streams:
 * 1. Bin FIFO: Contains flags (zero, numBits, sign) for context modeling.
 * 2. Bypass Buffer: Contains the actual residual values (abs(res)-1) for 
 *    direct arithmetic encoding.
 * 
 * The binarization logic is implemented natively using SystemC constructs.
 */

#include "binarizer.h"
#include <iostream>

void binarizer::process_binarize() {
    // ----------------------------------------------------
    // Reset phase: Initialize hardware and memory
    // ----------------------------------------------------
    fifo_in_rdy.write(false);
    bin_fifo_vld.write(false);
    bypass_fifo_vld.write(false);
    bin_data_out.write(0);
    bypass_data_out.write(0);
    point_idx = 0;

    wait();

    // ----------------------------------------------------
    // Main Continuous Hardware Loop
    // ----------------------------------------------------
    while (true) {
        // --- 1. READ STAGE ---
        fifo_in_rdy.write(true);
        do {
            wait();
        } while (!fifo_in_vld.read());

        data53_t din = data_in.read();
        fifo_in_rdy.write(false);

        // Extract fields from delta output
        sc_uint<5> laser_id = din.range(52, 48);
        sc_int<16> dx = (sc_int<16>)din.range(47, 32);
        sc_int<16> dy = (sc_int<16>)din.range(31, 16);
        sc_int<16> dz = (sc_int<16>)din.range(15, 0);

        // --- 2. CALCULATION STAGE (Binarization) ---
        sc_int<16> res[3] = {dx, dy, dz};
        sc_uint<1> zero[3];
        sc_uint<4> numBits[3];
        sc_uint<1> sign[3];
        sc_uint<15> value[3];

        for (int i = 0; i < 3; i++) {
            sc_uint<16> abs_res = (res[i] < 0) ? (sc_uint<16>)(-res[i]) : (sc_uint<16>)res[i];
            sign[i] = (res[i] < 0) ? 1 : 0;
            zero[i] = (abs_res == 0) ? 1 : 0;

            if (zero[i]) {
                value[i] = 0;
                numBits[i] = 0;
            } else {
                value[i] = abs_res - 1;
                // Native numBits calculation: 1 + floor(log2(value))
                // Using a bounded for loop for HLS synthesizability
                sc_uint<4> n = 0;
                sc_uint<15> temp = value[i];
                for (int b = 0; b < 15; b++) {
                    if (temp > 0) {
                        n++;
                        temp >>= 1;
                    }
                }
                numBits[i] = n;
            }
        }

        // --- 3. WRITE STAGE ---
        // Pack Bin FIFO data (32 bits)
        sc_uint<32> bin_out = 0;
        bin_out.range(31, 23) = point_idx;
        bin_out.range(22, 18) = laser_id;
        // Z flags
        bin_out.range(17, 17) = zero[2];
        bin_out.range(16, 13) = numBits[2];
        bin_out.range(12, 12) = sign[2];
        // Y flags
        bin_out.range(11, 11) = zero[1];
        bin_out.range(10, 7)  = numBits[1];
        bin_out.range(6, 6)   = sign[1];
        // X flags
        bin_out.range(5, 5)   = zero[0];
        bin_out.range(4, 1)   = numBits[0];
        bin_out.range(0, 0)   = sign[0];

        // Pack Bypass Buffer data (45 bits)
        sc_uint<45> bypass_out = 0;
        bypass_out.range(44, 30) = value[2];
        bypass_out.range(29, 15) = value[1];
        bypass_out.range(14, 0)  = value[0];

        bin_data_out.write(bin_out);
        bypass_data_out.write(bypass_out);
        
        bool bin_accepted = false;
        bool bypass_accepted = false;

        // Multi-sink handshake logic: Ensure each sink consumes data exactly once
        while (!(bin_accepted && bypass_accepted)) {
            bin_fifo_vld.write(!bin_accepted);
            bypass_fifo_vld.write(!bypass_accepted);
            
            wait();

            if (!bin_accepted && bin_fifo_vld.read() && bin_fifo_rdy.read()) {
                bin_accepted = true;
            }
            if (!bypass_accepted && bypass_fifo_vld.read() && bypass_fifo_rdy.read()) {
                bypass_accepted = true;
            }
        }

        bin_fifo_vld.write(false);
        bypass_fifo_vld.write(false);

        // Increment point index for the next point in the ring
        point_idx = (point_idx == 383) ? 0 : point_idx + 1;
    }
}
