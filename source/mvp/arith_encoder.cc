/**
 * @file arith_encoder.cc
 * @brief SystemC implementation of the Arithmetic Encoder Unit.
 */

#include "arith_encoder.h"
#include <iostream>

static const int16_t lut_interleaved[512] = {
    255,     0,   376,    -2,   471,    -5,   553,    -8,
    625,   -11,   690,   -15,   750,   -20,   805,   -24,
    857,   -29,   906,   -35,   952,   -41,   995,   -47,
   1037,   -53,  1077,   -60,  1114,   -67,  1151,   -74,
   1186,   -82,  1219,   -89,  1251,   -97,  1282,  -106,
   1312,  -114,  1341,  -123,  1369,  -132,  1396,  -141,
   1422,  -150,  1447,  -160,  1471,  -170,  1495,  -180,
   1518,  -190,  1540,  -201,  1561,  -211,  1582,  -222,
   1602,  -233,  1622,  -244,  1640,  -256,  1659,  -267,
   1676,  -279,  1694,  -291,  1710,  -303,  1727,  -315,
   1742,  -327,  1757,  -340,  1772,  -353,  1786,  -366,
   1800,  -379,  1814,  -392,  1827,  -405,  1839,  -419,
   1851,  -433,  1863,  -447,  1874,  -461,  1885,  -475,
   1896,  -489,  1906,  -504,  1916,  -518,  1925,  -533,
   1934,  -548,  1943,  -563,  1952,  -578,  1960,  -593,
   1968,  -609,  1975,  -624,  1982,  -640,  1989,  -656,
   1996,  -672,  2002,  -688,  2008,  -705,  2013,  -721,
   2019,  -738,  2024,  -754,  2029,  -771,  2033,  -788,
   2038,  -805,  2042,  -822,  2045,  -840,  2049,  -857,
   2052,  -875,  2055,  -892,  2058,  -910,  2060,  -928,
   2063,  -946,  2065,  -964,  2066,  -983,  2068, -1001,
   2069, -1020,  2070, -1038,  2071, -1057,  2072, -1076,
   2072, -1095,  2072, -1114,  2072, -1133,  2072, -1153,
   2072, -1172,  2071, -1192,  2070, -1211,  2069, -1231,
   2068, -1251,  2066, -1271,  2065, -1291,  2063, -1311,
   2061, -1332,  2058, -1352,  2056, -1373,  2053, -1393,
   2050, -1414,  2047, -1435,  2044, -1456,  2040, -1477,
   2037, -1498,  2033, -1520,  2029, -1541,  2025, -1562,
   2021, -1584,  2016, -1606,  2011, -1628,  2006, -1649,
   2001, -1671,  1996, -1694,  1991, -1716,  1985, -1738,
   1980, -1760,  1974, -1783,  1968, -1806,  1961, -1828,
   1955, -1851,  1949, -1874,  1942, -1897,  1935, -1920
};

void arith_encoder::process_arith() {
    // ----------------------------------------------------
    // Reset Phase
    // ----------------------------------------------------
    ae_rdy.write(false);
    ae_update_vld.write(false);
    ae_update_prob.write(0);
    byte_out_vld.write(false);
    byte_out.write(0);

    low = 0;
    range = 0xffff;
    cntr = 0;
    output_byte = 0;
    carry = 0;
    first_byte = true;

    state = IDLE;
    pending_carry_cnt = 0;
    pending_byte_valid = false;
    pending_byte_val = 0;

    wait();

    // ----------------------------------------------------
    // Main FSM
    // ----------------------------------------------------
    while (true) {
        switch (state) {
            case IDLE:
                ae_rdy.write(true);
                ae_update_vld.write(false);
                byte_out_vld.write(false);
                
                if (ae_vld.read() && ae_rdy.read()) {
                    bool bit = ae_bit.read();
                    sc_uint<16> prob = ae_prob.read();
                    
                    ae_rdy.write(false);

                    // Interval subdivision
                    sc_uint<32> range_x_prob = (range * prob) >> 16;
                    
                    // Interval selection
                    if (bit) {
                        low = low + range_x_prob;
                        range = range - range_x_prob;
                    } else {
                        range = range_x_prob;
                    }
                    if (range == 0) range = 1; // Prevent deadlock in RENORM loop

                    // Update probability using interleaved LUT
                    unsigned int addr = (prob >> 7 & ~1) | bit;
                    sc_uint<16> new_prob = prob + lut_interleaved[addr];

                    // Send updated probability back immediately
                    ae_update_vld.write(true);
                    ae_update_prob.write(new_prob);

                    state = RENORM;
                }
                break;

            case RENORM:
                if (ae_update_vld.read()) {
                    ae_update_vld.write(false);
                }

                if (range <= 0x4000) {
                    low <<= 1;
                    range <<= 1;
                    cntr++;

                    if (cntr == 8) {
                        if (low < (1<<24) && (low + range) >= (1<<24)) {
                            carry++;
                            low &= 0xffff;
                            cntr = 0;
                            // stay in RENORM
                        } else {
                            if (low >= (1<<24)) {
                                output_byte++;
                                pending_byte_val = 0x00;
                            } else {
                                pending_byte_val = 0xff;
                            }
                            pending_carry_cnt = carry;
                            state = EMIT_BYTE;
                        }
                    } else {
                        // Keep renormalizing
                    }
                } else {
                    state = IDLE;
                }
                break;

            case EMIT_BYTE:
                if (!first_byte) {
                    byte_out_vld.write(true);
                    byte_out.write(output_byte);
                    
                    if (byte_out_rdy.read() && byte_out_vld.read()) {
                        byte_out_vld.write(false);
                        first_byte = false;
                        if (pending_carry_cnt > 0) {
                            state = EMIT_CARRY;
                        } else {
                            output_byte = low >> 16;
                            low &= 0xffff;
                            cntr = 0;
                            carry = 0;
                            state = RENORM;
                        }
                    }
                } else {
                    first_byte = false;
                    if (pending_carry_cnt > 0) {
                        state = EMIT_CARRY;
                    } else {
                        output_byte = low >> 16;
                        low &= 0xffff;
                        cntr = 0;
                        carry = 0;
                        state = RENORM;
                    }
                }
                break;

            case EMIT_CARRY:
                byte_out_vld.write(true);
                byte_out.write(pending_byte_val);

                if (byte_out_rdy.read() && byte_out_vld.read()) {
                    byte_out_vld.write(false);
                    pending_carry_cnt--;
                    if (pending_carry_cnt == 0) {
                        output_byte = low >> 16;
                        low &= 0xffff;
                        cntr = 0;
                        carry = 0;
                        state = RENORM;
                    }
                }
                break;
        }

        wait();
    }
}
