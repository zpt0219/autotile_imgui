#pragma once

#include "model/recipe.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <list>
#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

namespace atm_desktop {

/**
 * Sheet thumbnails for the library grid.
 *
 * Rendering a sheet costs ~30ms, so doing it inline while drawing meant one
 * frozen frame per newly visible card — six seconds on a 200-recipe library.
 * Renders now happen on a worker thread and only the GL upload runs on the
 * main thread, which is the same split the batch exporter uses and the only
 * one allowed: GL calls are main-thread-only.
 *
 * All public methods are main-thread-only unless marked otherwise.
 */
class ThumbnailCache {
public:
    ThumbnailCache();
    ~ThumbnailCache();

    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    void initialize();

    /** Joins the worker and drops every texture. Must run while GL is alive. */
    void shutdown();

    /**
     * Texture for `hash`, or 0 if it is not ready yet.
     *
     * A miss queues the render and returns 0 straight away; call again on a
     * later frame. Draw a placeholder for 0 rather than blocking.
     */
    uint32_t get(const std::string& hash, const atm::Recipe& recipe);

    /** Uploads finished renders. Call once per frame, before drawing. */
    void drain_completed(int max_uploads_per_frame = 4);

    /** Drops the entry and invalidates any render already in flight for it. */
    void invalidate(const std::string& hash);

    void clear();

    /** Upper bound on retained textures; least recently used are evicted. */
    void set_capacity(size_t n);

    size_t size() const { return cache_.size(); }
    bool has_pending() const;

private:
    struct Request {
        std::string hash;
        atm::Recipe recipe;
        uint64_t version = 0;
    };

    struct Completed {
        std::string hash;
        std::vector<uint8_t> rgba;
        uint64_t version = 0;
    };

    struct Entry {
        uint32_t texture_id = 0;
        std::list<std::string>::iterator lru_pos;
    };

    void worker_main();
    void touch(const std::string& hash);
    void evict_if_needed();
    void destroy_texture(uint32_t id);

    // Main thread only.
    std::unordered_map<std::string, Entry> cache_;
    std::list<std::string> lru_;                 // front = most recently used
    size_t capacity_ = 192;
    bool initialized_ = false;

    // Shared with the worker.
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Request> requests_;
    std::deque<Completed> completed_;
    std::unordered_set<std::string> in_flight_;
    // Bumped on invalidate so a render already under way is discarded instead
    // of overwriting the newer recipe with a stale picture.
    std::unordered_map<std::string, uint64_t> versions_;

    std::thread worker_;
    std::atomic<bool> stop_{ false };
};

} // namespace atm_desktop
