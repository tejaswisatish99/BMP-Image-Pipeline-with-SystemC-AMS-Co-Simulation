#pragma once
#include <systemc>
#include <cstdint>
#include <iostream>

struct PcieDMA_Tap : sc_core::sc_module {
  sc_core::sc_in<bool>                clk;
  sc_core::sc_in< sc_dt::sc_bv<256> > data_in;
  sc_core::sc_in<bool>                valid_in;

  SC_HAS_PROCESS(PcieDMA_Tap);
  PcieDMA_Tap(sc_core::sc_module_name name);

  void set_expected_bytes(uint32_t n);

private:
  uint32_t expected_ = 0;
  uint64_t seen_     = 0;
  bool     started_  = false;
  bool     printed_  = false;
  sc_core::sc_time t0_, t1_;

  void run();
};

