/**
 * @file tb.cc
 * @brief SystemC implementation of the Testbench driver and monitor.
 */

#include "tb.h"

void tb::report_final() {
    std::cout << "Simulation finished. Total processed: " << total_processed << ". Errors: " << errors << std::endl;
}

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
    std::ifstream fg(res_file);
    if (!fg.is_open()) {
        std::cerr << "Error opening golden output file: " << res_file << std::endl;
        report_final();
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
    std::ifstream fi(in_file);
    if (!fi.is_open()) {
        std::cerr << "Error opening input file: " << in_file << std::endl;
        report_final();
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

        data53_t din = 0;
        din.range(52, 48) = theta;
        din.range(47, 32) = x;
        din.range(31, 16) = y;
        din.range(15, 0)  = z;

        data_in.write(din);
        fifo_in_vld.write(true);

        int wait_cnt = 0;
        do {
            wait();
            if (++wait_cnt > 100000) {
                std::cerr << "Source timeout waiting for fifo_in_rdy at nodeIdx=" << n_idx << std::endl;
                report_final();
                sc_stop();
                return;
            }
        } while (!fifo_in_rdy.read());

        nodeIdx_queue.push(n_idx);
    }
    fi.close();

    // ----------------------------------------------------
    // 3. Pad Final Packet
    // ----------------------------------------------------
    int padding = 384 - (total_golden % 384);
    if (padding != 384) {
        data_in.write(0);
        fifo_in_vld.write(true);
        for (int i = 0; i < padding; i++) {
            int wait_cnt = 0;
            do {
                wait();
                if (++wait_cnt > 100000) {
                    std::cerr << "Source timeout waiting for fifo_in_rdy during padding" << std::endl;
                    report_final();
                    sc_stop();
                    return;
                }
            } while (!fifo_in_rdy.read());
        }
    }

    fifo_in_vld.write(false);
    source_done = true;
    std::cout << "All inputs pushed to DUT." << std::endl;
}

void tb::sink() {
    byte_out_rdy.write(false);
    bypass_fifo_rdy.write(false);
    
    wait(20);
    
    int idle_cycles = 0;
    const int MAX_IDLE = 1000000; // 1M cycles

    while (true) {
        byte_out_rdy.write(true);
        bypass_fifo_rdy.write(true);
        wait();
        
        bool active = false;

        if (byte_out_vld.read() && byte_out_rdy.read()) {
            active = true;
            sc_uint<8> byte = byte_out.read();
            char c = (char)byte.to_uint();
            if (f_bin.is_open()) {
                f_bin.write(&c, 1);
            }
        }

        if (bypass_fifo_vld.read() && bypass_fifo_rdy.read()) {
            active = true;
            sc_uint<45> bypass_data = bypass_data_out.read();

            if (!nodeIdx_queue.empty()) {
                int n_idx = nodeIdx_queue.front();
                nodeIdx_queue.pop();

                if (golden_map.find(n_idx) != golden_map.end()) {
                    GoldenOut gout = golden_map[n_idx];
                    
                    // Calculate expected binarization
                    int res[3] = {gout.dx, gout.dy, gout.dz};
                    sc_uint<15> exp_value[3];

                    for (int i = 0; i < 3; i++) {
                        int abs_res = (res[i] < 0) ? -res[i] : res[i];
                        if (abs_res == 0) {
                            exp_value[i] = 0;
                        } else {
                            exp_value[i] = abs_res - 1;
                        }
                    }

                    // Verify Bypass Buffer
                    bool bypass_match = true;
                    if (bypass_data.range(44, 30) != exp_value[2]) bypass_match = false;
                    if (bypass_data.range(29, 15) != exp_value[1]) bypass_match = false;
                    if (bypass_data.range(14, 0) != exp_value[0]) bypass_match = false;

                    if (!bypass_match) {
                        std::cout << "Mismatch at nodeIdx=" << n_idx << std::endl;
                        errors++;
                    }

                    total_processed++;
                } else {
                    std::cerr << "NodeIdx " << n_idx << " not found in golden data!" << std::endl;
                }
            } else {
                std::cerr << "Received bypass data but nodeIdx_queue is empty!" << std::endl;
            }
        }

        if (active) {
            idle_cycles = 0;
        } else {
            idle_cycles++;
        }

        // End simulation if complete
        if (total_golden > 0 && total_processed >= total_golden) {
            // Wait a few more cycles to ensure all byte_out are captured
            for(int i=0; i<100; i++) {
        if (byte_out_vld.read() && byte_out_rdy.read()) {
            sc_uint<8> byte = byte_out.read();
                    char c = (char)byte.to_uint();
                    if (f_bin.is_open()) f_bin.write(&c, 1);
                }
                wait();
            }
            std::cout << "Simulation finished. Total processed: " << total_processed << ". Errors: " << errors << std::endl;
            sc_stop();
            return;
        }

        if (source_done && nodeIdx_queue.empty() && idle_cycles > 1000) {
            std::cout << "Simulation finished (source done and idle). Total processed: " << total_processed << ". Errors: " << errors << std::endl;
            sc_stop();
            return;
        }

        if (idle_cycles >= MAX_IDLE) {
            std::cout << "Simulation timeout: No activity for " << MAX_IDLE << " cycles." << std::endl;
            std::cout << "Processed: " << total_processed << " / " << total_golden << std::endl;
            sc_stop();
            return;
        }
    }
}
