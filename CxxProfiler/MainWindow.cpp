#include "MainWindow.h"
#include "NewDialog.h"
#include "Preferences.h"
#include "RunningDialog.h"
#include "Profiler.h"
#include "SymbolWidget.h"
#include "Symbols.h"
#include "Utils.h"
#include "Version.h"

MainWindow::MainWindow()
{
    ui.setupUi(this);

    {
        QSettings settings(GetSettingsFile(), QSettings::IniFormat);
        if (settings.contains("MainWindow/geometry"))
        {
            restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
        }
        if (!settings.contains("Preferences/VS2013"))
        {
            DetectVSLocations(settings);
        }
    }

    ui.actFileSave->setDisabled(true);

    setStatusBar(nullptr);

    QMenu* contextMenu = new QMenu(this);
    {
        actGoto = contextMenu->addAction(QString());

        QObject::connect(actGoto, &QAction::triggered, this, [this]()
        {
            SymbolWidget* symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
            QTreeWidget* tree = symbolWidget->getTree();

            QTreeWidget* otherTree = (symbolWidget == mFlatProfile ? mCallGraph->getTree() : mFlatProfile->getTree());

            QTreeWidgetItem* item = tree->selectedItems()[0];

            QList<QTreeWidgetItem*> items = otherTree->findItems(item->data(0, Qt::DisplayRole).toString(), Qt::MatchFixedString | Qt::MatchRecursive, 0);
            if (!items.isEmpty())
            {
                if (!items[0]->isHidden())
                {
                    mTabs->setCurrentIndex(1 - mTabs->currentIndex());
                    otherTree->setCurrentItem(items[0]);
                    otherTree->scrollToItem(items[0], QAbstractItemView::PositionAtCenter);
                }
            }
        });

        contextMenu->addSeparator();

        actCopyName = contextMenu->addAction("Copy name");
        actCopyModule = contextMenu->addAction("Copy module");
        actCopyFilename = contextMenu->addAction("Copy filename");

        contextMenu->addSeparator();

        actHide = contextMenu->addAction("Hide");

        QObject::connect(actHide, &QAction::triggered, this, [this]()
        {
            SymbolWidget* symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
            QTreeWidget* tree = symbolWidget->getTree();

            tree->setUpdatesEnabled(false);

            QList<QTreeWidgetItem*> items = tree->selectedItems();
            for (QTreeWidgetItem* item : items)
            {
                if (item->parent() != nullptr)
                {
                    item->setHidden(true);
                }
            }

            tree->setUpdatesEnabled(true);
        });

        actHideAll = contextMenu->addAction("Hide All");

        QObject::connect(actHideAll, &QAction::triggered, this, [this]()
        {
            SymbolWidget* symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
            QTreeWidget* tree = symbolWidget->getTree();

            tree->setUpdatesEnabled(false);

            QList<QTreeWidgetItem*> items = tree->selectedItems();
            foreach (QTreeWidgetItem* item, items)
            {
                QList<QTreeWidgetItem*> allItems = tree->findItems(item->data(0, Qt::DisplayRole).toString(), Qt::MatchRecursive | Qt::MatchFixedString);

                for (QTreeWidgetItem* i : allItems)
                {
                    if (i->parent() != nullptr)
                    {
                        i->setHidden(true);
                    }
                }
            }

            tree->setUpdatesEnabled(true);
        });

        actRestore = contextMenu->addAction("Restore All");

        QObject::connect(actRestore, &QAction::triggered, this, [this]()
        {
            SymbolWidget* symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
            symbolWidget->restoreItems();
        });

        contextMenu->addSeparator();

        actExpand = contextMenu->addAction("Expand all");
        actCollapse = contextMenu->addAction("Collapse all");

        contextMenu->addSeparator();

        actExploreTo = contextMenu->addAction("Explore to file");
        actOpenSymbolFile = contextMenu->addAction("Open file");
        actOpenSymbolVS = contextMenu->addAction("Open file in Visual Studio");

        QObject::connect(contextMenu, &QMenu::triggered, this, &MainWindow::popupAction);

        QObject::connect(contextMenu, &QMenu::aboutToShow, this, [this]()
        {
            bool singleSelected = false;
            bool somethingSelected = false;
            bool symbolSelected = true;
            bool fileExists = false;
            bool flatView = false;

            SymbolWidget* symbolWidget = nullptr;

            bool visible = mTabs->isVisible();
            if (visible)
            {
                symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
                QTreeWidget* tree = symbolWidget->getTree();

                flatView = symbolWidget == mFlatProfile;

                auto items = tree->selectedItems();

                somethingSelected = !items.isEmpty();
                singleSelected = items.count() == 1;
                symbolSelected = false;
                QTreeWidgetItem* symbol = nullptr;

                for (QTreeWidgetItem* item : items)
                {
                    if (item->parent() != nullptr)
                    {
                        symbolSelected = true;
                        symbol = item;
                        break;
                    }
                }

                if (symbolSelected)
                {
                    SymbolItem* symbolItem = static_cast<SymbolItem*>(symbol);
                    const QString& fname = symbolItem->getFilename();
                    fileExists = QFile::exists(fname);
                }
            }

            actGoto->setText(visible && symbolWidget == mCallGraph ? "Search in flat view" : "Search in call graph");
            actGoto->setEnabled(visible && singleSelected);

            actCopyName->setEnabled(visible && singleSelected);
            actCopyModule->setEnabled(visible && singleSelected && symbolSelected && flatView);
            actCopyFilename->setEnabled(visible && singleSelected && symbolSelected);

            actHide->setEnabled(visible && symbolSelected);
            actHideAll->setEnabled(visible && symbolSelected);
            actRestore->setEnabled(visible);

            actExpand->setEnabled(visible && somethingSelected);
            actCollapse->setEnabled(visible && somethingSelected);

            actExploreTo->setEnabled(visible && singleSelected && symbolSelected && fileExists);
            actOpenSymbolFile->setEnabled(visible && singleSelected && symbolSelected && fileExists);
            actOpenSymbolVS->setEnabled(visible && singleSelected && symbolSelected && fileExists);
        });
    }

    mFlatProfile = new SymbolWidget(contextMenu, this);

    mCallGraph = new SymbolWidget(contextMenu, this);
    mCallGraph->getTree()->headerItem()->setData(6, Qt::DisplayRole, "Called from");
    mCallGraph->getTree()->hideColumn(5);

    QObject::connect(mFlatProfile, &SymbolWidget::toggleShowWithEmptyFiles, this, &MainWindow::toggleShowWithEmptyFiles);
    QObject::connect(mCallGraph, &SymbolWidget::toggleShowWithEmptyFiles, this, &MainWindow::toggleShowWithEmptyFiles);

    QObject::connect(mFlatProfile, &SymbolWidget::setMinSamples, mCallGraph, &SymbolWidget::changeMinimumSamples);
    QObject::connect(mCallGraph, &SymbolWidget::setMinSamples, mFlatProfile, &SymbolWidget::changeMinimumSamples);

    mTabs = new QTabWidget;
    mTabs->addTab(mCallGraph, "Call Graph");
    mTabs->addTab(mFlatProfile, "Flat Profile");

    QObject::connect(ui.actHelpAbout, &QAction::triggered, this, [this]()
    {
        QMessageBox::about(
            this,
            "C/C++ Profiler",
            QString(
                "<h3>C/C++ Profiler v%1</h3>"
                "<p>Copyright (C) 2015 Martins Mozeiko</p>"
                "<p>E-mail: <a href=\"mailto:martins.mozeiko@gmail.com\">martins.mozeiko@gmail.com</a></p>"
                "<p>Web: <a href=\"https://github.com/mmozeiko/CxxProfiler\">https://github.com/mmozeiko/CxxProfiler</a></p>"
            ).arg(QApplication::applicationVersion()));
    });
    QObject::connect(ui.actHelpAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    QObject::connect(ui.actFileNew, &QAction::triggered, this, [this]()
    {
        if (!mDataSaved)
        {
            QMessageBox::StandardButton ret = QMessageBox::question(
                this,
                qApp->applicationName(),
                "Profiling information has not been saved.\n"
                "Do you want to save?",
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (ret == QMessageBox::Save)
            {
                if (!saveData())
                {
                    return;
                }
            }
            else if (ret == QMessageBox::Cancel)
            {
                return;
            }
            else // ret == QMessageBox::Discard
            {
                // pass
            }
        }

        NewDialog newDialog(this);
        int action = newDialog.exec();
        if (action == QDialog::Rejected)
        {
            return;
        }

        QByteArray data;
        uint32_t pointerSize = 0;
        {
            ProfilerOptions opt = newDialog.getOptions();
            Profiler profiler(opt);

            RunningDialog runningDialog(this, &profiler);

            if (action == NewDialog::RunNewApplication)
            {
                profiler.execute(newDialog.getApplication(), newDialog.getFolder(), newDialog.getArguments());
            }
            else if (action == NewDialog::AttachToProcess)
            {
                profiler.attach(newDialog.getProcessId());
            }

            if (runningDialog.exec() == QDialog::Accepted)
            {
                data = profiler.serializeCallStacks();
                pointerSize = profiler.getSizeOfPointer();
            }
        }
        if (!data.isEmpty())
        {
            loadData(pointerSize, data);
            mDataSaved = false;
        }
    });

    QObject::connect(ui.actFileOpen, &QAction::triggered, this, [this]()
    {
        if (!mDataSaved)
        {
            QMessageBox::StandardButton ret = QMessageBox::question(
                this,
                qApp->applicationName(),
                "Profiling information has not been saved.\n"
                "Do you want to save?",
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (ret == QMessageBox::Save)
            {
                if (!saveData())
                {
                    return;
                }
            }
            else if (ret == QMessageBox::Cancel)
            {
                return;
            }
            else // ret == QMessageBox::Discard
            {
                // pass
            }
        }

        QSettings settings(GetSettingsFile(), QSettings::IniFormat);
        QString lastFolder = settings.value("last", QString()).toString();

        QString fname = QFileDialog::getOpenFileName(this, qApp->applicationName(), lastFolder, "CxxProfiler Data (*.profiler)");
        if (!fname.isNull())
        {
            if (settings.isWritable())
            {
                settings.setValue("last", QFileInfo(fname).path());
            }

            QFile file(fname);
            if (!file.open(QIODevice::ReadOnly))
            {
                QMessageBox::critical(this, qApp->applicationName(), file.errorString());
                return;
            }

            char id[4];
            uint32_t version;
            uint32_t pointerSize;

            QDataStream in(&file);
            if (in.readRawData(id, 4) != 4 || memcmp(id, CXX_PROFILER_FILE_ID, 4) != 0)
            {
                QMessageBox::critical(this, qApp->applicationName(), "Invalid file header");
                return;
            }

            in >> version;
            if (in.status() != QDataStream::Ok || version != CXX_PROFILER_FILE_VERSION)
            {
                QMessageBox::critical(this, qApp->applicationName(), "Unsupported file version");
                return;
            }

            in >> pointerSize;
            if (in.status() != QDataStream::Ok)
            {
                QMessageBox::critical(this, qApp->applicationName(), "Failed to load data");
                return;
            }

            QByteArray data;
            in >> data;
            if (in.status() != QDataStream::Ok)
            {
                QMessageBox::critical(this, "Profiler Error", qApp->applicationName());
                return;
            }

            loadData(pointerSize, qUncompress(data));
            mDataSaved = true;
        }
    });

    QObject::connect(ui.actFilePreferences, &QAction::triggered, this, [this]()
    {
        Preferences(this).exec();
    });

    QObject::connect(ui.actFileSave, &QAction::triggered, this, &MainWindow::saveData);

    QObject::connect(ui.actFileQuit, &QAction::triggered, this, &QWidget::close);

    if (qApp->arguments().size() > 1 && qApp->arguments().at(1) == "-new")
    {
        QTimer::singleShot(0, ui.actFileNew, &QAction::trigger);
    }
}

MainWindow::~MainWindow()
{
    QSettings settings(GetSettingsFile(), QSettings::IniFormat);
    if (settings.isWritable())
    {
        settings.setValue("MainWindow/geometry", saveGeometry());
    }
}

void MainWindow::toggleShowWithEmptyFiles(bool show)
{
    SymbolWidget* callGraph = static_cast<SymbolWidget*>(mTabs->widget(0));
    SymbolWidget* flatProfile = static_cast<SymbolWidget*>(mTabs->widget(1));
    callGraph->setShowWithEmptyFiles(show);
    flatProfile->setShowWithEmptyFiles(show);

    mShowWithEmptyFiles = show;
    loadData(mDataPointerSize, mData);
}

void MainWindow::popupAction(QAction* action)
{
    SymbolWidget* symbolWidget = static_cast<SymbolWidget*>(mTabs->currentWidget());
    QTreeWidget* tree = symbolWidget->getTree();

    if (action == actCopyName || action == actCopyModule || action == actCopyFilename)
    {
        QTreeWidgetItem* item = tree->selectedItems()[0];
        QClipboard* clipboard = QApplication::clipboard();

        int column;
        if (action == actCopyName)
        {
            column = 0;
        }
        else if (action == actCopyModule)
        {
            column = 5;
        }
        else
        {
            column = 6;
        }

        clipboard->setText(item->data(column, Qt::DisplayRole).toString());
    }
    else if (action == actExploreTo || action == actOpenSymbolFile || action == actOpenSymbolVS)
    {
        QTreeWidgetItem* item = tree->selectedItems()[0];
        if (item->parent() != nullptr)
        {
            SymbolItem* symbolItem = static_cast<SymbolItem*>(item);
            if (action == actExploreTo)
            {
                symbolItem->exploreToFile();
            }
            else if (action == actOpenSymbolFile)
            {
                symbolItem->openFile();
            }
            else
            {
                symbolItem->openVSFile();
            }
        }
    }
    else if (action == actExpand || action == actCollapse)
    {
        tree->setUpdatesEnabled(false);

        QList<QTreeWidgetItem*> items = tree->selectedItems();
        while (!items.isEmpty())
        {
            QTreeWidgetItem* item = items.takeFirst();
            int count = item->childCount();
            for (int i=0; i<count; i++)
            {
                items.append(item->child(i));
            }

            if (action == actExpand)
            {
                tree->expandItem(item);
            }
            else
            {
                tree->collapseItem(item);
            }
        }

        tree->setUpdatesEnabled(true);
    }
}

void MainWindow::loadData(uint32_t pointerSize, const QByteArray& data)
{
    mData = data;
    mDataPointerSize = pointerSize;

    QTreeWidget* flatWidget = mFlatProfile->getTree();
    QTreeWidget* callGraphWidget = mCallGraph->getTree();

    FlatThreads flatThreads;
    CallGraphThreads callGraphThreads;
    FileProfile fileProfile;

    uint32_t totalCount = CreateProfile(pointerSize, mShowWithEmptyFiles, data, flatThreads, callGraphThreads, fileProfile);

    flatWidget->setUpdatesEnabled(false);
    flatWidget->clear();

    callGraphWidget->setUpdatesEnabled(false);
    callGraphWidget->clear();

    SymbolToTreeItem flatItems;

    for (const FlatThread& flatThread : flatThreads)
    {
        if (flatThread.second.isEmpty())
        {
            continue;
        }
        QTreeWidgetItem* item = new ThreadItem(flatThread.first);

        for (auto it = flatThread.second.begin(), eit = flatThread.second.end(); it != eit; ++it)
        {
            QTreeWidgetItem* child = new SymbolItem(it.key(), it.key()->file, it.key()->line, it.value().self, it.value().total, totalCount);
            flatItems.insert(it.key(), child);
            item->addChild(child);
        }

        flatWidget->addTopLevelItem(item);
    }

    for (const CallGraphThread& callGraphThread : callGraphThreads)
    {
        if (callGraphThread.second.childs.isEmpty())
        {
            continue;
        }
        struct Creator
        {
            Creator(uint32_t totalCount) : mTotalCount(totalCount)
            {
            }

            void addChilds(QTreeWidgetItem* item, const SymbolPtr& parent, const CallGraphSymbol& node)
            {
                for (auto it = node.childs.begin(), eit = node.childs.end(); it != eit; ++it)
                {
                    QString file = parent ? (parent->file.isEmpty() ? parent->module : parent->file) : QString();
                    QTreeWidgetItem* child = new SymbolItem(it.key().first, file, it.key().second, it.value().self, it.value().total, mTotalCount);
                    addChilds(child, it.key().first, it.value());
                    item->addChild(child);
                }
            }

            uint32_t mTotalCount;
        };

        QTreeWidgetItem* item = new ThreadItem(callGraphThread.first);
        Creator(totalCount).addChilds(item, SymbolPtr(), callGraphThread.second);
        callGraphWidget->addTopLevelItem(item);
    }

    SourceLoaderPtr codeLoader(new SourceLoader(totalCount, fileProfile));

    mFlatProfile->resetTree(1, flatItems, codeLoader);
    flatWidget->setUpdatesEnabled(true);
    flatWidget->setVisible(true);

    mCallGraph->resetTree(1, SymbolToTreeItem(), codeLoader);
    callGraphWidget->setUpdatesEnabled(true);
    callGraphWidget->setVisible(true);

    emit ui.actFileSave->setEnabled(true);
    setCentralWidget(mTabs);
}

bool MainWindow::saveData()
{
    QSettings settings(GetSettingsFile(), QSettings::IniFormat);
    QString lastFolder = settings.value("last", QString()).toString();

    QString fname = QFileDialog::getSaveFileName(this, qApp->applicationName(), lastFolder, "CxxProfiler Data (*.profiler)");
    if (fname.isNull())
    {
        return false;
    }

    if (settings.isWritable())
    {
        settings.setValue("last", QFileInfo(fname).path());
    }

    bool success = false;

    QFile file(fname);
    if (file.open(QIODevice::WriteOnly))
    {
        QDataStream out(&file);
        out.writeRawData(CXX_PROFILER_FILE_ID, sizeof(CXX_PROFILER_FILE_ID));
        out << CXX_PROFILER_FILE_VERSION
            << mDataPointerSize
            << qCompress(mData);

        success = out.status() == QDataStream::Ok;
    }

    if (success)
    {
        mDataSaved = true;
    }
    else
    {
        QMessageBox::critical(this, qApp->applicationName(), file.errorString());
    }

    return success;
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    if (mDataSaved)
    {
        ev->accept();
        return;
    }

    QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        qApp->applicationName(),
        "Profiling information has not been saved.\n"
        "Do you want to save?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)
    {
        if (saveData())
        {
            ev->accept();
        }
        else
        {
            ev->ignore();
        }
    }
    else if (ret == QMessageBox::Discard)
    {
        ev->accept();
    }
    else // ret == QMessageBox::Cancel
    {
        ev->ignore();
    }
}
