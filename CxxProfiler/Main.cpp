#include "MainWindow.h"
#include "Version.h"

namespace
{
    void EnableDebugPrivileges()
    {
        HANDLE token;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            LUID luid;
            if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
            {
                TOKEN_PRIVILEGES state;
                state.PrivilegeCount = 1;
                state.Privileges[0].Luid = luid;
                state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(token, FALSE, &state, sizeof(state), nullptr, nullptr);
            }
            CloseHandle(token);
        }
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("C/C++ Profiler");
    app.setWindowIcon(QIcon(":/CxxProfiler/Icon.png"));
    app.setApplicationVersion("2");

    EnableDebugPrivileges();

    MainWindow window;
    window.show();

    return app.exec();
}
