#pragma once
#include <systemc>
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>

struct LPDDR : sc_core::sc_module {
  // Clock
  sc_core::sc_in<bool> clk;

  // 256-bit write channel (from packer)
  sc_core::sc_in< sc_dt::sc_bv<256> > wdata;
  sc_core::sc_in<bool>                wvalid;
  sc_core::sc_out<bool>               wready;

  // 256-bit read channel (to a consumer)
  sc_core::sc_out< sc_dt::sc_bv<256> > rdata;
  sc_core::sc_out<bool>                rvalid;
  sc_core::sc_in<bool>                 rready;

  SC_HAS_PROCESS(LPDDR);
  LPDDR(sc_core::sc_module_name name);

  // Control / host helpers
  void set_expected_bytes(uint32_t n);
  void reset_counters();
  void report() const;

  // Host-side peek (unchanged behavior for your PGM write-back)
  void read_back(std::vector<uint8_t>& out) const;

private:
  // Storage
  std::vector<uint8_t> mem_;          // last written frame
  uint32_t expected_bytes_ = 0;

  // Write counters
  uint64_t wr_bytes_ = 0;
  uint64_t wr_bursts_ = 0;
  sc_core::sc_time wr_t0_, wr_t1_;
  bool wr_started_ = false;
  bool wr_done_    = false;

  // Read state
  uint32_t rd_idx_ = 0;
  bool     rd_phase_ = false;
  sc_core::sc_time rd_t0_, rd_t1_;

  // Process
  void run();

  // Helpers
  static inline uint8_t get_byte(const sc_dt::sc_bv<256>& v, int i) {
    // byte i := bits [8*i+7 : 8*i]
    return (uint8_t) v.range(8*i+7, 8*i).to_uint();
  }
  static inline void set_byte(sc_dt::sc_bv<256>& v, int i, uint8_t b) {
    sc_dt::sc_bv<8> bb = b;
    v.range(8*i+7, 8*i) = bb;
  }
};

