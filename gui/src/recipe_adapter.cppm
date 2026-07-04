module;
#include <slint.h>
#include "app-window.h"

export module cm.gui:recipe_adapter;
import std;
import mp_units;
import cm;
import cm.core;

namespace cm::gui {
export std::optional<Command> transform_command(const cm::Command& command, const cm::IngredientStore& ingredient_store)
{
    return std::visit(
        detail::Overloaded{[](const cm::ManualCommand& manual_command) -> std::optional<Command> {
                               return Command{
                                   .id = manual_command.id.raw(),
                                   .status = CommandStatus::NotStarted,
                                   .text = slint::SharedString{manual_command.instruction.c_str()},
                               };
                           },
                           [&ingredient_store](const cm::DispenseCommand& dispense_command) -> std::optional<Command> {
                               auto ingredient = ingredient_store.find_by_id(dispense_command.ingredient);
                               if (!ingredient) {
                                   return std::nullopt;
                               }

                               auto volume_str = std::format(
                                   "{}", units::value_cast<std::int32_t>(dispense_command.volume.in(units::milli_litre)));

                               return Command{
                                   .id = dispense_command.id.raw(),
                                   .status = CommandStatus::NotStarted,
                                   .text = slint::SharedString{ingredient->display_name.c_str()},
                                   .value = slint::SharedString{volume_str.c_str()},
                               };
                           },
                           [](auto&&) -> std::optional<Command> { return std::nullopt; }},
        command);
}

export std::shared_ptr<slint::Model<Command>> transform(const cm::Commands& commands, const cm::IngredientStore& ingredient_store)
{
    auto model = std::make_shared<slint::VectorModel<Command>>();

    const auto push_if_valid = [&](const cm::Command& c) {
        if (auto transformed = transform_command(c, ingredient_store)) {
            model->push_back(*transformed);
        }
    };

    for (auto&& c : commands) {
        std::visit(detail::Overloaded{[&](const cm::Command& command) { push_if_valid(command); },
                                      [&](const cm::ParallelCommand& commands) {
                                          for (auto&& c : commands) {
                                              push_if_valid(c);
                                          }
                                      }},
                   c);
    }

    return model;
}

export RecipeView transform(Recipe r, const cm::IngredientStore& ingredient_store)
{
    return RecipeView{
        .id = slint::SharedString{r.id.raw().c_str()},
        .name = slint::SharedString{r.display_name.c_str()},
        .tag_line = std::make_shared<slint::VectorModel<slint::SharedString>>(
            std::ranges::to<std::vector>(std::move(r.tags) | std::views::transform([](std::string tag) {
                                             std::ranges::transform(tag, tag.begin(), ::toupper);
                                             return slint::SharedString{tag.c_str()};
                                         }))),
        .description = slint::SharedString{r.description.c_str()},
        .image = slint::Image::load_from_path(r.image_path.c_str()),
        .commands = transform(r.commands, ingredient_store),
    };
}

export class RecipeModel : public slint::Model<RecipeView>
{
  public:
    explicit RecipeModel(const cm::RecipeStore& recipe_store, const cm::IngredientStore& ingredient_store)
        : recipe_store_{recipe_store}
        , ingredient_store_{ingredient_store}
    {
    }

    size_t row_count() const override
    {
        return recipe_store_.recipe_count();
    }

    std::optional<RecipeView> row_data(size_t row) const override
    {
        return recipe_store_
            .find_by_index(row) //
            .transform([this](auto&& r) { return transform(r, ingredient_store_); });
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
    const cm::RecipeStore& recipe_store_;
    const cm::IngredientStore& ingredient_store_;
};
} // namespace cm::gui
