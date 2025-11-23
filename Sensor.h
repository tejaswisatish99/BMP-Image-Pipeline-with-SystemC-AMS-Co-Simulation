#pragma once
#include <systemc-ams.h>
#include <vector>
#include <cstdint>

struct cmos_sensor : sca_tdf::sca_module {
    sca_tdf::sca_out<double> out;

    cmos_sensor(sc_core::sc_module_name nm, const std::vector<uint8_t>& img);

    void set_attributes() override;
    void processing() override;

private:
    std::vector<uint8_t> image_;
    std::size_t idx_ = 0;
};

