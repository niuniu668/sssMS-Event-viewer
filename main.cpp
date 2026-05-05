#include <QApplication>
#include <windows.h>
#include <shellapi.h>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.showNormal();
    w.raise();
    w.activateWindow();
    return app.exec();
}

#ifdef Q_OS_WIN
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW) {
        return -1;
    }

    QList<QByteArray> argsUtf8;
    QVector<char *> argv;
    argsUtf8.reserve(argc);
    argv.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        QByteArray arg = QString::fromWCharArray(argvW[i]).toLocal8Bit();
        argsUtf8.push_back(arg);
        argv.push_back(argsUtf8.back().data());
    }

    LocalFree(argvW);
    return main(argc, argv.data());
}
#endif
