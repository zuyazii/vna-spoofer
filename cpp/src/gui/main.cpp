#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("S Parameter Test System"));
    QApplication::setOrganizationName(QStringLiteral("VNA-CLI"));

    MainWindow window;
    window.show();

    return app.exec();
}
