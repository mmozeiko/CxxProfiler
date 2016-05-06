#pragma once

#include "Precompiled.h"

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument* parent);
    ~SyntaxHighlighter();

private:
    struct HighlightingRule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> mRules;

    QRegExp mCommentStart;
    QRegExp mCommentEnd;

    QTextCharFormat mKeywordFormat;
    QTextCharFormat mMultiLineCommentFormat;

    void highlightBlock(const QString &text) override;
};
