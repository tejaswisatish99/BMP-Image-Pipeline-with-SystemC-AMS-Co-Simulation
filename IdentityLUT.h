#pragma once
#include <cstdint>

struct IdentityLUT {
    IdentityLUT();

    // map 0..255 -> 0..255
    uint8_t apply(uint8_t in) const;

    // reset to identity
    void reset_identity();

    // compose transforms onto the existing table
    void compose_gain_offset(double gain, double offset);
    void compose_gamma(double gamma);

    // compatibility aliases (so both names compile)
    void apply_gain_offset(double g, double o) { compose_gain_offset(g, o); }
    void apply_gamma(double g)                 { compose_gamma(g); }

    // CSV I/O: either "val" per line, or "idx,val"
    bool load_csv(const char* path);
    bool dump_csv(const char* path) const;

private:
    uint8_t lut_[256];
};

