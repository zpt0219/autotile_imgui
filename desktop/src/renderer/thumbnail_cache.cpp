#include "thumbnail_cache.h"
#include "pattern/sheet.h"
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace atm_desktop {

ThumbnailCache::ThumbnailCache() = default;

ThumbnailCache::~ThumbnailCache() {
    shutdown();
}

void ThumbnailCache::initialize() {
    initialized_ = true;
}

void ThumbnailCache::shutdown() {
    clear();
    initialized_ = false;
}

void ThumbnailCache::clear() {
    for (auto& pair : cache_) {
        if (pair.second.texture_id != 0) {
            GLuint tex = static_cast<GLuint>(pair.second.texture_id);
            glDeleteTextures(1, &tex);
            pair.second.texture_id = 0;
        }
    }
    cache_.clear();
}

void ThumbnailCache::invalidate(const std::string& hash) {
    auto it = cache_.find(hash);
    if (it != cache_.end()) {
        if (it->second.texture_id != 0) {
            GLuint tex = static_cast<GLuint>(it->second.texture_id);
            glDeleteTextures(1, &tex);
        }
        cache_.erase(it);
    }
}

uint32_t ThumbnailCache::get_or_render_thumbnail(const std::string& hash, const atm::Recipe& recipe) {
    auto it = cache_.find(hash);
    if (it != cache_.end() && it->second.texture_id != 0) {
        return it->second.texture_id;
    }

    // Render 256x192 sheet RGBA buffer
    auto rgba_data = atm::render_sheet_rgba(recipe);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atm::SHEET_WIDTH, atm::SHEET_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data.data());

    ThumbnailItem item;
    item.texture_id = static_cast<uint32_t>(tex);
    item.width = atm::SHEET_WIDTH;
    item.height = atm::SHEET_HEIGHT;
    item.hash = hash;
    cache_[hash] = item;

    return item.texture_id;
}

} // namespace atm_desktop
