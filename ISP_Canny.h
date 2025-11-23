#pragma once
#include <systemc>
#include <vector>

// Forward declare the Verilated model (we include the real header in the .cpp)
class VCannyEdge;

// SystemC DE wrapper around the Verilated lab10 ISP (CannyEdge.v).
// Consumes a frame on (pix_in,valid_in) and streams the processed frame out.
struct ISP_Canny : sc_core::sc_module {
  // Clock (use the same DE clock as the rest of your top)
  sc_core::sc_in<bool> clk;

  // Upstream pixel stream (from ADC): 8-bit grayscale + valid + optional vsync
  sc_core::sc_in< sc_dt::sc_uint<8> > pix_in;
  sc_core::sc_in<bool>                valid_in;
  sc_core::sc_in<bool>                vsync_in;

  // Downstream pixel stream (to LUT)
  sc_core::sc_out< sc_dt::sc_uint<8> > pix_out;
  sc_core::sc_out<bool>                 valid_out;
  sc_core::sc_out<bool>                 vsync_out;

  SC_HAS_PROCESS(ISP_Canny);
  ISP_Canny(sc_core::sc_module_name name, int width, int height);
  ~ISP_Canny() override;

private:
  VCannyEdge* m_ = nullptr;

  const int W_;
  const int H_;
  const int N_; // W*H

  std::vector<uint8_t> memX_, memXG_, Gxy_, Theta_, bGxy_;

  // Helpers implemented in the .cpp
  void run();

  void tick();          // drive the Verilated clock +/- and wait()
  void reset_rtl();     // reset the RTL core
  void pulse_ce();      // emulate testbench chip-enable pulses

  inline uint8_t at(const std::vector<uint8_t>& v, int i, int j) const {
    if (i < 0 || j < 0 || i >= H_ || j >= W_) return 0;
    return v[static_cast<size_t>(i)*W_ + j];
  }

  void write_reg(int row, int col, uint8_t val);
  uint8_t read_reg(int which);
};

