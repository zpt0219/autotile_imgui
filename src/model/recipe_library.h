#pragma once

#include "recipe.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace atm {

struct RecipeEntry {
    std::string hash; // Unique stable identifier
    std::string name = "Untitled Recipe";
    Recipe recipe;
    std::unordered_map<std::string, std::string> tags;
};

enum class VariantAxisType {
    Pattern,
    Palette,
    TextureA,
    TextureB,
    Ribbon
};

struct VariantOption {
    std::string label;
    nlohmann::json value;
};

struct VariantAxis {
    std::string name;
    VariantAxisType type = VariantAxisType::Pattern;
    bool enabled = true;
    std::vector<VariantOption> options;
};

struct ExportSettings {
    std::string out_dir = "./export";
    std::string name_template = "{name}_{pattern}_{texA}";
    int scale = 1;
    bool export_png = true;
    bool export_json_sidecar = true;
};

class RecipeLibrary {
public:
    RecipeLibrary();
    ~RecipeLibrary() = default;

    const std::vector<std::shared_ptr<RecipeEntry>>& entries() const { return entries_; }
    std::vector<std::shared_ptr<RecipeEntry>>& entries() { return entries_; }

    const std::vector<VariantAxis>& axes() const { return axes_; }
    std::vector<VariantAxis>& axes() { return axes_; }

    const ExportSettings& export_settings() const { return export_settings_; }
    ExportSettings& export_settings() { return export_settings_; }

    std::shared_ptr<RecipeEntry> find_by_hash(const std::string& hash) const;
    int index_of_hash(const std::string& hash) const;

    std::shared_ptr<RecipeEntry> add_recipe(const Recipe& recipe, const std::string& name = "New Recipe", const std::string& specific_hash = "");
    bool remove_recipe(const std::string& hash);
    std::shared_ptr<RecipeEntry> duplicate_recipe(const std::string& hash, const std::string& specific_hash = "");
    bool reorder_recipe(int from_index, int to_index);

    nlohmann::json to_json() const;
    static std::unique_ptr<RecipeLibrary> from_json(const nlohmann::json& j);

    bool save_to_file(const std::string& filepath) const;
    static std::unique_ptr<RecipeLibrary> load_from_file(const std::string& filepath);

private:
    std::vector<std::shared_ptr<RecipeEntry>> entries_;
    std::vector<VariantAxis> axes_;
    ExportSettings export_settings_;
};

std::string generate_recipe_hash();

} // namespace atm
