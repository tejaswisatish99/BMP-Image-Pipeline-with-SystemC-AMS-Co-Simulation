#include "LPDDR.h"
using sc_core::SC_NS;
using sc_core::sc_time;
using sc_core::sc_time_stamp;

LPDDR::LPDDR(sc_core::sc_module_name name) : sc_module(name) {
  SC_CTHREAD(run, clk.pos());
}

void LPDDR::set_expected_bytes(uint32_t n) {
  expected_bytes_ = n;
  mem_.clear();
  mem_.reserve(n);
  wr_bytes_ = 0;
  wr_bursts_ = 0;
  wr_started_ = false;
  wr_done_ = false;
  rd_idx_ = 0;
  rd_phase_ = false;
}

void LPDDR::reset_counters() {
  wr_bytes_ = wr_bursts_ = 0;
  wr_started_ = wr_done_ = false;
  rd_idx_ = 0;
}

void LPDDR::report() const {
  double wr_gbps = 0.0, rd_gbps = 0.0;
  if (wr_started_) {
    sc_time dt = wr_t1_ - wr_t0_;
    if (dt.value() > 0) wr_gbps = (double)wr_bytes_ / (dt.to_seconds() * 1e9);
  }
  if (rd_phase_) {
    sc_time dt = rd_t1_ - rd_t0_;
    if (dt.value() > 0) rd_gbps = (double)expected_bytes_ / (dt.to_seconds() * 1e9);
  }
  std::cout << "[LPDDR] WRITE  bursts=" << wr_bursts_
            << " bytes_wr=" << wr_bytes_
            << " throughput=" << wr_gbps << " GB/s\n";
  if (wr_done_) {
    std::cout << "[LPDDR] READ   bytes_rd=" << expected_bytes_
              << " throughput=" << rd_gbps << " GB/s\n";
  }
}

void LPDDR::read_back(std::vector<uint8_t>& out) const {
  out = mem_;
}

void LPDDR::run() {
  // defaults
  wready.write(true);      // always ready (simple model)
  rvalid.write(false);
  rdata.write(0);
  wait();

  for (;;) {
    // ---------------------- WRITE path ----------------------
    if (wvalid.read() && wready.read()) {
      if (!wr_started_) { wr_started_ = true; wr_t0_ = sc_time_stamp(); }

      const sc_dt::sc_bv<256> v = wdata.read();
      // append up to expected_bytes_ bytes (clip last burst if partial)
      const uint32_t room = (expected_bytes_ > mem_.size()) ? (expected_bytes_ - (uint32_t)mem_.size()) : 0;
      const uint32_t take = room >= 32 ? 32u : room;
      for (uint32_t i=0; i<take; ++i) mem_.push_back(get_byte(v, i));

      wr_bytes_  += take;
      wr_bursts_ += 1;

      if (wr_bytes_ >= expected_bytes_ && !wr_done_) {
        wr_done_ = true;
        wr_t1_   = sc_time_stamp();
        // arm read-out on next cycles
        rd_phase_ = true;
        rd_idx_   = 0;
        rd_t0_    = sc_time_stamp();
      }
    }

    // ---------------------- READ path -----------------------
    if (rd_phase_) {
      if (rd_idx_ < expected_bytes_) {
        sc_dt::sc_bv<256> out;
        // pack next 32 bytes (clip on last burst)
        const uint32_t remain = expected_bytes_ - rd_idx_;
        const uint32_t take   = remain >= 32 ? 32u : remain;
        for (uint32_t i=0; i<take; ++i) set_byte(out, i, mem_[rd_idx_ + i]);

        rdata.write(out);
        rvalid.write(true);

        if (rready.read()) {
          rd_idx_ += take;
          if (rd_idx_ >= expected_bytes_) {
            rvalid.write(false);
            rd_t1_ = sc_time_stamp();
            sc_core::sc_stop(); 
            // We’re done; let the testbench decide when to sc_stop().
            // (Your main.cpp prints PGM and calls report() after sc_start().)
          }
        }
      } else {
        rvalid.write(false);
      }
    } else {
      rvalid.write(false);
    }

    wait();
  }
}

