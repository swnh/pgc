/**
 * @file chunk_mux.cc
 * @brief SystemC implementation of the Chunk Stream Multiplexer.
 */

#include "chunk_mux.h"
#include <iostream>

void chunk_mux::process_mux() {
    // Reset initialization
    aec_byte_rdy.write(false);
    bypass_rdy.write(false);
    chunk_out_vld.write(false);
    chunk_out_byte.write(0);

    aec_ptr = 1;
    bypass_ptr = 255;
    bypass_bit_idx = 8;
    stream_index = 0;

    for(int i=0; i<256; i++) chunk_buf[i] = 0;

    state = IDLE;

    wait();

    while (true) {
        // Sample inputs
        bool aec_vld = aec_byte_vld.read();
        sc_uint<8> aec_in = aec_byte_in.read();
        bool bp_vld = bypass_vld.read();
        sc_uint<57> bp_data = bypass_data_in.read();
        bool chunk_rdy = chunk_out_rdy.read();
        bool flush_sig = flush.read();

        state_t next_state = state;

        switch (state) {
            case IDLE:
                // Ready signals: only if we have space
                // aec_ptr points to next free byte for AEC
                // bypass_ptr points to CURRENT partial byte for bypass
                // available bytes = bypass_ptr - aec_ptr
                {
                    bool has_space = (aec_ptr < bypass_ptr);
                    aec_byte_rdy.write(has_space);
                    bypass_rdy.write(has_space);
                    
                    if (flush_sig) {
                        next_state = FINALIZE;
                    } else if (aec_vld && aec_byte_rdy.read()) {
                        aec_byte_rdy.write(false);
                        bypass_rdy.write(false);
                        chunk_buf[aec_ptr] = aec_in;
                        aec_ptr++;
                        if (aec_ptr == bypass_ptr) {
                            next_state = FINALIZE;
                        }
                    } else if (bp_vld && bypass_rdy.read()) {
                        aec_byte_rdy.write(false);
                        bypass_rdy.write(false);
                        
                        sc_uint<15> vals[3];
                        vals[0] = bp_data.range(14, 0);  // Z
                        vals[1] = bp_data.range(29, 15); // Y
                        vals[2] = bp_data.range(44, 30); // X
                        
                        sc_uint<4> nbs[3];
                        nbs[0] = bp_data.range(48, 45); // Z
                        nbs[1] = bp_data.range(52, 49); // Y
                        nbs[2] = bp_data.range(56, 53); // X

                        for (int k = 2; k >= 0; k--) {
                            int num_bypass_bits = (nbs[k] == 0) ? 0 : (nbs[k].to_int() - 1);
                            for (int b = 0; b < num_bypass_bits; b++) {
                                bool bit = (vals[k] >> b) & 1;
                                
                                if (--bypass_bit_idx < 0) {
                                    bypass_ptr--;
                                    bypass_bit_idx = 7;
                                    // Check for collision after allocating new byte
                                    if (aec_ptr == bypass_ptr) {
                                        // Collision! We must finalize.
                                        // In real hardware, we might need to stall in the middle of processing bits.
                                        // For now, we'll assume we can finish the current point's bits and then finalize.
                                        // Wait, the software allows this because it has large buffers.
                                        // Let's just finish the loop and then check.
                                    }
                                }
                                chunk_buf[bypass_ptr] = (chunk_buf[bypass_ptr] << 1) | (bit ? 1 : 0);
                            }
                        }
                        if (aec_ptr >= bypass_ptr) {
                            next_state = FINALIZE;
                        }
                    }
                }
                break;

            case FINALIZE:
                aec_byte_rdy.write(false);
                bypass_rdy.write(false);
                {
                    // Write header
                    chunk_buf[0] = aec_ptr - 1;

                    // Software padding logic
                    if (bypass_ptr < 255 || bypass_bit_idx < 8) {
                        int flushed_bits = bypass_bit_idx - 3;
                        // Shift current partial byte
                        chunk_buf[bypass_ptr] <<= bypass_bit_idx;
                        
                        if (flushed_bits < 0) {
                            bypass_ptr--;
                            chunk_buf[bypass_ptr] = 0;
                            flushed_bits += 8;
                        }
                        chunk_buf[bypass_ptr] |= (sc_uint<8>)(flushed_bits & 0x7);
                    }
                    
                    stream_index = 0;
                    next_state = STREAM_OUT;
                }
                break;

            case STREAM_OUT:
                aec_byte_rdy.write(false);
                bypass_rdy.write(false);
                if (chunk_rdy) {
                    chunk_out_byte.write(chunk_buf[stream_index]);
                    chunk_out_vld.write(true);
                    stream_index++;
                    if (stream_index == 256) {
                        // Reset for next chunk
                        aec_ptr = 1;
                        bypass_ptr = 255;
                        bypass_bit_idx = 8;
                        for(int i=0; i<256; i++) chunk_buf[i] = 0;
                        next_state = IDLE;
                        chunk_out_vld.write(false);
                    }
                } else {
                    chunk_out_vld.write(false);
                }
                break;
        }

        state = next_state;
        wait();
    }
}
