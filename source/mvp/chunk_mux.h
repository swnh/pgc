/**
 * @file chunk_mux.h
 * @brief SystemC header for the Chunk Stream Multiplexer.
 */

#ifndef CHUNK_MUX_H
#define CHUNK_MUX_H

#include <systemc.h>

SC_MODULE(chunk_mux) {
    // ---------------- Ports ----------------
    sc_in<bool> clk;
    sc_in<bool> rst;

    // Input from Arithmetic Encoder (AEC Bytes)
    sc_in<bool>         aec_byte_vld;
    sc_out<bool>        aec_byte_rdy;
    sc_in<sc_uint<8>>   aec_byte_in;

    // Input from Binarizer (Bypass Bits with numBits metadata)
    sc_in<bool>         bypass_vld;
    sc_out<bool>        bypass_rdy;
    sc_in<sc_uint<57>>  bypass_data_in;

    // Output to Testbench (Chunked Byte Stream)
    sc_out<bool>        chunk_out_vld;
    sc_in<bool>         chunk_out_rdy;
    sc_out<sc_uint<8>>  chunk_out_byte;

    // Control
    sc_in<bool>         flush;

    // ---------------- Internal State ----------------
    sc_uint<8>  chunk_buf[256];
    int         aec_ptr;          // Next write index for AEC bytes (starts at 1)
    int         bypass_ptr;       // Next write index for bypass bytes (starts at 255)
    int         bypass_bit_idx;   // Bits remaining in current bypass byte (starts at 8)
    sc_uint<8>  bypass_accum;     // Current bypass byte being packed
    int         stream_index;     // For outputting the chunk

    enum state_t { IDLE, FINALIZE, STREAM_OUT };
    state_t state;

    // ---------------- Process ----------------
    void process_mux();

    // ---------------- Constructor ----------------
    SC_CTOR(chunk_mux) {
        SC_CTHREAD(process_mux, clk.pos());
        reset_signal_is(rst, true);
    }
};

#endif // CHUNK_MUX_H
