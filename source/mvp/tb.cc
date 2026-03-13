/**
 * @file tb.cc
 * @brief SystemC implementation of the Testbench driver and monitor.
 */

#include "tb.h"

void tb::report_final() {
    std::cout << "Simulation finished. Total processed: " << total_processed << ". Errors: " << errors << std::endl;
}

void tb::source() {
    rst.write(true);
    fifo_in_vld.write(false);
    data_in.write(0);

    wait(10);
    rst.write(false);
    wait(10);

    std::ifstream fg(res_file);
    if (!fg.is_open()) {
        std::cerr << "Error opening golden output file: " << res_file << std::endl;
        sc_stop();
        return;
    }
    std::string line;
    std::getline(fg, line);
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

    std::ifstream fi(in_file);
    if (!fi.is_open()) {
        std::cerr << "Error opening input file: " << in_file << std::endl;
        sc_stop();
        return;
    }
    std::getline(fi, line);
    
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

    int padding = 384 - (total_golden % 384);
    if (padding != 384) {
        data_in.write(0);
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
    chunk_out_rdy.write(false);
    flush_out.write(false);
    
    wait(20);
    
    int idle_cycles = 0;
    const int MAX_IDLE = 1000000;

    while (true) {
        chunk_out_rdy.write(true);
        wait();
        
        bool active = false;

        // Verify residuals via monitor port
        if (mon_bypass_vld.read() && mon_bypass_rdy.read()) {
            active = true;
            sc_uint<57> bp_data = mon_bypass_data.read();
            
            if (!nodeIdx_queue.empty()) {
                int n_idx = nodeIdx_queue.front();
                nodeIdx_queue.pop();

                if (golden_map.find(n_idx) != golden_map.end()) {
                    GoldenOut gout = golden_map[n_idx];
                    
                    sc_uint<15> exp_val[3];
                    int res[3] = {gout.dx, gout.dy, gout.dz};
                    for(int i=0; i<3; i++) {
                        int abs_res = (res[i] < 0) ? -res[i] : res[i];
                        exp_val[i] = (abs_res == 0) ? 0 : (abs_res - 1);
                    }

                    if (bp_data.range(14, 0) != exp_val[0] || // X
                        bp_data.range(29, 15) != exp_val[1] || // Y
                        bp_data.range(44, 30) != exp_val[2])   // Z
                    {
                        std::cout << "Residual Mismatch at nodeIdx=" << n_idx 
                                  << " exp: " << exp_val[0] << "," << exp_val[1] << "," << exp_val[2]
                                  << " got: " << bp_data.range(14, 0) << "," << bp_data.range(29, 15) << "," << bp_data.range(44, 30) << std::endl;
                        errors++;
                    }
                    total_processed++;
                }
            }
        }

        // Handle chunk output
        if (chunk_out_vld.read() && chunk_out_rdy.read()) {
            active = true;
            sc_uint<8> byte = chunk_out_byte.read();
            char c = (char)byte.to_uint();
            if (f_bin.is_open()) {
                f_bin.write(&c, 1);
            }
        }

        if (active) {
            idle_cycles = 0;
        } else {
            idle_cycles++;
        }

        if (source_done && nodeIdx_queue.empty() && idle_cycles > 10000) {
            flush_out.write(true);
            wait();
            flush_out.write(false);
            report_final();
            sc_stop();
            return;
        }
        
        if (idle_cycles > MAX_IDLE) {
            std::cerr << "Sink timeout!" << std::endl;
            report_final();
            sc_stop();
            return;
        }
    }
}
