#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace atm {

struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixels; // RGBA
};

bool write_png(const std::string& path, int width, int height, const uint8_t* rgba_data);
bool read_png(const std::string& path, ImageData& out_image);

} // namespace atm
