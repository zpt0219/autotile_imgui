#include "preview_panel.h"
#include "view_model/view_model.h"
#include "pattern/blob47.h"
#include "ui/ui_constants.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace atm_desktop {

void PreviewPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Sheet Preview", &open_)) {
        bool use_zh = vm.use_zh;

        // Toolbar
        auto zoom_in = [](float current) {
            for (float z : ui::kZoomScales) {
                if (z > current + 0.01f) return z;
            }
            return ui::kZoomScales.back();
        };
        auto zoom_out = [](float current) {
            for (int i = static_cast<int>(ui::kZoomScales.size()) - 1; i >= 0; --i) {
                if (ui::kZoomScales[i] < current - 0.01f) return ui::kZoomScales[i];
            }
            return ui::kZoomScales.front();
        };

        if (ImGui::Button("-##ZoomOut")) {
            vm.zoom_level = zoom_out(vm.zoom_level);
        }
        ImGui::SameLine();
        ImGui::Text("%.2fx", vm.zoom_level);
        ImGui::SameLine();
        if (ImGui::Button("+##ZoomIn")) {
            vm.zoom_level = zoom_in(vm.zoom_level);
        }

        ImGui::SameLine(0, 15.0f);
        if (ImGui::Button(use_zh ? "原尺寸 (1x)" : "1:1")) {
            vm.zoom_level = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button(use_zh ? "双倍 (2x)" : "2x")) {
            vm.zoom_level = 2.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button(use_zh ? "重置平移" : "Reset Pan")) {
            pan_offset_ = ImVec2(0.0f, 0.0f);
        }

        ImGui::SameLine(0, 20.0f);
        ImGui::Checkbox(use_zh ? "网格" : "Grid", &vm.show_grid);
        ImGui::SameLine();
        ImGui::Checkbox(use_zh ? "棋盘背景" : "Checkerboard", &vm.show_checkerboard);

        if (focused_slot_ >= 0) {
            ImGui::SameLine(0, 20.0f);
            if (ImGui::Button(use_zh ? "退出单瓦片聚焦 [X]" : "Exit Focus Mode [X]")) {
                focused_slot_ = -1;
            }
        }

        ImGui::Separator();

        // Mouse pan with middle drag or right drag
        if (ImGui::IsWindowHovered() && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
            pan_offset_.x += ImGui::GetIO().MouseDelta.x;
            pan_offset_.y += ImGui::GetIO().MouseDelta.y;
        }

        // Ctrl + Mouse wheel zoom
        if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
            if (ImGui::GetIO().MouseWheel > 0.0f) {
                vm.zoom_level = zoom_in(vm.zoom_level);
            } else {
                vm.zoom_level = zoom_out(vm.zoom_level);
            }
        }

        // Ensure texture is rendered and uploaded
        auto* selected = vm.handler().selected_recipe();
        if (selected) {
            vm.sheet_renderer().ensure_uploaded(selected->recipe);
        }

        uint32_t tex_id = vm.sheet_renderer().get_texture_id();
        int base_w = vm.sheet_renderer().get_width();   // 256
        int base_h = vm.sheet_renderer().get_height();  // 192

        float disp_w = base_w * vm.zoom_level;
        float disp_h = base_h * vm.zoom_level;

        ImVec2 start_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_p0 = ImVec2(start_pos.x + pan_offset_.x, start_pos.y + pan_offset_.y);
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

        // Grid lines (8x6 tiles of 32px)
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

        // Focused tile highlight on sheet
        if (focused_slot_ >= 0 && focused_slot_ < 48) {
            int fx = focused_slot_ % 8;
            int fy = focused_slot_ / 8;
            float hx0 = canvas_p0.x + fx * (32.0f * vm.zoom_level);
            float hy0 = canvas_p0.y + fy * (32.0f * vm.zoom_level);
            float hx1 = hx0 + (32.0f * vm.zoom_level);
            float hy1 = hy0 + (32.0f * vm.zoom_level);
            draw_list->AddRect(ImVec2(hx0, hy0), ImVec2(hx1, hy1), IM_COL32(0, 230, 255, 255), 0.0f, 0, 3.0f);
        }

        // Mouse hover interaction & click to focus
        ImVec2 mouse_pos = ImGui::GetMousePos();
        if (mouse_pos.x >= canvas_p0.x && mouse_pos.x < canvas_p1.x &&
            mouse_pos.y >= canvas_p0.y && mouse_pos.y < canvas_p1.y) {
            float rel_x = mouse_pos.x - canvas_p0.x;
            float rel_y = mouse_pos.y - canvas_p0.y;
            int tile_x = static_cast<int>(rel_x / (32.0f * vm.zoom_level));
            int tile_y = static_cast<int>(rel_y / (32.0f * vm.zoom_level));
            int slot = tile_y * 8 + tile_x;

            if (slot >= 0 && slot < 48) {
                float hx0 = canvas_p0.x + tile_x * (32.0f * vm.zoom_level);
                float hy0 = canvas_p0.y + tile_y * (32.0f * vm.zoom_level);
                float hx1 = hx0 + (32.0f * vm.zoom_level);
                float hy1 = hy0 + (32.0f * vm.zoom_level);
                draw_list->AddRect(ImVec2(hx0, hy0), ImVec2(hx1, hy1), IM_COL32(255, 220, 0, 200), 0.0f, 0, 2.0f);

                // Click to toggle focus
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    focused_slot_ = (focused_slot_ == slot) ? -1 : slot;
                }

                // 3x3 Adjacency Diagram Tooltip
                uint8_t mask = atm::BLOB47_LAYOUT[slot];
                int canonical_idx = atm::blob_index_for_mask(mask);

                ImGui::BeginTooltip();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s %d", use_zh ? "瓦片槽位 (Slot)" : "Slot", slot);
                ImGui::Text("%s: (%d, %d)", use_zh ? "行列坐标" : "Grid (Col, Row)", tile_x, tile_y);
                ImGui::Text("%s: 0x%02X (%d)", use_zh ? "邻接掩码 (Mask)" : "Canonical Mask", mask, mask);
                ImGui::Text("%s: %d / 47", use_zh ? "规范序号" : "Canonical Index", canonical_idx);
                ImGui::Separator();
                ImGui::TextDisabled("%s", use_zh ? "3x3 邻接图 (绿色=地形A, 灰色=地形B):" : "3x3 Adjacency (Green=A, Gray=B):");

                // Draw 3x3 Adjacency Diagram
                bool n  = (mask & atm::N) != 0;
                bool e  = (mask & atm::E) != 0;
                bool s  = (mask & atm::S) != 0;
                bool w  = (mask & atm::W) != 0;
                bool ne = (mask & atm::NE) != 0;
                bool se = (mask & atm::SE) != 0;
                bool sw = (mask & atm::SW) != 0;
                bool nw = (mask & atm::NW) != 0;

                bool grid[3][3] = {
                    { nw, n, ne },
                    { w, true, e },
                    { sw, s, se }
                };

                float cell_sz = 16.0f;
                ImVec2 t_pos = ImGui::GetCursorScreenPos();
                ImDrawList* t_dl = ImGui::GetWindowDrawList();

                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        ImVec2 c0(t_pos.x + c * (cell_sz + 2.0f), t_pos.y + r * (cell_sz + 2.0f));
                        ImVec2 c1(c0.x + cell_sz, c0.y + cell_sz);
                        ImU32 col = grid[r][c] ? IM_COL32(76, 175, 80, 255) : IM_COL32(60, 64, 75, 255);
                        t_dl->AddRectFilled(c0, c1, col, 2.0f);
                        if (r == 1 && c == 1) {
                            t_dl->AddRect(c0, c1, IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);
                        }
                    }
                }
                ImGui::Dummy(ImVec2(3 * (cell_sz + 2.0f), 3 * (cell_sz + 2.0f)));

                ImGui::Separator();
                ImGui::TextDisabled("%s", use_zh ? "点击此瓦片进入单瓦片放大聚焦模式" : "Click tile to toggle Focus Mode");
                ImGui::EndTooltip();
            }
        }

        // Focused tile inspector overlay in corner
        if (focused_slot_ >= 0 && focused_slot_ < 48 && tex_id != 0) {
            int fx = focused_slot_ % 8;
            int fy = focused_slot_ / 8;
            uint8_t mask = atm::BLOB47_LAYOUT[focused_slot_];

            ImVec2 uv0(fx * 32.0f / 256.0f, fy * 32.0f / 192.0f);
            ImVec2 uv1((fx + 1) * 32.0f / 256.0f, (fy + 1) * 32.0f / 192.0f);

            ImGui::SetNextWindowPos(ImVec2(start_pos.x + 10, start_pos.y + 10), ImGuiCond_Always);
            ImGui::BeginChild("FocusedTileChild", ImVec2(160, 210), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "%s: %d", use_zh ? "聚焦瓦片" : "Focus Tile", focused_slot_);
            ImGui::Text("Mask: 0x%02X", mask);

            // 4x magnified single tile
            ImGui::Image(
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(tex_id)),
                ImVec2(128, 128),
                uv0, uv1
            );

            if (ImGui::Button(use_zh ? "关闭聚焦" : "Close Focus", ImVec2(-1, 0))) {
                focused_slot_ = -1;
            }
            ImGui::EndChild();
        }

        ImGui::Dummy(ImVec2(disp_w + std::abs(pan_offset_.x), disp_h + std::abs(pan_offset_.y)));
    }
    ImGui::End();
}

} // namespace atm_desktop
