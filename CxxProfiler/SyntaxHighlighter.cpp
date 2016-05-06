#include "SyntaxHighlighter.h"

// TODO http://www.kate-editor.org/syntax/3.9/isocpp.xml

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
        mRules.append(rule);

        rule.pattern = QRegExp("'.*'");
        rule.pattern.setMinimal(true);
        rule.format = quotationFormat;
        mRules.append(rule);

        rule.pattern = QRegExp("<[^\\s<]+>");
        rule.format = quotationFormat;
        mRules.append(rule);
    }

    mKeywordFormat.setForeground(Qt::blue);

    static const char* keywords[] = {
        "__abstract", "__alignof", "__asm", "__assume", "__based", "__box",
        "__cdecl", "__declspec", "__delegate", "__event", "__fastcall",
        "__forceinline", "__gc", "__hook", "__identifier", "__if_exists",
        "__if_not_exists", "__inline", "__int16", "__int32", "__int64",
        "__int8", "__interface", "__leave", "__m128", "__m128d", "__m128i",
        "__m256", "__m256d", "__m256i", "__m64", "__multiple_inheritance",
        "__nogc", "__noop", "__pin", "__property", "__raise", "__sealed",
        "__single_inheritance", "__stdcall", "__super", "__thiscall",
        "__try_cast", "__unaligned", "__unhook", "__uuidof", "__value",
        "__vectorcall", "__virtual_inheritance", "__w64", "__wchar_t",
        "abstract", "array", "auto", "bool", "break", "case", "catch",
        "char", "class", "const", "const_cast", "continue", "decltype",
        "default", "delegate", "delete", "deprecated", "dllexport",
        "dllimport", "do", "double", "dynamic_cast", "each", "else", "event",
        "explicit", "extern", "false", "finally", "float", "for", "friend",
        "friend_as", "gcnew", "generic", "goto", "if", "in", "initonly",
        "inline", "int", "interface", "interior_ptr", "literal", "long",
        "mutable", "naked", "namespace", "new", "noinline", "noreturn",
        "nothrow", "novtable", "nullptr", "operator", "private", "property",
        "protected", "public", "ref", "register", "reinterpret_cast",
        "return", "safecast", "sealed", "selectany", "short", "signed",
        "sizeof", "static", "static_assert", "static_cast", "struct",
        "switch", "template", "this", "thread", "throw", "true", "try",
        "typedef", "typename", "union", "unsigned", "using", "uuid", "value",
        "virtual", "void", "volatile", "wchar_t", "while"
    };  
 
    for (const char* keyword : keywords)
    {
        QString pattern = QString("\\b%1\\b").arg(keyword);

        rule.pattern = QRegExp(pattern);
        rule.format = mKeywordFormat;
        mRules.append(rule);
    }

    static const char* preprocKeywords[] = {
        "#define", "#error", "#import", "#undef", "#elif", "#if", "#include",
        "#using", "#else", "#ifdef", "#line", "#endif", "#ifndef", "#pragma"
    };
 
    for (const char* preprocKeyword : preprocKeywords)
    {
        QString pattern = QString("(\\b|^)%1\\b").arg(preprocKeyword);
        rule.pattern = QRegExp(pattern);
        rule.format = mKeywordFormat;
        mRules.append(rule);
    }

    {
        QTextCharFormat numberFormat;

        numberFormat.setForeground(Qt::red);
        rule.pattern = QRegExp("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?f?\\b");
        rule.format = numberFormat;
        mRules.append(rule);

        rule.pattern = QRegExp("\\b0x[0-9a-fA-F]+U?L?L?\\b");
        rule.format = numberFormat;
        mRules.append(rule);

        rule.pattern = QRegExp("\\b[0-9]+\\b");
        rule.format = numberFormat;
        mRules.append(rule);
    }
   
    {
        QTextCharFormat singleLineCommentFormat;
        singleLineCommentFormat.setForeground(Qt::darkGreen);
        rule.pattern = QRegExp("//[^\n]*");
        rule.format = singleLineCommentFormat;
        mRules.append(rule);
    }

    mMultiLineCommentFormat.setForeground(Qt::darkGreen);

    mCommentStart = QRegExp("/\\*");
    mCommentEnd = QRegExp("\\*/");
}

SyntaxHighlighter::~SyntaxHighlighter()
{
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule& rule : mRules)
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
        startIndex = mCommentStart.indexIn(text);
    }

    while (startIndex >= 0)
    {
        int endIndex = mCommentEnd.indexIn(text, startIndex);
        int commentLength;
        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else
        {
            commentLength = endIndex - startIndex + mCommentEnd.matchedLength();
        }
        setFormat(startIndex, commentLength, mMultiLineCommentFormat);
        startIndex = mCommentStart.indexIn(text, startIndex + commentLength);
     }
}