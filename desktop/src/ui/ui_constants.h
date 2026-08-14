#pragma once

#include <array>

namespace atm_desktop {
namespace ui {

constexpr float DEFAULT_PROPERTY_LABEL_WIDTH = 120.0f;
constexpr float COMPACT_INPUT_WIDTH = 80.0f;
constexpr float SLIDER_WIDTH = 140.0f;

// 13 standard zoom scales for tile & sheet viewports
inline constexpr std::array<float, 13> kZoomScales = {
    0.25f, 0.33f, 0.50f, 0.67f, 0.75f, 1.00f, 1.50f, 2.00f, 3.00f, 4.00f, 6.00f, 8.00f, 12.00f
};

} // namespace ui
} // namespace atm_desktop
