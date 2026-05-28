#include "app-window.h"
#include <spdlog/spdlog.h>

import cm;

int main(int argc, char** argv)
{
    auto logger = cm::log::create_or_get("main");


    cm::log::critical(*logger, "test", "");
    //SPDLOG_LOGGER_INFO(logger, "Initialize GUI.");
    auto ui = ui::AppWindow::create();

    ui->run();
    return 0;
}
