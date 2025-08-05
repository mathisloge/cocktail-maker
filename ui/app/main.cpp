#include <QApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <cm/recipe.hpp>
#include <cm/recipe_store.hpp>
#include "ApplicationState.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"

Q_IMPORT_QML_PLUGIN(CocktailMaker_UiPlugin)

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("CocktailMaker"));
    QCoreApplication::setOrganizationName(QStringLiteral("com.mathisloge.cocktail-maker"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    app.setPalette(palette);

    QQmlApplicationEngine engine;

    auto &&app_state = engine.singletonInstance<cm::app::ApplicationState *>("CocktailMaker.App", "ApplicationState");
    Q_ASSERT(app_state != nullptr);

    std::shared_ptr<cm::RecipeStore> recipe_store = std::make_shared<cm::RecipeStore>();

    auto recipe =
        cm::make_recipe()
            .with_name("Only Water")
            .with_steps()
            .with_step(std::make_unique<cm::DispenseLiquidCmd>("water", 250 * mp_units::si::milli<mp_units::si::litre>))
            .add()
            .create();
    recipe_store->add_recipe(std::move(recipe));

    app_state->recipe_store = recipe_store;

    engine.loadFromModule("CocktailMaker.App", "Main");

    return app.exec();
}
