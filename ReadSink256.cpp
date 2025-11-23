#include "ReadSink256.h"
using sc_core::sc_time_stamp;

ReadSink256::ReadSink256(sc_core::sc_module_name name) : sc_module(name) {
  SC_CTHREAD(run, clk.pos());
}

void ReadSink256::set_expected_bytes(uint32_t n) {
  expected_ = n; got_ = 0; started_ = false; printed_ = false;
}

void ReadSink256::run() {
  ready_out.write(true);  // always ready
  wait();
  for (;;) {
    if (valid_in.read()) {
      if (!started_) { started_ = true; t0_ = sc_time_stamp(); }
      got_ += 32;
      if (!printed_ && expected_ && got_ >= expected_) {
        t1_ = sc_time_stamp();
        double gbps = 0.0;
        auto dt = t1_ - t0_;
        if (dt.value() > 0) gbps = (double)expected_ / (dt.to_seconds() * 1e9);
        std::cout << "[READSINK] bytes=" << expected_ << " throughput=" << gbps << " GB/s\n";
        printed_ = true;
      }
    }
    ready_out.write(true);
    wait();
  }
}

