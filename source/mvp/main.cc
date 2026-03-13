/**
 * @file main.cc
 * @brief SystemC simulation entry point.
 * 
 * Instantiates the top-level module wrapper and starts the SystemC 
 * simulation engine kernel. The simulation will run continuously until 
 * explicitly stopped via `sc_stop()` within the testbench.
 */

#include <systemc.h>
#include <iostream>
#include "top.h"

int sc_main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_csv> <result_csv>" << std::endl;
        return -1;
    }

    // Instantiate top-level environment
    top t("top", argv[1], argv[2]);
    
    // Begin time-driven execution
    sc_start();
    
    return 0;
}