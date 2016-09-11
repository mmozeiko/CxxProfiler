#pragma once

#include "Precompiled.h"
#include "Symbols.h"

struct ProfilerOptions
{
    uint32_t samplingFreqInMs;
    bool downloadSymbols;
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

    void stop();
    void attach(DWORD pid);
    void execute(const QString& command, const QString& folder, const QString& arguments);

    uint32_t getSizeOfPointer() const;
    uint32_t getThreadCount() const;
    uint64_t getCollectedSamples() const;
    QByteArray serializeCallStacks() const;

signals:
    void attached(DWORD pid);
    void message(const QString& message);
    void finished();

private:
    uint32_t mPointerSize;
    uint64_t mCollectedSamples = 0;
    uint32_t mThreadCount = 0;

    QHash<uint64_t, uint32_t> mCallStackIndex;
    CallStack mCallStack;

    QVector<SymbolPtr> mSymbols;
    QVector<QString> mStrings;
    QHash<uint64_t, QString> mModules;

    HANDLE mPipe;
    QProcess mHelper;
    QByteArray mBuffer;
    OVERLAPPED mOverlapped;

    int cmdMessage(uint8_t* data, uint32_t size);
    int cmdStackSamples(uint8_t* data, uint32_t size);
    int cmdNewString(uint8_t* data, uint32_t size);
    int cmdNewSymbol(uint8_t* data, uint32_t size);
    int cmdProcessStart(uint8_t* data, uint32_t size);
    int cmdProcessEnd(uint8_t* data, uint32_t size);
    int cmdThreadAdd(uint8_t* data, uint32_t size);
    int cmdThreadRemove(uint8_t* data, uint32_t size);
    int cmdModuleLoad(uint8_t* data, uint32_t size);
    int cmdModuleUnload(uint8_t* data, uint32_t size);
    int cmdSymbols(uint8_t* data, uint32_t size);

    void run() override;

    void send(const QByteArray& bytes);
};
