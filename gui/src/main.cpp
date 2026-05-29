#include "app-window.h"

import cm;

int main(int argc, char** argv)
{
    auto logger = cm::log::create_or_get("main");

    cm::log::info(*logger, "test");
    auto ui = ui::AppWindow::create();

    ui->run();
    return 0;
}
