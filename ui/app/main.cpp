#include <QApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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

    engine.loadFromModule("CocktailMaker.App", "Main");

    return app.exec();
}
