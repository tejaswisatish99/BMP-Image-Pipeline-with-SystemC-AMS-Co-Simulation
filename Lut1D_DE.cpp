#include "Lut1D_DE.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

static inline uint8_t clamp_u8(int v){ return (uint8_t)std::min(255, std::max(0, v)); }

Lut1D_DE::Lut1D_DE(sc_core::sc_module_name name) : sc_module(name) {
  SC_METHOD(step);
  sensitive << clk.pos();
  load_identity();
}

void Lut1D_DE::step() {
  const bool vld = valid_in.read();
  const bool vs  = vsync_in.read();

  // Pass through syncs
  vsync_out.write(vs);

  if (vld) {
    uint8_t x = (uint8_t)pix_in.read().to_uint();
    uint8_t y = lut_[x];
    pix_out.write(y);
    valid_out.write(true);

    if (stats_en_) {
      ++hist_in_[x];
      ++hist_out_[y];
      ++pix_index_;
    }
  } else {
    valid_out.write(false);
  }

  // On vsync rising edge: auto-dump and reset per-frame counters if paths set
  const bool vs_rise = (vs && !prev_vsync_);
  if (vs_rise && stats_en_) {
    if (!hist_in_path_.empty())  (void)dump_hist_in (hist_in_path_);
    if (!hist_out_path_.empty()) (void)dump_hist_out(hist_out_path_);
    // Typically you want per-frame histograms. Reset after dump:
    reset_stats();
  }
  prev_vsync_ = vs;
}

void Lut1D_DE::load_identity() {
  for (int i=0;i<256;++i) lut_[(size_t)i] = (uint8_t)i;
}

void Lut1D_DE::apply_gain_offset(double g, double o) {
  for (int i=0;i<256;++i) {
    int v = (int)std::lround(g*i + o);
    lut_[(size_t)i] = clamp_u8(v);
  }
}

void Lut1D_DE::apply_gamma(double gamma) {
  if (gamma <= 0.0) return;
  for (int i=0;i<256;++i) {
    double n = i/255.0;
    int v = (int)std::lround(std::pow(n, gamma) * 255.0);
    lut_[(size_t)i] = clamp_u8(v);
  }
}

bool Lut1D_DE::load_lut_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::array<int,256> tmp{};
  int count = 0;

  std::string line;
  while (std::getline(f, line) && count < 256) {
    if (line.empty()) continue;
    std::stringstream ss(line);
    if (line.find(',') != std::string::npos) {
      int idx=-1,val=-1; char comma;
      ss >> idx >> comma >> val;
      if (!ss.fail() && val >= 0) tmp[count++] = val;
    } else {
      int val=-1; ss >> val;
      if (!ss.fail() && val >= 0) tmp[count++] = val;
    }
  }
  if (count != 256) return false;
  for (int i=0;i<256;++i) lut_[(size_t)i] = clamp_u8(tmp[(size_t)i]);
  return true;
}

bool Lut1D_DE::dump_lut(const std::string& path) const {
  std::ofstream f(path);
  if (!f) return false;
  for (int i=0;i<256;++i) f << (int)lut_[(size_t)i] << "\n";
  return true;
}

// ---- Stats controls ----
void Lut1D_DE::enable_stats(bool en) { stats_en_ = en; }
void Lut1D_DE::reset_stats() {
  hist_in_.fill(0);
  hist_out_.fill(0);
  pix_index_ = 0;
}

bool Lut1D_DE::dump_hist_in (const std::string& path) const {
  std::ofstream f(path);
  if (!f) return false;
  // CSV: value,count
  for (int i=0;i<256;++i) f << i << "," << hist_in_[(size_t)i]  << "\n";
  return true;
}

bool Lut1D_DE::dump_hist_out(const std::string& path) const {
  std::ofstream f(path);
  if (!f) return false;
  // CSV: value,count
  for (int i=0;i<256;++i) f << i << "," << hist_out_[(size_t)i] << "\n";
  return true;
}

void Lut1D_DE::set_hist_in_dump_path (const std::string& path)  { hist_in_path_  = path; }
void Lut1D_DE::set_hist_out_dump_path(const std::string& path)  { hist_out_path_ = path; }

