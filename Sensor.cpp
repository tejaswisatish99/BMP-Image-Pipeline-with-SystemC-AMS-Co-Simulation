#include "Sensor.h"
#include <cstdio>

cmos_sensor::cmos_sensor(sc_core::sc_module_name nm, const std::vector<uint8_t>& img)
: sca_tdf::sca_module(nm), out("out"), image_(img) {
    std::printf("[SENSOR] Loaded %zu pixels from host image.\n", image_.size());
}

void cmos_sensor::set_attributes() {
    out.set_timestep(sc_core::sc_time(10, sc_core::SC_NS));
}

void cmos_sensor::processing() {
    if (idx_ >= image_.size()) {
        out.write(0.0);
        return;
    }
    const double v = static_cast<double>(image_[idx_]);
    out.write(v);
    ++idx_;
}

