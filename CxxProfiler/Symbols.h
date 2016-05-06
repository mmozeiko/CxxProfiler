#pragma once

struct Symbol
{
    QString name;
    QString file;
    uint64_t address;
    uint32_t size;
    uint32_t line;
    uint32_t lineLast;

    QString module;
};

typedef QSharedPointer<Symbol> SymbolPtr;

/*****/

struct FlatSymbol
{
    uint32_t self = 0;
    uint32_t total = 0;
};

typedef QHash<SymbolPtr, FlatSymbol> FlatSymbols;
typedef QPair<QString, FlatSymbols> FlatThread;
typedef QVector<FlatThread> FlatThreads;

/*****/

struct CallGraphSymbol;
typedef QHash<QPair<SymbolPtr, uint32_t>, CallGraphSymbol> CallGraphSymbols;

struct CallGraphSymbol
{
    uint32_t self = 0;
    uint32_t total = 0;
    CallGraphSymbols childs;
};

typedef QPair<QString, CallGraphSymbol> CallGraphThread;
typedef QVector<CallGraphThread> CallGraphThreads;

/*****/

typedef QMap<uint32_t, SymbolPtr> DefinitionLineToSymbol;
struct FileSamples
{
    DefinitionLineToSymbol defLineToSymbol;
    QHash<uint32_t, SymbolPtr> lineToSymbol;
    QHash<uint32_t, uint32_t> perLine;
    QMap<uint32_t, uint32_t> perAddress;
};

typedef QHash<QString, FileSamples> FileProfile;

/*****/

uint32_t CreateProfile(uint32_t pointerSize, bool needDllExports, const QByteArray& data,
    FlatThreads& flatThreads,
    CallGraphThreads& callGraphThreads,
    FileProfile& fileProfile);
