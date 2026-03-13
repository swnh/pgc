/**
 * @file tb.h
 * @brief SystemC header file for the Testbench environment.
 */

#ifndef TB_H
#define TB_H

#include <systemc.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <queue>
#include <map>

#include "delta.h"

struct GoldenOut {
    int dx;
    int dy;
    int dz;
    int mode;
    int thetaIdx;
};

SC_MODULE(tb) {
    // ---------------- Interface Ports ----------------
    sc_in<bool> clk;
    sc_out<bool> rst;

    // TB -> Delta
    sc_in<bool>         fifo_in_rdy;
    sc_out<bool>        fifo_in_vld;
    sc_out<data53_t>    data_in;

    // ChunkMux -> TB
    sc_in<bool>         chunk_out_vld;
    sc_out<bool>        chunk_out_rdy;
    sc_in<sc_uint<8>>   chunk_out_byte;

    // Monitor ports for verification
    sc_in<bool>         mon_bypass_vld;
    sc_in<bool>         mon_bypass_rdy;
    sc_in<sc_uint<57>>  mon_bypass_data;

    // Flush output to chunk mux
    sc_out<bool>        flush_out;

    // ---------------- Processes ----------------
    void source();
    void sink();
    void report_final();

    // ---------------- Internal State ----------------
    std::queue<int> nodeIdx_queue;
    std::map<int, GoldenOut> golden_map;

    int errors;
    int total_processed;
    int total_golden;
    bool source_done;
    
    std::string in_file;
    std::string res_file;
    std::ofstream f_bin;

    tb(sc_module_name name, std::string in_file_, std::string res_file_) 
        : sc_module(name), in_file(in_file_), res_file(res_file_) {
        SC_CTHREAD(source, clk.pos());
        SC_CTHREAD(sink, clk.pos());
        errors = 0;
        total_processed = 0;
        total_golden = 0;
        source_done = false;
        
        f_bin.open("output.bin", std::ios::binary);
    }

    ~tb() {
        if (f_bin.is_open()) f_bin.close();
    }
};

#endif // TB_H
