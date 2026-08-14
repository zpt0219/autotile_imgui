#include "preview_panel.h"
#include "view_model/view_model.h"
#include "pattern/blob47.h"
#include <imgui.h>
#include <cmath>

namespace atm_desktop {

void PreviewPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Sheet Preview", &open_)) {
        // Top Toolbar
        ImGui::Text("Zoom:");
        ImGui::SameLine();
        if (ImGui::RadioButton("1x", vm.zoom_level == 1.0f)) vm.zoom_level = 1.0f;
        ImGui::SameLine();
        if (ImGui::RadioButton("2x", vm.zoom_level == 2.0f)) vm.zoom_level = 2.0f;
        ImGui::SameLine();
        if (ImGui::RadioButton("3x", vm.zoom_level == 3.0f)) vm.zoom_level = 3.0f;
        ImGui::SameLine();
        if (ImGui::RadioButton("4x", vm.zoom_level == 4.0f)) vm.zoom_level = 4.0f;

        ImGui::SameLine(0, 20.0f);
        ImGui::Checkbox("Grid", &vm.show_grid);
        ImGui::SameLine();
        ImGui::Checkbox("Checkerboard", &vm.show_checkerboard);

        ImGui::Separator();

        // Ensure texture is updated
        auto* selected = vm.handler().selected_recipe();
        if (selected) {
            vm.sheet_renderer().ensure_uploaded(selected->recipe);
        }

        uint32_t tex_id = vm.sheet_renderer().get_texture_id();
        int base_w = vm.sheet_renderer().get_width();   // 256
        int base_h = vm.sheet_renderer().get_height();  // 192

        float disp_w = base_w * vm.zoom_level;
        float disp_h = base_h * vm.zoom_level;

        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + disp_w, canvas_p0.y + disp_h);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Background checkerboard
        if (vm.show_checkerboard) {
            float check_sz = 8.0f * vm.zoom_level;
            for (float y = canvas_p0.y; y < canvas_p1.y; y += check_sz) {
                for (float x = canvas_p0.x; x < canvas_p1.x; x += check_sz) {
                    int ix = static_cast<int>((x - canvas_p0.x) / check_sz);
                    int iy = static_cast<int>((y - canvas_p0.y) / check_sz);
                    ImU32 col = ((ix + iy) & 1) ? IM_COL32(50, 50, 50, 255) : IM_COL32(35, 35, 35, 255);
                    draw_list->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + check_sz, canvas_p1.x), std::min(y + check_sz, canvas_p1.y)), col);
                }
            }
        } else {
            draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(20, 20, 20, 255));
        }

        // Texture image
        if (tex_id != 0) {
            draw_list->AddImage(
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(tex_id)),
                canvas_p0, canvas_p1,
                ImVec2(0, 0), ImVec2(1, 1)
            );
        }

        // Tile Grid Lines (8x6 tiles of 32px)
        if (vm.show_grid) {
            float tile_size_disp = 32.0f * vm.zoom_level;
            for (int x = 0; x <= 8; ++x) {
                float gx = canvas_p0.x + x * tile_size_disp;
                draw_list->AddLine(ImVec2(gx, canvas_p0.y), ImVec2(gx, canvas_p1.y), IM_COL32(255, 255, 255, 60));
            }
            for (int y = 0; y <= 6; ++y) {
                float gy = canvas_p0.y + y * tile_size_disp;
                draw_list->AddLine(ImVec2(canvas_p0.x, gy), ImVec2(canvas_p1.x, gy), IM_COL32(255, 255, 255, 60));
            }
        }

        // Mouse hover interaction & tooltip
        ImVec2 mouse_pos = ImGui::GetMousePos();
        if (mouse_pos.x >= canvas_p0.x && mouse_pos.x < canvas_p1.x &&
            mouse_pos.y >= canvas_p0.y && mouse_pos.y < canvas_p1.y) {
            float rel_x = mouse_pos.x - canvas_p0.x;
            float rel_y = mouse_pos.y - canvas_p0.y;
            int tile_x = static_cast<int>(rel_x / (32.0f * vm.zoom_level));
            int tile_y = static_cast<int>(rel_y / (32.0f * vm.zoom_level));
            int slot = tile_y * 8 + tile_x;

            if (slot >= 0 && slot < 48) {
                // Highlight hovered tile
                float hx0 = canvas_p0.x + tile_x * (32.0f * vm.zoom_level);
                float hy0 = canvas_p0.y + tile_y * (32.0f * vm.zoom_level);
                float hx1 = hx0 + (32.0f * vm.zoom_level);
                float hy1 = hy0 + (32.0f * vm.zoom_level);
                draw_list->AddRect(ImVec2(hx0, hy0), ImVec2(hx1, hy1), IM_COL32(255, 220, 0, 200), 0.0f, 0, 2.0f);

                int mask = atm::BLOB47_LAYOUT[slot];
                ImGui::BeginTooltip();
                ImGui::Text("Slot %d: Tile (%d, %d)", slot, tile_x, tile_y);
                ImGui::Text("Canonical Mask: %d (0x%02X)", mask, mask);
                ImGui::EndTooltip();
            }
        }

        ImGui::Dummy(ImVec2(disp_w, disp_h));
    }
    ImGui::End();
}

} // namespace atm_desktop
