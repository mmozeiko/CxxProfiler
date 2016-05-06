#include "SymbolWidget.h"
#include "Utils.h"
#include "Symbols.h"

ThreadItem::ThreadItem(const QString& name)
    : mName(name)
{
}

QVariant ThreadItem::data(int column, int role) const
{
    if (role == Qt::DisplayRole && column == 0)
    {
        return mName;
    }

    return QTreeWidgetItem::data(column, role);
}

SymbolItem::SymbolItem(const SymbolPtr& symbol, const QString& file, uint32_t line, uint32_t self, uint32_t total, uint32_t all)
    : mSymbol(symbol)
    , mFile(file)
    , mLine(line)
    , mSelf(self)
    , mTotal(total)
    , mAll(all)
{
}

void SymbolItem::exploreToFile() const
{
    OpenInExplorer(mFile);
}

void SymbolItem::openVSFile() const
{
    OpenInVisualStudio(mFile, mLine);
}

void SymbolItem::openFile() const
{
    OpenInEditor(mFile);
}

const QString& SymbolItem::getFilename() const
{
    return mFile;
}

const QString& SymbolItem::getDefinitionFilename() const
{
    return mSymbol->file;
}

uint32_t SymbolItem::getDefinitionLine() const
{
    return mSymbol->line;
}

uint32_t SymbolItem::getDefinitionLineLast() const
{
    return mSymbol->lineLast;
}

uint32_t SymbolItem::getLine() const
{
    return mLine;
}

QVariant SymbolItem::data(int column, int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (column)
        {
        case 0:
            return mSymbol->name;
        case 1:
            return mSelf;
        case 2:
            return mTotal;
        case 3:
            return QString::number(100.0 * mSelf / mAll, 'f', 2);
        case 4:
            return QString::number(100.0 * mTotal / mAll, 'f', 2);
        case 5:
            return mSymbol->module;
        case 6:
            return mLine == 0 || mLine == ~0U ? mFile : QString("%1:%2").arg(mFile).arg(mLine);
        }
    }

    return QTreeWidgetItem::data(column, role);
}

bool SymbolItem::operator < (const QTreeWidgetItem& otherItem) const
{
    const SymbolItem& other = static_cast<const SymbolItem&>(otherItem);

    int64_t cmp = 0;

    switch (treeWidget()->sortColumn())
    {
    case 1:
        cmp = int64_t(mSelf) - int64_t(other.mSelf);
        break;

    case 2:
        cmp = int64_t(mTotal) - int64_t(other.mTotal);
        break;

    case 3:
        cmp = int64_t(mSelf) * other.mAll - int64_t(other.mSelf) * mAll;
        break;

    case 4:
        cmp = int64_t(mTotal) * other.mAll - int64_t(other.mTotal) * mAll;
        break;

    case 5:
        cmp = QString::compare(mSymbol->module, other.mSymbol->module, Qt::CaseInsensitive);
        break;

    case 6:
        cmp = QString::compare(mFile, other.mFile, Qt::CaseInsensitive);
        if (cmp == 0)
        {
            cmp = int64_t(mLine) - other.mLine;
        }
        break;
    }

    if (cmp == 0)
    {
        cmp = QString::compare(mSymbol->name, other.mSymbol->name, Qt::CaseInsensitive);
    }

    return cmp < 0;
}

SymbolWidget::SymbolWidget(QMenu* menu, QWidget* parent)
    : QWidget(parent)
    , mMenu(menu)
{
    ui.setupUi(this);

    ui.cbFilterChoice->addItem("Contains", static_cast<int>(Qt::MatchContains));
    ui.cbFilterChoice->addItem("Equals", static_cast<int>(Qt::MatchFixedString));
    ui.cbFilterChoice->addItem("Starts with", static_cast<int>(Qt::MatchStartsWith));
    ui.cbFilterChoice->addItem("Ends With", static_cast<int>(Qt::MatchEndsWith));
    ui.cbFilterChoice->addItem("Wildcard", static_cast<int>(Qt::MatchWildcard));
    ui.cbFilterChoice->addItem("RegExp", static_cast<int>(Qt::MatchRegExp));

    ui.splitter->setCollapsible(0, false);
    ui.treeWidget->header()->setSectionsMovable(false);
    ui.treeWidget->header()->setSectionsClickable(true);
    ui.treeWidget->header()->resizeSection(0, 400);
    ui.treeWidget->header()->resizeSection(1, 50);
    ui.treeWidget->header()->resizeSection(2, 50);
    ui.treeWidget->header()->resizeSection(3, 50);
    ui.treeWidget->header()->resizeSection(4, 50);
    ui.treeWidget->header()->resizeSection(5, 150);

    QObject::connect(ui.treeWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        mMenu->exec(ui.treeWidget->mapToGlobal(pos));
    });

    QObject::connect(ui.txtFilter, &QLineEdit::returnPressed, ui.btnSearch, &QPushButton::click);
    QObject::connect(ui.txtFilter, &QLineEdit::textChanged, this, [this]()
    {
        mSearchIndex = -1;
    });
    QObject::connect(ui.btnSearch, &QPushButton::clicked, this, [this]()
    {
        if (mSearchIndex == -1)
        {
            Qt::MatchFlag flag = static_cast<Qt::MatchFlag>(ui.cbFilterChoice->itemData(ui.cbFilterChoice->currentIndex()).toInt());
            mSearchItems = ui.treeWidget->findItems(ui.txtFilter->text(), Qt::MatchRecursive | flag);
            mSearchIndex = 0;
        }

        if (!mSearchItems.isEmpty())
        {
            mSearchIndex = (mSearchIndex + 1) % mSearchItems.count();
            QTreeWidgetItem* item = mSearchItems[mSearchIndex];
            if (!item->isHidden())
            {
                ui.treeWidget->setCurrentItem(item);
                ui.treeWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
        }
    });

    QObject::connect(ui.btnFilter, &QPushButton::clicked, this, [this]()
    {
        uint32_t samples = ui.spnMinSamples->value();

        Qt::MatchFlag flag = static_cast<Qt::MatchFlag>(ui.cbFilterChoice->itemData(ui.cbFilterChoice->currentIndex()).toInt());
        QList<QTreeWidgetItem*> items = ui.treeWidget->findItems(ui.txtFilter->text(), Qt::MatchRecursive | flag);

        QSet<const QTreeWidgetItem*> validItems;
        for (const QTreeWidgetItem* item : items)
        {
            if (item->parent() == nullptr)
            {
                continue;
            }

            QList<const QTreeWidgetItem*> childs;
            childs.append(item);

            while (!childs.isEmpty())
            {
                const QTreeWidgetItem* child = childs.takeFirst();
                int count = child->childCount();
                for (int i = 0; i<count; i++)
                {
                    childs.append(child->child(i));
                }
                validItems.insert(child);
            }

            while (item != nullptr)
            {
                validItems.insert(item);
                item = item->parent();
            }
        }

        ui.treeWidget->setUpdatesEnabled(false);
        {
            QList<QTreeWidgetItem*> items;
            items.append(ui.treeWidget->invisibleRootItem());

            while (!items.isEmpty())
            {
                QTreeWidgetItem* child = items.takeFirst();
                int count = child->childCount();
                for (int i = 0; i<count; i++)
                {
                    items.append(child->child(i));
                }

                if (child->parent() == nullptr)
                {
                    child->setHidden(false);
                }
                else
                {
                    bool hidden = child->data(2, Qt::DisplayRole).toUInt() < samples;
                    bool needToHide = !validItems.contains(child);
                    child->setHidden(needToHide || hidden);
                }
            }
        }
        ui.treeWidget->setUpdatesEnabled(true);
    });

    QObject::connect(ui.chkShowAllSymbols, &QCheckBox::toggled, this, &SymbolWidget::toggleShowWithEmptyFiles);
    QObject::connect(ui.spnMinSamples, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SymbolWidget::setMinSamples);
    QObject::connect(ui.spnMinSamples, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SymbolWidget::changeMinimumSamples);

    QObject::connect(ui.treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        QList<QTreeWidgetItem*> items = ui.treeWidget->selectedItems();
        if (items.count() == 1)
        {
            QTreeWidgetItem* item = items[0];
            QTreeWidgetItem* parent = item->parent();
            if (parent != nullptr)
            {
                SymbolItem* symbol = static_cast<SymbolItem*>(item);
                SymbolItem* parentSym;
                if (parent->parent() == nullptr)
                {
                    // flat
                    parentSym = symbol;
                }
                else
                {
                    // graph
                    parentSym = static_cast<SymbolItem*>(parent);
                }

                loadSource(symbol->getFilename(), symbol->getLine(), parentSym->getDefinitionLine(), parentSym->getDefinitionLineLast());
            }
        }
    });

    QObject::connect(ui.txtSource, &SourceWidget::lineClicked, this, [this](int line)
    {
        SymbolItem* item = static_cast<SymbolItem*>(ui.treeWidget->currentItem());
        QString defFname = item->getDefinitionFilename();
        uint32_t defLine = item->getDefinitionLine();
        uint32_t defLineLast = item->getDefinitionLineLast();

        line += mLineOffset - 1;

        if (mCodeLoaded.split('@')[0] == defFname && mLoadingForLine == defLine)
        {
            bool changed = false;

            //disconnect(ui.treeWidget, SIGNAL(itemSelectionChanged()), this, nullptr);

            int childCount = item->childCount();
            for (int i = 0; i<childCount; i++)
            {
                SymbolItem* child = static_cast<SymbolItem*>(item->child(i));
                if (child->getLine() == (uint32_t)line)
                {
                    changed = true;
                    ui.treeWidget->setCurrentItem(child);
                    rememberInHistory();
                    loadSource(child->getDefinitionFilename(), child->getDefinitionLine(), child->getDefinitionLine(), child->getDefinitionLineLast());
                    break;
                }
            }

            //connect(ui.treeWidget, SIGNAL(itemSelectionChanged()), this, SLOT(doItemSelectionChanged()));

            if (!changed)
            {
                SymbolPtr symbol = mCodeLoader->findSymbol(mCodeLoaded.split('@')[0], line);

                if (symbol && mSymbolToTreeItem.contains(symbol))
                {
                    rememberInHistory();
                    ui.treeWidget->setCurrentItem(mSymbolToTreeItem[symbol]);
                    return;
                }
            }
        }
        else
        {
            item->setExpanded(true);
            rememberInHistory();
            loadSource(defFname, defLine, defLine, defLineLast);
        }
    });

    QObject::connect(ui.txtSource, &SourceWidget::backClicked, this, [this]()
    {
        if (!mHistory.isEmpty())
        {
            History elem = mHistory.last();
            mHistory.pop_back();

            //disconnect(ui.treeWidget, SIGNAL(itemSelectionChanged()), this, nullptr);
            QTreeWidgetItem* item = ui.treeWidget->currentItem();
            if (item != nullptr && item->parent() != nullptr)
            {
                ui.treeWidget->setCurrentItem(item->parent());
            }
            //connect(ui.treeWidget, SIGNAL(itemSelectionChanged()), this, SLOT(doItemSelectionChanged()));

            loadSource(elem.file, elem.line, elem.lineFrom, elem.lineTo);
        }
    });

    QObject::connect(&mCodeWatcher, &QFutureWatcher<LoadResult>::finished, this, [this]()
    {
        LoadResult result = mCodeWatcher.result();
        ui.txtSource->setPlainText(result.source);
        ui.txtSource->setPercents(result.percents);
        if (result.loaded)
        {
            mCodeLoaded = result.loadedFileName;
            mLineOffset = result.loadedFrom;
            moveSourceToLine(mLoadingForLine - result.loadedFrom);
        }
        else
        {
           emit moveSourceToLine(0);
        }
    });
}

SymbolWidget::~SymbolWidget()
{
    mCodeWatcher.cancel();
}

QTreeWidget* SymbolWidget::getTree() const
{
    return ui.treeWidget;
}

void SymbolWidget::resetTree(int columnToSort, const SymbolToTreeItem& symbolToTreeItem, SourceLoaderPtr codeLoader)
{
    ui.treeWidget->sortByColumn(columnToSort, Qt::DescendingOrder);
    if (ui.treeWidget->topLevelItemCount() > 0)
    {
        ui.treeWidget->topLevelItem(0)->setExpanded(true);
    }

    mSearchIndex = -1;
    ui.txtFilter->setFocus(Qt::OtherFocusReason);

    emit changeMinimumSamples(ui.spnMinSamples->value());

    resetSource();

    mCodeLoader = codeLoader;
    mSymbolToTreeItem = symbolToTreeItem;

    moveSourceToLine(0);
    mHistory.clear();

    {
        auto items = ui.treeWidget->findItems("main", Qt::MatchFlag::MatchContains | Qt::MatchRecursive);
        if (!items.isEmpty())
        {
            QTreeWidgetItem* item = items.front();
            ui.treeWidget->setCurrentItem(item);
            ui.treeWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }

}

void SymbolWidget::restoreItems()
{
    uint32_t samples = ui.spnMinSamples->value();

    ui.treeWidget->setUpdatesEnabled(false);

    QList<QTreeWidgetItem*> items;
    items.append(ui.treeWidget->invisibleRootItem());

    while (!items.isEmpty())
    {
        QTreeWidgetItem* child = items.takeFirst();
        int count = child->childCount();
        for (int i = 0; i<count; i++)
        {
            items.append(child->child(i));
        }

        bool hidden = child->data(2, Qt::DisplayRole).toUInt() < samples;
        child->setHidden(child->parent() != nullptr && hidden);
    }

    ui.treeWidget->setUpdatesEnabled(true);
}

void SymbolWidget::setShowWithEmptyFiles(bool show)
{
    QSignalBlocker block(ui.chkShowAllSymbols);
    ui.chkShowAllSymbols->setChecked(show);
}

void SymbolWidget::changeMinimumSamples(int samples)
{
    {
        QSignalBlocker block(ui.spnMinSamples);
        ui.spnMinSamples->setValue(samples);
    }

    ui.treeWidget->setUpdatesEnabled(false);

    QList<QTreeWidgetItem*> items;
    items.append(ui.treeWidget->invisibleRootItem());

    while (!items.isEmpty())
    {
        QTreeWidgetItem* child = items.takeFirst();
        int count = child->childCount();
        for (int i = 0; i<count; i++)
        {
            items.append(child->child(i));
        }

        bool hidden = child->data(2, Qt::DisplayRole).toUInt() < static_cast<uint32_t>(samples);
        child->setHidden(child->parent() != nullptr && hidden);
    }

    ui.treeWidget->setUpdatesEnabled(true);
}

void SymbolWidget::loadSource(const QString& fname, uint32_t line, int lineFrom, int lineTo)
{
    mCodeWatcher.cancel();

    mLoadingForLine = line;
    mLineFrom = lineFrom;
    mLineTo = lineTo;

    if (mCodeLoaded != fname)
    {
        mCodeLoaded = QString();
        mCodeLoading = fname;
        moveSourceToLine(0);
        ui.txtSource->setPlainText("[loading...]");

        mCodeWatcher.setFuture(mCodeLoader->load(fname, lineFrom, lineTo));
    }
    else if (!fname.isEmpty())
    {
        moveSourceToLine(line - 1);
    }
    else
    {
        moveSourceToLine(0);
    }
}

void SymbolWidget::moveSourceToLine(uint32_t line)
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    if (line == 0)
    {
        ui.txtSource->setExtraSelections(extraSelections);
        return;
    }

    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = ui.txtSource->textCursor();
    selection.cursor.clearSelection();
    selection.cursor.movePosition(QTextCursor::Start);
    selection.cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);

    extraSelections.append(selection);
    ui.txtSource->setExtraSelections(extraSelections);

    QTextCursor cursor = selection.cursor;
    cursor.movePosition(QTextCursor::Start);
    ui.txtSource->setTextCursor(cursor);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line + ui.txtSource->height() / ui.txtSource->fontMetrics().height() / 2 - 1);
    ui.txtSource->setTextCursor(cursor);
}

void SymbolWidget::resetSource()
{
    ui.txtSource->setPlainText("[source code not available]");
    ui.txtSource->setPercents(QStringList());

    mCodeWatcher.cancel();
    mCodeWatcher.waitForFinished();

    mCodeLoaded = QString();
}

void SymbolWidget::rememberInHistory()
{
    History h;
    h.file = mCodeLoaded.split('@')[0];
    h.line = mLoadingForLine;
    h.lineFrom = mLineFrom;
    h.lineTo = mLineTo;
    mHistory.append(h);
}
