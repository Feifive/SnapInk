#include "app/mainwindow.h"

#include <QApplication>
int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow w;

    return a.exec();
}
