#include "file_dialog.h"
#include "tinyfd/tinyfiledialogs.h"
#include <vector>

namespace fd {

std::optional<std::string> open_file(
    const std::string& title,
    const std::string& default_path,
    const std::vector<std::string>& filter_patterns,
    const std::string& filter_description
) {
    std::vector<const char*> patterns;
    for (const auto& p : filter_patterns) {
        patterns.push_back(p.c_str());
    }

    const char* res = tinyfd_openFileDialog(
        title.c_str(),
        default_path.empty() ? nullptr : default_path.c_str(),
        static_cast<int>(patterns.size()),
        patterns.empty() ? nullptr : patterns.data(),
        filter_description.empty() ? nullptr : filter_description.c_str(),
        0
    );

    if (res) {
        return std::string(res);
    }
    return std::nullopt;
}

std::optional<std::string> save_file(
    const std::string& title,
    const std::string& default_path,
    const std::vector<std::string>& filter_patterns,
    const std::string& filter_description
) {
    std::vector<const char*> patterns;
    for (const auto& p : filter_patterns) {
        patterns.push_back(p.c_str());
    }

    const char* res = tinyfd_saveFileDialog(
        title.c_str(),
        default_path.empty() ? nullptr : default_path.c_str(),
        static_cast<int>(patterns.size()),
        patterns.empty() ? nullptr : patterns.data(),
        filter_description.empty() ? nullptr : filter_description.c_str()
    );

    if (res) {
        return std::string(res);
    }
    return std::nullopt;
}

std::optional<std::string> select_folder(
    const std::string& title,
    const std::string& default_path
) {
    const char* res = tinyfd_selectFolderDialog(
        title.c_str(),
        default_path.empty() ? nullptr : default_path.c_str()
    );

    if (res) {
        return std::string(res);
    }
    return std::nullopt;
}

} // namespace fd
