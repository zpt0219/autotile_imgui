#include "sheet_renderer.h"
#include "pattern/sheet.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace atm_desktop {

SheetRenderer::SheetRenderer()
    : rgba_buffer_(256 * 192 * 4, 0) {}

SheetRenderer::~SheetRenderer() {
    cleanup();
}

void SheetRenderer::initialize() {
    if (texture_id_ != 0) return;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    texture_id_ = static_cast<uint32_t>(tex);

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        width_, height_, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        rgba_buffer_.data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
    needs_upload_ = true;
}

void SheetRenderer::cleanup() {
    if (texture_id_ != 0) {
        GLuint tex = static_cast<GLuint>(texture_id_);
        glDeleteTextures(1, &tex);
        texture_id_ = 0;
    }
}

void SheetRenderer::update(const atm::Recipe& recipe, const atm::PaintOverrides& overrides, atm::DirtyMask dirty) {
    accumulated_dirty_ = static_cast<atm::DirtyMask>(accumulated_dirty_ | dirty);
    needs_upload_ = true;
    (void)recipe;
    (void)overrides;
}

void SheetRenderer::ensure_uploaded(const atm::Recipe& recipe, const atm::PaintOverrides& overrides) {
    if (!needs_upload_ && texture_id_ != 0) return;

    if (texture_id_ == 0) {
        initialize();
    }

    // Render 256x192 RGBA sheet from recipe
    rgba_buffer_ = atm::render_sheet_rgba(recipe, overrides);

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        0, 0, width_, height_,
        GL_RGBA, GL_UNSIGNED_BYTE,
        rgba_buffer_.data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);

    needs_upload_ = false;
    accumulated_dirty_ = atm::DIRTY_NONE;
}

} // namespace atm_desktop
