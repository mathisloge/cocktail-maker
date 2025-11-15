// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/asio.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

using namespace ftxui;

int main()
{

    boost::asio::thread_pool thread_pool{3};
    boost::asio::cancellation_signal cancel_signal;
    cm::Drv8825StepperMotorDriver stepper1{cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {22}},
                                           cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {27}},
                                           cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {17}}};
    const std::vector<std::string> enabled_entries = {
        "Disabled",
        "Enabled",
    };
    const std::vector<std::string> direction_entries = {
        "Left",
        "Right",
    };

    int enabled_selected = 0;
    int direction_selected = 0;
    std::string steps;

    Component enabled_toggle = Toggle(&enabled_entries, &enabled_selected);
    Component direction_toggle = Toggle(&direction_entries, &direction_selected);

    Component steps_input = Input(&steps, "1000");
    steps_input |= CatchEvent([&](Event event) { return event.is_character() && !std::isdigit(event.character()[0]); });
    steps_input |= CatchEvent([&](Event event) { return event.is_character() && steps.size() > 9; });

    std::string run_title = "Run";
    constexpr auto kVelocity = (700 * cm::units::step) / (1 * cm::units::si::second);
    constexpr auto kAllSteps = 12963;
    auto run_button = Button(&run_title, [&] {
        enabled_selected = !enabled_selected;
        if (enabled_selected) {
            boost::asio::co_spawn(
                thread_pool.executor(),
                [&]() -> cm::async<void> {
                    co_await stepper1.enable();
                    co_await stepper1.step(kAllSteps, kVelocity);
                    co_await stepper1.disable();
                    enabled_selected = 0;
                    run_title = enabled_selected ? "Stop" : "Run";
                },
                boost::asio::bind_cancellation_slot(cancel_signal.slot(), boost::asio::detached));
        }
        else {
            cancel_signal.emit(boost::asio::cancellation_type::all);
            boost::asio::co_spawn(
                thread_pool.executor(), [&]() -> cm::async<void> { co_await stepper1.disable(); }, boost::asio::detached);
        }

        run_title = enabled_selected ? "Stop" : "Run";
    });

    auto container = Container::Vertical({enabled_toggle, direction_toggle, run_button});

    auto renderer = Renderer(container, [&] {
        return vbox({text("Choose your options:"),
                     text(""),
                     hbox(text(" * Motor      : "), enabled_toggle->Render()),
                     hbox(text(" * Direction  : "), direction_toggle->Render()),
                     hbox(run_button->Render())});
    });

    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(renderer);
    cancel_signal.emit(boost::asio::cancellation_type::all);

    return EXIT_SUCCESS;
}
