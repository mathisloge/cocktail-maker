#include "cm/ui/RecipeFactory.hpp"
#include "cm/ui/RecipeDetail.hpp"

namespace cm::ui
{
RecipeFactory::RecipeFactory(std::shared_ptr<RecipeStore> recipe_store)
    : recipe_store_{std::move(recipe_store)}
{}

RecipeDetail *RecipeFactory::create(const QString &recipeName)
{
    auto it = recipe_store_->recipes().find(recipeName.toStdString());
    if (it == recipe_store_->recipes().cend())
    {
        return nullptr;
    }
    return new RecipeDetail{it->second}; // NOLINT(cppcoreguidelines-owning-memory)
}
} // namespace cm::ui
