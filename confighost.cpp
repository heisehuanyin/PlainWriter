#include "confighost.h"

#include <QTextFrame>

ConfigHost::ConfigHost()
{
    qRegisterMetaType<QTextBlock>("QTextBlock");
}

void ConfigHost::novelFrameFormat(QTextFrameFormat &basicFormatOut) const
{
    basicFormatOut.setBackground(QColor(250, 250, 250));
    basicFormatOut.setMargin(2);
}

void ConfigHost::novelLabelFrameFormat(QTextFrameFormat &labelFormatOut) const
{
    labelFormatOut.setBackground(QColor(250,250,250));
    labelFormatOut.setPadding(10);

    labelFormatOut.setBorder(2);
    labelFormatOut.setBorderBrush(Qt::lightGray);
}


void ConfigHost::novelTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    pFormatOut.setHeadingLevel(1);
    pFormatOut.setAlignment(Qt::AlignHCenter);

    wFormatOut.setFontWeight(900);
    wFormatOut.setFontPointSize(40);
}

void ConfigHost::volumeFrameFormat(QTextFrameFormat &volumeFormatOut) const
{
    volumeFormatOut.setMargin(0);
    volumeFormatOut.setTopMargin(10);
    volumeFormatOut.setBottomMargin(10);
    volumeFormatOut.setPadding(0);
}

void ConfigHost::volumeLabelFrameFormat(QTextFrameFormat &labelFormatOut) const
{
    labelFormatOut.setBorder(8);
    labelFormatOut.setBorderBrush(QColor(240, 240, 240));
    labelFormatOut.setBorderStyle(QTextFrameFormat::BorderStyle_Double);
}

void ConfigHost::volumeTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    pFormatOut.setHeadingLevel(2);
    pFormatOut.setBackground(QColor(240, 240, 240));

    wFormatOut.setFontPointSize(30);
}

void ConfigHost::chapterFrameFormat(QTextFrameFormat &pageFormatOut) const
{
    pageFormatOut.setBorderStyle(QTextFrameFormat::BorderStyle::BorderStyle_Dotted);
    pageFormatOut.setBorder(2);
    pageFormatOut.setMargin(10);
    pageFormatOut.setLeftMargin(2);
}

void ConfigHost::chapterLabelFrameFormat(QTextFrameFormat &labelFormatOut) const
{
    labelFormatOut.setBackground(QColor(220,220,220));
}

void ConfigHost::chapterTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    wFormatOut.setFontPointSize(25);

    pFormatOut.setTextIndent(wFormatOut.fontPointSize());
    pFormatOut.setHeadingLevel(3);
}

void ConfigHost::chapterTextFrameFormat(QTextFrameFormat &formatOut) const
{
    formatOut.setTopMargin(10);
    formatOut.setBottomMargin(10);
}

void ConfigHost::chapterTextFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    wFormatOut.setFontPointSize(18);

    pFormatOut.setTopMargin(wFormatOut.fontPointSize());
    pFormatOut.setLeftMargin(wFormatOut.fontPointSize());
    pFormatOut.setRightMargin(wFormatOut.fontPointSize());
    pFormatOut.setTextIndent(wFormatOut.fontPointSize()*2);
}

void ConfigHost::warringFormat(QTextCharFormat &formatOut) const
{
    formatOut.setFontUnderline(true);
    formatOut.setForeground(Qt::red);
}

void ConfigHost::keywordsFormat(QTextCharFormat &formatOut) const
{
    formatOut.setFontItalic(true);
    formatOut.setForeground(Qt::blue);
}

QList<QString> ConfigHost::warringWords() const{
    QList<QString> list;
    list.append("八九");
    list.append("上海");

    return list;
}

QList<QString> ConfigHost::keywordsList() const{
    QList<QString> list;
    list.append("主角");
    list.append("天意");

    return list;
}
