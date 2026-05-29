module;
#include <slint.h>
#include "app-window.h"

export module cm.gui:recipe_adapter;
import std;
import cm;

namespace cm::gui {
export class RecipeModel : public slint::Model<RecipeView>
{
  public:
    explicit RecipeModel(std::vector<Recipe> recipes)
    {
        for (auto&& r : recipes) {
            recipes_.emplace_back(RecipeView{
                .name = slint::SharedString{r.display_name.c_str()},
                .tag_line = std::make_shared<slint::VectorModel<slint::SharedString>>(
                    std::ranges::to<std::vector>(std::move(r.tags) | std::views::transform([](std::string tag) {
                                                     std::ranges::transform(tag, tag.begin(), ::toupper);
                                                     return slint::SharedString{tag.c_str()};
                                                 }))),
                .image = slint::Image::load_from_path(r.image_path.c_str()),
            });
        }
    }

    size_t row_count() const override
    {
        return recipes_.size();
    }

    std::optional<RecipeView> row_data(size_t row) const override
    {
        if (row >= recipes_.size()) {
            return std::nullopt;
        }
        const auto& p = recipes_[row];
        return p;
    }

    // Aufruf wenn sich etwas im Repository ändert:
    void item_added(size_t index)
    {
        notify_row_added(index, 1);
    }

    void item_changed(size_t index)
    {
        notify_row_changed(index);
    }

    void item_removed(size_t index)
    {
        notify_row_removed(index, 1);
    }

  private:
    std::vector<RecipeView> recipes_;
};
} // namespace cm::gui