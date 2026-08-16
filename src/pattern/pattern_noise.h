#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace atm {

enum class NoiseId {
    White,
    Blue,
    Ordered
};

enum class NoiseTargetId {
    Edge,
    TerrainA,
    TerrainB
};

NoiseId parse_noise_id(const std::string& name);
std::string noise_id_to_string(NoiseId id);
bool is_known_noise(const std::string& name);

NoiseTargetId parse_noise_target_id(const std::string& name);
std::string noise_target_id_to_string(NoiseTargetId id);

float sample_noise(NoiseId noise, int x, int y, int32_t seed);

int noise_step(
    const std::vector<NoiseId>& noises,
    int x,
    int y,
    int32_t seed = 0,
    float strength = 1.0f
);

} // namespace atm
