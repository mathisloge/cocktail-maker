#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/when_any.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <slint.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm.testing;
import cm;
import cm.gui;
import cm.sim;

#include "slint_events.test.hpp"

using namespace std::chrono_literals;
namespace units = cm::units;

namespace {
class TestPodState final : public cm::PodState
{
  public:
    void update_info(cm::PodInfo /*info*/) override
    {
    }

    void update_state(ConnectionState /*state*/) override
    {
    }
};

/// A pod that refuses every pour and counts how often it was stopped.
class RefusingPod final : public cm::IPod, public std::enable_shared_from_this<RefusingPod>
{
  public:
    cm::PodId pod_id() const override
    {
        return cm::PodId{"Refusing"};
    }

    cm::Task<void> run(std::unique_ptr<cm::PodState> /*state*/) override
    {
        co_return;
    }

    std::expected<std::unique_ptr<cm::Dispenser>, cm::DispenserNotFoundError> create_dispenser(cm::DispenserId id) override
    {
        return std::make_unique<cm::Pump>(weak_from_this(), pod_id(), id);
    }

    cm::Task<void> highlight_dispenser(cm::DispenserId /*id*/, std::chrono::milliseconds /*duration*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_calibrate_with_ref_weight(cm::DispenserId /*id*/, units::Grams /*grams*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_tare(cm::DispenserId /*id*/) override
    {
        co_return;
    }

    cm::Task<units::Grams> load_cell_weight(cm::DispenserId /*id*/) override
    {
        co_return 0.0 * units::si::gram;
    }

    cm::Task<cm::DispenseReport> dispense(cm::DispenserId /*id*/, units::Litre /*volume*/) override
    {
        throw cm::PodReceiveError{pod_id(), proto::field::ErrorCodeVal::HardwareFault};
        co_return cm::DispenseReport{};
    }

    cm::Task<units::Litre> dispenser_calibrate(cm::DispenserId /*id*/, std::chrono::milliseconds /*run_time*/) override
    {
        co_return 0.0 * units::milli_litre;
    }

    cm::Task<void> force_safe_state() override
    {
        ++stops;
        if (stop_times_out) {
            throw cm::PodTimeoutError{pod_id()};
        }
        co_return;
    }

    int stops = 0;
    bool stop_times_out = false;
};

cm::sim::ClientConfig fast_config()
{
    return cm::sim::ClientConfig{
        .load_cell_action = 20ms,
        .ack_delay = 5ms,
        .quick_reply = 1ms,
        .settle = 5ms,
        .stall = 30ms,
        .flow = 1000.0 * units::milli_litre / units::si::second,
    };
}

/// The Station's parts around a real Pod that talks to a simulated client, all on the calling thread.
struct SimStation
{
    slint::ComponentHandle<cm::gui::AppWindow> ui = cm::gui::AppWindow::create();
    boost::asio::io_context ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard = boost::asio::make_work_guard(ctx);
    cm::AsyncScope scope{ctx};
    cm::IngredientStore ingredients;
    cm::RecipeStore recipes;
    cm::PodRegistry registry;
    // Declared before the config and the tracker, so that it outlives what writes into it.
    cm::testing::TempDir dir{"cm-station-flows-test"};
    cm::StationConfig config{ingredients, dir.file("mapping.json")};
    cm::FillLevelTracker fill_levels{
        ctx.get_executor(), dir.file("levels.json"), cm::FillLevelOptions{.refill_settle_time = 1ms}};
    std::shared_ptr<cm::sim::Client> client;
    std::shared_ptr<cm::Pod> pod;

    const cm::IngredientId rum{"rum"};
    const cm::IngredientId lime{"lime"};
    const cm::PodDispenser rum_address{cm::PodId{"SimPod"}, cm::DispenserId{0}};
    const cm::PodDispenser lime_address{cm::PodId{"SimPod"}, cm::DispenserId{1}};

    explicit SimStation(cm::sim::ClientConfig client_config = fast_config())
        : client{std::make_shared<cm::sim::Client>(scope, "SimPod", cm::Version{.major = 2}, std::move(client_config))}
    {
        ingredients.init_ingredients({
            cm::Ingredient{.id = rum, .display_name = "Rum", .type = {}, .boost_category = cm::BoostCategory::fixed},
            cm::Ingredient{.id = lime, .display_name = "Limette", .type = {}, .boost_category = cm::BoostCategory::fixed},
        });
        config.update_dispenser_ingredient_mapping(rum, rum_address);
        config.update_dispenser_ingredient_mapping(lime, lime_address);

        pod = std::make_shared<cm::Pod>(cm::sim::open_station_stream(*client));
        registry.register_pod(pod);

        scope.spawn(client->run());
        scope.spawn(pod->run(std::make_unique<TestPodState>()));
        REQUIRE(
            pump_until([this]() { return registry.dispenser_of_pod(rum_address.pod_id, rum_address.dispenser_id).has_value(); }));
        pump_for(100ms);
    }

    ~SimStation()
    {
        stop();
    }

    /// Ends all work, as `Application::shutdown` does. A station with a bridge has to call it before the bridge is destroyed.
    void stop()
    {
        if (stopped) {
            return;
        }
        stopped = true;
        registry.unregister_pod(pod);
        cm::run_until_complete(ctx, scope.stop_and_join());
        ctx.restart();
        ctx.poll();
    }

    bool stopped = false;

    void pump_once()
    {
        ctx.poll();
        ctx.restart();
        flush_slint_events();
    }

    void pump_for(std::chrono::milliseconds duration)
    {
        const auto end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < end) {
            pump_once();
            std::this_thread::sleep_for(1ms);
        }
    }

    bool pump_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout = 5s)
    {
        const auto end = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < end) {
            pump_once();
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    bool popup_open() const
    {
        return ui->get_manual_command_popup_open();
    }

    std::string popup_text() const
    {
        return std::string{ui->get_popup_command().text.data()};
    }

    cm::DispenseCommand pour_command(cm::IngredientId ingredient, double millilitres)
    {
        return cm::DispenseCommand{cm::CommandId{1}, std::move(ingredient), millilitres * units::milli_litre};
    }

    std::shared_ptr<cm::gui::MachineAdapter> make_adapter()
    {
        return std::make_shared<cm::gui::MachineAdapter>(ui, ctx.get_executor(), ingredients, registry, config, fill_levels);
    }

    /// Starts the pour in the scope and sets `done` when it has finished.
    void start_pour(std::shared_ptr<cm::gui::MachineAdapter> adapter, cm::DispenseCommand command, bool& done)
    {
        const auto run = [](std::shared_ptr<cm::gui::MachineAdapter> a, cm::DispenseCommand c, bool& d) -> cm::Task<void> {
            co_await a->execute_command(std::move(c));
            d = true;
        };
        scope.spawn(run(std::move(adapter), std::move(command), done));
    }
};
} // namespace

TEST_CASE_METHOD(SimStation, "A mix pours the rest after a refill", "[flow]")
{
    auto& bottle = client->bottle(rum_address.dispenser_id);
    bottle.liquid = 30 * units::si::gram;
    client->power_cycle_load_cells();
    fill_levels.empty_bottle_tared(rum_address);
    auto adapter = make_adapter();
    bool done = false;

    start_pour(adapter, pour_command(rum, 100), done);

    REQUIRE(pump_until([this]() { return popup_open(); }));
    CHECK(popup_text() == "Rum nachfüllen. Bitte Personal rufen.");
    CHECK(fill_levels.record(rum_address)->suspected_empty);
    CHECK_FALSE(done);

    bottle.liquid = 500 * units::si::gram;
    ui->invoke_manual_command_confirmed();

    REQUIRE(pump_until([&done]() { return done; }));
    CHECK(bottle.liquid == 430 * units::si::gram);
    CHECK_FALSE(popup_open());
    CHECK_FALSE(fill_levels.record(rum_address)->suspected_empty);
    CHECK(fill_levels.record(rum_address)->capacity.has_value());
}

TEST_CASE_METHOD(SimStation, "Confirming without a refill asks again and leaves the level alone", "[flow]")
{
    auto& bottle = client->bottle(rum_address.dispenser_id);
    bottle.liquid = 30 * units::si::gram;
    client->power_cycle_load_cells();

    fill_levels.empty_bottle_tared(rum_address);
    auto adapter = make_adapter();
    bool done = false;
    start_pour(adapter, pour_command(rum, 100), done);

    REQUIRE(pump_until([this]() { return popup_open(); }));
    const auto before = fill_levels.record(rum_address);
    REQUIRE(before.has_value());
    CHECK(before->suspected_empty);

    ui->invoke_manual_command_confirmed();
    REQUIRE(pump_until([this]() { return !popup_open(); }));
    REQUIRE(pump_until([this]() { return popup_open(); }));

    CHECK(fill_levels.record(rum_address) == before);
    CHECK_FALSE(done);

    bottle.liquid = 500 * units::si::gram;
    ui->invoke_manual_command_confirmed();
    REQUIRE(pump_until([&done]() { return done; }));
}

TEST_CASE_METHOD(SimStation, "Two bottles running dry at once get one dialog after the other", "[flow]")
{
    auto& rum_bottle = client->bottle(rum_address.dispenser_id);
    auto& lime_bottle = client->bottle(lime_address.dispenser_id);
    rum_bottle.liquid = 0 * units::si::gram;
    lime_bottle.liquid = 0 * units::si::gram;

    auto adapter = make_adapter();
    bool first_done = false;
    bool second_done = false;
    start_pour(adapter, pour_command(rum, 10), first_done);
    start_pour(adapter, pour_command(lime, 10), second_done);

    REQUIRE(pump_until([this]() { return popup_open(); }));
    const auto first_text = popup_text();
    pump_for(100ms);
    CHECK(popup_text() == first_text);

    rum_bottle.liquid = 500 * units::si::gram;
    lime_bottle.liquid = 500 * units::si::gram;
    ui->invoke_manual_command_confirmed();

    REQUIRE(pump_until([this, &first_text]() { return popup_open() && popup_text() != first_text; }));
    ui->invoke_manual_command_confirmed();

    REQUIRE(pump_until([&]() { return first_done && second_done; }));
    CHECK_FALSE(popup_open());
}

TEST_CASE_METHOD(SimStation, "An abort ends both waiting prompts and frees the dialog again", "[flow]")
{
    auto& rum_bottle = client->bottle(rum_address.dispenser_id);
    auto& lime_bottle = client->bottle(lime_address.dispenser_id);
    rum_bottle.liquid = 0 * units::si::gram;
    lime_bottle.liquid = 0 * units::si::gram;

    auto adapter = make_adapter();
    cm::AwaitableBool abort{ctx.get_executor()};
    bool finished = false;
    const auto run = [&]() -> cm::Task<void> {
        std::vector<cm::Task<void>> pours;
        pours.emplace_back(adapter->execute_command(pour_command(rum, 10)));
        pours.emplace_back(adapter->execute_command(pour_command(lime, 10)));
        co_await exec::when_any(cm::join_all(std::move(pours)), abort.async_wait());
        finished = true;
    };
    scope.spawn(run());

    REQUIRE(pump_until([this]() { return popup_open(); }));
    pump_for(100ms);
    abort = true;

    REQUIRE(pump_until([&finished]() { return finished; }));
    REQUIRE(pump_until([this]() { return !popup_open(); }));

    bool third_done = false;
    start_pour(adapter, pour_command(rum, 10), third_done);
    REQUIRE(pump_until([this]() { return popup_open(); }));
    rum_bottle.liquid = 500 * units::si::gram;
    ui->invoke_manual_command_confirmed();
    REQUIRE(pump_until([&third_done]() { return third_done; }));
}

TEST_CASE_METHOD(SimStation, "The admin row shows the level and takes a refill", "[flow]")
{
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, registry, scope, fill_levels);
    auto pod_state = bridge->create_pod_state();
    pod_state->update_info(cm::PodInfo{.id = rum_address.pod_id, .firmware_version = {}, .num_pumps = 2, .num_valves = 0});
    pod_state->update_state(cm::PodState::ConnectionState::connected);
    pump_for(10ms);

    const auto row_text = [&bridge]() -> std::string {
        const auto gui_pod = bridge->pod_model()->row_data(0);
        if (not gui_pod.has_value()) {
            return {};
        }
        const auto dispenser = gui_pod->dispensers->row_data(0);
        return dispenser.has_value() ? std::string{dispenser->fill_text.data()} : std::string{};
    };
    REQUIRE(row_text() == "unbekannt");

    fill_levels.empty_bottle_tared(rum_address);
    REQUIRE(pump_until([&row_text]() { return row_text() == "ca. 0 mL"; }));

    client->bottle(rum_address.dispenser_id).liquid += 300 * units::si::gram;
    cm::gui::Pod gui_pod;
    gui_pod.id = slint::SharedString{rum_address.pod_id.raw()};
    cm::gui::Dispenser gui_dispenser;
    gui_dispenser.id = rum_address.dispenser_id.raw();
    ui->global<cm::gui::StationStateContext>().invoke_mark_dispenser_refilled(gui_pod, gui_dispenser);

    CHECK(pump_until([&row_text]() { return row_text() == "ca. 300 mL · 100 %"; }));

    pod_state.reset();
    pump_for(10ms);
}

namespace {
/// A simulated station with the bridge that starts a mix, and one recipe that needs 40 mL of rum.
struct WarningStation : SimStation
{
    cm::gui::ProcessContextBridge bridge{scope, ui, recipes, ingredients, config, registry, fill_levels};
    std::optional<cm::gui::Page> last_page;

    explicit WarningStation(cm::sim::ClientConfig client_config = fast_config())
        : SimStation{std::move(client_config)}
    {
        use_recipe("drink", rum);
        bridge.init();
        ui->global<cm::gui::StationStateContext>().on_navigate_to([this](cm::gui::Page page) {
            const auto previous = last_page.value_or(cm::gui::Page::MixGlassSelectionPage);
            last_page = page;
            ui->global<cm::gui::StationStateContext>().invoke_navigated_to(previous, page);
        });
        ui->global<cm::gui::ProcessContext>().set_boost(0);
        ui->global<cm::gui::ProcessContext>().set_glass_volume(250);
    }

    ~WarningStation()
    {
        stop();
    }

    void use_recipe(const std::string& id, const cm::IngredientId& ingredient)
    {
        recipes.init_recipes({cm::Recipe{
            .id = cm::RecipeId{id},
            .display_name = id,
            .description = "",
            .tags = {},
            .image_path = "",
            .nominal_serving_volume = 250 * units::milli_litre,
            .commands = {cm::Command{cm::DispenseCommand{cm::CommandId{1}, ingredient, 40 * units::milli_litre}}},
        }});

        auto active = ui->global<cm::gui::RecipeContext>().get_active_recipe();
        active.id = slint::SharedString{id};
        ui->global<cm::gui::RecipeContext>().set_active_recipe(active);
    }

    void confirm_glass()
    {
        ui->global<cm::gui::ProcessContext>().invoke_process_active_recipe();
    }

    bool warning_open() const
    {
        return ui->get_fill_warning_popup_open();
    }

    std::vector<std::string> warning_lines() const
    {
        std::vector<std::string> lines;
        const auto model = ui->global<cm::gui::ProcessContext>().get_fill_warning_lines();
        for (std::size_t row = 0; row < model->row_count(); ++row) {
            lines.emplace_back(model->row_data(row)->data());
        }
        return lines;
    }

    void fill_rum_to(double grams)
    {
        fill_levels.empty_bottle_tared(rum_address);
        client->bottle(rum_address.dispenser_id).liquid += grams * units::si::gram;
    }
};
} // namespace

TEST_CASE_METHOD(WarningStation, "A drink that fits starts at once", "[flow][warning]")
{
    fill_rum_to(200);

    confirm_glass();

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixPage; }));
    CHECK_FALSE(warning_open());
}

TEST_CASE_METHOD(WarningStation, "An unknown level starts at once", "[flow][warning]")
{
    confirm_glass();

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixPage; }));
    CHECK_FALSE(warning_open());
}

TEST_CASE_METHOD(WarningStation, "A drink that may not fit warns and can be cancelled", "[flow][warning]")
{
    fill_rum_to(15);

    confirm_glass();

    REQUIRE(pump_until([this]() { return warning_open(); }));
    CHECK(warning_lines() == std::vector<std::string>{"Rum: ca. 15 mL da, 40 mL nötig"});
    CHECK_FALSE(last_page.has_value());

    ui->global<cm::gui::ProcessContext>().invoke_fill_warning_canceled();

    REQUIRE(pump_until([this]() { return !warning_open(); }));
    pump_for(50ms);
    CHECK_FALSE(last_page.has_value());
}

TEST_CASE_METHOD(WarningStation, "A warned drink can be started anyway", "[flow][warning]")
{
    fill_rum_to(15);

    confirm_glass();
    REQUIRE(pump_until([this]() { return warning_open(); }));

    ui->global<cm::gui::ProcessContext>().invoke_fill_warning_confirmed();

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixPage; }));
    CHECK_FALSE(warning_open());
}

TEST_CASE_METHOD(WarningStation, "A bottle topped up without the button gives no warning", "[flow][warning]")
{
    fill_rum_to(15);
    confirm_glass();
    REQUIRE(pump_until([this]() { return warning_open(); }));
    ui->global<cm::gui::ProcessContext>().invoke_fill_warning_canceled();
    REQUIRE(pump_until([this]() { return !warning_open(); }));

    client->bottle(rum_address.dispenser_id).liquid += 300 * units::si::gram;
    confirm_glass();

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixPage; }));
    CHECK_FALSE(warning_open());
}

TEST_CASE_METHOD(WarningStation, "A pod that is gone gives no warning and the mix fails as before", "[flow][warning]")
{
    config.update_dispenser_ingredient_mapping(lime, cm::PodDispenser{cm::PodId{"Gone"}, cm::DispenserId{0}});
    use_recipe("lime_drink", lime);

    confirm_glass();

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixErrorPage; }));
    CHECK_FALSE(warning_open());
}

TEST_CASE_METHOD(WarningStation, "Abbrechen in the refill dialog closes it and stops the pods once", "[flow][warning]")
{
    client->bottle(rum_address.dispenser_id).liquid = 0 * units::si::gram;
    confirm_glass();
    REQUIRE(pump_until([this]() { return popup_open(); }));
    REQUIRE(client->stats().emergency_stops == 0);

    ui->global<cm::gui::ProcessContext>().invoke_abort();

    REQUIRE(pump_until(
        [this]() { return !popup_open() && client->stats().emergency_stops == 1 && last_page == cm::gui::Page::MixErrorPage; }));
    pump_for(100ms);
    CHECK(client->stats().emergency_stops == 1);
    CHECK_FALSE(popup_open());
}

TEST_CASE_METHOD(WarningStation, "The confirm button waits for the check", "[flow][warning]")
{
    fill_rum_to(200);

    confirm_glass();
    CHECK(ui->global<cm::gui::ProcessContext>().get_checking_fill_level());

    REQUIRE(pump_until([this]() { return last_page == cm::gui::Page::MixPage; }));
    CHECK_FALSE(ui->global<cm::gui::ProcessContext>().get_checking_fill_level());
}

TEST_CASE_METHOD(WarningStation, "The confirm button comes back when a warning opens", "[flow][warning]")
{
    fill_rum_to(15);

    confirm_glass();

    REQUIRE(pump_until([this]() { return warning_open(); }));
    CHECK_FALSE(ui->global<cm::gui::ProcessContext>().get_checking_fill_level());
}

namespace {
cm::sim::ClientConfig slow_reply_config()
{
    auto config = fast_config();
    config.quick_reply = 200ms;
    return config;
}

struct SlowCheckStation : WarningStation
{
    SlowCheckStation()
        : WarningStation{slow_reply_config()}
    {
    }
};
} // namespace

TEST_CASE_METHOD(SlowCheckStation, "A guest who leaves during the check gets no drink", "[flow][warning]")
{
    fill_rum_to(200);

    confirm_glass();
    ui->global<cm::gui::StationStateContext>().invoke_navigate_to(cm::gui::Page::CocktailListPage);

    pump_for(600ms);

    CHECK(last_page == cm::gui::Page::CocktailListPage);
    CHECK(client->stats().dispense_requests == 0);
    CHECK_FALSE(warning_open());
    CHECK_FALSE(ui->global<cm::gui::ProcessContext>().get_checking_fill_level());
}

TEST_CASE_METHOD(SlowCheckStation, "A second press during the check starts one mix", "[flow][warning]")
{
    fill_rum_to(200);

    confirm_glass();
    confirm_glass();

    REQUIRE(pump_until([this]() { return client->stats().dispense_requests > 0; }));
    pump_for(300ms);

    CHECK(client->stats().dispense_requests == 1);
    CHECK(client->stats().emergency_stops == 0);
    CHECK(last_page == cm::gui::Page::MixPage);
}

namespace {
cm::sim::ClientConfig calibration_config()
{
    auto config = fast_config();
    config.flow = 30.0 * units::milli_litre / units::si::second;
    return config;
}

/// A simulated station with the calibration dialog's bridge, at a flow of 30 mL/s.
struct CalibrationStation : SimStation
{
    using Status = cm::gui::CalibrationStepStatus;

    cm::gui::DispenserCalibrationBridge bridge{scope, ui, registry, fill_levels};
    cm::gui::Pod gui_pod;
    cm::gui::Dispenser gui_dispenser;

    explicit CalibrationStation(cm::sim::ClientConfig client_config = calibration_config())
        : SimStation{std::move(client_config)}
    {
        bridge.init();
        gui_pod.id = slint::SharedString{rum_address.pod_id.raw()};
        gui_dispenser.id = rum_address.dispenser_id.raw();
    }

    ~CalibrationStation()
    {
        stop();
    }

    const cm::gui::DispenserCalibrationContext& context() const
    {
        return ui->global<cm::gui::DispenserCalibrationContext>();
    }

    cm::gui::DispenserCalibrationState state() const
    {
        return context().get_state();
    }

    bool clear_scale_tare()
    {
        context().invoke_load_cell_tare(gui_pod, gui_dispenser);
        return pump_until([this]() { return state().load_cell_tare_status == Status::Success; });
    }

    bool reference()
    {
        context().invoke_calibrate_load_cell_ref_weight(gui_pod, gui_dispenser, 100);
        return pump_until([this]() { return state().load_cell_status == Status::Success; });
    }

    bool empty_bottle_tare()
    {
        context().invoke_empty_bottle_tare(gui_pod, gui_dispenser);
        return pump_until([this]() { return state().empty_bottle_tare_status == Status::Success; });
    }

    void start_run(int seconds)
    {
        context().invoke_calibrate_dispenser(gui_pod, gui_dispenser, seconds);
    }

    bool run_finished_with(Status status)
    {
        return pump_until([this, status]() { return state().dispenser_status == status; });
    }
};
} // namespace

TEST_CASE_METHOD(CalibrationStation, "The whole calibration procedure succeeds", "[flow][calibration]")
{
    REQUIRE(clear_scale_tare());
    client->bottle(rum_address.dispenser_id).glass += 100 * units::si::gram;
    REQUIRE(reference());
    CHECK(pump_until([this]() { return std::string{state().reference_reading.data()} == "100 g"; }));
    client->bottle(rum_address.dispenser_id).glass -= 100 * units::si::gram;

    REQUIRE(empty_bottle_tare());
    client->bottle(rum_address.dispenser_id).liquid = 500 * units::si::gram;

    start_run(2);
    REQUIRE(run_finished_with(Status::Success));
    CHECK(std::string{state().measured_amount.data()} == "60 mL");
    CHECK_FALSE(state().below_recommended_amount);
    CHECK(state().error_message.empty());
}

TEST_CASE_METHOD(CalibrationStation, "Taring the empty scale forgets the level", "[flow][calibration]")
{
    fill_levels.empty_bottle_tared(rum_address);
    REQUIRE(fill_levels.record(rum_address).has_value());

    REQUIRE(clear_scale_tare());

    CHECK_FALSE(fill_levels.record(rum_address).has_value());
}

TEST_CASE_METHOD(CalibrationStation, "Taring the empty bottle sets the level to 0", "[flow][calibration]")
{
    REQUIRE(empty_bottle_tare());

    const auto record = fill_levels.record(rum_address);
    REQUIRE(record.has_value());
    CHECK(record->level == 0 * units::si::gram);
}

TEST_CASE_METHOD(CalibrationStation, "A run updates the level and works without the load cell card", "[flow][calibration]")
{
    fill_levels.empty_bottle_tared(rum_address);
    client->bottle(rum_address.dispenser_id).liquid += 100 * units::si::gram;
    bool refreshed = false;
    const auto refresh = [&]() -> cm::Task<void> {
        auto dispenser = registry.dispenser_of_pod(rum_address.pod_id, rum_address.dispenser_id);
        co_await fill_levels.refresh(rum_address, **dispenser);
        refreshed = true;
    };
    scope.spawn(refresh());
    REQUIRE(pump_until([&refreshed]() { return refreshed; }));
    REQUIRE(fill_levels.record(rum_address)->level == 100 * units::si::gram);

    start_run(1);
    REQUIRE(run_finished_with(Status::Success));

    CHECK(pump_until([this]() { return fill_levels.record(rum_address)->level == 70 * units::si::gram; }));
    CHECK(state().below_recommended_amount);
}

TEST_CASE_METHOD(CalibrationStation, "A run with next to nothing coming out explains itself", "[flow][calibration]")
{
    client->bottle(rum_address.dispenser_id).liquid = 3 * units::si::gram;

    start_run(1);
    REQUIRE(run_finished_with(Status::Error));

    // codespell:ignore-next-line
    CHECK(std::string{state().error_message.data()}.starts_with("Es ist fast nichts geflossen."));
}

// codespell:ignore-begin
TEST_CASE_METHOD(CalibrationStation, "A step on a dispenser the Pod doesn't have says so", "[flow][calibration]")
{
    auto unknown = gui_dispenser;
    unknown.id = 99;

    context().invoke_load_cell_tare(gui_pod, unknown);

    REQUIRE(pump_until([this]() { return state().load_cell_tare_status == Status::Error; }));
    CHECK(std::string{state().error_message.data()} == "Es konnte kein Dispenser gefunden werden.");
}

// codespell:ignore-end

TEST_CASE_METHOD(CalibrationStation, "A valve calibrates like a pump", "[flow][calibration]")
{
    gui_dispenser.id = 5;

    start_run(1);

    CHECK(run_finished_with(Status::Success));
}

TEST_CASE_METHOD(CalibrationStation, "A failed reference attempt doesn't keep the old reading", "[flow][calibration]")
{
    REQUIRE(clear_scale_tare());
    REQUIRE(reference());
    REQUIRE(pump_until([this]() { return not state().reference_reading.empty(); }));

    // The first calibration has used up its tare, so the Pod refuses this one.
    context().invoke_calibrate_load_cell_ref_weight(gui_pod, gui_dispenser, 100);
    REQUIRE(pump_until([this]() { return state().load_cell_status == Status::Error; }));

    CHECK(state().reference_reading.empty());
}

namespace {
cm::sim::ClientConfig late_ack_config()
{
    auto config = calibration_config();
    config.ack_delay = 700ms;
    return config;
}

struct LateAckStation : CalibrationStation
{
    LateAckStation()
        : CalibrationStation{late_ack_config()}
    {
    }
};
} // namespace

TEST_CASE_METHOD(LateAckStation, "A run whose Ack is late gets stopped, as it may already be pouring", "[flow][calibration]")
{
    start_run(1);

    REQUIRE(run_finished_with(Status::Error));
    CHECK(pump_until([this]() { return client->stats().emergency_stops == 1; }));
}

// codespell:ignore-next-line
TEST_CASE_METHOD(CalibrationStation, "Stopp ends a running calibration", "[flow][calibration]")
{
    CHECK_FALSE(context().invoke_is_busy());
    start_run(5);
    REQUIRE(run_finished_with(Status::Running));
    pump_for(100ms);
    CHECK(context().invoke_is_busy());

    context().invoke_stop_calibration();

    REQUIRE(run_finished_with(Status::Error));
    CHECK_FALSE(context().invoke_is_busy());
    CHECK(std::string{state().error_message.data()}.contains("der Lauf wurde gestoppt"));
}

TEST_CASE_METHOD(CalibrationStation, "A new start clears the old error", "[flow][calibration]")
{
    client->bottle(rum_address.dispenser_id).liquid = 3 * units::si::gram;
    start_run(1);
    REQUIRE(run_finished_with(Status::Error));

    client->bottle(rum_address.dispenser_id).liquid = 500 * units::si::gram;
    start_run(1);

    REQUIRE(run_finished_with(Status::Running));
    pump_for(10ms);
    CHECK(state().error_message.empty());
    REQUIRE(run_finished_with(Status::Success));
}

namespace {
/// A station whose only pod refuses every pour, with one recipe that needs it.
struct FailingMixStation
{
    slint::ComponentHandle<cm::gui::AppWindow> ui = cm::gui::AppWindow::create();
    boost::asio::io_context ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard = boost::asio::make_work_guard(ctx);
    cm::AsyncScope scope{ctx};
    cm::IngredientStore ingredients;
    cm::RecipeStore recipes;
    cm::PodRegistry registry;
    // Declared before the config and the tracker, so that it outlives what writes into it.
    cm::testing::TempDir dir{"cm-failing-mix-test"};
    cm::StationConfig config{ingredients, dir.file("mapping.json")};
    cm::FillLevelTracker fill_levels{ctx.get_executor(), dir.file("levels.json")};
    std::shared_ptr<RefusingPod> pod = std::make_shared<RefusingPod>();
    cm::gui::ProcessContextBridge bridge{scope, ui, recipes, ingredients, config, registry, fill_levels};
    std::optional<cm::gui::Page> last_page;

    FailingMixStation()
    {
        const cm::IngredientId rum{"rum"};
        ingredients.init_ingredients(
            {cm::Ingredient{.id = rum, .display_name = "Rum", .type = {}, .boost_category = cm::BoostCategory::fixed}});
        config.update_dispenser_ingredient_mapping(rum, cm::PodDispenser{pod->pod_id(), cm::DispenserId{0}});
        registry.register_pod(pod);

        recipes.init_recipes({cm::Recipe{
            .id = cm::RecipeId{"drink"},
            .display_name = "Drink",
            .description = "",
            .tags = {},
            .image_path = "",
            .nominal_serving_volume = 250 * units::milli_litre,
            .commands = {cm::Command{cm::DispenseCommand{cm::CommandId{1}, rum, 40 * units::milli_litre}}},
        }});

        bridge.init();
        ui->global<cm::gui::StationStateContext>().on_navigate_to([this](cm::gui::Page page) {
            const auto previous = last_page.value_or(cm::gui::Page::MixGlassSelectionPage);
            last_page = page;
            ui->global<cm::gui::StationStateContext>().invoke_navigated_to(previous, page);
        });

        auto active = ui->global<cm::gui::RecipeContext>().get_active_recipe();
        active.id = "drink";
        ui->global<cm::gui::RecipeContext>().set_active_recipe(active);
        ui->global<cm::gui::ProcessContext>().set_boost(0);
        ui->global<cm::gui::ProcessContext>().set_glass_volume(250);
    }

    ~FailingMixStation()
    {
        registry.unregister_pod(pod);
        cm::run_until_complete(ctx, scope.stop_and_join());
    }

    bool pump_until(const std::function<bool()>& predicate)
    {
        const auto end = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < end) {
            ctx.poll();
            ctx.restart();
            flush_slint_events();
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }
};
} // namespace

TEST_CASE_METHOD(FailingMixStation, "A failed mix stops the pods once and shows the error page", "[flow]")
{
    ui->global<cm::gui::ProcessContext>().invoke_process_active_recipe();

    REQUIRE(pump_until([this]() { return pod->stops == 1 && last_page == cm::gui::Page::MixErrorPage; }));

    const auto end = std::chrono::steady_clock::now() + 100ms;
    pump_until([this, end]() { return pod->stops != 1 || std::chrono::steady_clock::now() >= end; });
    CHECK(pod->stops == 1);
}

TEST_CASE_METHOD(FailingMixStation, "A stop that times out after a failed mix still shows the error page", "[flow]")
{
    pod->stop_times_out = true;

    ui->global<cm::gui::ProcessContext>().invoke_process_active_recipe();

    REQUIRE(pump_until([this]() { return pod->stops == 1 && last_page == cm::gui::Page::MixErrorPage; }));
}
