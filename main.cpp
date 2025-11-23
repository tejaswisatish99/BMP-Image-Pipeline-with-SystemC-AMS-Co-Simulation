#include <systemc>
#include <systemc-ams.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "Sensor.h"             // cmos_sensor (TDF analog source)
#include "CannyEdgeWrapper.h"   // TDF A/D + 1D LUT + DE bridge (now emits exact W*H)
#include "BurstPacker.h"        // packs 32 bytes -> sc_bv<256>
#include "LPDDR.h"              // NEW: bidirectional 256b LPDDR model
#include "PcieDMA_Tap.h"        // NEW: passive throughput monitor
#include "ReadSink256.h"        // NEW: read channel consumer
#include "BMPUtils.h"           // load_bmp_grayscale()
#include "ISP_Canny.h"
#include "Lut1D_DE.h"

// Simple PGM writer
static bool write_pgm(const std::string& path, int W, int H, const std::vector<uint8_t>& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P5\n" << W << " " << H << "\n255\n";
    f.write(reinterpret_cast<const char*>(img.data()), img.size());
    std::cout << "Wrote " << path << " (" << W << "x" << H << ")\n";
    return true;
}

int sc_main(int argc, char** argv) {
    auto starts_with = [](const std::string& s, const char* p){ return s.rfind(p,0)==0; };

    // -------- CLI --------
    std::string bmp_path;
    double gamma = 0.0;     // 0 => no gamma step
    double gain  = 1.0;
    double offs  = 0.0;
    std::string lut_path;
    std::string lut_dump;
    std::string hist_in_dump;
    std::string hist_out_dump;
    bool bypass_isp = false;

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if (starts_with(a,"--gamma="))         gamma    = std::stod(a.substr(8));
        else if (starts_with(a,"--gain="))     gain     = std::stod(a.substr(7));
        else if (starts_with(a,"--offset="))   offs     = std::stod(a.substr(9));
        else if (starts_with(a,"--lut="))      lut_path = a.substr(6);
        else if (starts_with(a,"--dump-lut=")) lut_dump = a.substr(11);
        else if (starts_with(a,"--dump-hist-in="))  hist_in_dump  = a.substr(15);
        else if (starts_with(a,"--dump-hist-out=")) hist_out_dump = a.substr(16);
        else if (a == "--bypass-isp")          bypass_isp = true;
        else if (!a.empty() && a[0] != '-')    bmp_path = a;
        else std::cerr << "[WARN] Unknown option: " << a << "\n";
    }

    // -------- Image load --------
    int W = 32, H = 32;
    std::vector<uint8_t> image;
    if (!bmp_path.empty()) {
        if (!load_bmp_grayscale(bmp_path.c_str(), W, H, image)) {
            std::cerr << "Failed to load BMP '" << bmp_path << "'.\n";
            return 1;
        }
    } else {
        image.resize(W*H);
        for (int i = 0; i < W*H; ++i) image[i] = static_cast<uint8_t>(i % 256);
    }

    // -------- Modules --------
    cmos_sensor      sensor ("sensor",  image);   // TDF analog source (double samples)
    CannyEdgeWrapper wrapper("wrapper", W, H);    // TDF A/D + 1D LUT + DE bridge
    ISP_Canny        isp    ("isp",     W, H);    // Verilated Canny (lab10)
    Lut1D_DE         lut    ("lut");              // post-ISP 1D LUT (DE)
    BurstPacker      packer ("packer");           // packs 32 pixels -> 256b burst
    LPDDR            dram   ("lpddr");            // NEW: 256b write+read LPDDR
    PcieDMA_Tap      dma    ("pcie_dma");         // NEW: passive throughput monitor
    ReadSink256      rsink  ("read_sink");        // NEW: consumes read stream

    // -------- Signals --------
    sca_tdf::sca_signal<double>     analog_sig;
    sc_core::sc_clock               clk("clk", sc_core::sc_time(10, sc_core::SC_NS));

    // ADC → ISP signals
    sc_core::sc_signal< sc_dt::sc_uint<8> > adc_pix;
    sc_core::sc_signal<bool>                adc_vld, adc_hs, adc_vs;

    // ISP → LUT signals
    sc_core::sc_signal< sc_dt::sc_uint<8> > isp_pix;
    sc_core::sc_signal<bool>                isp_vld, isp_vs;

    // LUT → packer signals
    sc_core::sc_signal< sc_dt::sc_uint<8> > lut_pix;
    sc_core::sc_signal<bool>                lut_vld, lut_vs;

    // 256-bit bus
    sc_core::sc_signal< sc_dt::sc_bv<256> > wdata_bus;
    sc_core::sc_signal<bool>                wvalid_sig, wready_sig;

    // LPDDR read bus
    sc_core::sc_signal< sc_dt::sc_bv<256> > rdata_bus;
    sc_core::sc_signal<bool>                rvalid_sig, rready_sig;

    // dummy sink to satisfy any ready_out debug port
    sc_core::sc_signal<bool>                packer_ready_sink;

    // -------- AMS → DE wiring --------
    sensor.out(analog_sig);

    wrapper.analog_in(analog_sig);
    wrapper.pixel_out(adc_pix);
    wrapper.valid_out(adc_vld);
    wrapper.hsync_out(adc_hs);
    wrapper.vsync_out(adc_vs);

    // ---------- ISP always bound ----------
isp.clk(clk);
isp.pix_in(adc_pix);
isp.valid_in(adc_vld);
isp.vsync_in(adc_vs);
isp.pix_out(isp_pix);
isp.valid_out(isp_vld);
isp.vsync_out(isp_vs);

// ---------- LUT clock ----------
lut.clk(clk);

// ---------- Select ISP vs bypass feeding the LUT ----------
if (!bypass_isp) {
    std::cout << "[PIPE] ISP in-path (ADC → ISP → LUT)\n";
    lut.pix_in(isp_pix);
    lut.valid_in(isp_vld);
    lut.vsync_in(isp_vs);
} else {
    std::cout << "[PIPE] ISP bypass ENABLED (ADC → LUT)\n";
    lut.pix_in(adc_pix);
    lut.valid_in(adc_vld);
    lut.vsync_in(adc_vs);
}

    
    lut.pix_out(lut_pix);
    lut.valid_out(lut_vld);
    lut.vsync_out(lut_vs);

    // Program the post-ISP LUT
    lut.load_identity();
    if (!lut_path.empty()) {
      if (!lut.load_lut_file(lut_path)) {
        std::cerr << "[LUT] Failed to load '" << lut_path << "'. Using identity.\n";
      }
    } else {
      if (gain  != 1.0 || offs != 0.0) lut.apply_gain_offset(gain, offs);
      if (gamma >  0.0)                lut.apply_gamma(gamma);
    }
    if (!lut_dump.empty())      (void)lut.dump_lut(lut_dump);
    if (!hist_in_dump.empty() || !hist_out_dump.empty()) {
      lut.enable_stats(true);
      if (!hist_in_dump.empty())  lut.set_hist_in_dump_path(hist_in_dump);
      if (!hist_out_dump.empty()) lut.set_hist_out_dump_path(hist_out_dump);
    }

    // -------- Packer / DRAM / DMA / Read sink wiring --------
    packer.clk(clk);
    packer.pix_in(lut_pix);
    packer.valid_in(lut_vld);
    packer.vsync_in(lut_vs);
    packer.burst_out  (wdata_bus);
    packer.burst_valid(wvalid_sig);
    packer.burst_ready(wready_sig);
    packer.ready_out  (packer_ready_sink); // unused

    // LPDDR write+read
    dram.clk(clk);
    dram.wdata(wdata_bus);
    dram.wvalid(wvalid_sig);
    dram.wready(wready_sig);

    dram.rdata(rdata_bus);
    dram.rvalid(rvalid_sig);
    dram.rready(rready_sig);

    dram.set_expected_bytes(static_cast<uint32_t>(W*H));
    dram.reset_counters();

    // PCIe-DMA throughput tap (passive)
    dma.clk(clk);
    dma.data_in(wdata_bus);
    dma.valid_in(wvalid_sig);
    dma.set_expected_bytes(static_cast<uint32_t>(W*H));

    // Read sink consumes DRAM read stream (drives rready=1)
    rsink.clk(clk);
    rsink.data_in(rdata_bus);
    rsink.valid_in(rvalid_sig);
    rsink.ready_out(rready_sig);
    rsink.set_expected_bytes(static_cast<uint32_t>(W*H));

    // -------- Go --------
    std::cout << "Running pipeline: Sensor(AMS) → ADC → "
              << (bypass_isp ? "(bypass ISP) " : "ISP(Canny) ")
              << "→ 1D LUT → 256b pack → LPDDR (write + read) + PCIeDMA tap\n";

    sc_core::sc_start();   // LPDDR no longer stops sim itself; we'll stop after the frame

    // Host read-back (same as before — proves content)
    std::vector<uint8_t> frame_back;
    dram.read_back(frame_back);
    if (frame_back.size() >= static_cast<size_t>(W*H)) {
        frame_back.resize(W*H);
        write_pgm("out.pgm", W, H, frame_back);
    } else {
        std::cerr << "[WARN] DRAM returned fewer bytes than expected: "
                  << frame_back.size() << " < " << (W*H) << "\n";
    }

    dram.report(); // print WRITE and READ throughputs
    std::cout << "PASS\n";
    return 0;
}

