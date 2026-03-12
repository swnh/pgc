/**
 * @file main.cc
 * @brief SystemC simulation entry point.
 * 
 * Instantiates the top-level module wrapper and starts the SystemC 
 * simulation engine kernel. The simulation will run continuously until 
 * explicitly stopped via `sc_stop()` within the testbench.
 */

#include <systemc.h>
#include "top.h"

int sc_main(int argc, char* argv[]) {
    // Instantiate top-level environment
    top t("top");
    
    // Begin time-driven execution
    sc_start();
    
    return 0;
}