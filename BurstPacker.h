#pragma once
#include <systemc>
#include <systemc-ams.h>

struct BurstPacker : sc_core::sc_module {
    // Clk
    sc_core::sc_in<bool> clk;

    // Upstream pixel stream (8-bit) + control
    sc_core::sc_in< sc_dt::sc_uint<8> > pix_in;
    sc_core::sc_in<bool>                valid_in;
    sc_core::sc_in<bool>                vsync_in;   // frame boundary (optional)
    sc_core::sc_out<bool>               ready_out;  // backpressure to upstream

    // Downstream burst interface (256-bit)
    sc_core::sc_out< sc_dt::sc_bv<256> > burst_out;    // 32 bytes per burst
    sc_core::sc_out<bool>                burst_valid;  // burst_out is valid
    sc_core::sc_in<bool>                 burst_ready;  // downstream can take it

    SC_HAS_PROCESS(BurstPacker);
    BurstPacker(sc_core::sc_module_name n);

private:
    void run();

    sc_dt::sc_bv<256> shreg_{}; // shift/reg packer
    unsigned          count_ = 0;

    // Keep a single pending valid burst if downstream stalls
    bool              hold_valid_ = false;
    sc_dt::sc_bv<256> hold_data_{};
};

