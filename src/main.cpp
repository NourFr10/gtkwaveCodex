#include <QApplication>
#include <QString>

#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.resize(1280, 720);
    window.show();

    if (argc > 1)
    {
        window.openFstFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
