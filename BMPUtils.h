#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Loads an uncompressed BMP (8-bit indexed, 24- or 32-bit BGR/BGRA) and converts to 8-bit grayscale.
// Returns true on hit. On hit, W,H and 'out' are filled (size = W*H).
bool load_bmp_grayscale(const std::string& path, int& W, int& H, std::vector<uint8_t>& out);

