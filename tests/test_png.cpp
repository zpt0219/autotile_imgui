#include <doctest/doctest.h>
#include "util/image.h"
#include <vector>
#include <cstdio>

TEST_CASE("PNG write and read back 2x2") {
    std::vector<uint8_t> rgba = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 255
    };
    const std::string tmp_path = "test_2x2.png";
    bool written = atm::write_png(tmp_path, 2, 2, rgba.data());
    CHECK(written);

    atm::ImageData img;
    bool read = atm::read_png(tmp_path, img);
    CHECK(read);
    CHECK(img.width == 2);
    CHECK(img.height == 2);
    CHECK(img.channels == 4);
    CHECK(img.pixels == rgba);

    std::remove(tmp_path.c_str());
}
