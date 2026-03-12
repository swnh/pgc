/**
 * @file tb.h
 * @brief SystemC header file for the Testbench environment.
 * 
 * Includes the signal stimulus generators (`source()`) and validation 
 * monitors (`sink()`) required to automatically verify the `delta` DUT 
 * against dumped reference CSV files.
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

// Include the custom 53-bit data type definition
#include "delta.h"

/**
 * @struct GoldenOut
 * @brief Struct to hold expected (golden) output data for comparison.
 */
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

    // DUT input interface (stimulus driver)
    sc_in<bool>         fifo_in_rdy;
    sc_out<bool>        fifo_in_vld;
    sc_out<data53_t>    data_in;

    // DUT output interface (response checker)
    sc_in<data53_t>     data_out;
    sc_out<bool>        fifo_out_rdy;
    sc_in<bool>         fifo_out_vld;

    // ---------------- Processes ----------------
    /**
     * @brief Reads `scene-0061_00_in.csv`, formats data, and writes to DUT.
     */
    void source();

    /**
     * @brief Monitors DUT outputs, aligns with nodeIdx via FIFO queue, 
     * and compares results against `scene-0061_00_res.csv`.
     */
    void sink();

    // ---------------- Internal State ----------------
    std::queue<int> nodeIdx_queue;             ///< Synchronizes index order from source to sink
    std::map<int, GoldenOut> golden_map;       ///< Stores reference data keyed by nodeIdx

    int errors;              ///< Tracks number of mismatches
    int total_processed;     ///< Tracks number of responses evaluated
    int total_golden;        ///< Total number of golden items expected

    SC_CTOR(tb) {
        SC_CTHREAD(source, clk.pos());
        SC_CTHREAD(sink, clk.pos());
        errors = 0;
        total_processed = 0;
        total_golden = 0;
    }
};

#endif // TB_H