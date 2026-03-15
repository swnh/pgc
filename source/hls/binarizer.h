#pragma once
#include <systemc.h>

static const sc_uint<9> CTX_GT0_BASE   = 0;
static const sc_uint<9> CTX_SIGN_BASE  = 3;
static const sc_uint<9> CTX_NBITS_BASE = 6;
static const int        CTX_NBITS_K    = 15;

// ── ilog2 equivalent — MSB position, returns -1 for x=0 ──────────
static inline sc_uint<4> msb_pos(sc_uint<16> x) {
    if (x[15]) return 15;
    if (x[14]) return 14;
    if (x[13]) return 13;
    if (x[12]) return 12;
    if (x[11]) return 11;
    if (x[10]) return 10;
    if (x[ 9]) return  9;
    if (x[ 8]) return  8;
    if (x[ 7]) return  7;
    if (x[ 6]) return  6;
    if (x[ 5]) return  5;
    if (x[ 4]) return  4;
    if (x[ 3]) return  3;
    if (x[ 2]) return  2;
    if (x[ 1]) return  1;
    return 0;  // x==1 only — x==0 guarded before calling
}

// ── matches reference: numBits = 1 + ilog2(abs(res) - 1) ─────────
//
//  |res|   value=abs-1   ilog2(value)   numBits   nbm1   bypass bits
//    1          0            -1             0        0      none
//   2-3        1-2           0-1            1        0      none
//   4-7        3-6           1-2            2        1      v[0]
//  8-15        7-14          2-3            3        2      v[1:0]
// 16-31       15-30          3-4            4        3      v[2:0]
//  ...
// ~20000      ~19999          14            15       14     v[13:0]
//
static inline sc_uint<4> calc_num_bits(sc_int<16> res) {
    // res==0 handled upstream (isNonZero gate), but guard anyway
    if (res == 0) return 0;

    sc_uint<16> a   = (res < 0) ? (sc_uint<16>)(-res) : (sc_uint<16>)(res);
    sc_uint<16> val = a - 1;

    // val==0 means |res|==1 → ilog2(0)=-1 → numBits=0
    // msb_pos(0) would return 0 (wrong), so guard here
    if (val == 0) return 0;

    return (sc_uint<4>)(1 + msb_pos(val));
}

SC_MODULE(binarizer) {

    // ── ports ──────────────────────────────────────────────────────
    sc_in<bool>             clk;
    sc_in<bool>             rst;

    sc_in<sc_uint<53>>      delta_data_in;
    sc_in<bool>             delta_out_vld;
    sc_in<bool>             ctx_rdy;

    // ctx stream: one 60-bit port per axis
    // each 60b: {bin(1),addr(9)} × 6 bins
    // bin order [59:50]=zero [49:40]=nb3 [39:30]=nb2 [29:20]=nb1 [19:10]=nb0 [9:0]=sign
    sc_out<sc_uint<60>>     ctx_bin_dx;
    sc_out<sc_uint<60>>     ctx_bin_dy;
    sc_out<sc_uint<60>>     ctx_bin_dz;
    sc_out<bool>            ctx_bin_vld;

    // bypass stream: {nbm1(4),value(14)} × 3 axes = 54 bits
    // [53:36]=dx [35:18]=dy [17:0]=dz
    sc_out<sc_uint<54>>     bypass_bin_out;
    sc_out<bool>            bypass_vld;

    sc_out<bool>            binarizer_in_rdy;

    // ── accumulators ───────────────────────────────────────────────
    sc_uint<10>  ctx_buf[3][6];   // [axis][bin 0-5]
    sc_uint<18>  bypass_buf[3];   // [axis] {nbm1[3:0], value[13:0]}

    void binarization() {
        ctx_bin_dx.write(0);
        ctx_bin_dy.write(0);
        ctx_bin_dz.write(0);
        ctx_bin_vld.write(false);
        bypass_bin_out.write(0);
        bypass_vld.write(false);
        binarizer_in_rdy.write(false);
        for (int k = 0; k < 3; k++) {
            bypass_buf[k] = 0;
            for (int b = 0; b < 6; b++) ctx_buf[k][b] = 0;
        }
        wait();

        while (true) {
            binarizer_in_rdy.write(true);
            wait();
            if (!delta_out_vld.read()) continue;

            binarizer_in_rdy.write(false);

            sc_uint<53> pkt = delta_data_in.read();
            sc_int<16> res[3];
            res[0] = (sc_int<16>)pkt.range(47, 32);
            res[1] = (sc_int<16>)pkt.range(31, 16);
            res[2] = (sc_int<16>)pkt.range(15,  0);

            for (int k = 0; k < 3; k++) {
                bypass_buf[k] = 0;
                for (int b = 0; b < 6; b++) ctx_buf[k][b] = 0;
            }

            for (int k = 0; k < 3; k++) {
                sc_int<16> residual  = res[k];
                sc_uint<1> isNonZero = (residual != 0) ? sc_uint<1>(1)
                                                       : sc_uint<1>(0);
                // bin 0 — zero flag
                sc_uint<10> b0;
                b0[9]          = isNonZero;
                b0.range(8, 0) = CTX_GT0_BASE + (sc_uint<9>)k;
                ctx_buf[k][0]  = b0;

                if (!isNonZero) continue;

                sc_uint<16> absval = (residual < 0)
                                   ? (sc_uint<16>)(-residual)
                                   : (sc_uint<16>)(residual);
                sc_uint<16> value   = absval - 1;
                sc_uint<4>  numBits = calc_num_bits(residual);
                sc_uint<4>  nbm1    = (numBits > 0)
                                    ? (sc_uint<4>)(numBits - 1)
                                    : sc_uint<4>(0);

                // bins 1-4 — numBits tree depth=4
                sc_uint<5> treeNode = 1;
                for (int n = 3; n >= 0; n--) {
                    sc_uint<1> tbit = (sc_uint<1>)((numBits >> n) & 1);
                    sc_uint<9> addr = CTX_NBITS_BASE
                                    + (sc_uint<9>)(k * CTX_NBITS_K)
                                    + (sc_uint<9>)((int)treeNode - 1);
                    sc_uint<10> bn;
                    bn[9]          = tbit;
                    bn.range(8, 0) = addr;
                    ctx_buf[k][1 + (3 - n)] = bn;   // slots 1,2,3,4
                    treeNode = (sc_uint<5>)((treeNode << 1) | (int)tbit);
                }

                // bin 5 — sign
                sc_uint<1> sign = (residual < 0) ? sc_uint<1>(1)
                                                  : sc_uint<1>(0);
                sc_uint<10> bs;
                bs[9]          = sign;
                bs.range(8, 0) = CTX_SIGN_BASE + (sc_uint<9>)k;
                ctx_buf[k][5]  = bs;

                // bypass
                sc_uint<18> bp;
                bp.range(17, 14) = nbm1;
                bp.range(13,  0) = value.range(13, 0);
                bypass_buf[k]    = bp;
            }

            // pack ctx per axis — 6 bins × 10 bits = 60 bits
            // bin0 at [59:50] .. bin5 at [9:0]
            sc_uint<60> ctx_words[3];
            for (int k = 0; k < 3; k++) {
                ctx_words[k] = 0;
                for (int b = 0; b < 6; b++) {
                    int lo = (5 - b) * 10;
                    ctx_words[k].range(lo + 9, lo) = ctx_buf[k][b];
                }
            }

            // pack bypass
            sc_uint<54> bypass_word = 0;
            bypass_word.range(53, 36) = bypass_buf[0];
            bypass_word.range(35, 18) = bypass_buf[1];
            bypass_word.range(17,  0) = bypass_buf[2];

            ctx_bin_dx.write(ctx_words[0]);
            ctx_bin_dy.write(ctx_words[1]);
            ctx_bin_dz.write(ctx_words[2]);
            bypass_bin_out.write(bypass_word);
            ctx_bin_vld.write(true);
            bypass_vld.write(true);

            do { wait(); } while (!ctx_rdy.read());

            ctx_bin_vld.write(false);
            bypass_vld.write(false);
        }
    }

    SC_CTOR(binarizer) {
        SC_CTHREAD(binarization, clk.pos());
        async_reset_signal_is(rst, true);
    }
};