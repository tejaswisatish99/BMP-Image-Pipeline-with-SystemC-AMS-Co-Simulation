#pragma once
#include <systemc-ams.h>
#include <systemc>
#include <cstdint>
#include <string>
#include "IdentityLUT.h"

// TDF module: analog_in (double) -> quantize 8b -> apply 1D LUT -> DE bridge
// Guarantees exactly W*H valid cycles per frame.
// - valid_out  : high on every pixel
// - hsync_out  : 1-cycle pulse at the first pixel of every row
// - vsync_out  : 1-cycle pulse at the LAST pixel of every frame
struct CannyEdgeWrapper : sca_tdf::sca_module {
  // Ports
  sca_tdf::sca_in<double>                          analog_in;
  sca_tdf::sca_de::sca_out< sc_dt::sc_uint<8> >    pixel_out;
  sca_tdf::sca_de::sca_out<bool>                   valid_out;
  sca_tdf::sca_de::sca_out<bool>                   hsync_out;
  sca_tdf::sca_de::sca_out<bool>                   vsync_out;

  // Ctor
  CannyEdgeWrapper(sc_core::sc_module_name nm, int W, int H);

  // AMS hooks
  void set_attributes() override;
  void processing() override;

  // LUT API
  void load_identity();
  bool load_lut_file(const std::string& path);
  void apply_gain_offset(double gain, double offset);
  void apply_gamma(double gamma);
  bool dump_lut(const std::string& path) const;

private:
  // Geometry
  const int W_;
  const int H_;
  const int N_;            // total pixels per frame
  int       idx_ = 0;      // 0 .. N_-1 (position inside frame)

  // LUT
  IdentityLUT lut_;
};

