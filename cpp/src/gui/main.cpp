#include "MainWindow.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    QFont defaultFont = app.font();
    defaultFont.setPointSizeF(defaultFont.pointSizeF() * 1.15);
    app.setFont(defaultFont);
    QApplication::setApplicationName(QStringLiteral("S Parameter Test System"));
    QApplication::setOrganizationName(QStringLiteral("VNA-CLI"));

    MainWindow window;
    window.show();

    return app.exec();
}
