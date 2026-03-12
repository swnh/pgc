/**
 * @file tb.cc
 * @brief SystemC implementation of the Testbench driver and monitor.
 * 
 * Functions:
 * - `source()`: Acts as the stimulus generator. It parses the expected output file
 *   to build an expected mapping `golden_map`, then parses the input file and feeds
 *   points into the DUT's `data_in` port using a handshaking mechanism.
 * - `sink()`: Acts as the validation monitor. It listens to the DUT's `data_out`
 *   port, extracts the calculated deltas, and compares them against the loaded 
 *   reference data to confirm functional correctness.
 */

#include "tb.h"

void tb::source() {
    // ----------------------------------------------------
    // System Reset Phase
    // ----------------------------------------------------
    rst.write(true);
    fifo_in_vld.write(false);
    data_in.write(0);

    wait(10);
    rst.write(false);
    wait(10);

    // ----------------------------------------------------
    // 1. Load Expected Results (Golden Data)
    // ----------------------------------------------------
    std::ifstream fg("/home/swnh/pgc/experiments/golden_ref_dump/scene-0061/scene-0061_00_res.csv");
    if (!fg.is_open()) {
        std::cerr << "Error opening golden output file!" << std::endl;
        sc_stop();
        return;
    }
    std::string line;
    std::getline(fg, line); // skip header: nodeIdx,dx,dy,dz,mode,thetaIdx
    while (std::getline(fg, line)) {
        if (line.empty() || line[0] == '\r') continue;
        std::stringstream ss(line);
        std::string token;
        int n_idx, dx, dy, dz, mode, theta;
        std::getline(ss, token, ','); n_idx = std::stoi(token);
        std::getline(ss, token, ','); dx = std::stoi(token);
        std::getline(ss, token, ','); dy = std::stoi(token);
        std::getline(ss, token, ','); dz = std::stoi(token);
        std::getline(ss, token, ','); mode = std::stoi(token);
        std::getline(ss, token, ','); theta = std::stoi(token);

        GoldenOut gout = {dx, dy, dz, mode, theta};
        golden_map[n_idx] = gout;
        total_golden++;
    }
    fg.close();

    std::cout << "Loaded " << total_golden << " golden results." << std::endl;

    // ----------------------------------------------------
    // 2. Load Inputs and Drive Stimulus
    // ----------------------------------------------------
    std::ifstream fi("/home/swnh/pgc/experiments/golden_ref_dump/scene-0061/scene-0061_00_in.csv");
    if (!fi.is_open()) {
        std::cerr << "Error opening input file!" << std::endl;
        sc_stop();
        return;
    }
    std::getline(fi, line); // skip header: nodeIdx,x,y,z,thetaIdx
    
    while (std::getline(fi, line)) {
        if (line.empty() || line[0] == '\r') continue;
        std::stringstream ss(line);
        std::string token;
        int n_idx, x, y, z, theta;
        std::getline(ss, token, ','); n_idx = std::stoi(token);
        std::getline(ss, token, ','); x = std::stoi(token);
        std::getline(ss, token, ','); y = std::stoi(token);
        std::getline(ss, token, ','); z = std::stoi(token);
        std::getline(ss, token, ','); theta = std::stoi(token);

        // Pack values into the 53-bit payload
        data53_t din = 0;
        din.range(52, 48) = theta;
        din.range(47, 32) = x;
        din.range(31, 16) = y;
        din.range(15, 0)  = z;

        data_in.write(din);
        fifo_in_vld.write(true);

        // Wait until DUT asserts ready (meaning it has captured the word)
        do {
            wait();
        } while (!fifo_in_rdy.read());

        // Queue the nodeIdx to synchronize the output checking side
        nodeIdx_queue.push(n_idx);
    }
    fi.close();

    // ----------------------------------------------------
    // 3. Pad Final Packet
    // ----------------------------------------------------
    // If the input data is not a perfect multiple of the 384 packet size, 
    // pad with zero-data to ensure the hardware flushes the final batch completely.
    int padding = 384 - (total_golden % 384);
    if (padding != 384) {
        data_in.write(0);
        for (int i = 0; i < padding; i++) {
            do {
                wait();
            } while (!fifo_in_rdy.read());
        }
    }

    fifo_in_vld.write(false);
    std::cout << "All inputs pushed to DUT." << std::endl;
}

void tb::sink() {
    fifo_out_rdy.write(false);
    
    // Wait out the reset period
    wait(20);
    
    while (true) {
        // Assert we are ready to receive data
        fifo_out_rdy.write(true);
        wait(); // Wait for next cycle boundary
        
        // Check if DUT produced valid data this cycle
        if (fifo_out_vld.read()) {
            data53_t dout = data_out.read();
            
            // Extract response payload
            sc_uint<5> laser_id = dout.range(52, 48);
            sc_int<16> dx = (int)dout.range(47, 32);
            sc_int<16> dy = (int)dout.range(31, 16);
            sc_int<16> dz = (int)dout.range(15, 0);

            // Pop corresponding ID out of synchronization queue
            if (nodeIdx_queue.empty()) {
                // Ignore empty outputs coming from artificial padding
                continue;
            }
            int n_idx = nodeIdx_queue.front();
            nodeIdx_queue.pop();

            // Validate against parsed expected dataset
            if (golden_map.find(n_idx) != golden_map.end()) {
                GoldenOut gout = golden_map[n_idx];
                bool match = true;
                
                // Compare payload parameters
                if ((int)laser_id != gout.thetaIdx || (int)dx != gout.dx || (int)dy != gout.dy || (int)dz != gout.dz) {
                    match = false;
                }

                if (!match) {
                    std::cout << "Mismatch at nodeIdx=" << n_idx 
                              << " Expected: theta=" << gout.thetaIdx << " dx=" << gout.dx << " dy=" << gout.dy << " dz=" << gout.dz
                              << " Got: theta=" << (int)laser_id << " dx=" << (int)dx << " dy=" << (int)dy << " dz=" << (int)dz << std::endl;
                    errors++;
                }
                total_processed++;
                
                // Periodic reporting
                if (total_processed % 1000 == 0) {
                    std::cout << "Processed " << total_processed << " points..." << std::endl;
                }
            } else {
                std::cerr << "NodeIdx " << n_idx << " not found in golden data!" << std::endl;
            }
            
            // End simulation if complete
            if (total_processed == total_golden && nodeIdx_queue.empty()) {
                std::cout << "Simulation finished. Total processed: " << total_processed << ". Errors: " << errors << std::endl;
                sc_stop();
            }
        }
    }
}