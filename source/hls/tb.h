#pragma once
#include <systemc.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

// ── golden structs ────────────────────────────────────────────────

struct GoldenIn {
    int32_t nodeIdx;
    int16_t x, y, z;
    int32_t thetaIdx;
};

struct GoldenDelta {               // from _res.csv
    int16_t dx, dy, dz;
};

struct GoldenBin {                 // from _bin.csv, one row per (nodeIdx, k)
    int32_t nodeIdx;
    int32_t thetaIdx;
    int     k;                     // axis 0=dx 1=dy 2=dz
    int     isNonZero;
    int     numBits;               // 1+ilog2(value), 0 when |res|==1
    int     value;                 // abs(res)-1
    int     sign;                  // res<0 ? 1 : 0
};

// key for bin lookup: (nodeIdx << 2) | k
static inline int64_t bin_key(int32_t nodeIdx, int k) {
    return ((int64_t)nodeIdx << 2) | k;
}

// ── CSV helpers ───────────────────────────────────────────────────

static std::vector<std::string> csv_tok(const std::string& line) {
    std::vector<std::string> v;
    std::stringstream ss(line);
    std::string t;
    while (std::getline(ss, t, ',')) v.push_back(t);
    return v;
}

static std::vector<GoldenIn> load_in(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    std::vector<GoldenIn> rows;
    std::string line; bool hdr = true;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (hdr) { hdr = false; continue; }
        auto t = csv_tok(line);
        if (t.size() < 5) continue;
        GoldenIn r;
        r.nodeIdx  = std::stoi(t[0]);
        r.x        = (int16_t)std::stoi(t[1]);
        r.y        = (int16_t)std::stoi(t[2]);
        r.z        = (int16_t)std::stoi(t[3]);
        r.thetaIdx = std::stoi(t[4]);
        rows.push_back(r);
    }
    return rows;
}

static std::unordered_map<int32_t, GoldenDelta>
load_delta(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    std::unordered_map<int32_t, GoldenDelta> m;
    std::string line; bool hdr = true;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (hdr) { hdr = false; continue; }
        auto t = csv_tok(line);
        if (t.size() < 4) continue;
        int32_t nidx = std::stoi(t[0]);
        GoldenDelta r;
        r.dx = (int16_t)std::stoi(t[1]);
        r.dy = (int16_t)std::stoi(t[2]);
        r.dz = (int16_t)std::stoi(t[3]);
        m[nidx] = r;
    }
    return m;
}

// nodeIdx,thetaIdx,k,isNonZero,numBits,value,sign
static std::unordered_map<int64_t, GoldenBin>
load_bins(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    std::unordered_map<int64_t, GoldenBin> m;
    std::string line; bool hdr = true;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (hdr) { hdr = false; continue; }
        auto t = csv_tok(line);
        if (t.size() < 7) continue;
        GoldenBin r;
        r.nodeIdx   = std::stoi(t[0]);
        r.thetaIdx  = std::stoi(t[1]);
        r.k         = std::stoi(t[2]);
        r.isNonZero = std::stoi(t[3]);
        r.numBits   = std::stoi(t[4]);
        r.value     = std::stoi(t[5]);
        r.sign      = std::stoi(t[6]);
        m[bin_key(r.nodeIdx, r.k)] = r;
    }
    return m;
}

// ── decode helpers ────────────────────────────────────────────────

// Decode one axis 60-bit ctx slot + 18-bit bypass slot back into fields.
// Returns false if slot is zero (residual == 0).
struct DecodedAxis {
    int isNonZero;
    int numBits;    // reconstructed from tree bits
    int value;      // reconstructed from bypass + implicit MSB
    int sign;
};

static DecodedAxis decode_axis(sc_uint<60> ctx, sc_uint<18> bp) {
    DecodedAxis d;

    // bin0 [59:50] — zero flag
    d.isNonZero = (int)ctx[59];

    if (!d.isNonZero) {
        d.numBits = 0;
        d.value   = 0;
        d.sign    = 0;
        return d;
    }

    // bins 1-4 [49:40][39:30][29:20][19:10] — numBits tree MSB→LSB
    // each {bin[9], addr[8:0]}, we only need the bin bit (bit 9 of slot)
    int nb = 0;
    nb = (nb << 1) | (int)ctx[49];  // tree bit n=3
    nb = (nb << 1) | (int)ctx[39];  // tree bit n=2
    nb = (nb << 1) | (int)ctx[29];  // tree bit n=1
    nb = (nb << 1) | (int)ctx[19];  // tree bit n=0
    d.numBits = nb;

    // bin5 [9:0] — sign
    d.sign = (int)ctx[9];

    // bypass: {nbm1[3:0], value[13:0]}
    sc_uint<4>  nbm1  = (sc_uint<4>) bp.range(17, 14);
    sc_uint<14> vbits = (sc_uint<14>)bp.range(13,  0);

    // reconstruct value:
    // numBits bits total, MSB is implicit 1 (not transmitted)
    // transmitted bits are value[nbm1-1:0] = value without MSB
    // value = (1 << nbm1) | vbits[nbm1-1:0]
    if (d.numBits == 0) {
        // |res|==1: value=0, no bypass
        d.value = 0;
    } else {
        int nbm1_int = (int)nbm1;
        int mask = (nbm1_int > 0) ? ((1 << nbm1_int) - 1) : 0;
        int low  = (int)vbits.to_uint() & mask;
        d.value  = (1 << nbm1_int) | low;   // implicit MSB + transmitted bits
    }

    return d;
}

// ── testbench ─────────────────────────────────────────────────────

SC_MODULE(Tb) {

    sc_in_clk               clk;
    sc_out<bool>            rst;

    sc_out<sc_uint<53>>     data_in;
    sc_out<bool>            tb_out_vld;
    sc_in<bool>             delta_in_rdy;

    sc_out<bool>            ctx_rdy;

    sc_in<sc_uint<60>>      ctx_bin_dx;
    sc_in<sc_uint<60>>      ctx_bin_dy;
    sc_in<sc_uint<60>>      ctx_bin_dz;
    sc_in<bool>             ctx_bin_vld;
    sc_in<sc_uint<54>>      bypass_bin_out;
    sc_in<bool>             bypass_vld;

    sc_in<sc_uint<53>>      delta_tap;
    sc_in<bool>             delta_tap_vld;

    // ── golden data ────────────────────────────────────────────────
    std::vector<GoldenIn>                       g_in;
    std::unordered_map<int32_t,  GoldenDelta>   g_delta;   // keyed nodeIdx
    std::unordered_map<int64_t,  GoldenBin>     g_bins;    // keyed (nodeIdx,k)

    // ── counters ───────────────────────────────────────────────────
    int delta_pass = 0, delta_fail = 0, delta_skip = 0;
    int bin_pass   = 0, bin_fail   = 0, bin_skip   = 0;

    void check_axis(int32_t nodeIdx, int k,
                    sc_uint<60> ctx, sc_uint<18> bp) {

        auto it = g_bins.find(bin_key(nodeIdx, k));
        if (it == g_bins.end()) {
            bin_skip++;
            return;
        }

        const GoldenBin& ref = it->second;
        DecodedAxis got = decode_axis(ctx, bp);

        const char* ax = (k==0)?"dx":(k==1)?"dy":"dz";
        bool ok = true;

        // isNonZero
        if (got.isNonZero != ref.isNonZero) {
            std::cout << "  BIN FAIL nodeIdx=" << nodeIdx
                      << " " << ax
                      << " isNonZero: exp=" << ref.isNonZero
                      << " got=" << got.isNonZero << "\n";
            ok = false;
        }

        if (ref.isNonZero) {
            // numBits
            if (got.numBits != ref.numBits) {
                std::cout << "  BIN FAIL nodeIdx=" << nodeIdx
                          << " " << ax
                          << " numBits: exp=" << ref.numBits
                          << " got=" << got.numBits << "\n";
                ok = false;
            }
            // value (only meaningful when numBits > 0)
            if (ref.numBits > 0 && got.value != ref.value) {
                std::cout << "  BIN FAIL nodeIdx=" << nodeIdx
                          << " " << ax
                          << " value: exp=" << ref.value
                          << " got=" << got.value << "\n";
                ok = false;
            }
            // sign
            if (got.sign != ref.sign) {
                std::cout << "  BIN FAIL nodeIdx=" << nodeIdx
                          << " " << ax
                          << " sign: exp=" << ref.sign
                          << " got=" << got.sign << "\n";
                ok = false;
            }
        }

        if (ok) {
            bin_pass++;
        } else {
            bin_fail++;
            // full detail on failure
            std::cout << "         got:"
                      << " isNonZero=" << got.isNonZero
                      << " numBits="   << got.numBits
                      << " value="     << got.value
                      << " sign="      << got.sign << "\n";
            std::cout << "         ref:"
                      << " isNonZero=" << ref.isNonZero
                      << " numBits="   << ref.numBits
                      << " value="     << ref.value
                      << " sign="      << ref.sign << "\n";
        }
    }

    void run() {
        // ── reset ──────────────────────────────────────────────────
        rst.write(true);
        tb_out_vld.write(false);
        ctx_rdy.write(false);
        data_in.write(0);
        wait(5);
        rst.write(false);
        wait();

        for (size_t i = 0; i < g_in.size(); i++) {
            const GoldenIn& in = g_in[i];

            // ── pack and drive input ───────────────────────────────
            sc_uint<53> word = 0;
            word.range(52, 48) = (sc_uint<5>) in.thetaIdx;
            word.range(47, 32) = (sc_uint<16>)(uint16_t)in.x;
            word.range(31, 16) = (sc_uint<16>)(uint16_t)in.y;
            word.range(15,  0) = (sc_uint<16>)(uint16_t)in.z;

            while (!delta_in_rdy.read()) wait();
            data_in.write(word);
            tb_out_vld.write(true);
            wait();
            tb_out_vld.write(false);

            // ── check delta output against _res.csv ────────────────
            while (!delta_tap_vld.read()) wait();

            sc_uint<53> dtap = delta_tap.read();
            int16_t got_dx = (int16_t)(uint16_t)dtap.range(47,32).to_uint();
            int16_t got_dy = (int16_t)(uint16_t)dtap.range(31,16).to_uint();
            int16_t got_dz = (int16_t)(uint16_t)dtap.range(15, 0).to_uint();

            auto dit = g_delta.find(in.nodeIdx);
            if (dit == g_delta.end()) {
                delta_skip++;
            } else {
                const GoldenDelta& ref = dit->second;
                bool ok = (got_dx == ref.dx) &&
                          (got_dy == ref.dy) &&
                          (got_dz == ref.dz);
                if (ok) {
                    delta_pass++;
                } else {
                    delta_fail++;
                    std::cout << "DELTA FAIL nodeIdx=" << in.nodeIdx
                              << " laser=" << in.thetaIdx
                              << "  exp dx=" << ref.dx
                              << " dy=" << ref.dy << " dz=" << ref.dz
                              << "  got dx=" << got_dx
                              << " dy=" << got_dy << " dz=" << got_dz
                              << "\n";
                }
            }

            // ── check binarizer output against _bin.csv ────────────
            while (!ctx_bin_vld.read()) wait();
            ctx_rdy.write(true);

            sc_uint<54> bp  = bypass_bin_out.read();
            sc_uint<60> cdx = ctx_bin_dx.read();
            sc_uint<60> cdy = ctx_bin_dy.read();
            sc_uint<60> cdz = ctx_bin_dz.read();

            check_axis(in.nodeIdx, 0, cdx, (sc_uint<18>)bp.range(53, 36));
            check_axis(in.nodeIdx, 1, cdy, (sc_uint<18>)bp.range(35, 18));
            check_axis(in.nodeIdx, 2, cdz, (sc_uint<18>)bp.range(17,  0));

            wait();
            ctx_rdy.write(false);
        }

        // ── summary ────────────────────────────────────────────────
        std::cout << "\n=== DELTA: "
                  << delta_pass << " PASS  "
                  << delta_fail << " FAIL  "
                  << delta_skip << " SKIP ===\n";
        std::cout << "=== BINS:  "
                  << bin_pass   << " PASS  "
                  << bin_fail   << " FAIL  "
                  << bin_skip   << " SKIP  ("
                  << "axes checked: " << (bin_pass + bin_fail + bin_skip)
                  << ") ===\n";
        sc_stop();
    }

    SC_CTOR(Tb) {
        const std::string base =
            "/home/swnh/pgc/source/hls/golden/scene-0061/scene-0061_00_";

        g_in    = load_in   (base + "in.csv");
        g_delta = load_delta(base + "res.csv");
        g_bins  = load_bins (base + "bin.csv");

        std::cout << "[TB] loaded "
                  << g_in.size()    << " inputs,  "
                  << g_delta.size() << " delta golden,  "
                  << g_bins.size()  << " bin golden\n";

        SC_CTHREAD(run, clk.pos());
    }
};
