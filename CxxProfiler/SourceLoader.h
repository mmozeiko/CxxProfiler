#pragma once

#include "Precompiled.h"
#include "Symbols.h"

struct LoadResult
{
    QString loadedFileName;
    int loadedFrom;
    QString source;
    QStringList percents;
    bool loaded;
};

class SourceLoader : public QObject
{
    Q_OBJECT
public:
    explicit SourceLoader(uint32_t totalSamples, const FileProfile& fileProfile);
    ~SourceLoader();

    QFuture<LoadResult> load(const QString& fname, int lineFrom, int lineTo);

    SymbolPtr findSymbol(const QString& fname, int line) const;

private:
    uint32_t totalSamples;
    FileProfile fileProfile;
};

typedef QSharedPointer<SourceLoader> SourceLoaderPtr;
