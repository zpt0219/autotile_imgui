#pragma once

#include "model/recipe.h"
#include "pattern/sheet.h"
#include "command/library_callbacks.h"
#include <vector>
#include <cstdint>

namespace atm_desktop {

class SheetRenderer {
public:
    SheetRenderer();
    ~SheetRenderer();

    void initialize();
    void cleanup();

    void update(const atm::Recipe& recipe, const atm::PaintOverrides& overrides = atm::PaintOverrides{}, atm::DirtyMask dirty = atm::DIRTY_ALL);
    void ensure_uploaded(const atm::Recipe& recipe, const atm::PaintOverrides& overrides = atm::PaintOverrides{});

    uint32_t get_texture_id() const { return texture_id_; }
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    const std::vector<uint8_t>& get_rgba_buffer() const { return rgba_buffer_; }

private:
    uint32_t texture_id_ = 0;
    int width_ = 256;
    int height_ = 192;
    std::vector<uint8_t> rgba_buffer_;
    atm::DirtyMask accumulated_dirty_ = atm::DIRTY_ALL;
    bool needs_upload_ = true;
};

} // namespace atm_desktop
