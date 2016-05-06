#include "Profiler.h"

Profiler::Profiler(const ProfilerOptions& options)
    : mOptions(options)
{
    moveToThread(this);
    start(QThread::TimeCriticalPriority);
}

Profiler::~Profiler()
{
    mRunning = false;
    quit();
    wait();
}

void Profiler::attach(DWORD pid)
{
    QTimer::singleShot(0, this, [=]()
    {
        emit message(QString("Attaching to pid %1").arg(pid));

        if (!DebugActiveProcess(pid))
        {
            emit message("DebugActiveProcess failed - " + qt_error_string());
            return;
        }
        DebugSetProcessKillOnExit(FALSE);

        mIsAttached = true;
        process();
    });
}

void Profiler::execute(const QString& command, const QString& folder, const QString& arguments)
{
    QTimer::singleShot(0, this, [=]()
    {
        emit message(QString("Launching '%1'").arg(command));

        QString cmd = command;
        if (!arguments.isNull())
        {
            cmd += ' ' + arguments;
        }

        QVarLengthArray<wchar_t> appArray(command.length() + 1);
        appArray[command.toWCharArray(appArray.data())] = 0;

        QVarLengthArray<wchar_t> cmdArray(cmd.length() + 1);
        cmdArray[cmd.toWCharArray(cmdArray.data())] = 0;

        QVarLengthArray<wchar_t> folderArray(folder.length() + 1);
        folderArray[folder.toWCharArray(folderArray.data())] = 0;

        STARTUPINFOW si = {};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi;
        if (!CreateProcessW(appArray.constData(), cmdArray.data(), nullptr, nullptr, FALSE, DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, nullptr, folderArray.data(), &si, &pi))
        {
            emit message("CreateProcess failed - " + qt_error_string());
            return;
        }
        DebugSetProcessKillOnExit(TRUE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        process();
    });
}

bool Profiler::isAttached() const
{
    return mIsAttached;
}

uint32_t Profiler::getSizeOfPointer() const
{
    return mIsWow64 ? sizeof(uint32_t) : sizeof(uint64_t);
}

uint32_t Profiler::getThreadCount() const
{
    return mThreadCount;
}

uint64_t Profiler::getCollectedSamples() const
{
    return mCollectedSamples;
}

QByteArray Profiler::serializeCallStacks() const
{
    QHash<Symbol*, uint32_t> symbolId;
    QHash<QString, uint32_t> stringId;

    // collecting symbols that are used
    symbolId[nullptr] = 0;
    stringId[QString()] = 0;
    for (const ThreadCallStack& threadCallStack : mCallStack)
    {
        for (const CallStackEntry& entry : threadCallStack)
        {
            if (entry.symbol)
            {
                Symbol* symbol = entry.symbol.data();

                if (!symbolId.contains(symbol))
                {
                    symbolId.insert(symbol, symbolId.count());
                }

                if (!stringId.contains(symbol->module))
                {
                    stringId.insert(symbol->module, stringId.count());
                }

                if (!stringId.contains(symbol->file))
                {
                    stringId.insert(symbol->file, stringId.count());
                }
            }
        }
    }

    QByteArray result;
    {
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);

        QDataStream out(&buffer);

        // writing strings - filenames & modules
        out << uint32_t(stringId.count() - 1);
        {
            QHashIterator<QString, uint32_t> it(stringId);
            while (it.hasNext())
            {
                it.next();
                if (!it.key().isEmpty())
                {
                    out << it.value() << it.key();
                }
            }
        }

        // writing symbols
        out << uint32_t(symbolId.count() - 1);
        {
            QHashIterator<Symbol*, uint32_t> it(symbolId);
            while (it.hasNext())
            {
                it.next();
                const Symbol* symbol = it.key();
                if (symbol)
                {
                    out << it.value();
                    out << symbol->name;
                    if (mIsWow64)
                    {
                        out << uint32_t(symbol->address);
                    }
                    else
                    {
                        out << uint64_t(symbol->address);
                    }
                    out << symbol->size
                        << stringId[symbol->module]
                        << stringId[symbol->file]
                        << symbol->line
                        << symbol->lineLast;
                }
            }
        }

        // writing call stack
        out << (uint32_t)(mCallStack.count());
        for (const ThreadCallStack& threadCallStack : mCallStack)
        {
            out << uint32_t(threadCallStack.count());
            for (const CallStackEntry& entry : threadCallStack)
            {
                out << symbolId[entry.symbol.data()] << entry.line << entry.offset;
            }
        }
    }

    return result;
}

void Profiler::stop()
{
    mRunning = false;
}

void Profiler::process()
{
    timeBeginPeriod(1);

    while (mRunning)
    {
        DEBUG_EVENT ev;

        if (WaitForDebugEvent(&ev, mOptions.samplingFreqInMs))
        {
            LONG status = DBG_CONTINUE;
            switch (ev.dwDebugEventCode)
            {
            case CREATE_PROCESS_DEBUG_EVENT:
                createProcess(ev.dwProcessId, ev.dwThreadId, &ev.u.CreateProcessInfo);
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
                exitProcess(ev.dwThreadId, &ev.u.ExitProcess);
                emit finished();
                break;

            case CREATE_THREAD_DEBUG_EVENT:
                if (ev.u.CreateThread.hThread != nullptr)
                {
                    createThread(ev.dwThreadId, &ev.u.CreateThread);
                }
                break;

            case EXIT_THREAD_DEBUG_EVENT:
                exitThread(ev.dwThreadId, &ev.u.ExitThread);
                break;

            case LOAD_DLL_DEBUG_EVENT:
                if (ev.u.LoadDll.hFile != nullptr)
                {
                    loadDll(&ev.u.LoadDll);
                }
                break;

            case UNLOAD_DLL_DEBUG_EVENT:
                unloadDll(&ev.u.UnloadDll);
                break;

            case EXCEPTION_DEBUG_EVENT:
                if (!ev.u.Exception.dwFirstChance)
                {
                    status = DBG_EXCEPTION_NOT_HANDLED;
                }
                break;

            case OUTPUT_DEBUG_STRING_EVENT:
                if (mOptions.captureDebugOutputString)
                {
                    outputDebugString(&ev.u.DebugString);
                }
                break;
            }

            if (!ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status))
            {
                emit message("ContinueDebugEvent failed - " + qt_error_string());
            }
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_SEM_TIMEOUT)
            {
                if (mProcess != nullptr && mSymbolsInitialized)
                {
                    sample();
                }
            }
            else
            {
                emit message("WaitForDebugEvent failed - " + qt_error_string(err));
            }
        }
    }

    if (mProcess != nullptr)
    {
        SymCleanup(mProcess);
        mProcess = nullptr;
    }

    timeEndPeriod(1);
}

void Profiler::sample()
{
    for (auto it = mThreads.begin(), eit = mThreads.end(); it != eit; ++it)
    {
        DWORD threadId = it.key();
        HANDLE thread = it.value();

        if (SuspendThread(thread) == - 1)
        {
            continue;
        }

        union
        {
            WOW64_CONTEXT ctx32;
            CONTEXT ctx64;
        };
        PVOID ctx = nullptr;
        DWORD machine;

        STACKFRAME64 frame = {};
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        if (mIsWow64)
        {
            machine = IMAGE_FILE_MACHINE_I386;
            ctx32.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;
            if (Wow64GetThreadContext(thread, &ctx32))
            {
                frame.AddrPC.Offset = ctx32.Eip;
                frame.AddrFrame.Offset = ctx32.Ebp;
                frame.AddrStack.Offset = ctx32.Esp;
                ctx = &ctx32;
            }
        }
        else
        {
            machine = IMAGE_FILE_MACHINE_AMD64;
            ctx64.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            if (GetThreadContext(thread, &ctx64))
            {
                frame.AddrPC.Offset = ctx64.Rip;
                frame.AddrFrame.Offset = ctx64.Rbp;
                frame.AddrStack.Offset = ctx64.Rsp;
                ctx = &ctx64;
            }
        }

        if (ctx == nullptr)
        {
            emit message("GetThreadContext failed - " + qt_error_string());
        }
        else
        {
            ThreadCallStack& callstack = mCallStack[mCallStackIndex[threadId]];
            bool good = false;
            int delta = 0;
            DWORD64 lastStack = 0;
            while (StackWalk64(machine, mProcess, thread, &frame, ctx, nullptr, 
                SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            {
                if (frame.AddrPC.Offset == 0
                  || frame.AddrStack.Offset <= lastStack
                  || (frame.AddrStack.Offset % (mIsWow64 ? sizeof(uint32_t) : sizeof(uint64_t))) != 0)
                {
                    break;
                }
                lastStack = frame.AddrStack.Offset;

                uint64_t address = frame.AddrPC.Offset;

                CallStackEntry entry;
                entry.symbol = lookupSymbol(address - delta);
                if (entry.symbol)
                {
                    entry.line = lookupLine(address - delta);
                    entry.offset = static_cast<uint32_t>(address - entry.symbol->line);
                    callstack.append(entry);
                    good = true;
                }
                delta = 1;
            }

            if (good)
            {
                callstack.append(CallStackEntry());
                ++mCollectedSamples;
            }
        }

        ResumeThread(thread);
    }
}

void Profiler::createProcess(DWORD processId, DWORD threadId, const CREATE_PROCESS_DEBUG_INFO* info)
{
    emit message(QString("Process attached, pid=0x%1, tid=0x%2")
        .arg(processId, 8, 16, QChar('0'))
        .arg(threadId, 8, 16, QChar('0')));
    mThreads.insert(threadId, info->hThread);
    mCallStackIndex.insert(threadId, mCallStack.count());
    mCallStack.append(ThreadCallStack());

    mThreadCount = 1;

    mProcess = info->hProcess;
    mProcessId = processId;
    mProcessBase = (DWORD64)info->lpBaseOfImage;
    IsWow64Process(mProcess, &mIsWow64);

    DWORD options = SYMOPT_UNDNAME | SYMOPT_LOAD_LINES;
    if (mOptions.downloadSymbols)
    {
        options |= SYMOPT_FAVOR_COMPRESSED | SYMOPT_IGNORE_NT_SYMPATH;
    }
    if (mIsWow64)
    {
        options |= SYMOPT_INCLUDE_32BIT_MODULES;
    };
    SymSetOptions(options);

    BOOL initialized;
    if (mOptions.downloadSymbols)
    {
        QDir appFolder = qApp->applicationDirPath();
        appFolder.mkdir("symbols");

        QString server = "https://msdl.microsoft.com/download/symbols";

        QString path = QString("SRV*%1*%2").arg(QDir::toNativeSeparators(appFolder.filePath("symbols"))).arg(server);
        QVarLengthArray<wchar_t> pathArray(path.length() + 1);
        pathArray[path.toWCharArray(pathArray.data())] = 0;

        initialized = SymInitializeW(mProcess, pathArray.constData(), FALSE);
    }
    else
    {
        initialized = SymInitializeW(mProcess, nullptr, FALSE);
    }

    if (initialized)
    {
        QString name = getFileNameFromHandle(info->hFile);
        loadModule(info->hFile, name, (DWORD64)info->lpBaseOfImage);
        mSymbolsInitialized = true;
    }
    else
    {
        emit message("SymInitialize failed - " + qt_error_string());
    }

    emit attached(mProcess);
}

void Profiler::exitProcess(DWORD threadId, const EXIT_PROCESS_DEBUG_INFO* info)
{
    emit message(QString("Process finished, exit code %1").arg(info->dwExitCode));

    if (mSymbolsInitialized)
    {
        unloadModule(mProcessBase);
        SymCleanup(mProcess);
        mSymbolsInitialized = false;
    }

    mThreads.remove(threadId);
    mCallStackIndex.remove(threadId);
    mProcess = nullptr;
    --mThreadCount;
}

void Profiler::createThread(DWORD threadId, const CREATE_THREAD_DEBUG_INFO* info)
{
    emit message(QString("Thread started, tid=0x%1")
        .arg(threadId, 8, 16, QChar('0')));
    mThreads.insert(threadId, info->hThread);
    mCallStackIndex.insert(threadId, mCallStack.count());
    mCallStack.append(ThreadCallStack());
    ++mThreadCount;
}

void Profiler::exitThread(DWORD threadId, const EXIT_THREAD_DEBUG_INFO* info)
{
    emit message(QString("Thread finished, tid=0x%1, exit code %2")
        .arg(threadId, 8, 16, QChar('0'))
        .arg(info->dwExitCode));
    mThreads.remove(threadId);
    mCallStackIndex.remove(threadId);
    --mThreadCount;
}

void Profiler::loadDll(const LOAD_DLL_DEBUG_INFO* info)
{
    QString name = QString::null;
    if (info->lpImageName != nullptr)
    {
        name = getStringFromPointer(info->lpImageName, info->fUnicode != 0);
    }
    if (name.isNull())
    {
        name = getFileNameFromHandle(info->hFile);
    }

    if (mSymbolsInitialized)
    {
        loadModule(info->hFile, name, (uint64_t)info->lpBaseOfDll);
    }

    if (name.isNull())
    {
        emit message(QString("DLL loaded, base=%1")
            .arg(formatAddress((uint64_t)info->lpBaseOfDll)));
    }
    else
    {
        emit message(QString("DLL loaded, base=%1, %2")
            .arg(formatAddress((uint64_t)info->lpBaseOfDll))
            .arg(name));
    }
}

void Profiler::unloadDll(const UNLOAD_DLL_DEBUG_INFO* info)
{
    emit message(QString("DLL unloaded, base=%1")
        .arg(formatAddress((uint64_t)info->lpBaseOfDll)));
    unloadModule((uint64_t)info->lpBaseOfDll);
}

static QString& rtrim(QString& str)
{
    while (str.size() > 0 && str.at(str.size() - 1).isSpace())
    {
        str.chop(1);
    }
    return str;
}

void Profiler::outputDebugString(const OUTPUT_DEBUG_STRING_INFO* info)
{
    if (info->fUnicode)
    {
        QVarLengthArray<wchar_t> buffer(info->nDebugStringLength - 1);
        SIZE_T read;
        if (ReadProcessMemory(mProcess, info->lpDebugStringData, buffer.data(), buffer.size() * sizeof(wchar_t), &read))
        {
            QString str = QString::fromWCharArray(buffer.data(), static_cast<int>(read));
            emit message(rtrim(str));
        }
    }
    else
    {
        QVarLengthArray<char> buffer(info->nDebugStringLength - 1);
        SIZE_T read;
        if (ReadProcessMemory(mProcess, info->lpDebugStringData, buffer.data(), buffer.size() * sizeof(char), &read))
        {
            QString str = QString::fromLocal8Bit(buffer.data(), static_cast<int>(read));
            emit message(rtrim(str));
        }
    }
}

QString Profiler::getStringFromPointer(LPVOID address, bool unicode) const
{
    uint64_t ptr = 0;
    SIZE_T pointerSize = mIsWow64 ? sizeof(uint32_t) : sizeof(uint64_t);
    SIZE_T read;
    if (ReadProcessMemory(mProcess, address, &ptr, pointerSize, &read) && read == pointerSize)
    {
        if (pointerSize == sizeof(uint32_t) && (uint32_t)ptr != 0
          || pointerSize == sizeof(uint64_t) && (uint64_t)ptr != 0)
        {
            wchar_t buffer[MAX_PATH + 1];
            if (ReadProcessMemory(mProcess, (LPCVOID)ptr, buffer, sizeof(buffer), &read) && read > 0)
            {
                buffer[MAX_PATH] = 0;
                if (unicode)
                {
                    return QString::fromWCharArray(buffer);
                }
                else
                {
                    return QString::fromLocal8Bit(reinterpret_cast<char*>(buffer));
                }
            }
        }
    }

    return QString::null;
}

QString Profiler::getFileNameFromHandle(HANDLE file) const
{
    wchar_t path[MAX_PATH];
    DWORD length = GetFinalPathNameByHandleW(file, path, _countof(path), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length == 0)
    {
        return QString::null;
    }

    int offset = (wmemcmp(path, L"\\\\?\\", 4) == 0) ? 4 : 0;
    return QString::fromWCharArray(path + offset, length - offset);
}

QString Profiler::formatAddress(uint64_t address) const
{
    return QString("0x%1").arg(address, mIsWow64 ? 8 : 16, 16, QChar('0'));
}

SymbolPtr Profiler::lookupSymbol(uint64_t address)
{
    // check for cached symbol
    {
        auto it = mSymbols.upperBound(address);
        if (it-- != mSymbols.begin())
        {
            if (address >= it.key() && address < it.key() + it.value()->size)
            {
                return it.value();
            }
        }
    }

    // resolve symbol

    SYMBOL_INFO_PACKAGEW info;
    info.si.SizeOfStruct = sizeof(info.si);
    info.si.MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement;
    if (!SymFromAddrW(mProcess, address, &displacement, &info.si))
    {
        return SymbolPtr();
    }

    // for weird pdb info (function size == 0) try looking up symbol by address
    if (info.si.Size == 0)
    {
        auto it = mSymbols.upperBound(address);
        if (it-- != mSymbols.begin() && it.value()->address == info.si.Address)
        {
            return it.value();
        }
    }

    SymbolPtr symbol(new Symbol());
    symbol->name = QString::fromWCharArray(info.si.Name, info.si.NameLen);
    symbol->address = info.si.Address;
    symbol->size = info.si.Size;
    symbol->module = QString::null;

    IMAGEHLP_LINEW64 line;
    line.SizeOfStruct = sizeof(line);
    DWORD offset;
    if (SymGetLineFromAddrW64(mProcess, info.si.Address, &offset, &line))
    {
        symbol->file = QString::fromWCharArray(line.FileName);
        symbol->line = line.LineNumber;
        symbol->lineLast = line.LineNumber;
    }
    else
    {
        symbol->line = 0;
    }

    if (SymGetLineFromAddrW64(mProcess, info.si.Address + info.si.Size - 1, &offset, &line))
    {
        symbol->lineLast = line.LineNumber;
    }
    else
    {
        symbol->lineLast = symbol->line;
    }

    // resolve module
    auto it = mModules.upperBound(address);
    if (it-- != mModules.begin())
    {
        if (address >= it->address && address < it->address + it->size)
        {
            symbol->module = it->name;
        }
    }

    return mSymbols.insert(symbol->address, symbol).value();
}

uint32_t Profiler::lookupLine(uint64_t address) const
{
    DWORD offset;
    IMAGEHLP_LINEW64 info;
    info.SizeOfStruct = sizeof(info);
    if (SymGetLineFromAddrW64(mProcess, address, &offset, &info))
    {
        return info.LineNumber;
    }

    return ~(uint32_t)0;
}

void Profiler::loadModule(HANDLE file, const QString& name, uint64_t base)
{
    QVarLengthArray<wchar_t> nameArray(name.size() + 1);
    nameArray[name.toWCharArray(nameArray.data())] = 0;

    Module module;
    module.handle = file;
    module.name = "[unknown]";
    module.address = base;
    module.size = 0;

    if (SymLoadModuleExW(mProcess, file, name.isNull() ? nullptr : nameArray.constData(), nullptr, base, 0, nullptr, 0) == 0)
    {
        emit message(QString("SymLoadModuleEx failed - %1").arg(qt_error_string()));
    }
    else
    {
        IMAGEHLP_MODULEW64 moduleInfo = {};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        if (SymGetModuleInfoW64(mProcess, base, &moduleInfo))
        {
            module.name = QString::fromWCharArray(moduleInfo.ModuleName);
            module.address = moduleInfo.BaseOfImage;
            module.size = moduleInfo.ImageSize;
        }
        else
        {
            emit message(QString("SymGetModuleInfo64 failed - %1").arg(qt_error_string()));
        }
    }

    mModules.insert(module.address, module);
}

void Profiler::unloadModule(uint64_t base)
{
    if (mSymbolsInitialized)
    {
        if (!SymUnloadModule64(mProcess, base))
        {
            emit message("SymUnloadModule64 failed - " + qt_error_string());
        }
    }

    auto module = mModules.find(base);
    if (module == mModules.end())
    {
        return;
    }

    CloseHandle(module->handle);

    uint32_t size = module->size;
    mModules.erase(module);

    auto it = mSymbols.lowerBound(base);
    auto eit = mSymbols.lowerBound(base + size);
    if (it != mSymbols.end() && eit != it)
    {
        while (it != eit)
        {
            it = mSymbols.erase(it);
        }
    }
}
