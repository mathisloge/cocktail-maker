// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

namespace cm
{
class Recipe;
using RecipeMap = std::unordered_map<std::string, std::shared_ptr<Recipe>>;
class RecipeStore : public std::enable_shared_from_this<RecipeStore>
{
  public:
    void add_recipe(std::shared_ptr<Recipe> recipe);
    const RecipeMap &recipes() const;
    ~RecipeStore();

  private:
    RecipeMap recipes_;
};
} // namespace cm
