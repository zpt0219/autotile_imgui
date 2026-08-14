#include "recipe_library.h"
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <chrono>

namespace atm {

std::string generate_recipe_hash() {
    static std::mt19937_64 rng(static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    uint64_t val = rng();
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << val;
    return ss.str();
}

RecipeLibrary::RecipeLibrary() {
    // Populate with default recipe entry
    auto entry = std::make_shared<RecipeEntry>();
    entry->hash = generate_recipe_hash();
    entry->name = "Default Meadow";
    entry->recipe = get_default_recipe();
    entries_.push_back(entry);
}

std::shared_ptr<RecipeEntry> RecipeLibrary::find_by_hash(const std::string& hash) const {
    for (const auto& entry : entries_) {
        if (entry->hash == hash) return entry;
    }
    return nullptr;
}

int RecipeLibrary::index_of_hash(const std::string& hash) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i]->hash == hash) return static_cast<int>(i);
    }
    return -1;
}

std::shared_ptr<RecipeEntry> RecipeLibrary::add_recipe(const Recipe& recipe, const std::string& name, const std::string& specific_hash) {
    auto entry = std::make_shared<RecipeEntry>();
    entry->hash = specific_hash.empty() ? generate_recipe_hash() : specific_hash;
    entry->name = name;
    entry->recipe = recipe;
    entries_.push_back(entry);
    return entry;
}

bool RecipeLibrary::remove_recipe(const std::string& hash) {
    int idx = index_of_hash(hash);
    if (idx < 0) return false;
    entries_.erase(entries_.begin() + idx);
    return true;
}

std::shared_ptr<RecipeEntry> RecipeLibrary::duplicate_recipe(const std::string& hash, const std::string& specific_hash) {
    auto src = find_by_hash(hash);
    if (!src) return nullptr;
    auto copy = std::make_shared<RecipeEntry>();
    copy->hash = specific_hash.empty() ? generate_recipe_hash() : specific_hash;
    copy->name = src->name + " (Copy)";
    copy->recipe = src->recipe;
    copy->tags = src->tags;
    
    int idx = index_of_hash(hash);
    entries_.insert(entries_.begin() + idx + 1, copy);
    return copy;
}

bool RecipeLibrary::reorder_recipe(int from_index, int to_index) {
    if (from_index < 0 || from_index >= static_cast<int>(entries_.size())) return false;
    if (to_index < 0 || to_index >= static_cast<int>(entries_.size())) return false;
    if (from_index == to_index) return true;

    auto item = entries_[from_index];
    entries_.erase(entries_.begin() + from_index);
    entries_.insert(entries_.begin() + to_index, item);
    return true;
}

nlohmann::json RecipeLibrary::to_json() const {
    nlohmann::json j;
    j["version"] = 1;
    
    nlohmann::json entries_json = nlohmann::json::array();
    for (const auto& entry : entries_) {
        nlohmann::json e;
        e["hash"] = entry->hash;
        e["name"] = entry->name;
        e["recipe"] = recipe_to_json(entry->recipe);
        e["tags"] = entry->tags;
        entries_json.push_back(e);
    }
    j["entries"] = entries_json;

    nlohmann::json axes_json = nlohmann::json::array();
    for (const auto& axis : axes_) {
        nlohmann::json a;
        a["name"] = axis.name;
        a["type"] = static_cast<int>(axis.type);
        a["enabled"] = axis.enabled;
        nlohmann::json opts = nlohmann::json::array();
        for (const auto& opt : axis.options) {
            opts.push_back({
                { "label", opt.label },
                { "value", opt.value }
            });
        }
        a["options"] = opts;
        axes_json.push_back(a);
    }
    j["axes"] = axes_json;

    j["exportSettings"] = {
        { "outDir", export_settings_.out_dir },
        { "nameTemplate", export_settings_.name_template },
        { "exportPng", export_settings_.export_png },
        { "exportJsonSidecar", export_settings_.export_json_sidecar }
    };

    return j;
}

std::unique_ptr<RecipeLibrary> RecipeLibrary::from_json(const nlohmann::json& j) {
    auto lib = std::make_unique<RecipeLibrary>();
    lib->entries_.clear();

    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& item : j["entries"]) {
            auto entry = std::make_shared<RecipeEntry>();
            entry->hash = item.value("hash", generate_recipe_hash());
            entry->name = item.value("name", "Untitled");
            if (item.contains("recipe")) {
                entry->recipe = sanitize_recipe(item["recipe"]);
            }
            if (item.contains("tags") && item["tags"].is_object()) {
                for (auto it = item["tags"].begin(); it != item["tags"].end(); ++it) {
                    if (it.value().is_string()) {
                        entry->tags[it.key()] = it.value().get<std::string>();
                    }
                }
            }
            lib->entries_.push_back(entry);
        }
    }

    if (j.contains("axes") && j["axes"].is_array()) {
        for (const auto& item : j["axes"]) {
            VariantAxis axis;
            axis.name = item.value("name", "Axis");
            axis.type = static_cast<VariantAxisType>(item.value("type", 0));
            axis.enabled = item.value("enabled", true);
            if (item.contains("options") && item["options"].is_array()) {
                for (const auto& opt_item : item["options"]) {
                    VariantOption opt;
                    opt.label = opt_item.value("label", "");
                    opt.value = opt_item.value("value", nlohmann::json::object());
                    axis.options.push_back(opt);
                }
            }
            lib->axes_.push_back(axis);
        }
    }

    if (j.contains("exportSettings") && j["exportSettings"].is_object()) {
        const auto& es = j["exportSettings"];
        lib->export_settings_.out_dir = es.value("outDir", "./export");
        lib->export_settings_.name_template = es.value("nameTemplate", "{name}_{pattern}_{texA}");
        lib->export_settings_.export_png = es.value("exportPng", true);
        lib->export_settings_.export_json_sidecar = es.value("exportJsonSidecar", true);
    }

    if (lib->entries_.empty()) {
        auto def = std::make_shared<RecipeEntry>();
        def->hash = generate_recipe_hash();
        def->name = "Default Meadow";
        def->recipe = get_default_recipe();
        lib->entries_.push_back(def);
    }

    return lib;
}

bool RecipeLibrary::save_to_file(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << to_json().dump(2);
    return true;
}

std::unique_ptr<RecipeLibrary> RecipeLibrary::load_from_file(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return nullptr;
    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return nullptr;
    }
    return from_json(j);
}

} // namespace atm
