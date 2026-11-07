module;
#include <simdjson.h>

// An internal partition: the catalog loaders share it, and lib.cppm doesn't re-export it.
module cm:catalog_loader;

import std;

namespace cm {
/**
 * Parses every `.json` file directly in `dir` with `parse(document, path)`, in the order of the file names.
 *
 * @throws std::runtime_error naming the file, if a file can't be read or parsed.
 * @throws std::filesystem::filesystem_error if `dir` doesn't exist.
 */
template <typename T, typename Parse>
std::vector<T> load_json_catalog(const std::filesystem::path& dir, const std::string_view kind, Parse parse)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    // The order of a directory listing isn't specified, and the UI shows the catalog in this order.
    std::ranges::sort(files);

    std::vector<T> items;
    items.reserve(files.size());
    simdjson::ondemand::parser parser;
    for (const auto& file : files) {
        try {
            const simdjson::padded_string json = simdjson::padded_string::load(file.string());
            simdjson::ondemand::document document = parser.iterate(json);
            items.push_back(parse(document, file));
        }
        catch (const std::exception& error) {
            throw std::runtime_error{std::format("Could not load {} '{}': {}", kind, file.string(), error.what())};
        }
    }
    return items;
}
} // namespace cm
