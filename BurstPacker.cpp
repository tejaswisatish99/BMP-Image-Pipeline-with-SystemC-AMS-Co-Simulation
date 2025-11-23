#include "BurstPacker.h"
using sc_dt::sc_uint;
using sc_dt::sc_bv;

BurstPacker::BurstPacker(sc_core::sc_module_name n)
: sc_core::sc_module(n),
  clk("clk"),
  pix_in("pix_in"),
  valid_in("valid_in"),
  vsync_in("vsync_in"),
  ready_out("ready_out"),
  burst_out("burst_out"),
  burst_valid("burst_valid"),
  burst_ready("burst_ready")
{
    SC_METHOD(run);
    sensitive << clk.pos();
    dont_initialize();
}

void BurstPacker::run() {
   
    bool can_accept = true;

    // If we are holding a burst that hasn't been accepted yet,
    // present it and stall upstream until burst_ready is true.
    if (hold_valid_) {
        burst_out.write(hold_data_);
        burst_valid.write(true);
        can_accept = burst_ready.read();      // only accept new pixels if the burst was taken
        if (burst_ready.read()) {
            hold_valid_ = false;              // accepted this cycle
        }
    } else {
        burst_valid.write(false);
    }

    // Upstream backpressure
    ready_out.write(can_accept);

    // Accept a pixel if upstream is sending and we can take it
    if (can_accept && valid_in.read()) {
        sc_uint<8> b = pix_in.read();
        sc_bv<8>   bb; bb = b.to_uint();
        // pack into lane [8*count_ +: 8]
        shreg_.range(8*count_ + 7, 8*count_) = bb;
        count_++;

        // Emit a burst when 32 bytes are packed
        if (count_ == 32) {
            // If downstream is ready now and we aren't holding, we can send immediately
            if (!hold_valid_ && burst_ready.read()) {
                burst_out.write(shreg_);
                burst_valid.write(true);
                // consumed immediately; next cycle we deassert valid
            } else {
                // Hold the burst for a later cycle
                hold_data_  = shreg_;
                hold_valid_ = true;
            }
            // reset the packer for next burst
            shreg_ = 0;
            count_ = 0;
        }
    }

    (void)vsync_in.read();
}

