#pragma once

#include "Precompiled.h"
#include "Utils.h"
#include "ui_RunningDialog.h"

class Profiler;

class RunningDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RunningDialog(QWidget* parent, Profiler* profiler);
    ~RunningDialog();

private slots:
    void updateInfo();

private:
    Ui::RunningDialog ui;

    QTimer mTimer;
    CpuUsage mCpuUsage;

    uint64_t mLastProcessTime;
    HANDLE mProcess;

    Profiler* mProfiler;
};
