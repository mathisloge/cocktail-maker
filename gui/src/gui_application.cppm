module;
#include "app-window.h"

export module cm.gui:gui_application;
import cm;
import :dispenser_calibration_bridge;
import :recipe_context_bridge;
import :glass_context_bridge;
import :process_context_bridge;

namespace cm::gui {
export class GuiApplication : public Application
{

  public:
    GuiApplication();
    void init(const std::filesystem::path& db_dir) override;
    void run(std::unique_ptr<PodDiscovery> pod_discovery);

  private:
    slint::ComponentHandle<AppWindow> ui_ = cm::gui::AppWindow::create();
    DispenserCalibrationBridge dispenser_calibration_bridge_{get_executor(), ui_, pod_registry_};
    RecipeContextBridge recipe_context_bridge_{get_executor(), ui_, recipe_store_, ingredient_store_, station_config_};
    GlassContextBridge glass_context_bridge_{ui_};
    ProcessContextBridge process_context_bridge_{
        get_executor(), ui_, recipe_store_, ingredient_store_, station_config_, pod_registry_};
};

} // namespace cm::gui