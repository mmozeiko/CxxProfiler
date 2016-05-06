#include "Symbols.h"

uint32_t CreateProfile(uint32_t pointerSize, bool withEmptyFiles, const QByteArray& data,
    FlatThreads& flatThreads,
    CallGraphThreads& callGraphThreads,
    FileProfile& fileProfile)
{
    uint32_t sampleCount = 0;

    QDataStream in(data);

    // load strings
    QVector<QString> strings;
    {
        uint32_t stringCount;
        in >> stringCount;
        strings.resize(stringCount + 1);
        for (uint32_t i = 0; i<stringCount; i++)
        {
            uint32_t  index;
            in >> index;
            in >> strings[index];
        }
    }

    // load symbols
    QVector<SymbolPtr> symbols;
    {
        uint32_t symbolCount;
        in >> symbolCount;
        symbols.resize(symbolCount + 1);
        for (uint32_t i = 0; i<symbolCount; i++)
        {
            uint32_t index;
            in >> index;

            SymbolPtr& symbol = symbols[index] = SymbolPtr(new Symbol);

            in >> symbol->name;

            if (pointerSize == sizeof(uint32_t))
            {
                uint32_t address;
                in >> address;
                symbol->address = address;
            }
            else
            {
                in >> symbol->address;
            }
            in >> symbol->size;

            in >> index;
            symbol->module = strings[index];

            in >> index;
            symbol->file = strings[index];

            in >> symbol->line >> symbol->lineLast;
        }
    }

    // load threads
    {
        uint32_t threadCount;
        in >> threadCount;

        flatThreads.reserve(threadCount);
        callGraphThreads.reserve(threadCount);

        for (uint32_t i = 0; i<threadCount; i++)
        {
            QString threadName = (i == 0 ? "Main Thread" : QString("Thread #%1").arg(i));

            uint32_t count;
            in >> count;

            struct CallStackEntry
            {
                SymbolPtr symbol;
                uint32_t line;
                uint32_t offset;
            };

            typedef QVector<CallStackEntry> CallStack;
            QVector<CallStack> callStacks;

            // load callstacks
            {
                CallStack callStack;
                bool startingWithEmptyFile = true;

                CallStackEntry lastEntryWithFile = {};

                for (uint32_t k = 0; k<count; k++)
                {
                    CallStackEntry entry;
                    uint32_t id;
                    in >> id >> entry.line >> entry.offset;

                    if (id == 0)
                    {
                        if (!withEmptyFiles)
                        {
                            while (!callStack.isEmpty() && callStack.last().symbol->file.isEmpty())
                            {
                                callStack.pop_back();
                            }
                        }

                        if (!callStack.isEmpty())
                        {
                            callStacks.append(callStack);
                            callStack.clear();
                        }
                        startingWithEmptyFile = true;
                    }
                    else
                    {
                        entry.symbol = symbols[id];

                        if (startingWithEmptyFile && entry.symbol->file.isEmpty())
                        {
                            lastEntryWithFile = entry;
                        }

                        if (startingWithEmptyFile && !entry.symbol->file.isEmpty())
                        {
                            startingWithEmptyFile = false;
                        }

                        if (!startingWithEmptyFile || startingWithEmptyFile && withEmptyFiles)
                        {
                            if (lastEntryWithFile.symbol)
                            {
                                callStack.append(lastEntryWithFile);
                                lastEntryWithFile = {};
                            }
                            callStack.append(entry);
                        }
                    }
                }

                sampleCount += callStacks.count();
            }

            // calculate flat profile
            {
                FlatSymbols flatSymbols;

                for (const CallStack& callStack : callStacks)
                {
                    SymbolPtr symbol = callStack[0].symbol;

                    flatSymbols[symbol].self += 1;
                    flatSymbols[symbol].total += 1;

                    SymbolPtr prev = symbol;

                    for (int k = 1; k<callStack.count(); k++)
                    {
                        symbol = callStack[k].symbol;
                        if (prev != symbol)
                        {
                            flatSymbols[symbol].total += 1;
                            prev = symbol;
                        }
                    }
                }

                if (!flatSymbols.isEmpty())
                {
                    flatThreads.append(FlatThread(threadName, flatSymbols));
                }
            }

            // calculate call graph
            {
                CallGraphSymbol root;

                for (const CallStack& callStack : callStacks)
                {
                    CallGraphSymbol* node = &root;

                    uint32_t parentLine = 0;

                    for (int k = callStack.count() - 1; k >= 0; k--)
                    {
                        const CallStackEntry& entry = callStack[k];
                        const SymbolPtr& symbol = entry.symbol;

                        QPair<SymbolPtr, quint32> key = qMakePair(symbol, parentLine);

                        CallGraphSymbols::iterator it = node->childs.find(key);
                        if (it == node->childs.end())
                        {
                            it = node->childs.insert(key, CallGraphSymbol());
                        }

                        node = &it.value();
                        node->total += 1;

                        parentLine = entry.line;
                    }

                    node->self += 1;
                }

                if (!root.childs.isEmpty())
                {
                    callGraphThreads.append(CallGraphThread(threadName, root));
                }
            }

            // calculate file samples
            {
                for (const CallStack& callStack : callStacks)
                {
                    for (const CallStackEntry& entry : callStack)
                    {
                        const SymbolPtr& symbol = entry.symbol;
                        if (!symbol->file.isEmpty())
                        {
                            FileSamples& samples = fileProfile[symbol->file];
                            if (entry.line != 0)
                            {
                                samples.perLine[entry.line] += 1;
                            }
                            samples.perAddress[entry.offset] += 1;
                        }
                    }

                    uint32_t parentLine = 0;
                    QString parentFname;

                    for (int k = callStack.count() - 1; k >= 0; k--)
                    {
                        const CallStackEntry& entry = callStack[k];
                        const SymbolPtr& symbol = entry.symbol;

                        if (!parentFname.isEmpty() && parentLine != 0)
                        {
                            fileProfile[parentFname].lineToSymbol[parentLine] = symbol;
                        }

                        parentLine = entry.line;
                        parentFname = symbol->file;
                    }
                }
            }

            // symbol definition lines
            for (const SymbolPtr& symbol : symbols)
            {
                if (symbol)
                {
                    fileProfile[symbol->file].defLineToSymbol[symbol->line] = symbol;
                }
            }
        }
    }

    return sampleCount;
}
