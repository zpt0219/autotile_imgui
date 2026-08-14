#pragma once

#include "model/recipe.h"
#include <string>
#include <optional>

namespace atm {

std::string encode_recipe(const Recipe& recipe);
std::optional<Recipe> decode_recipe(const std::string& hash);

} // namespace atm
