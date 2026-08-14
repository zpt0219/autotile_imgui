#pragma once

#include "model/recipe_library.h"
#include <string>
#include <vector>
#include <cstdint>

namespace atm {

struct ZipImportResult {
    std::vector<RecipeEntry> entries;
    std::vector<std::string> errors;
    bool success = false;
};

// Import recipes from a .zip file on disk
ZipImportResult import_recipes_from_zip(const std::string& zip_path);

// Import recipes from memory buffer containing a .zip
ZipImportResult import_recipes_from_zip_memory(const uint8_t* data, size_t size);

} // namespace atm
