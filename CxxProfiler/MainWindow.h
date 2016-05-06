#pragma once

#include "Precompiled.h"
#include "ui_MainWindow.h"

class SymbolWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow();

private slots:
    void toggleShowWithEmptyFiles(bool show);
    void popupAction(QAction* action);
    bool saveData();

private:
    Ui::MainWindow ui;

    QTabWidget* mTabs;
    SymbolWidget* mCallGraph;
    SymbolWidget* mFlatProfile;

    QAction* actGoto;
    QAction* actCopyName;
    QAction* actCopyModule;
    QAction* actCopyFilename;
    QAction* actHide;
    QAction* actHideAll;
    QAction* actRestore;
    QAction* actExpand;
    QAction* actCollapse;
    QAction* actExploreTo;
    QAction* actOpenSymbolFile;
    QAction* actOpenSymbolVS;

    bool mShowWithEmptyFiles = false;

    QByteArray mData;
    bool mDataSaved = true;
    uint32_t mDataPointerSize;

    void loadData(uint32_t pointerSize, const QByteArray& data);

    void closeEvent(QCloseEvent* ev) override;
};
