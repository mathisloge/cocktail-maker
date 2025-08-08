#pragma once
#include <filesystem>
#include "units.hpp"

namespace cm
{
struct Glass
{
    std::string display_name;
    std::filesystem::path image_path;
    units::Litre capacity;
};
} // namespace cm
