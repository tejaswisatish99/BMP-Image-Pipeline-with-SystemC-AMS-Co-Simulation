#include "PcieDMA_Tap.h"
using sc_core::sc_time_stamp;

PcieDMA_Tap::PcieDMA_Tap(sc_core::sc_module_name name) : sc_module(name) {
  SC_CTHREAD(run, clk.pos());
}

void PcieDMA_Tap::set_expected_bytes(uint32_t n) {
  expected_ = n; seen_ = 0; started_ = false; printed_ = false;
}

void PcieDMA_Tap::run() {
  wait();
  for (;;) {
    if (valid_in.read()) {
      if (!started_) { started_ = true; t0_ = sc_time_stamp(); }
      seen_ += 32; // 256 bits per beat
      if (!printed_ && expected_ && seen_ >= expected_) {
        t1_ = sc_time_stamp();
        double gbps = 0.0;
        auto dt = t1_ - t0_;
        if (dt.value() > 0) gbps = (double)expected_ / (dt.to_seconds() * 1e9);
        std::cout << "[PCIEDMA] bytes=" << expected_ << " throughput=" << gbps << " GB/s\n";
        printed_ = true;
      }
    }
    wait();
  }
}

