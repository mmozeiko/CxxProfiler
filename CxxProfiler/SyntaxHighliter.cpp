#include "SyntaxHighliter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    {
        QTextCharFormat quotationFormat;
        quotationFormat.setForeground(Qt::darkRed);

        rule.pattern = QRegExp("\".*\"");
        rule.pattern.setMinimal(true);
        rule.format = quotationFormat;
        highlightingRules.append(rule);

        rule.pattern = QRegExp("'.*'");
        rule.pattern.setMinimal(true);
        rule.format = quotationFormat;
        highlightingRules.append(rule);

        rule.pattern = QRegExp("<[^\\s<]+>");
        rule.format = quotationFormat;
        highlightingRules.append(rule);
    }

    keywordFormat.setForeground(Qt::blue);

    static const char* keywords[] = {
        "__alignof", "array", "__asm", "__assume", "__based", "bool", "break", "case", "catch",
        "__cdecl", "char", "class", "const", "const_cast", "continue", "__declspec", "default", "delete", "deprecated",
        "dllexport", "dllimport", "do", "double", "dynamic_cast", "else", "enum", "__event", "__except",
        "explicit", "extern", "false", "__fastcall", "__finally", "finally", "float", "for", "each", "in", "__forceinline", "friend",
        "goto", "__hook", "__identifier", "if", "__if_exists", "__if_not_exists", "__inline", "inline",
        "int", "__int8", "__int16", "__int32", "__int64", "__interface", "__leave", "long",
        "__m64", "__m128", "__m128d", "__m128i", "__multiple_inheritance", "mutable", "naked", "namespace", "new", "noinline",
        "__noop", "noreturn", "nothrow", "novtable", "nullptr", "operator", "private", "protected",
        "public", "__raise", "register", "reinterpret_cast", "return", "selectany", "short",
        "signed", "__single_inheritance", "sizeof", "static", "static_cast", "__stdcall", "struct", "__super", "switch", "template", "this", "thread",
        "throw", "true", "try", "__try", "__try_cast", "typedef", "typeid", "typename", "__unaligned", "__unhook",
        "union", "unsigned", "using", "uuid", "__uuidof", "virtual", "__virtual_inheritance", "void", "volatile", "__w64",
        "__wchar_t", "wchar_t", "while"
    };
 
    for (int i=0; i<sizeof(keywords)/sizeof(keywords[0]); i++)
    {
        QString pattern = QString("\\b%1\\b").arg(keywords[i]);

        rule.pattern = QRegExp(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    static const char* preprocKeywords[] = {
        "#define", "#error", "#import", "#undef", "#elif", "#if", "#include",
        "#using", "#else", "#ifdef", "#line", "#endif", "#ifndef", "#pragma"
    };
 
    for (int i=0; i<sizeof(preprocKeywords)/sizeof(preprocKeywords[0]); i++)
    {
        QString pattern = QString("(\\b|^)%1\\b").arg(preprocKeywords[i]);
        rule.pattern = QRegExp(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    {
        QTextCharFormat numberFormat;

        numberFormat.setForeground(Qt::red);
        rule.pattern = QRegExp("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?f?\\b");
        rule.format = numberFormat;
        highlightingRules.append(rule);

        rule.pattern = QRegExp("\\b0x[0-9a-fA-F]+U?L?L?\\b");
        rule.format = numberFormat;
        highlightingRules.append(rule);

        rule.pattern = QRegExp("\\b[0-9]+\\b");
        rule.format = numberFormat;
        highlightingRules.append(rule);
    }
   
    {
        QTextCharFormat singleLineCommentFormat;
        singleLineCommentFormat.setForeground(Qt::darkGreen);
        rule.pattern = QRegExp("//[^\n]*");
        rule.format = singleLineCommentFormat;
        highlightingRules.append(rule);
    }

    multiLineCommentFormat.setForeground(Qt::darkGreen);

    commentStartExpression = QRegExp("/\\*");
    commentEndExpression = QRegExp("\\*/");
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, highlightingRules)
    {
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        while (index >= 0)
        {
            int length = expression.matchedLength();
            setFormat(index, length, rule.format);
            index = expression.indexIn(text, index + length);
        }
    }
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
    {
        startIndex = commentStartExpression.indexIn(text);
    }

    while (startIndex >= 0)
    {
        int endIndex = commentEndExpression.indexIn(text, startIndex);
        int commentLength;
        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else
        {
            commentLength = endIndex - startIndex + commentEndExpression.matchedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = commentStartExpression.indexIn(text, startIndex + commentLength);
     }
}