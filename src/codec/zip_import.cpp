#include "zip_import.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "miniz/miniz.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace atm {

/** Frees a `mz_zip_reader_extract_file_to_heap` buffer on every exit path. */
class MinizBuffer {
public:
    explicit MinizBuffer(void* p) : p_(p) {}
    ~MinizBuffer() { if (p_) mz_free(p_); }
    MinizBuffer(const MinizBuffer&) = delete;
    MinizBuffer& operator=(const MinizBuffer&) = delete;

private:
    void* p_;
};

static ZipImportResult parse_zip_archive(mz_zip_archive* zip_archive) {
    ZipImportResult result;
    mz_uint num_files = mz_zip_reader_get_num_files(zip_archive);

    if (num_files == 0) {
        result.errors.push_back("ZIP archive is empty");
        return result;
    }

    for (mz_uint i = 0; i < num_files; ++i) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip_archive, i, &file_stat)) {
            continue;
        }

        if (mz_zip_reader_is_file_a_directory(zip_archive, i)) {
            continue;
        }

        std::string filename = file_stat.m_filename;
        std::string lower_fn = filename;
        std::transform(lower_fn.begin(), lower_fn.end(), lower_fn.begin(), ::tolower);

        if (lower_fn.length() >= 5 && lower_fn.substr(lower_fn.length() - 5) == ".json") {
            size_t file_size = 0;
            void* p = mz_zip_reader_extract_file_to_heap(zip_archive, file_stat.m_filename, &file_size, 0);
            if (!p) {
                result.errors.push_back("Failed to extract: " + filename);
                continue;
            }

            // Owns `p` for the rest of this iteration. Freeing by hand here is
            // how a double free crept in once: the buffer was released at the
            // top of the try and again in the catch, so any malformed sidecar
            // freed it twice. The destructor runs exactly once on every path.
            MinizBuffer buf(p);

            try {
                std::string json_text(reinterpret_cast<const char*>(p), file_size);

                auto j = nlohmann::json::parse(json_text);
                RecipeEntry entry;

                // Determine name
                std::string base_name = filename;
                size_t slash_pos = base_name.find_last_of("/\\");
                if (slash_pos != std::string::npos) {
                    base_name = base_name.substr(slash_pos + 1);
                }
                if (base_name.length() > 5 && base_name.substr(base_name.length() - 5) == ".json") {
                    base_name = base_name.substr(0, base_name.length() - 5);
                }
                if (base_name.length() > 7 && base_name.substr(base_name.length() - 7) == ".recipe") {
                    base_name = base_name.substr(0, base_name.length() - 7);
                }

                entry.name = j.value("name", base_name);
                entry.hash = j.value("hash", generate_recipe_hash());

                if (j.contains("recipe") && j["recipe"].is_object()) {
                    entry.recipe = sanitize_recipe(j["recipe"]);
                } else {
                    entry.recipe = sanitize_recipe(j);
                }

                if (j.contains("tags") && j["tags"].is_object()) {
                    for (auto it = j["tags"].begin(); it != j["tags"].end(); ++it) {
                        if (it.value().is_string()) {
                            entry.tags[it.key()] = it.value().get<std::string>();
                        }
                    }
                }

                result.entries.push_back(entry);
            } catch (const std::exception& e) {
                result.errors.push_back("Error parsing " + filename + ": " + e.what());
            }
        }
    }

    result.success = !result.entries.empty();
    if (result.entries.empty() && result.errors.empty()) {
        result.errors.push_back("No recipe JSON sidecars found in ZIP archive");
    }

    return result;
}

ZipImportResult import_recipes_from_zip(const std::string& zip_path) {
    ZipImportResult result;
    mz_zip_archive zip_archive;
    std::memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, zip_path.c_str(), 0)) {
        result.errors.push_back("Failed to open ZIP file: " + zip_path);
        return result;
    }

    result = parse_zip_archive(&zip_archive);
    mz_zip_reader_end(&zip_archive);
    return result;
}

ZipImportResult import_recipes_from_zip_memory(const uint8_t* data, size_t size) {
    ZipImportResult result;
    if (!data || size == 0) {
        result.errors.push_back("Empty data buffer for ZIP");
        return result;
    }

    mz_zip_archive zip_archive;
    std::memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_mem(&zip_archive, data, size, 0)) {
        result.errors.push_back("Failed to read ZIP from memory buffer");
        return result;
    }

    result = parse_zip_archive(&zip_archive);
    mz_zip_reader_end(&zip_archive);
    return result;
}

} // namespace atm
