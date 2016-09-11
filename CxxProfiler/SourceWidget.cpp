#include "SourceWidget.h"
#include "SyntaxHighlighter.h"

namespace
{
    class PercentAreaWidget : public QWidget
    {
    public:
        explicit PercentAreaWidget(SourceWidget* source)
            : QWidget(source)
            , mSource(source)
        {
            setMouseTracking(true);
        }

        QSize sizeHint() const override
        {
            return QSize(mSource->percentAreaWidth(), 0);
        }

    private:
        SourceWidget* mSource;

        void paintEvent(QPaintEvent *ev) override
        {
            mSource->percentAreaPaintEvent(ev);
        }

        void mouseMoveEvent(QMouseEvent* ev) override
        {
            mSource->percentMouseMoveEvent(ev);
        }

        void mouseReleaseEvent(QMouseEvent* ev) override
        {
            mSource->percentMouseReleaseEvent(ev);
        }

        void leaveEvent(QEvent* event) override
        {
            mSource->percentLeaveEvent(event);
        }
    };
}

SourceWidget::SourceWidget(QWidget* parent)
    : QPlainTextEdit(parent)
    , mActiveLine(-1)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new SyntaxHighlighter(document());

    mPercentArea = new PercentAreaWidget(this);

    QObject::connect(this, &QPlainTextEdit::blockCountChanged, this, [this]()
    {
        setViewportMargins(percentAreaWidth(), 0, 0, 0);
    });

    QObject::connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy)
    {
        if (dy == 0)
        {
            mPercentArea->update(0, rect.y(), mPercentArea->width(), rect.height());
        }
        else
        {
            mPercentArea->scroll(0, dy);
        }
        
        if (rect.contains(viewport()->rect()))
        {
            emit blockCountChanged(0);
        }
    });

    emit blockCountChanged(0);
}

SourceWidget::~SourceWidget()
{
}

int SourceWidget::percentAreaWidth() const
{
    return 6 + fontMetrics().width(QLatin1Char('9')) * 7;
}

void SourceWidget::setPercents(const QStringList& percents)
{
    mPercents = percents;
    mActiveLine = -1;
}

void SourceWidget::percentAreaPaintEvent(QPaintEvent* ev)
{
    QPainter painter(mPercentArea);
    painter.setBrush(QBrush(QColor(Qt::lightGray)));
    painter.drawLine(ev->rect().right(), ev->rect().top(), ev->rect().right(), ev->rect().bottom());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    while (block.isValid() && top <= ev->rect().bottom())
    {
        if (block.isVisible() && bottom >= ev->rect().top())
        {
            if (blockNumber < mPercents.count())
            {
                QString percent = mPercents[blockNumber];
                painter.setPen(mActiveLine == blockNumber ? Qt::blue : Qt::red);
                painter.drawText(0, top, mPercentArea->width() - 3, fontMetrics().height(), Qt::AlignRight, percent);
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void SourceWidget::percentMouseMoveEvent(QMouseEvent* ev)
{
    mActiveLine = -1;
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    int y = ev->y();

    while (block.isValid() && block.isVisible())
    {
        if (y >= top && y < bottom)
        {
            if (blockNumber < mPercents.count() && !mPercents[blockNumber].isEmpty())
            {
                mActiveLine = blockNumber;
            }
            break;
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }

    mPercentArea->setCursor(mActiveLine == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    mPercentArea->repaint();
}

void SourceWidget::percentMouseReleaseEvent(QMouseEvent* ev)
{
    if (mActiveLine == -1 || ev->button() != Qt::LeftButton)
    {
        return;
    }

    emit lineClicked(mActiveLine + 1);
}

void SourceWidget::percentLeaveEvent(QEvent*)
{
    mActiveLine = -1;
    mPercentArea->setCursor(Qt::ArrowCursor);
    mPercentArea->repaint();
}

void SourceWidget::resizeEvent(QResizeEvent* ev)
{
    QPlainTextEdit::resizeEvent(ev);

    QRect cr = contentsRect();
    mPercentArea->setGeometry(QRect(cr.left(), cr.top(), percentAreaWidth(), cr.height()));
}

void SourceWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    QScopedPointer<QMenu> menu(createStandardContextMenu());
    menu->insertSeparator(menu->actions()[0]);
    QAction* action = new QAction("&Back", menu.data());
    action->setShortcut(Qt::Key_Backspace);
    menu->insertAction(menu->actions()[0], action);
    QObject::connect(action, &QAction::triggered, this, &SourceWidget::backClicked);
    menu->exec(ev->globalPos());
}

void SourceWidget::keyReleaseEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Backspace)
    {
        emit backClicked();
        return;
    }

    QPlainTextEdit::keyReleaseEvent(ev);
}

void SourceWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::BackButton)
    {
        emit backClicked();
        return;
    }

    QPlainTextEdit::mouseReleaseEvent(ev);
}
