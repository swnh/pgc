/**
 * @file delta.h
 * @brief SystemC header file for the Delta Calculation Unit.
 * 
 * This module is responsible for calculating the delta (difference) between the 
 * current point's coordinates and the previously recorded coordinates for a specific laser.
 * 
 * Architecture details:
 * - Includes a 384-element FIFO to buffer incoming coordinate packets.
 * - Includes a 32-element History Buffer memory to store the last known state of up to 32 unique lasers.
 * - Employs a handshaking interface (`valid`/`ready`) for both input and output sides.
 * - All computations are implemented as a single cycle-accurate `SC_CTHREAD` to facilitate future HLS synthesis.
 * 
 * Data format (53 bits):
 * - [52:48]: Laser ID (5 bits) - Used as the address for the History Buffer.
 * - [47:32]: X coordinate (16 bits)
 * - [31:16]: Y coordinate (16 bits)
 * - [15:0]:  Z coordinate (16 bits)
 */

#ifndef DELTA_H
#define DELTA_H

#include <systemc.h>

typedef sc_uint<53> data53_t;

SC_MODULE(delta) {
    // ---------------- Ports ----------------
    // Clock and Reset
    sc_in<bool>         clk;
    sc_in<bool>         rst;

    // Input Interface (Valid/Ready Handshake)
    sc_out<bool>        fifo_in_rdy;  ///< High when module can accept a new point
    sc_in<bool>         fifo_in_vld;  ///< High when testbench is providing a valid point
    sc_in<data53_t>     data_in;      ///< 53-bit incoming data word

    // Output Interface (Valid/Ready Handshake)
    sc_out<data53_t>    data_out;     ///< 53-bit calculated delta output word
    sc_in<bool>         fifo_out_rdy; ///< High when downstream is ready to receive
    sc_out<bool>        fifo_out_vld; ///< High when delta calculation is valid

    // ---------------- Internal State & Memory ----------------
    // Input FIFO
    data53_t fifo_mem[384];           ///< Packet buffer (stores up to 384 points)
    sc_uint<9>  wr_ptr;               ///< FIFO write pointer
    sc_uint<9>  rd_ptr;               ///< FIFO read pointer
    sc_uint<9>  fifo_count;           ///< Tracks the number of items currently in the FIFO

    // History Buffer
    data53_t hist_mem[32];            ///< Memory storing the previous point (address = laser ID)

    // Pipeline State Machine
    enum state_t { IDLE, CALC, WRITE_OUT, OUTPUT };
    state_t state;                    ///< Current state of the pipeline calculation
    data53_t curr_data;               ///< Register holding the point currently being processed

    // ---------------- Process ----------------
    /**
     * @brief The main hardware pipeline thread. 
     * Driven synchronously by `clk.pos()`.
     */
    void process_delta();

    // ---------------- Constructor ----------------
    SC_CTOR(delta) {
        SC_CTHREAD(process_delta, clk.pos());
        reset_signal_is(rst, true);   // Active high reset
    }
};

#endif // DELTA_H