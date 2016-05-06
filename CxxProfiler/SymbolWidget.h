#pragma once

#include "Precompiled.h"
#include "ui_SymbolWidget.h"
#include "SourceLoader.h"

class SourceViewer;

struct Symbol;
typedef QSharedPointer<Symbol> SymbolPtr;

class ThreadItem : public QTreeWidgetItem
{
public:
    explicit ThreadItem(const QString& name);
    QVariant data(int column, int role) const;

private:
    QString mName;
};

typedef QHash<SymbolPtr, QTreeWidgetItem*> SymbolToTreeItem;

class SymbolItem : public QTreeWidgetItem
{
public:
    explicit SymbolItem(const SymbolPtr& symbol, const QString& file, uint32_t line, uint32_t self, uint32_t total, uint32_t all);

    void exploreToFile() const;
    void openVSFile() const;
    void openFile() const;
    const QString& getDefinitionFilename() const;
    const QString& getFilename() const;
    uint32_t getDefinitionLine() const;
    uint32_t getDefinitionLineLast() const;
    uint32_t getLine() const;
    QVariant data(int column, int role) const;
    bool operator < (const QTreeWidgetItem& otherItem) const;

private:
    SymbolPtr mSymbol;
    QString mFile;
    uint32_t mLine;

    uint32_t mSelf;
    uint32_t mTotal;
    uint32_t mAll;
};

class SymbolWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SymbolWidget(QMenu* menu, QWidget* parent);
    ~SymbolWidget();

    QTreeWidget* getTree() const;
    void resetTree(int columnToSort, const SymbolToTreeItem& symbolToTreeItem, SourceLoaderPtr codeLoader);
    void restoreItems();

    void setShowWithEmptyFiles(bool show);

public slots:
    void changeMinimumSamples(int samples);

signals:
    void toggleShowWithEmptyFiles(bool show);
    void setMinSamples(int value);

private:
    Ui::SymbolWidget ui;

    QMenu* mMenu;

    int mSearchIndex = -1;
    QList<QTreeWidgetItem*> mSearchItems;

    struct History
    {
        QString file;
        uint32_t line;
        int lineFrom;
        int lineTo;
    };
    QVector<History> mHistory;

    SourceLoaderPtr mCodeLoader;
    QFutureWatcher<LoadResult> mCodeWatcher;

    QString mCodeLoaded;

    QString mCodeLoading;
    uint32_t mLoadingForLine;
    int mLineFrom;
    int mLineTo;
    int mLineOffset;

    SymbolToTreeItem mSymbolToTreeItem;

    void loadSource(const QString& fname, uint32_t line, int lineFrom, int lineCount);
    void moveSourceToLine(uint32_t line);
    void resetSource();
    void rememberInHistory();
};
