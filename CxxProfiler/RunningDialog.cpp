#include "RunningDialog.h"
#include "Profiler.h"

RunningDialog::RunningDialog(QWidget* parent, Profiler* profiler)
    : QDialog(parent)
    , mTimer(this)
    , mProcess(nullptr)
    , mProfiler(profiler)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui.pbProgress->hide();
    ui.txtLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui.btnBox->addButton("&Stop", QDialogButtonBox::AcceptRole);

    QObject::connect(mProfiler, &Profiler::message, ui.txtLog, &QPlainTextEdit::appendPlainText, Qt::QueuedConnection);

    QObject::connect(ui.txtLog, &QPlainTextEdit::textChanged, this, [this]()
    {
        QScrollBar* bar = ui.txtLog->verticalScrollBar();
        if ((bar->maximum() - bar->value()) < bar->singleStep())
        {
            bar->triggerAction(QAbstractSlider::SliderToMaximum);
        }
    });

    QObject::connect(mProfiler, &Profiler::attached, this, [this](HANDLE process)
    {
        ui.pbProgress->show();
        mProcess = process;
        mLastProcessTime = mCpuUsage.getProcessTime(process);
        emit updateInfo();
        mTimer.start(100);
    }, Qt::QueuedConnection);

    QObject::connect(this, &QDialog::finished, mProfiler, &Profiler::stop);
    QObject::connect(mProfiler, &Profiler::finished, this, &QDialog::accept, Qt::QueuedConnection);

    QObject::connect(&mTimer, &QTimer::timeout, this, &RunningDialog::updateInfo);
}

RunningDialog::~RunningDialog()
{
}

void RunningDialog::updateInfo()
{
    const double MegaByte = 1024.0 * 1024.0;

    ui.txtThreadCount->setText(QString::number(mProfiler->getThreadCount()));
    ui.txtCollectedSamples->setText(QString::number(mProfiler->getCollectedSamples()));

    mCpuUsage.update();
    double usage = mCpuUsage.getUsage(mProcess, &mLastProcessTime);
    ui.txtCpuUsage->setText(QString("%1 %").arg(usage, 0, 'f', 2));

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
    {
        ui.txtVirtualMemory->setText(QString("%1 MB").arg((status.ullTotalVirtual - status.ullAvailVirtual) / MegaByte, 0, 'f', 2));
    }

    PROCESS_MEMORY_COUNTERS_EX counters;
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(mProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), counters.cb))
    {
        ui.txtPrivateBytes->setText(QString("%1 MB").arg(counters.PrivateUsage / MegaByte, 0, 'f', 2));
        ui.txtWorkingSet->setText(QString("%1 MB").arg(counters.WorkingSetSize / MegaByte, 0, 'f', 2));
    }
}
