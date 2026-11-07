module;

export module cm.gui:mix_error_page;

import std;
import cm;

namespace cm::gui {
export class AbortError : public std::runtime_error
{
  public:
    AbortError()
        : runtime_error{"Prozess wurde abgebrochen."} // codespell:ignore
    {
    }
};

export class RecipeNotFoundError : public std::runtime_error
{
  public:
    explicit RecipeNotFoundError(RecipeId recipe_id)
        : runtime_error{std::format("Recipe '{}' not found", recipe_id)}
        , recipe_id_{std::move(recipe_id)}
    {
    }

    RecipeId recipe_id() const
    {
        return recipe_id_;
    }

  private:
    RecipeId recipe_id_;
};

export enum class ErrorItemCategory
{
    Ingredient,
    Abort,
    Unknown
};

/// What the error page shows after a failed mix: a short text for the guest and the details for the staff.
export struct MixErrorPage
{
    std::string customer_title;
    std::string customer_message;
    std::string technical_title;
    std::string technical_description;
    ErrorItemCategory item_category = ErrorItemCategory::Unknown;
    std::string item_name;
    std::string item_details;
};

/// Words the error a mix failed with for the error page. An error it doesn't know gets a generic page with its details.
export [[nodiscard]] MixErrorPage mix_error_page(const std::exception_ptr& error,
                                                 const StationConfig& station_config,
                                                 const IngredientStore& ingredient_store);
} // namespace cm::gui
