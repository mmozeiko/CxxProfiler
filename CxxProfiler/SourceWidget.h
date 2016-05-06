#pragma once

#include "Precompiled.h"

class SourceWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit SourceWidget(QWidget* parent);
    ~SourceWidget();

    int percentAreaWidth() const;
    void setPercents(const QStringList& percents);

    void percentAreaPaintEvent(QPaintEvent* ev);
    void percentMouseMoveEvent(QMouseEvent* ev);
    void percentMouseReleaseEvent(QMouseEvent* ev);
    void percentLeaveEvent(QEvent* ev);

signals:
    void lineClicked(int line);
    void backClicked();

private:
    QWidget* mPercentArea;
    QStringList mPercents;
    int mActiveLine;

    void resizeEvent(QResizeEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;
};
