module;
#include <slint.h>
#include "app-window.h"

export module cm.gui:station_state_bridge;

import std;
import cm;

namespace cm::gui {
export class IngredientModel : public slint::Model<gui::Ingredient>
{
};
} // namespace cm::gui
