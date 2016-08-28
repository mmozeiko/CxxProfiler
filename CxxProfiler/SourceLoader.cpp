#include "SourceLoader.h"
#include "Utils.h"
#include <limits.h>

enum
{
    ADDITIONAL_LINES = 10,
    MAX_FILE_SIZE = 256 * 1024,
};

SourceLoader::SourceLoader(uint32_t totalSamples, const FileProfile& fileProfile)
    : totalSamples(totalSamples)
    , fileProfile(fileProfile)
{
    QSettings settings(GetSettingsFile(), QSettings::IniFormat);
    vs2013 = QDir::toNativeSeparators(settings.value("Preferences/VS2013", QString()).toString());
    vs2015 = QDir::toNativeSeparators(settings.value("Preferences/VS2015", QString()).toString());
    sdk10 = QDir::toNativeSeparators(settings.value("Preferences/SDK10", QString()).toString());
}

SourceLoader::~SourceLoader()
{
}

SymbolPtr SourceLoader::findSymbol(const QString& fname, int line) const
{
    auto& samples = fileProfile[fname];
    auto it = samples.lineToSymbol.find(line);
    if (it == samples.lineToSymbol.end())
    {
        return SymbolPtr();
    }
    else
    {
        return it.value();
    }
}

QFuture<LoadResult> SourceLoader::load(const QString& fname, int lineFrom, int lineTo)
{
    return QtConcurrent::run([=]()
    {
        LoadResult result;

        if (fname.isEmpty())
        {
            result.source = QString("[source file not available]");
            result.loaded = false;
            return result;
        }

        QString fname2 = fname;
        if (fname.startsWith("f:\\dd\\vctools\\crt\\crtw32\\") && !vs2013.isEmpty())
        {
            QString path = vs2013 + "\\crt\\src\\";
            if (QFileInfo(path + QFileInfo(fname).fileName()).isFile())
            {
                fname2 = path + QFileInfo(fname).fileName();
            }
            else
            {
                path = vs2013 + "\\crt\\src\\intel\\";
                if (QFileInfo(path + QFileInfo(fname).fileName()).isFile())
                {
                    fname2 = path + QFileInfo(fname).fileName();
                }
            }
        }
        else if (fname.startsWith("f:\\dd\\vctools\\crt\\vcstartup\\src\\startup\\") && !vs2015.isEmpty())
        {
            QString path = vs2015 + "\\crt\\src\\vcruntime\\";
            if (QFileInfo(path + QFileInfo(fname).fileName()).isFile())
            {
                fname2 = path + QFileInfo(fname).fileName();
            }
        }
        else if (fname.startsWith("d:\\th\\minkernel\\crts\\ucrt\\src\\appcrt\\") && !sdk10.isEmpty())
        {
            QString path = sdk10 + "\\ucrt\\" + fname.right(fname.length() - QLatin1String("d:\\th\\minkernel\\crts\\ucrt\\src\\appcrt\\").size());
            if (QFileInfo(path).isFile())
            {
                fname2 = path;
            }
        }

        QFile file(fname2);
        if (!file.exists())
        {
            result.source = QString("[source file doesn't exist - %1]").arg(fname);
            result.loaded = false;
            return result;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            result.source = QString("[error opening file - %1]").arg(file.errorString());
            result.loaded = false;
            return result;
        }

        int loadLineFrom = lineFrom;
        int loadLineTo = lineTo;
        if (file.size() < MAX_FILE_SIZE)
        {
            result.loadedFileName = fname;
            loadLineFrom = ADDITIONAL_LINES;
            loadLineTo = INT_MAX - 1 - ADDITIONAL_LINES;
        }
        else
        {
            result.loadedFileName = QString("%1@%2@%3").arg(fname).arg(lineFrom).arg(lineTo);
        }
        loadLineFrom = qMax(1, loadLineFrom - ADDITIONAL_LINES);
        loadLineTo = qMin(INT_MAX - 1, loadLineTo + ADDITIONAL_LINES);

        result.loadedFrom = loadLineFrom;

        QStringList lines;
        QStringList percents;

        auto prof = fileProfile.find(fname);
        int lineNr = 1;

        bool needEmptyLine = false;

        QTextStream stream(&file);
        for (;;)
        {
            QString line = stream.readLine();
            if (line.isNull())
            {
                break;
            }

            if (lineNr >= loadLineFrom && lineNr <= loadLineTo)
            {
                if (prof != fileProfile.end() && prof->perLine.contains(lineNr))
                {
                    uint32_t samples = prof->perLine[lineNr];
                    percents.append(QString("%1%").arg(samples * 100.0 / totalSamples, 0, 'f', 2));
                }
                else
                {
                    percents.append(QString());
                }

                if (needEmptyLine)
                {
                    if (!line.trimmed().isEmpty())
                    {
                        lines.append(QString::null);
                    }
                    needEmptyLine = false;
                }

                lines.append(line);
            }

            lineNr++;
        }

        result.percents = percents;
        result.source = lines.join("\n");
        result.loaded = true;
        return result;
    });
}
