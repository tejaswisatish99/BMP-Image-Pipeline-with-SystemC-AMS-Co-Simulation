#include "CannyEdgeWrapper.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

static inline uint8_t clamp_u8(int v) {
  return (uint8_t)std::min(255, std::max(0, v));
}

CannyEdgeWrapper::CannyEdgeWrapper(sc_core::sc_module_name nm, int W, int H)
: sca_tdf::sca_module(nm),
  analog_in("analog_in"),
  pixel_out("pixel_out"),
  valid_out("valid_out"),
  hsync_out("hsync_out"),
  vsync_out("vsync_out"),
  W_(W), H_(H), N_(W*H), idx_(0) {}

void CannyEdgeWrapper::set_attributes() {
  // Match your DE clock period (10 ns in your main.cpp)
  set_timestep(sc_core::sc_time(10, sc_core::SC_NS));
}

void CannyEdgeWrapper::processing() {
  // Read analog sample and quantize to 8-bit
  const double vin = analog_in.read();
  const int    q   = (int)std::lround(vin);
  const uint8_t x  = clamp_u8(q);

  // Apply 1D LUT
  const uint8_t y = lut_.apply(x);

  // Drive DE bridge
  pixel_out.write(sc_dt::sc_uint<8>(y));
  valid_out.write(true);

  // HSYNC: pulse 1 cycle at start of each row
  const bool line_start = (idx_ % W_) == 0;
  hsync_out.write(line_start);

  // VSYNC: pulse 1 cycle at LAST pixel of the frame
  const bool last_pixel = (idx_ == (N_ - 1));
  vsync_out.write(last_pixel);

  // Advance pixel index (wrap to next frame)
  idx_ = last_pixel ? 0 : (idx_ + 1);
}

// --- LUT helpers ---
void CannyEdgeWrapper::load_identity() {
  lut_.reset_identity();
}

bool CannyEdgeWrapper::load_lut_file(const std::string& path) {
  return lut_.load_csv(path.c_str());
}

void CannyEdgeWrapper::apply_gain_offset(double gain, double offset) {
  lut_.compose_gain_offset(gain, offset);
}

void CannyEdgeWrapper::apply_gamma(double gamma) {
  lut_.compose_gamma(gamma);
}

bool CannyEdgeWrapper::dump_lut(const std::string& path) const {
  return lut_.dump_csv(path.c_str());
}

