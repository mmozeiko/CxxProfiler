#pragma once

#include "Precompiled.h"
#include "Symbols.h"

struct ProfilerOptions
{
    uint32_t samplingFreqInMs;
    bool captureDebugOutputString;
    bool downloadSymbols;
};

struct Module
{
    HANDLE handle;
    QString name;
    uint64_t address;
    uint32_t size;
};

struct CallStackEntry
{
    SymbolPtr symbol;
    uint32_t line = 0;
    uint32_t offset = 0;
};

typedef QVector<CallStackEntry> ThreadCallStack;
typedef QVector<ThreadCallStack> CallStack;

class Profiler : public QThread
{
    Q_OBJECT

public:
    Profiler(const ProfilerOptions& options);
    ~Profiler();

    void attach(DWORD pid);
    void execute(const QString& command, const QString& folder, const QString& arguments);
    
    bool isAttached() const;
    uint32_t getSizeOfPointer() const;
    uint32_t getThreadCount() const;
    uint64_t getCollectedSamples() const;
    QByteArray serializeCallStacks() const;

public slots:
    void stop();

signals:
    void attached(HANDLE process);
    void message(const QString& message);
    void finished();

private:
    void process();
    void sample();

    void createProcess(DWORD processId, DWORD threadId, const CREATE_PROCESS_DEBUG_INFO* info);
    void exitProcess(DWORD threadId, const EXIT_PROCESS_DEBUG_INFO* info);
    void createThread(DWORD threadId, const CREATE_THREAD_DEBUG_INFO* info);
    void exitThread(DWORD threadId, const EXIT_THREAD_DEBUG_INFO* info);
    void loadDll(const LOAD_DLL_DEBUG_INFO* info);
    void unloadDll(const UNLOAD_DLL_DEBUG_INFO* info);
    void outputDebugString(const OUTPUT_DEBUG_STRING_INFO* info);

    QString getStringFromPointer(LPVOID address, bool unicode) const;
    QString getFileNameFromHandle(HANDLE file) const;
    QString formatAddress(uint64_t address) const;

    volatile bool mRunning = true;
    ProfilerOptions mOptions;

    HANDLE mProcess = nullptr;
    DWORD mProcessId;
    DWORD64 mProcessBase;

    BOOL mIsWow64 = FALSE;
    bool mSymbolsInitialized = false;
    bool mIsAttached = false;

    QAtomicInteger<uint32_t> mThreadCount = 0;
    QHash<DWORD, HANDLE> mThreads;

    QHash<DWORD, uint32_t> mCallStackIndex;
    CallStack mCallStack;
    QAtomicInteger<uint64_t> mCollectedSamples = 0;

    // symbol cache
    QMap<uint64_t, SymbolPtr> mSymbols;
    QMap<uint64_t, Module> mModules;

    SymbolPtr lookupSymbol(uint64_t address);
    uint32_t lookupLine(uint64_t address) const;

    void loadModule(HANDLE file, const QString& name, uint64_t base);
    void unloadModule(uint64_t base);
};
