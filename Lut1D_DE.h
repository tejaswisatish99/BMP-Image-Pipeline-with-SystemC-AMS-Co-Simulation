#pragma once
#include <systemc>
#include <array>
#include <string>
#include <cstdint>

// Post-ISP 1D LUT in DE domain: y = LUT[x] when valid_in is high.
// Also collects histograms (per frame) of input and output values and can dump CSV on vsync.
struct Lut1D_DE : sc_core::sc_module {
  // Ports
  sc_core::sc_in<bool>               clk;

  sc_core::sc_in< sc_dt::sc_uint<8> > pix_in;
  sc_core::sc_in<bool>                valid_in;
  sc_core::sc_in<bool>                vsync_in;

  sc_core::sc_out< sc_dt::sc_uint<8> > pix_out;
  sc_core::sc_out<bool>                 valid_out;
  sc_core::sc_out<bool>                 vsync_out;

  SC_HAS_PROCESS(Lut1D_DE);
  Lut1D_DE(sc_core::sc_module_name name);

  // LUT programming helpers
  void load_identity();
  bool load_lut_file(const std::string& path);   // CSV (256 entries) or "idx,val"
  void apply_gain_offset(double gain, double offset);
  void apply_gamma(double gamma);
  bool dump_lut(const std::string& path) const;

  // Stats controls
  void enable_stats(bool en);
  void reset_stats();                            // zero both histograms
  bool dump_hist_in (const std::string& path) const;
  bool dump_hist_out(const std::string& path) const;
  void set_hist_in_dump_path (const std::string& path);   // auto-dump at each vsync if set
  void set_hist_out_dump_path(const std::string& path);   // auto-dump at each vsync if set

private:
  // LUT
  std::array<uint8_t,256> lut_{};

  // Stats
  bool stats_en_ = false;
  std::array<uint64_t,256> hist_in_{};
  std::array<uint64_t,256> hist_out_{};
  uint64_t pix_index_ = 0;
  bool prev_vsync_ = false;
  std::string hist_in_path_;
  std::string hist_out_path_;

  // Process
  void step();  // posedge clocked
};

