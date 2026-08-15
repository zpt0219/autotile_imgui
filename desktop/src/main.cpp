#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "model/recipe.h"
#include "model/recipe_library.h"
#include "pattern/sheet.h"
#include "codec/recipe_codec.h"
#include "app.h"

namespace fs = std::filesystem;

static atm::RGB parse_color_value(const nlohmann::json& j) {
    if (j.is_string()) {
        return atm::parse_hex_colour(j.get<std::string>());
    } else if (j.is_object()) {
        uint8_t r = j.value("r", 0);
        uint8_t g = j.value("g", 0);
        uint8_t b = j.value("b", 0);
        return { r, g, b };
    }
    return { 0, 0, 0 };
}

static atm::PaintOverrides parse_overrides(const nlohmann::json& j) {
    atm::PaintOverrides ov;
    if (!j.is_object()) return ov;

    if (j.contains("noiseTargets") && j["noiseTargets"].is_array()) {
        std::vector<atm::NoiseTargetId> targets;
        for (const auto& item : j["noiseTargets"]) {
            if (item.is_string()) {
                targets.push_back(atm::parse_noise_target_id(item.get<std::string>()));
            }
        }
        ov.noise_targets = targets;
    }

    if (j.contains("noiseColours") && j["noiseColours"].is_object()) {
        const auto& nc = j["noiseColours"];
        if (nc.contains("b")) {
            ov.noise_colours.b = parse_color_value(nc["b"]);
        }
        if (nc.contains("edge")) {
            ov.noise_colours.edge = parse_color_value(nc["edge"]);
        }
        if (nc.contains("a")) {
            ov.noise_colours.a = parse_color_value(nc["a"]);
        }
    }

    return ov;
}

static int run_render_corpus(const std::string& manifest_path_str, const std::string& out_dir_str) {
    fs::path manifest_path(manifest_path_str);
    if (!fs::exists(manifest_path)) {
        std::cerr << "Error: manifest path does not exist: " << manifest_path_str << std::endl;
        return 1;
    }

    fs::path out_dir(out_dir_str);
    fs::create_directories(out_dir);

    std::ifstream manifest_file(manifest_path);
    if (!manifest_file.is_open()) {
        std::cerr << "Error: could not open manifest: " << manifest_path_str << std::endl;
        return 1;
    }

    nlohmann::json manifest_json;
    try {
        manifest_file >> manifest_json;
    } catch (const std::exception& e) {
        std::cerr << "Error: failed to parse manifest json: " << e.what() << std::endl;
        return 1;
    }

    fs::path base_dir = manifest_path.parent_path();
    const auto& cases = manifest_json["cases"];

    std::cout << "Rendering " << cases.size() << " cases to " << out_dir.string() << "..." << std::endl;

    for (const auto& c : cases) {
        std::string cid = c["id"].get<std::string>();
        std::string recipe_rel = c["recipe"].get<std::string>();
        fs::path recipe_path = base_dir / recipe_rel;

        std::ifstream recipe_file(recipe_path);
        if (!recipe_file.is_open()) {
            std::cerr << "Warning: could not open recipe: " << recipe_path.string() << std::endl;
            continue;
        }

        nlohmann::json case_obj;
        recipe_file >> case_obj;

        atm::Recipe recipe = atm::sanitize_recipe(case_obj.value("recipe", nlohmann::json::object()));
        atm::PaintOverrides overrides;
        if (case_obj.contains("overrides")) {
            overrides = parse_overrides(case_obj["overrides"]);
        }

        // Render level grid
        std::string lvl = atm::render_level_grid(recipe, overrides);
        fs::path lvl_out = out_dir / (cid + ".lvl");
        std::ofstream lvl_file(lvl_out, std::ios::binary);
        lvl_file.write(lvl.data(), lvl.size());

        // Render RGBA sheet
        std::vector<uint8_t> rgba = atm::render_sheet_rgba(recipe, overrides);
        fs::path rgba_out = out_dir / (cid + ".rgba");
        std::ofstream rgba_file(rgba_out, std::ios::binary);
        rgba_file.write(reinterpret_cast<const char*>(rgba.data()), rgba.size());
    }

    std::cout << "Done rendering corpus." << std::endl;
    return 0;
}

static int run_headless() {
    std::cout << R"({"status":"ready","version":"1.0.0"})" << std::endl;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            std::string cmd = j.value("cmd", "");
            if (cmd == "ping") {
                std::cout << R"({"status":"ok","pong":true})" << std::endl;
            } else if (cmd == "encode") {
                atm::Recipe r = atm::sanitize_recipe(j.value("recipe", nlohmann::json::object()));
                std::string code = atm::encode_recipe(r);
                std::cout << nlohmann::json{ { "status", "ok" }, { "code", code } }.dump() << std::endl;
            } else if (cmd == "decode") {
                std::string code = j.value("code", "");
                auto r = atm::decode_recipe(code);
                if (r.has_value()) {
                    std::cout << nlohmann::json{ { "status", "ok" }, { "recipe", recipe_to_json(*r) } }.dump() << std::endl;
                } else {
                    std::cout << R"({"status":"error","message":"Invalid share code"})" << std::endl;
                }
            } else if (cmd == "exit" || cmd == "quit") {
                break;
            } else {
                std::cout << R"({"status":"error","message":"Unknown command"})" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << nlohmann::json{ { "status", "error" }, { "message", e.what() } }.dump() << std::endl;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    std::string startup_library;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--library") {
            if (i + 1 < argc) {
                startup_library = argv[++i];
                continue;
            }
            std::cerr << "Error: --library requires a path" << std::endl;
            return 1;
        } else if (arg == "--render-corpus") {
            if (i + 1 < argc) {
                std::string manifest = argv[++i];
                std::string out_dir = ".";
                if (i + 1 < argc && std::string(argv[i + 1]) == "--out") {
                    i += 2;
                    if (i < argc) {
                        out_dir = argv[i];
                    }
                }
                return run_render_corpus(manifest, out_dir);
            } else {
                std::cerr << "Error: --render-corpus requires a manifest.json path" << std::endl;
                return 1;
            }
        } else if (arg == "--headless") {
            return run_headless();
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "AutoTile Mixer Desktop (Native C++ / ImGui)" << std::endl;
            std::cout << "Usage:" << std::endl;
            std::cout << "  autotile_mixer                      Launch desktop GUI" << std::endl;
            std::cout << "  autotile_mixer --render-corpus <m>  Render corpus sheets" << std::endl;
            std::cout << "  autotile_mixer --headless           Start headless JSON stdin/stdout mode" << std::endl;
            std::cout << "  autotile_mixer --library <file>     Launch GUI with a .atmlib already open" << std::endl;
            return 0;
        }
    }

    // Launch Desktop GUI Application
    atm_desktop::App app;
    if (!app.initialize()) {
        std::cerr << "Failed to initialize desktop application." << std::endl;
        return 1;
    }

    if (!startup_library.empty()) {
        app.load_library(startup_library);
    }

    app.run();
    return 0;
}
