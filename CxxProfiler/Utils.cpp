#include "Utils.h"

namespace
{
    uint64_t to64(FILETIME ft)
    {
        return (((uint64_t)ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
    }

    uint64_t getWallTime()
    {
        FILETIME time;
        GetSystemTimeAsFileTime(&time);
        return to64(time);
    }
}

CpuUsage::CpuUsage()
    : mLastWallTime(getWallTime())
    , mWallDelta(0)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    mCpuCoreCount = info.dwNumberOfProcessors;
}

uint64_t CpuUsage::getProcessTime(HANDLE process) const
{
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;

    if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime))
    {
        return to64(kernelTime) + to64(userTime);
    }

    return 0;
}

void CpuUsage::update()
{
    uint64_t wallTime = getWallTime();
    mWallDelta = wallTime - mLastWallTime;
    mLastWallTime = wallTime;
}

double CpuUsage::getUsage(HANDLE process, uint64_t* lastProcessTime) const
{
    if (mWallDelta == 0)
    {
        return 0.0;
    }

    uint64_t processTime = getProcessTime(process);
    if (processTime == 0)
    {
        return 0.0;
    }
    uint64_t processDelta = processTime - *lastProcessTime;
    *lastProcessTime = processTime;

    int32_t coreCount = getCoresForProcess(process);
    return 100.0 * processDelta / mWallDelta / coreCount;
}

int32_t CpuUsage::getCoresForProcess(HANDLE process) const
{
    DWORD_PTR processAffinityMask;
    DWORD_PTR systemAffinityMask;
    if (!GetProcessAffinityMask(process, &processAffinityMask, &systemAffinityMask) || processAffinityMask == 0)
    {
        return mCpuCoreCount;
    }

    int32_t count = 0;
    while (processAffinityMask)
    {
        count++;
        processAffinityMask &= processAffinityMask - 1;
    }
    return count;
}

void OpenInVisualStudio(const QString& file, uint32_t line)
{
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"VisualStudio.DTE", &clsid)))
    {
        return;
    }

    CComPtr<IUnknown> punk;
    if (FAILED(GetActiveObject(clsid, nullptr, &punk)))
    {
        if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, EnvDTE::IID__DTE, (LPVOID*)&punk)))
        {
            return;
        }
    }

    CComPtr<EnvDTE::_DTE> dte;
    dte = punk;

    CComPtr<EnvDTE::ItemOperations> itemOps;
    if (FAILED(dte->get_ItemOperations(&itemOps)))
    {
        return;
    }

    QVarLengthArray<wchar_t> fileArray(file.length());
    file.toWCharArray(fileArray.data());

    CComBSTR bstrFilename(fileArray.length(), fileArray.constData());
    CComBSTR bstrKind(EnvDTE::vsViewKindCode);
    CComPtr<EnvDTE::Window> window;
    if (FAILED(itemOps->OpenFile(bstrFilename, bstrKind, &window)))
    {
        return;
    }

    CComPtr<EnvDTE::Document> doc;
    if (FAILED(dte->get_ActiveDocument(&doc)))
    {
        return;
    }

    CComPtr<IDispatch> selectionDispatch;
    if (FAILED(doc->get_Selection(&selectionDispatch)))
    {
        return;
    }

    CComPtr<EnvDTE::TextSelection> selection;
    if (FAILED(selectionDispatch->QueryInterface(&selection)))
    {
        return;
    }

    if (FAILED(selection->GotoLine(line, FALSE)))
    {
        return;
    }

    return;
}

void OpenInExplorer(const QString& file)
{
    QString args = "/select," + file;

    QVarLengthArray<wchar_t> argsArray(args.length() + 1);
    argsArray[args.toWCharArray(argsArray.data())] = 0;

    ShellExecuteW(nullptr, L"open", L"explorer.exe", argsArray.constData(), nullptr, SW_SHOWNORMAL);
}

void OpenInEditor(const QString& file)
{
    QVarLengthArray<wchar_t> fileArray(file.length() + 1);
    fileArray[file.toWCharArray(fileArray.data())] = 0;

    ShellExecuteW(nullptr, L"open", fileArray.constData(), nullptr, nullptr, SW_SHOWNORMAL);
}

QString GetSettingsFile()
{
    return QDir(qApp->applicationDirPath()).filePath("config.ini");
}

void DetectVSLocations(QSettings& settings)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QString vs2013 = env.value("VS120COMNTOOLS", "C:\\Program Files(x86)\\Microsoft Visual Studio 12.0\\Common7\\Tools\\");
    vs2013 += "..\\..\\VC";
    if (!QFileInfo(vs2013).isDir())
    {
        vs2013.clear();
    }

    QString vs2015 = env.value("VS140COMNTOOLS", "C:\\Program Files(x86)\\Microsoft Visual Studio 14.0\\Common7\\Tools\\");
    vs2015 += "..\\..\\VC";
    if (!QFileInfo(vs2015).isDir())
    {
        vs2015.clear();
    }

    QString sdk10 = "C:\\Program Files (x86)\\Windows Kits\\10\\Source\\10.0.10586.0";
    if (!QFileInfo(sdk10).isDir())
    {
        sdk10.clear();
    }

    settings.setValue("Preferences/VS2013", QDir::cleanPath(vs2013));
    settings.setValue("Preferences/VS2015", QDir::cleanPath(vs2015));
    settings.setValue("Preferences/SDK10", QDir::cleanPath(sdk10));
}
