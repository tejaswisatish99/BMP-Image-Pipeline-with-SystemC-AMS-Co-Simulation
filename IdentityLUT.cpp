#include "IdentityLUT.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

static inline uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

IdentityLUT::IdentityLUT() { reset_identity(); }

void IdentityLUT::reset_identity() {
    for (int i = 0; i < 256; ++i) lut_[i] = static_cast<uint8_t>(i);
}

uint8_t IdentityLUT::apply(uint8_t in) const { return lut_[in]; }

void IdentityLUT::compose_gain_offset(double gain, double offset) {
    // apply onto current LUT values
    for (int i = 0; i < 256; ++i) {
        int y = static_cast<int>(std::lround(gain * lut_[i] + offset));
        lut_[i] = clamp_u8(y);
    }
}

void IdentityLUT::compose_gamma(double gamma) {
    if (gamma <= 0.0) return;
    // standard gamma encode: out = 255 * (in/255)^(1/gamma)
    for (int i = 0; i < 256; ++i) {
        double x = lut_[i] / 255.0;
        double y = std::pow(x, 1.0 / gamma);
        int out  = static_cast<int>(std::lround(y * 255.0));
        lut_[i]  = clamp_u8(out);
    }
}

bool IdentityLUT::load_csv(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;

    reset_identity();
    char line[256];
    int idx = 0;
    while (std::fgets(line, sizeof(line), f)) {
        // skip comments/blank
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        // accept "idx,val" or just "val"
        int a = -1, b = -1;
        if (std::strchr(p, ',')) {
            if (std::sscanf(p, "%d,%d", &a, &b) == 2) {
                if (0 <= a && a < 256) lut_[a] = clamp_u8(b);
            }
        } else {
            if (std::sscanf(p, "%d", &b) == 1) {
                if (idx < 256) lut_[idx++] = clamp_u8(b);
            }
        }
    }
    std::fclose(f);
    return true;
}

bool IdentityLUT::dump_csv(const char* path) const {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    for (int i = 0; i < 256; ++i) {
        std::fprintf(f, "%d,%d\n", i, static_cast<int>(lut_[i]));
    }
    std::fclose(f);
    return true;
}

