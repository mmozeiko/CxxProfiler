#include "MainWindow.h"

extern "C"
{
#include "backend.h"
}

int main(int argc, char* argv[])
{
    int arg_count;
    LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &arg_count);
    if (arg_count == 2)
    {
        backend_main(args[1]);
        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName("C/C++ Profiler");
    app.setWindowIcon(QIcon(":/CxxProfiler/Icon.png"));
    app.setApplicationVersion("3");

    MainWindow window;
    window.show();

    return app.exec();
}
