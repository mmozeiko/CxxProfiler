#include "Profiler.h"
#include "backend.h"
#include "backend_commands.h"

Profiler::Profiler(const ProfilerOptions& options)
    : mPipe(INVALID_HANDLE_VALUE)
{
    mStrings.push_back(QString());
    mSymbols.push_back(SymbolPtr());

    QString name = "\\\\.\\pipe\\" + QUuid::createUuid().toString();

    mPipe = CreateNamedPipeW(name.toStdWString().c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE  | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 65536, 65536, 0, NULL);
    Assert(mPipe != INVALID_HANDLE_VALUE);

    QStringList args;
    args << name;

    mHelper.start(qApp->applicationFilePath(), args , QIODevice::NotOpen);

    if (!mHelper.waitForStarted(1000))
    {
        // not started?
        qDebug() << "error?";
    }

    memset(&mOverlapped, 0, sizeof(mOverlapped));
    mOverlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Assert(mOverlapped.hEvent);

    BOOL ok = ConnectNamedPipe(mPipe, &mOverlapped);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            DWORD count;
            ok = GetOverlappedResult(mPipe, &mOverlapped, &count, TRUE);
            Assert(ok);
        }
        else if (err != ERROR_PIPE_CONNECTED)
        {
            qDebug() << "error?";
        }
    }

    uint32_t samplingUsec = options.samplingFreqInMs * 1000;
    uint32_t downloadSymbols = options.downloadSymbols ? 1 : 0;
    uint32_t size = sizeof(samplingUsec) + sizeof(downloadSymbols);

    QByteArray data;
    data.append((char)CXX_CMD_SET_OPTIONS);
    data.append((char*)&size, sizeof(size));
    data.append((char*)&samplingUsec, sizeof(samplingUsec));
    data.append((char*)&downloadSymbols, sizeof(downloadSymbols));
    Assert(data.size() == CXX_CMD_HEADER + size);

    send(data);

    moveToThread(this);
    start();
}

Profiler::~Profiler()
{
    stop();

    mHelper.waitForFinished(1000);
    if (mHelper.state() != QProcess::NotRunning)
    {
        mHelper.terminate();
    }

    CloseHandle(mOverlapped.hEvent);
    CloseHandle(mPipe);
}

void Profiler::attach(DWORD pid)
{
    uint32_t size = sizeof(pid);
    QByteArray data;
    data.append((char)CXX_CMD_ATTACH_PROCESS);
    data.append((char*)&size, sizeof(size));
    data.append((char*)&pid, sizeof(pid));
    Assert(data.size() == CXX_CMD_HEADER + size);

    send(data);
}

void Profiler::execute(const QString& command, const QString& folder, const QString& arguments)
{
    QString cmd = command;
    if (!arguments.isNull())
    {
        cmd += ' ' + arguments;
    }

    QByteArray commandUtf8 = command.toUtf8();
    QByteArray argumentsUtf8 = cmd.toUtf8();
    QByteArray folderUtf8 = folder.toUtf8();

    uint32_t size = 3 * sizeof(uint32_t) + commandUtf8.length() + argumentsUtf8.length() + folderUtf8.length();

    QByteArray data;
    data.append((char)CXX_CMD_CREATE_PROCESS);
    data.append((char*)&size, sizeof(size));

    size = commandUtf8.length();
    data.append((char*)&size, sizeof(size));

    size = argumentsUtf8.length();
    data.append((char*)&size, sizeof(size));

    size = folderUtf8.length();
    data.append((char*)&size, sizeof(size));

    data.append(commandUtf8).append(argumentsUtf8).append(folderUtf8);

    Assert(data.size() == CXX_CMD_HEADER + 3 * sizeof(uint32_t) + commandUtf8.length() + argumentsUtf8.length() + folderUtf8.length());

    send(data);
}

uint32_t Profiler::getSizeOfPointer() const
{
    return mPointerSize;
}

uint32_t Profiler::getThreadCount() const
{
    return mCallStackIndex.size();
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
                    if (mPointerSize == sizeof(uint32_t))
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
    uint32_t size = 0;

    QByteArray data;
    data.append((char)CXX_CMD_STOP);
    data.append((char*)&size, sizeof(uint32_t));
    Assert(data.size() == CXX_CMD_HEADER + size);

    send(data);
}

int Profiler::cmdMessage(uint8_t* data, uint32_t size)  
{
    if (size < 2 * sizeof(uint32_t))
    {
        return 0;
    }

    uint32_t length1 = *(uint32_t*)data;
    uint32_t length2 = *(uint32_t*)(data + sizeof(uint32_t));
    if (size < 2 * sizeof(uint32_t) + length1 + length2)
    {
        return 0;
    }

    QString msg1 = QString::fromUtf8((char*)(data + 2 * sizeof(uint32_t)), length1);
    emit message(msg1);
    if (length2)
    {
        QString msg2 = QString::fromUtf8((char*)(data + 2 * sizeof(uint32_t) + length1), length2);
        emit message(msg2);
    }

    return 2 * sizeof(uint32_t) + length1 + length2;
}

int Profiler::cmdStackSamples(uint8_t* data, uint32_t size)
{
    uint32_t threadId;
    uint32_t count;
    if (size < sizeof(threadId) + sizeof(count))
    {
        return 0;
    }
    threadId = *(uint32_t*)(data + 0);
    count = *(uint32_t*)(data + sizeof(threadId));

    uint32_t cmdSize = sizeof(threadId) + sizeof(count) + count * sizeof(backend_stack_entry);
    if (size < cmdSize)
    {
        return 0;
    }

    backend_stack_entry* entries = (backend_stack_entry*)(data + sizeof(threadId) + sizeof(count));

    ThreadCallStack& callstack = mCallStack[mCallStackIndex[threadId]];
    for (uint32_t i = 0; i < count; i++)
    {
        CallStackEntry entry;
        entry.symbol = mSymbols[entries[i].symbol];
        entry.line = entries[i].line;
        entry.offset = entries[i].offset;
        callstack.append(entry);
    }

    callstack.append(CallStackEntry());
    mCollectedSamples++;

    return cmdSize;
}

int Profiler::cmdNewString(uint8_t* data, uint32_t size)
{
    uint32_t length;
    if (size < sizeof(length))
    {
        return 0;
    }

    length = *(uint32_t*)data;
    if (size < sizeof(length) + length)
    {
        return 0;
    }

    QString str = QString::fromUtf8((char*)(data + sizeof(length)), length);
    mStrings.push_back(std::move(str));

    return sizeof(length) + length;
}

int Profiler::cmdNewSymbol(uint8_t* data, uint32_t size)
{
    backend_reply_symbol info;
    if (size < sizeof(info))
    {
        return 0;
    }

    info = *(backend_reply_symbol*)data;

    SymbolPtr symbol(new Symbol);
    symbol->name = mStrings[info.name];
    symbol->file = mStrings[info.file];
    symbol->address = info.address;
    symbol->size = info.size;
    symbol->line = info.line;
    symbol->lineLast = info.line_last;
    symbol->module = mStrings[info.module];

    mSymbols.push_back(symbol);

    return sizeof(info);
}

int Profiler::cmdProcessStart(uint8_t* data, uint32_t size)
{
    uint32_t processId;
    uint32_t ptrSize;

    uint32_t cmdSize = sizeof(processId) + sizeof(ptrSize);
    if (size < cmdSize)
    {
        return 0;
    }
    processId = *(uint32_t*)(data + 0);
    ptrSize = *(uint32_t*)(data + sizeof(processId));

    mPointerSize = ptrSize;

    emit message(QString::asprintf("Process attached, pid=0x%08x", processId));
    emit attached(processId);

    return cmdSize;
}

int Profiler::cmdProcessEnd(uint8_t* data, uint32_t size)
{
    uint32_t exitCode;
    if (size < sizeof(exitCode))
    {
        return 0;
    }

    exitCode = *(uint32_t*)data;

    emit message(QString::asprintf("Process finished, exit code %u", exitCode));
    return sizeof(uint32_t);
}

int Profiler::cmdThreadAdd(uint8_t* data, uint32_t size)
{
    uint32_t threadId;
    uint64_t entry;
    if (size < sizeof(threadId) + sizeof(entry))
    {
        return 0;
    }

    threadId = *(uint32_t*)(data + 0);
    entry = *(uint64_t*)(data + sizeof(threadId));

    emit message(QString::asprintf("Thread started, tid=0x%08x, entry=0x%llx", threadId, entry));

    mCallStackIndex.insert(threadId, mCallStack.count());
    mCallStack.append(ThreadCallStack());

    return sizeof(threadId) + sizeof(entry);
}

int Profiler::cmdThreadRemove(uint8_t* data, uint32_t size)
{
    uint32_t threadId;
    if (size < sizeof(threadId))
    {
        return 0;
    }

    threadId = *(uint32_t*)data;

    emit message(QString::asprintf("Thread finished, tid=0x%08x", threadId));

    mCallStackIndex.remove(threadId);

    return sizeof(threadId);
}

int Profiler::cmdModuleLoad(uint8_t* data, uint32_t size)
{
    uint64_t base;
    uint32_t length;
    if (size < sizeof(base) + sizeof(length))
    {
        return 0;
    }

    base = *(uint64_t*)(data + 0);
    length = *(uint32_t*)(data + sizeof(base));
    if (size < sizeof(base) + sizeof(length) + length)
    {
        return 0;
    }

    QString name = QString::fromUtf8((char*)(data + sizeof(base) + sizeof(length)), length);
    mModules.insert(base, name);

    if (name.isEmpty())
    {
        emit message(QString::asprintf("Module loaded, base=0x%llx", base));
    }
    else
    {
        emit message(QString::asprintf("Module loaded, base=0x%llx, %ls", base, name.utf16()));
    }

    return sizeof(base) + sizeof(length) + length;
}

int Profiler::cmdModuleUnload(uint8_t* data, uint32_t size)
{
    uint64_t base;
    if (size < sizeof(base))
    {
        return 0;
    }

    base = *(uint64_t*)data;
    QString name = mModules[base];
    if (name.isEmpty())
    {
        emit message(QString::asprintf("Module unloaded, base=0x%llx", base));
    }
    else
    {
        emit message(QString::asprintf("Module unloaded, base=0x%llx, %ls", base, name.utf16()));
    }

    return sizeof(base);
}

int Profiler::cmdSymbols(uint8_t* data, uint32_t size)
{
    uint32_t status;
    if (size < sizeof(status))
    {
        return 0;
    }

    status = *(uint32_t*)data;
    switch (status)
    {
    case CXX_SYMBOL_STATUS_DOWNLOADING:
        emit message("Downloading pdb...");
        break;
    case CXX_SYMBOL_STATUS_LOADED_PRIVATE:
        emit message("Loaded private symbols & lines");
        break;
    case CXX_SYMBOL_STATUS_LOADED_PUBLIC:
        emit message("Loaded public symbols");
        break;
    case CXX_SYMBOL_STATUS_LOADED_EXPORT:
        emit message("Loaded export symbols");
        break;
    }

    return sizeof(status);
}

void Profiler::run()
{
    for (;;)
    {
        char buffer[4096];
        DWORD read;

        BOOL ok = ReadFile(mPipe, buffer, _countof(buffer), &read, &mOverlapped);
        if (!ok)
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                ok = GetOverlappedResult(mPipe, &mOverlapped, &read, TRUE);
            }
        }
        if (!ok || read == 0)
        {
            break;
        }

        mBuffer.append(buffer, read);

        while (mBuffer.length() >= CXX_CMD_HEADER)
        {
            uint32_t size = *(uint32_t*)(mBuffer.data() + 1);
            uint8_t* data = (uint8_t*)mBuffer.data() + CXX_CMD_HEADER;
            if (mBuffer.length() < CXX_CMD_HEADER + size)
            {
                break;
            }
            int used = 0;
            switch (mBuffer[0])
            {
            case CXX_REPLY_MESSAGE:
                used = cmdMessage(data, size);
                break;
            case CXX_REPLY_STACK_SAMPLES:
                used = cmdStackSamples(data, size);
                break;
            case CXX_REPLY_NEW_STRING:
                used = cmdNewString(data, size);
                break;
            case CXX_REPLY_NEW_SYMBOL:
                used = cmdNewSymbol(data, size);
                break;
            case CXX_REPLY_PROCESS_START:
                used = cmdProcessStart(data, size);
                break;
            case CXX_REPLY_PROCESS_END:
                used = cmdProcessEnd(data, size);
                break;
            case CXX_REPLY_THREAD_ADD:
                used = cmdThreadAdd(data, size);
                break;
            case CXX_REPLY_THREAD_REMOVE:
                used = cmdThreadRemove(data, size);
                break;
            case CXX_REPLY_MODULE_LOAD:
                used = cmdModuleLoad(data, size);
                break;
            case CXX_REPLY_MODULE_UNLOAD:
                used = cmdModuleUnload(data, size);
                break;
            case CXX_REPLY_SYMBOLS:
                used = cmdSymbols(data, size);
                break;
            }
            if (used)
            {
                mBuffer.remove(0, CXX_CMD_HEADER + used);
            }
        }
    }

    emit finished();
}

void Profiler::send(const QByteArray& bytes)
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    BOOL ok = WriteFile(mPipe, bytes.data(), bytes.length(), NULL, &overlapped);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            DWORD written;
            GetOverlappedResult(mPipe, &overlapped, &written, TRUE);
        }
    }
    CloseHandle(overlapped.hEvent);
}
