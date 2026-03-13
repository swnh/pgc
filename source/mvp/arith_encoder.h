/**
 * @file arith_encoder.h
 * @brief SystemC header for the Arithmetic Encoder Unit.
 */

#ifndef ARITH_ENCODER_H
#define ARITH_ENCODER_H

#include <systemc.h>

SC_MODULE(arith_encoder) {
    // ---------------- Ports ----------------
    sc_in<bool> clk;
    sc_in<bool> rst;

    // Input from Context Modeler
    sc_in<bool>         ae_vld;
    sc_out<bool>        ae_rdy;
    sc_in<bool>         ae_bit;
    sc_in<sc_uint<16>>  ae_prob;

    // Update feedback to Context Modeler
    sc_out<bool>        ae_update_vld;
    sc_out<sc_uint<16>> ae_update_prob;

    // Output Bitstream (to Testbench / File)
    sc_out<bool>        byte_out_vld;
    sc_in<bool>         byte_out_rdy;
    sc_out<sc_uint<8>>  byte_out;

    // ---------------- Internal State ----------------
    sc_uint<32> low;
    sc_uint<16> range;
    sc_uint<4>  cntr;
    sc_uint<8>  output_byte;
    sc_uint<16> carry;
    bool        first_byte;

    enum state_t { IDLE, RENORM, EMIT_CARRY, EMIT_BYTE, EMIT_0xFF };
    state_t state;

    sc_uint<16> pending_carry_cnt;
    bool        pending_byte_valid;
    sc_uint<8>  pending_byte_val;

    // ---------------- Process ----------------
    void process_arith();

    // ---------------- Constructor ----------------
    SC_CTOR(arith_encoder) {
        SC_CTHREAD(process_arith, clk.pos());
        reset_signal_is(rst, true);
    }
};

#endif // ARITH_ENCODER_H
