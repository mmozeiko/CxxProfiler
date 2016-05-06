#include "NewDialog.h"

namespace
{
    class ProcessInfoItem : public QTreeWidgetItem
    {
        Q_DISABLE_COPY(ProcessInfoItem)
    public:
        ProcessInfoItem(CpuUsage& cpuUsage, DWORD pid, HANDLE handle, const QString& name)
            : mPid(pid)
            , mHandle(handle)
            , mName(name)
            , mUsage(0.0)
            , mLastProcessTime(cpuUsage.getProcessTime(handle))
        {
        }

        ~ProcessInfoItem()
        {
            CloseHandle(mHandle);
        }

        QVariant data(int column, int role) const
        {
            if (role == Qt::DisplayRole)
            {
                switch (column)
                {
                case 0:
                    return static_cast<uint32_t>(mPid);
                case 1:
                    return QString::number(mUsage, 'f', 2);
                case 2:
                    return mName;
                }
            }

            return QTreeWidgetItem::data(column, role);
        }

        bool operator < (const QTreeWidgetItem& otherItem) const
        {
            const ProcessInfoItem& other = static_cast<const ProcessInfoItem&>(otherItem);

            switch (treeWidget()->sortColumn())
            {
                case 1:
                {
                    if (mUsage < other.mUsage)
                    {
                        return true;
                    }
                    else if (mUsage > other.mUsage)
                    {
                        return false;
                    }
                    break;
                }

                case 2:
                {
                    int cmp = QString::compare(mName, other.mName, Qt::CaseInsensitive);
                    if (cmp < 0)
                    {
                        return true;
                    }
                    else if (cmp > 0)
                    {
                        return false;
                    }
                    break;
                }
            }

            return mPid < other.mPid;
        }

        DWORD getPid() const
        {
            return mPid;
        }

        void update(CpuUsage& cpuUsage)
        {
            mUsage = cpuUsage.getUsage(mHandle, &mLastProcessTime);
            emit emitDataChanged();
        }

    private:
        DWORD mPid;
        HANDLE mHandle;
        QString mName;
        double mUsage;
        uint64_t mLastProcessTime;
    };

    bool IsProcessElevated()
    {
        bool result = false;

        HANDLE token;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            TOKEN_ELEVATION elevation;
            DWORD size;
            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
            {
                result = elevation.TokenIsElevated != 0;
            }
            CloseHandle(token);
        }

        return result;
    }
}

NewDialog::NewDialog(QWidget* parent)
    : QDialog(parent)
    , mTimer(this)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    {
        QSettings settings(GetSettingsFile(), QSettings::IniFormat);
        ui.lineRunNewApplication->setText(QDir::toNativeSeparators(settings.value("NewDialog/application", QString()).toString()));
        ui.lineRunNewFolder->setText(QDir::toNativeSeparators(settings.value("NewDialog/folder", QString()).toString()));
        ui.lineRunNewArguments->setText(settings.value("NewDialog/arguments", QString()).toString());
        ui.chkOptionsCapture->setChecked(settings.value("NewDialog/debugOutput", true).toBool());
        ui.chkDownloadSymbols->setChecked(settings.value("NewDialog/downloadSymbols", true).toBool());
        ui.spnOptionsSamplingFreq->setValue(settings.value("NewDialog/samplingFrequency", 5).toInt());
    }

    ui.lblIcon->setPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(64, 64));
    ui.lblInfo->setText(
        "For more accurate results please do following steps when building executable (and dll files it depends):\n"
        "1) enable optimizations (C/C++ -> Optimizations -> Optimization, /O2)\n"
        "2) generate debug information in compiler (C/C++ -> General -> Debug Information Format, /Zi)\n"
        "3) generate debug information in linker (Linker -> Debugging -> Generate Debug Info, /DEBUG)\n"
        "4) for 32-bit build, don't omit frame pointers (C/C++ -> Command Line -> put in Additional Options '/Oy-')\n"
        "5) for VS2013 and newer add /Zo in additional compiler options for enhanced debug information\n"
    );

    if (!IsProcessElevated())
    {
        QPushButton* btnShowAll = ui.btnAttach->addButton("&Show all", QDialogButtonBox::NoRole);

        QIcon icon = QApplication::style()->standardIcon(QStyle::SP_VistaShield);
        btnShowAll->setIcon(icon);

        QObject::connect(btnShowAll, &QPushButton::clicked, this, [this]()
        {
            QVarLengthArray<wchar_t> appArray(qApp->applicationFilePath().length() + 1);
            appArray[qApp->applicationFilePath().toWCharArray(appArray.data())] = 0;

            QVarLengthArray<wchar_t> folderArray(qApp->applicationDirPath().length() + 1);
            folderArray[qApp->applicationDirPath().toWCharArray(folderArray.data())] = 0;

            saveSettings();

            if ((int)ShellExecuteW(nullptr, L"runas", appArray.constData(), L"-new", folderArray.constData(), SW_SHOW) > 32)
            {
                qApp->quit();
            }
        });
    }
    QPushButton* btnAttach = ui.btnAttach->addButton("&Attach", QDialogButtonBox::NoRole);

    QObject::connect(ui.lblDownloadInfo, &QLabel::linkActivated, this, [this]()
    {
        QMessageBox::information(
            this,
            "C/C++ Profiler",
            "<p>This will download symbols from <a href=\"http://support.microsoft.com/kb/311503/\">Microsoft Symbol Server</a>.</p>"
            "<p>It will make profiling session slower when first time while it downloads and caches debug information.</p>");
    });

    QObject::connect(ui.btnRunNewApplication, &QPushButton::clicked, this, [this]()
    {
        QString fname = ui.lineRunNewApplication->text();
        if (fname.isEmpty())
        {
            QSettings settings(GetSettingsFile(), QSettings::IniFormat);
            fname =  settings.value("last", QString()).toString();
        }
        else
        {
            fname = QFileInfo(fname).path();
        }

        fname = QFileDialog::getOpenFileName(
            this,
            "Choose application to run",
            fname,
            "Executables (*.exe)");

        if (!fname.isNull())
        {
            QSettings settings(GetSettingsFile(), QSettings::IniFormat);
            if (settings.isWritable())
            {
                settings.setValue("last", QFileInfo(fname).path());
            }

            ui.lineRunNewApplication->setText(QDir::toNativeSeparators(fname));
            ui.lineRunNewFolder->setText(QDir::toNativeSeparators(QFileInfo(fname).path()));
        }
    });

    QObject::connect(ui.btnRunNewFolder, &QPushButton::clicked, this, [this]()
    {
        QString dir = ui.lineRunNewFolder->text();
        if (dir.isEmpty())
        {
            QSettings settings(GetSettingsFile(), QSettings::IniFormat);
            dir = settings.value("last", QString()).toString();
        }

        dir = QFileDialog::getExistingDirectory(this, "Choose application startup folder", dir);

        if (!dir.isNull())
        {
            QSettings settings(GetSettingsFile(), QSettings::IniFormat);
            if (settings.isWritable())
            {
                settings.setValue("last", dir);
            }

            ui.lineRunNewFolder->setText(QDir::toNativeSeparators(dir));
        }
    });

    QObject::connect(ui.btnRunNew, &QPushButton::clicked, this, [this]()
    {
        if (!ui.lineRunNewApplication->text().isEmpty())
        {
            done(RunNewApplication);
        }
    });

    QObject::connect(btnAttach, &QPushButton::clicked, this, [this]()
    {
        if (!ui.treeAttach->selectedItems().isEmpty())
        {
            done(AttachToProcess);
        }
    });

    QObject::connect(ui.treeAttach, &QTreeWidget::itemDoubleClicked, btnAttach, &QPushButton::click);

    ui.treeAttach->header()->resizeSection(0, 50);
    ui.treeAttach->header()->resizeSection(1, 80);
    ui.treeAttach->header()->resizeSection(2, 300);
    ui.treeAttach->header()->setSectionsMovable(false);
    ui.treeAttach->header()->setSectionsClickable(true);
    ui.treeAttach->sortByColumn(1, Qt::DescendingOrder);

    QObject::connect(&mTimer, &QTimer::timeout, this, &NewDialog::updateProcessList);
    QObject::connect(this, &QDialog::finished, &mTimer, &QTimer::stop);

    emit updateProcessList();
    mTimer.start(1000);
}

NewDialog::~NewDialog()
{
    saveSettings();
}

void NewDialog::saveSettings() const
{
    QSettings settings(GetSettingsFile(), QSettings::IniFormat);
    if (settings.isWritable())
    {
        settings.setValue("NewDialog/application", QDir(ui.lineRunNewApplication->text()).path());
        settings.setValue("NewDialog/folder", QDir(ui.lineRunNewFolder->text()).path());
        settings.setValue("NewDialog/arguments", ui.lineRunNewArguments->text());
        settings.setValue("NewDialog/debugOutput", ui.chkOptionsCapture->isChecked());
        settings.setValue("NewDialog/downloadSymbols", ui.chkDownloadSymbols->isChecked());
        settings.setValue("NewDialog/samplingFrequency", ui.spnOptionsSamplingFreq->value());
    }
}

QString NewDialog::getApplication() const
{
    return ui.lineRunNewApplication->text();
}

QString NewDialog::getFolder() const
{
    return ui.lineRunNewFolder->text();
}

QString NewDialog::getArguments() const
{
    return ui.lineRunNewArguments->text();
}

DWORD NewDialog::getProcessId() const
{
    return static_cast<ProcessInfoItem*>(ui.treeAttach->selectedItems()[0])->getPid();
}

ProfilerOptions NewDialog::getOptions() const
{
    ProfilerOptions opt;
    opt.captureDebugOutputString = ui.chkOptionsCapture->isChecked();
    opt.downloadSymbols = ui.chkDownloadSymbols->isChecked();
    opt.samplingFreqInMs = ui.spnOptionsSamplingFreq->value();
    return opt;
}

void NewDialog::updateProcessList()
{
    mCpuUsage.update();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snap)
    {
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    QSet<DWORD> activeProcesses;
    QVector<ProcessInfoItem*> newProcesses;

    if (Process32FirstW(snap, &pe32))
    {
        DWORD myPid = GetCurrentProcessId();
        do
        {
            DWORD pid = pe32.th32ProcessID;

            if (pid == myPid)
            {
                continue;
            }

            activeProcesses.insert(pid);
            if (!mProcessIds.contains(pid))
            {
                HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
                if (handle != nullptr)
                {
                    newProcesses.append(new ProcessInfoItem(mCpuUsage, pid, handle, QString::fromWCharArray(pe32.szExeFile)));
                }
            }
        } while (Process32NextW(snap, &pe32));

        ui.treeAttach->setUpdatesEnabled(false);
        ui.treeAttach->setSortingEnabled(false);
        for (int i = 0; i < ui.treeAttach->topLevelItemCount(); i++)
        {
            ProcessInfoItem* item = static_cast<ProcessInfoItem*>(ui.treeAttach->topLevelItem(i));

            if (activeProcesses.contains(item->getPid()))
            {
                item->update(mCpuUsage);
            }
            else
            {
                mProcessIds.remove(item->getPid());
                delete ui.treeAttach->takeTopLevelItem(i);
            }
        }
        for (auto process : newProcesses)
        {
            mProcessIds.insert(process->getPid());
            ui.treeAttach->addTopLevelItem(process);
        }
        ui.treeAttach->setSortingEnabled(true);
        ui.treeAttach->sortByColumn(ui.treeAttach->sortColumn(), ui.treeAttach->header()->sortIndicatorOrder());
        ui.treeAttach->setUpdatesEnabled(true);
    }

    CloseHandle(snap);
}
