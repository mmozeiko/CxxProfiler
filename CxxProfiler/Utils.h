#pragma once

#include "Precompiled.h"

class CpuUsage
{
    Q_DISABLE_COPY(CpuUsage)
public:
    CpuUsage();

    uint64_t getProcessTime(HANDLE process) const;

    void update();
    double getUsage(HANDLE process, uint64_t* lastProcessTime) const;

private:
    uint64_t mLastWallTime;
    uint64_t mWallDelta;
    int32_t mCpuCoreCount;

    int32_t getCoresForProcess(HANDLE process) const;
};

void OpenInVisualStudio(const QString& file, uint32_t line);
void OpenInExplorer(const QString& file);
void OpenInEditor(const QString& file);

QString GetSettingsFile();
void DetectVSLocations(QSettings& settings);
