#pragma once

#include "model/recipe.h"
#include <unordered_map>
#include <string>
#include <cstdint>

namespace atm_desktop {

struct ThumbnailItem {
    uint32_t texture_id = 0;
    int width = 64;
    int height = 48;
    std::string hash;
};

class ThumbnailCache {
public:
    ThumbnailCache();
    ~ThumbnailCache();

    void initialize();
    void shutdown();

    uint32_t get_or_render_thumbnail(const std::string& hash, const atm::Recipe& recipe);
    void invalidate(const std::string& hash);
    void clear();

private:
    std::unordered_map<std::string, ThumbnailItem> cache_;
    bool initialized_ = false;
};

} // namespace atm_desktop
