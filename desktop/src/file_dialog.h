#pragma once

#include <string>
#include <vector>
#include <optional>

namespace fd {

std::optional<std::string> open_file(
    const std::string& title,
    const std::string& default_path = "",
    const std::vector<std::string>& filter_patterns = {},
    const std::string& filter_description = ""
);

std::optional<std::string> save_file(
    const std::string& title,
    const std::string& default_path = "",
    const std::vector<std::string>& filter_patterns = {},
    const std::string& filter_description = ""
);

std::optional<std::string> select_folder(
    const std::string& title,
    const std::string& default_path = ""
);

} // namespace fd
