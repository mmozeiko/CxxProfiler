#pragma once

#include "Precompiled.h"
#include "Utils.h"
#include "Profiler.h"
#include "ui_NewDialog.h"

class NewDialog : public QDialog
{
    Q_OBJECT
public:
    enum
    {
        RunNewApplication = -1,
        AttachToProcess = -2,
    };

    explicit NewDialog(QWidget* parent);
    ~NewDialog();

    QString getApplication() const;
    QString getFolder() const;
    QString getArguments() const;

    DWORD getProcessId() const;

    ProfilerOptions getOptions() const;

private slots:
    void updateProcessList();

private:
    Ui::NewDialog ui;
    QTimer mTimer;
    CpuUsage mCpuUsage;

    QSet<DWORD> mProcessIds;

    void saveSettings() const;
};
