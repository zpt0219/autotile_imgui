#include "image.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996 4244)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#if defined(_MSC_VER)
    #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif

namespace atm {

bool write_png(const std::string& path, int width, int height, const uint8_t* rgba_data) {
    if (width <= 0 || height <= 0 || !rgba_data) return false;
    int stride = width * 4;
    return stbi_write_png(path.c_str(), width, height, 4, rgba_data, stride) != 0;
}

bool read_png(const std::string& path, ImageData& out_image) {
    int w = 0, h = 0, ch = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return false;
    out_image.width = w;
    out_image.height = h;
    out_image.channels = 4;
    out_image.pixels.assign(data, data + (w * h * 4));
    stbi_image_free(data);
    return true;
}

} // namespace atm
