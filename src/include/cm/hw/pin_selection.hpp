#pragma once
#include <filesystem>
#include <gpiod.hpp>

namespace cm
{
template <typename T>
struct PinSelection
{
    std::filesystem::path chip;
    gpiod::line::offset offset;
};
} // namespace cm
