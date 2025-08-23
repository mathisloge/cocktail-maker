// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <filesystem>
#include <fmt/core.h>
#include <memory>
#include <vector>
#include "units.hpp"

namespace cm {
class Command;
class Recipe;

using ProductionStep = std::vector<std::unique_ptr<Command>>;
using ProductionSteps = std::vector<ProductionStep>;

class RecipeBuilder
{
  public:
    class StepBuilder
    {
        RecipeBuilder& parent_;
        ProductionStep steps_;

      public:
        StepBuilder(RecipeBuilder& parent);
        StepBuilder& with_step(std::unique_ptr<Command> command);
        RecipeBuilder& add();
    };

    RecipeBuilder& with_nominal_serving_volume(units::Litre litre);
    RecipeBuilder& with_name(std::string name);
    RecipeBuilder& with_description(std::string description);
    RecipeBuilder& with_image(std::filesystem::path image_path);
    StepBuilder with_steps();
    std::shared_ptr<Recipe> create();

  private:
    std::string name_;
    std::string description_;
    units::Litre nominal_serving_volume_;
    std::filesystem::path image_path_;
    ProductionSteps steps_;
};

RecipeBuilder make_recipe();

class Recipe : std::enable_shared_from_this<Recipe>
{
  public:
    Recipe(std::string name,
           ProductionSteps steps,
           std::string description,
           std::filesystem::path image_path,
           units::Litre nominal_serving_volume);
    ~Recipe();

    const std::string& name() const;
    const std::string& description() const;
    const std::filesystem::path& image_path() const;
    const ProductionSteps& production_steps() const;
    units::Litre nominal_serving_volume() const;
    std::shared_ptr<Recipe> scaled_to(units::Litre target_volume) const;

  private:
    std::string name_;
    std::string description_;
    std::filesystem::path image_path_;
    units::Litre nominal_serving_volume_; // reference serving volume (e.g. glass size)
    ProductionSteps steps_;

    friend struct fmt::formatter<Recipe>;
};
} // namespace cm

template <>
struct fmt::formatter<cm::Recipe> : formatter<string_view>
{
    auto format(const cm::Recipe& r, format_context& ctx) const -> format_context::iterator;
};
