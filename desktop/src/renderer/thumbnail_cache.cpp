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
    if (initialized_) return;
    stop_ = false;
    worker_ = std::thread(&ThumbnailCache::worker_main, this);
    initialized_ = true;
}

void ThumbnailCache::shutdown() {
    // Join before touching GL: the worker owns nothing but CPU buffers, yet a
    // live thread pushing into completed_ while we tear down is a race for no
    // reason.
    if (worker_.joinable()) {
        stop_ = true;
        cv_.notify_all();
        worker_.join();
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requests_.clear();
        completed_.clear();
        in_flight_.clear();
    }
    clear();
    initialized_ = false;
}

void ThumbnailCache::worker_main() {
    for (;;) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !requests_.empty(); });
            if (stop_) return;
            req = std::move(requests_.front());
            requests_.pop_front();
        }

        // The expensive part, deliberately outside the lock.
        std::vector<uint8_t> rgba = atm::render_sheet_rgba(req.recipe);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            in_flight_.erase(req.hash);
            auto it = versions_.find(req.hash);
            const uint64_t current = (it == versions_.end()) ? 0 : it->second;
            if (current != req.version) continue;  // superseded while rendering
            completed_.push_back(Completed{ req.hash, std::move(rgba), req.version });
        }
    }
}

uint32_t ThumbnailCache::get(const std::string& hash, const atm::Recipe& recipe) {
    auto it = cache_.find(hash);
    if (it != cache_.end() && it->second.texture_id != 0) {
        touch(hash);
        return it->second.texture_id;
    }

    if (!initialized_) initialize();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (in_flight_.count(hash)) return 0;  // already queued
        in_flight_.insert(hash);
        const uint64_t version = versions_[hash];
        requests_.push_back(Request{ hash, recipe, version });
    }
    cv_.notify_one();
    return 0;
}

void ThumbnailCache::drain_completed(int max_uploads_per_frame) {
    std::vector<Completed> batch;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!completed_.empty() && static_cast<int>(batch.size()) < max_uploads_per_frame) {
            batch.push_back(std::move(completed_.front()));
            completed_.pop_front();
        }
    }
    if (batch.empty()) return;

    for (auto& done : batch) {
        // Re-check under the lock's value: invalidate() may have fired between
        // the worker finishing and this upload.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto vit = versions_.find(done.hash);
            const uint64_t current = (vit == versions_.end()) ? 0 : vit->second;
            if (current != done.version) continue;
        }

        auto existing = cache_.find(done.hash);
        if (existing != cache_.end()) {
            destroy_texture(existing->second.texture_id);
            lru_.erase(existing->second.lru_pos);
            cache_.erase(existing);
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     atm::SHEET_WIDTH, atm::SHEET_HEIGHT, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, done.rgba.data());

        lru_.push_front(done.hash);
        Entry entry;
        entry.texture_id = static_cast<uint32_t>(tex);
        entry.lru_pos = lru_.begin();
        cache_[done.hash] = entry;
    }

    evict_if_needed();
}

void ThumbnailCache::touch(const std::string& hash) {
    auto it = cache_.find(hash);
    if (it == cache_.end()) return;
    lru_.erase(it->second.lru_pos);
    lru_.push_front(hash);
    it->second.lru_pos = lru_.begin();
}

void ThumbnailCache::evict_if_needed() {
    while (cache_.size() > capacity_ && !lru_.empty()) {
        const std::string victim = lru_.back();
        lru_.pop_back();
        auto it = cache_.find(victim);
        if (it != cache_.end()) {
            destroy_texture(it->second.texture_id);
            cache_.erase(it);
        }
    }
}

void ThumbnailCache::destroy_texture(uint32_t id) {
    if (id == 0) return;
    GLuint tex = static_cast<GLuint>(id);
    glDeleteTextures(1, &tex);
}

void ThumbnailCache::invalidate(const std::string& hash) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++versions_[hash];  // discards any render already under way
    }
    auto it = cache_.find(hash);
    if (it != cache_.end()) {
        destroy_texture(it->second.texture_id);
        lru_.erase(it->second.lru_pos);
        cache_.erase(it);
    }
}

void ThumbnailCache::clear() {
    for (auto& pair : cache_) {
        destroy_texture(pair.second.texture_id);
    }
    cache_.clear();
    lru_.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& v : versions_) ++v.second;
    completed_.clear();
}

void ThumbnailCache::set_capacity(size_t n) {
    capacity_ = (n < 1) ? 1 : n;
    evict_if_needed();
}

bool ThumbnailCache::has_pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !requests_.empty() || !completed_.empty() || !in_flight_.empty();
}

} // namespace atm_desktop
