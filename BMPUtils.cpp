#include "BMPUtils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

// Little-endian helpers
static uint16_t rd16(std::ifstream& f) { uint8_t b[2]; f.read((char*)b,2); return uint16_t(b[0] | (b[1]<<8)); }
static uint32_t rd32(std::ifstream& f) { uint8_t b[4]; f.read((char*)b,4); return uint32_t(b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24)); }

static inline uint8_t to_gray_u8(uint8_t r, uint8_t g, uint8_t b) {
    // integer approx of 0.299R + 0.587G + 0.114B
    return uint8_t((77*int(r) + 150*int(g) + 29*int(b) + 128) >> 8);
}

bool load_bmp_grayscale(const std::string& path, int& W, int& H, std::vector<uint8_t>& out) {
    W = H = 0;
    out.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[BMP] Cannot open: " << path << "\n";
        return false;
    }

    // BITMAPFILEHEADER
    uint16_t bfType = rd16(f);
    if (bfType != 0x4D42) { // 'BM'
        std::cerr << "[BMP] Not a BMP file: " << path << "\n";
        return false;
    }
    uint32_t bfSize = rd32(f);
    (void)bfSize;
    (void)rd16(f); (void)rd16(f);              // reserved1, reserved2
    uint32_t bfOffBits = rd32(f);              // pixel data offset

    // DIB header (assume BITMAPINFOHEADER or compatible)
    uint32_t dibSize = rd32(f);
    if (dibSize < 40) {
        std::cerr << "[BMP] Unsupported DIB header size: " << dibSize << "\n";
        return false;
    }
    int32_t width  = (int32_t)rd32(f);
    int32_t height = (int32_t)rd32(f);
    uint16_t planes = rd16(f);
    uint16_t bitcount = rd16(f);
    uint32_t compression = rd32(f);
    uint32_t imgSize = rd32(f);
    (void)imgSize;
    (void)rd32(f); (void)rd32(f);              // xppm, yppm
    uint32_t clrUsed = rd32(f);
    uint32_t clrImp  = rd32(f);
    (void)clrImp;

    if (planes != 1 || (bitcount!=8 && bitcount!=24 && bitcount!=32) || compression!=0) {
        std::cerr << "[BMP] Unsupported format (bitcount=" << bitcount
                  << ", compression=" << compression << ").\n";
        return false;
    }

    bool topDown = (height < 0);
    W = width;
    H = std::abs(height);
    if (W <= 0 || H <= 0) {
        std::cerr << "[BMP] Invalid dimensions.\n";
        return false;
    }

    // Read palette if 8-bit
    struct Pal { uint8_t b,g,r,a; };
    std::vector<Pal> palette;
    if (bitcount == 8) {
        size_t n = clrUsed ? clrUsed : 256;
        palette.resize(n);
        // Palette starts right after the 40-byte BITMAPINFOHEADER (or after dibSize)
        // We already consumed 40 bytes. If dibSize>40, skip the remaining.
        if (dibSize > 40) f.seekg(dibSize - 40, std::ios::cur);
        f.read(reinterpret_cast<char*>(palette.data()), palette.size()*4);
    } else {
        // Skip remaining DIB bytes if any
        if (dibSize > 40) f.seekg(dibSize - 40, std::ios::cur);
    }

    // Seek to pixel array
    f.seekg(bfOffBits, std::ios::beg);

    out.assign(size_t(W)*size_t(H), 0);

    // Row stride aligned to 4 bytes
    auto stride = [&](int bitsPerPixel)->int {
        int bytesPerRow = ((W * bitsPerPixel + 31) / 32) * 4;
        return bytesPerRow;
    };

    if (bitcount == 8) {
        int rowStride = stride(8);
        std::vector<uint8_t> row(rowStride);
        for (int y = 0; y < H; ++y) {
            int dstY = topDown ? y : (H-1-y);
            f.read((char*)row.data(), rowStride);
            for (int x = 0; x < W; ++x) {
                uint8_t idx = row[x];
                Pal p = palette.size() ? palette[idx % palette.size()] : Pal{uint8_t(idx),uint8_t(idx),uint8_t(idx),255};
                out[dstY*W + x] = to_gray_u8(p.r, p.g, p.b);
            }
        }
    } else if (bitcount == 24) {
        int rowStride = stride(24);
        std::vector<uint8_t> row(rowStride);
        for (int y = 0; y < H; ++y) {
            int dstY = topDown ? y : (H-1-y);
            f.read((char*)row.data(), rowStride);
            for (int x = 0; x < W; ++x) {
                uint8_t b = row[3*x + 0];
                uint8_t g = row[3*x + 1];
                uint8_t r = row[3*x + 2];
                out[dstY*W + x] = to_gray_u8(r,g,b);
            }
        }
    } else { // 32-bit
        int rowStride = W * 4;
        std::vector<uint8_t> row(rowStride);
        for (int y = 0; y < H; ++y) {
            int dstY = topDown ? y : (H-1-y);
            f.read((char*)row.data(), rowStride);
            for (int x = 0; x < W; ++x) {
                uint8_t b = row[4*x + 0];
                uint8_t g = row[4*x + 1];
                uint8_t r = row[4*x + 2];
                out[dstY*W + x] = to_gray_u8(r,g,b);
            }
            // 32-bit rows are naturally 4-byte aligned; no extra padding.
        }
    }

    if (!f) {
        std::cerr << "[BMP] Unexpected EOF while reading pixels.\n";
        return false;
    }

    std::cout << "[BMP] Loaded " << W << "x" << H << " from " << path << "\n";
    return true;
}

