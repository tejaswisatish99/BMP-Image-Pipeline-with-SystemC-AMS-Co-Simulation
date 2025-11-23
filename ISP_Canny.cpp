#include "ISP_Canny.h"

// Verilator needs a global sc_time_stamp()
#include <systemc>
using sc_core::sc_time_stamp;

#include "verilated.h"
#include "VCannyEdge.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

// ------ env helpers ------
static inline bool env_on(const char* n){
  if (const char* v = std::getenv(n)) return v[0] && v[0] != '0';
  return false;
}
static inline int env_int(const char* n, int dflt){
  if (const char* v = std::getenv(n)) { char* e=nullptr; long x=strtol(v,&e,10); if (e!=v) return (int)x; }
  return dflt;
}

// ------ runtime switches ------
static const bool ISP_ULTRA   = env_on("ISP_ULTRA");   // even fewer waits than LIGHT
static const bool ISP_LIGHT   = env_on("ISP_LIGHT");   // reduce pulses -> faster sim
static const bool ISP_VERBOSE = env_on("ISP_VERBOSE"); // print stage progress
static const bool ISP_QUIET   = env_on("ISP_QUIET");   // hide stage prints
static const int  ISP_ROW_STEP= env_int("ISP_ROW_STEP", 8); // progress granularity

ISP_Canny::ISP_Canny(sc_core::sc_module_name name, int width, int height)
: sc_module(name), W_(width), H_(height), N_(width*height)
{
  SC_CTHREAD(run, clk.pos());
  memX_.resize(N_);  memXG_.resize(N_);
  Gxy_.resize(N_);   Theta_.resize(N_);
  bGxy_.resize(N_);
  m_ = new VCannyEdge;
}

ISP_Canny::~ISP_Canny() { delete m_; m_ = nullptr; }

// Advance Verilated clock; in ULTRA we don’t consume simulation time
void ISP_Canny::tick() {
  if (ISP_ULTRA) {
    m_->clk = 1; m_->eval();
    m_->clk = 0; m_->eval();
  } else {
    m_->clk = 1; m_->eval(); wait();
    m_->clk = 0; m_->eval(); wait();
  }
}

void ISP_Canny::reset_rtl() {
  m_->rst_b = 0; m_->eval(); tick(); tick();
  m_->rst_b = 1; m_->eval(); tick();
}

// CE pulsing; ULTRA does the absolute minimum
void ISP_Canny::pulse_ce() {
  if (ISP_ULTRA || ISP_LIGHT) {
    m_->bCE = 1; m_->eval();
    m_->bCE = 0; m_->eval();
  } else {
    m_->bCE = 1; m_->eval(); tick();
    m_->bCE = 0; m_->eval(); tick();
    m_->bCE = 1; m_->eval(); tick();
  }
}

void ISP_Canny::write_reg(int row, int col, uint8_t val) {
  m_->bWE = 0;                  // write
  m_->dAddrRegRow = row;
  m_->dAddrRegCol = col;
  m_->InData      = val;
  pulse_ce();
}

uint8_t ISP_Canny::read_reg(int which) {
  m_->bWE = 1;                  // read
  m_->dReadReg = which;
  if (ISP_ULTRA) {
    m_->bCE = 1; m_->eval();
    m_->bCE = 0; m_->eval();
    return (uint8_t)m_->OutData;
  } else if (ISP_LIGHT) {
    m_->bCE = 1; m_->eval(); wait();
    m_->bCE = 0; m_->eval();
    return (uint8_t)m_->OutData;
  } else {
    m_->bCE = 1; m_->eval(); tick();
    m_->bCE = 0; m_->eval(); tick();
    uint8_t v = (uint8_t)m_->OutData;
    m_->bCE = 1; m_->eval(); tick();
    return v;
  }
}

void ISP_Canny::run() {
  // flush logs immediately
  std::cout.setf(std::ios::unitbuf);

  // default outputs
  pix_out.write(0);
  valid_out.write(false);
  vsync_out.write(false);

  reset_rtl();

  while (true) {
    // -------- Ingest one frame from upstream --------
    int n = 0;
    bool saw_vsync = false;
    unsigned idle = 0;
    const unsigned IDLE_LIMIT = (unsigned)env_int("ISP_IDLE_LIMIT", 200000);

    while (n < N_) {
      if (valid_in.read()) { memX_[n++] = (uint8_t)pix_in.read().to_uint(); idle = 0; }
      if (vsync_in.read()) saw_vsync = true;

      if (++idle > IDLE_LIMIT) {
        if (ISP_VERBOSE && !ISP_QUIET)
          std::cout << "[ISP] ingest timeout n=" << n << "/" << N_
                    << " vsync=" << (saw_vsync?1:0) << "\n";
        break;
      }
      wait();
    }
    if (n < N_) {
      std::fill(memX_.begin() + n, memX_.begin() + N_, 0);
      if (!ISP_QUIET)
        std::cout << "[ISP] WARNING: short frame " << n << "/" << N_
                  << " (vsync=" << (saw_vsync?1:0) << "), padded remainder\n";
    } else if (!ISP_QUIET) {
      std::cout << "[ISP] Ingested " << N_ << " pixels\n";
    }

    // Common defaults
    m_->bOPEnable = 1;
    m_->dWriteReg = 0;
    m_->OPMode    = 0;

    const int row_step = (ISP_ROW_STEP > 0 ? ISP_ROW_STEP : 8);

    // ====== GAUSSIAN 5x5 (memX -> memXG) ======
    m_->OPMode    = 0;           // MODE_GAUSSIAN
    m_->dWriteReg = 0;           // WRITE_REGX
    for (int i=0; i<H_; ++i) {
      for (int j=0; j<W_; ++j) {
        if (i<2 || j<2 || i>=H_-2 || j>=W_-2) { memXG_[i*W_+j] = memX_[i*W_+j]; continue; }
        for (int k=-2; k<=2; ++k)
          for (int l=-2; l<=2; ++l)
            write_reg(k+2, l+2, at(memX_, i+k, j+l));

        // latch/compute then read REG_GAUSSIAN (=0)
        m_->bOPEnable = 0; m_->eval();
        if (!ISP_ULTRA) { if (ISP_LIGHT) wait(); else tick(); }
        m_->bOPEnable = 1; m_->eval();
        if (!ISP_ULTRA) { if (ISP_LIGHT) wait(); else tick(); }

        memXG_[i*W_+j] = read_reg(0);
      }
      if (ISP_VERBOSE && !ISP_QUIET && (i%row_step==0))
        std::cout << "[ISP] GAUSS row " << i << "/" << H_ << "\n";
      if (ISP_ULTRA && (i % 1024 == 0)) wait();
    }
    if (!ISP_QUIET) std::cout << "[ISP] GAUSS done\n";

    // ====== SOBEL 3x3 (memXG -> Gxy, Theta) ======
    m_->OPMode    = 1;           // MODE_SOBEL
    m_->dWriteReg = 0;           // WRITE_REGX
    for (int i=0; i<H_; ++i) {
      for (int j=0; j<W_; ++j) {
        for (int k=-1; k<=1; ++k)
          for (int l=-1; l<=1; ++l)
            write_reg(k+1, l+1, at(memXG_, i+k, j+l));
        Gxy_[i*W_+j]   = read_reg(1); // REG_GRADIENT
        Theta_[i*W_+j] = read_reg(2); // REG_DIRECTION
      }
      if (ISP_VERBOSE && !ISP_QUIET && (i%row_step==0))
        std::cout << "[ISP] SOBEL row " << i << "/" << H_ << "\n";
      if (ISP_ULTRA && (i % 1024 == 0)) wait();
    }
    if (!ISP_QUIET) std::cout << "[ISP] SOBEL done\n";

    // ====== NMS 3x3 (Gxy + Theta -> bGxy) ======
    m_->OPMode    = 2;  // MODE_NMS
    m_->dWriteReg = 0;  // WRITE_REGX first
    for (int i=0; i<H_; ++i) {
      for (int j=0; j<W_; ++j) {
        for (int k=-1; k<=1; ++k)
          for (int l=-1; l<=1; ++l)
            write_reg(k+1, l+1, at(Gxy_, i+k, j+l));
        m_->dWriteReg = 1; // WRITE_REGY
        for (int k=-1; k<=1; ++k)
          for (int l=-1; l<=1; ++l)
            write_reg(k+1, l+1, at(Theta_, i+k, j+l));
        m_->dWriteReg = 0;
        bGxy_[i*W_+j] = read_reg(3); // REG_NMS
      }
      if (ISP_VERBOSE && !ISP_QUIET && (i%row_step==0))
        std::cout << "[ISP] NMS row " << i << "/" << H_ << "\n";
     if (ISP_ULTRA && (i % 1024 == 0)) wait();
    }
    if (!ISP_QUIET) std::cout << "[ISP] NMS done\n";

    // ====== HYSTERESIS 3x3 (bGxy -> final) ======
    m_->OPMode    = 3;  // MODE_HYSTERESIS
    m_->dWriteReg = 0;  // WRITE_REGX
    for (int i=0; i<H_; ++i) {
      for (int j=0; j<W_; ++j) {
        for (int k=-1; k<=1; ++k)
          for (int l=-1; l<=1; ++l)
            write_reg(k+1, l+1, at(bGxy_, i+k, j+l));
        m_->bOPEnable = 0; m_->eval();
        if (!ISP_ULTRA) { if (ISP_LIGHT) wait(); else tick(); }
        m_->bOPEnable = 1; m_->eval();
        if (!ISP_ULTRA) { if (ISP_LIGHT) wait(); else tick(); }
        bGxy_[i*W_+j] = read_reg(4); // REG_HYSTERESIS
      }
      if (ISP_VERBOSE && !ISP_QUIET && (i%row_step==0))
        std::cout << "[ISP] HYSTERESIS row " << i << "/" << H_ << "\n";
      if (ISP_ULTRA && (i % 1024 == 0)) wait();
    }
    if (!ISP_QUIET) std::cout << "[ISP] HYSTERESIS done\n";

    // -------- Stream out the processed frame --------
    for (int n2=0; n2<N_; ++n2) {
      pix_out.write(bGxy_[n2]);
      valid_out.write(true);
      vsync_out.write(n2 == N_ - 1); // pulse vsync on last pixel
      wait();
    }
    valid_out.write(false);
    vsync_out.write(false);
    wait();

    if (!ISP_QUIET) std::cout << "[ISP] Frame complete\n";
  }
}

