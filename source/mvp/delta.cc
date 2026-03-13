/**
 * @file delta.cc
 * @brief SystemC implementation of the Delta Calculation Unit.
 * 
 * This module computes the spatial delta (dx, dy, dz) between an incoming 3D point
 * and the previous point mapped to the same laser ID.
 * 
 * Pipeline Architecture:
 * 1. Input FIFO writing: Captures streaming input when `fifo_in_rdy` is high. 
 *    Reads precisely 384 points (one packet).
 * 2. Processing State Machine: Runs concurrently as points enter the FIFO.
 *    - IDLE: Reads next point from the FIFO, fetches previous point from the 
 *      History Buffer using `laser_id`.
 *    - CALC: Computes `dx`, `dy`, and `dz`.
 *    - WRITE_OUT: Writes the new `curr_data` back to the History Buffer 
 *      and formats the output word.
 *    - OUTPUT: Holds `fifo_out_vld` high until the downstream sink is ready.
 * 
 * At the conclusion of 384 processed points, the system resets its packet 
 * counters and begins listening for the next incoming data packet.
 */

#include "delta.h"
#include <iostream>

void delta::process_delta() {
    // ----------------------------------------------------
    // Reset phase: Initialize hardware and memory
    // ----------------------------------------------------
    fifo_in_rdy.write(false);
    fifo_out_vld.write(false);
    data_out.write(0);

    wr_ptr = 0;
    rd_ptr = 0;
    fifo_count = 0;
    state = IDLE;
    curr_data = 0;

    // Clear history buffer entries
    for (int i = 0; i < 32; i++) {
        hist_mem[i] = 0;
    }

    int points_received = 0;
    int points_processed = 0;

    // Pipeline registers
    sc_uint<5> laser_id;
    sc_uint<16> x, y, z;
    sc_uint<16> prev_x, prev_y, prev_z;
    sc_int<16> dx, dy, dz;
    data53_t prev_data;
    data53_t out_val;

    // Wait for the end of the reset cycle
    wait();

    // ----------------------------------------------------
    // Main Continuous Hardware Loop
    // ----------------------------------------------------
    while (true) {
        // --- 1. SAMPLE INPUTS ---
        bool vld_in = fifo_in_vld.read();
        data53_t d_in = data_in.read();
        bool rdy_out = fifo_out_rdy.read();

        bool do_write_fifo = false;
        bool do_read_fifo = false;

        // The module stops accepting inputs after 384 points until processing finishes
        bool is_rdy = (points_received < 384);

        // --- 2. INPUT LOGIC ---
        // Push incoming streaming data into the local FIFO buffer
        if (is_rdy && vld_in) {
            fifo_mem[wr_ptr] = d_in;
            wr_ptr = (wr_ptr == 383) ? 0 : wr_ptr + 1;
            do_write_fifo = true;
            points_received++;
        }

        // --- 3. PROCESSING LOGIC ---
        state_t next_state = state;
        bool set_vld_out = false;
        data53_t next_out_val = out_val;

        switch (state) {
            case IDLE:
                // Only begin processing if FIFO has data AND we haven't processed a full packet
                if (fifo_count > 0 && points_processed < 384) {
                    curr_data = fifo_mem[rd_ptr];
                    rd_ptr = (rd_ptr == 383) ? 0 : rd_ptr + 1;
                    do_read_fifo = true;
                    
                    laser_id = curr_data.range(52, 48);
                    x = curr_data.range(47, 32);
                    y = curr_data.range(31, 16);
                    z = curr_data.range(15, 0);

                    prev_data = hist_mem[laser_id];
                    next_state = CALC;
                }
                break;

            case CALC:
                prev_x = prev_data.range(47, 32);
                prev_y = prev_data.range(31, 16);
                prev_z = prev_data.range(15, 0);

                // Compute coordinate deltas
                dx = (sc_int<16>)x - (sc_int<16>)prev_x;
                dy = (sc_int<16>)y - (sc_int<16>)prev_y;
                dz = (sc_int<16>)z - (sc_int<16>)prev_z;
                
                next_state = WRITE_OUT;
                break;

            case WRITE_OUT:
                hist_mem[laser_id] = curr_data; // Store the *current* point as the next *previous* point

                // Format the output datapath
                next_out_val = 0;
                next_out_val.range(52, 48) = laser_id;
                next_out_val.range(47, 32) = dx;
                next_out_val.range(31, 16) = dy;
                next_out_val.range(15, 0)  = dz;

                set_vld_out = true;
                next_state = OUTPUT;
                break;

            case OUTPUT:
                set_vld_out = true; // Hold valid flag high until sink confirms ready
                if (rdy_out) {
                    set_vld_out = false;
                    points_processed++;
                    
                    if (points_processed == 384) {
                        // End of packet 
                        points_received = 0;
                        points_processed = 0;
                        next_state = IDLE;
                    } else if (fifo_count > 0) {
                        // Immediately transition to the next point read if FIFO is already buffered
                        curr_data = fifo_mem[rd_ptr];
                        rd_ptr = (rd_ptr == 383) ? 0 : rd_ptr + 1;
                        do_read_fifo = true;
                        
                        laser_id = curr_data.range(52, 48);
                        x = curr_data.range(47, 32);
                        y = curr_data.range(31, 16);
                        z = curr_data.range(15, 0);

                        prev_data = hist_mem[laser_id];
                        next_state = CALC;
                    } else {
                        // Return to idle and wait for FIFO to refill
                        next_state = IDLE;
                    }
                }
                break;
        }

        // --- 4. STATE UPDATES ---
        state = next_state;
        out_val = next_out_val;

        // Keep FIFO fill-level updated symmetrically
        if (do_write_fifo && !do_read_fifo) {
            fifo_count++;
        } else if (!do_write_fifo && do_read_fifo) {
            fifo_count--;
        }

        // --- 5. DRIVE OUTPUTS FOR NEXT CYCLE ---
        fifo_in_rdy.write(points_received < 384);
        fifo_out_vld.write(set_vld_out);
        data_out.write(out_val);

        // Advance to the next clock cycle
        wait();
    }
}