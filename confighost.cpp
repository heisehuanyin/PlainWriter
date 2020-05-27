#include "confighost.h"

ConfigHost::ConfigHost()
{

}

void ConfigHost::basicFrameFormat(QTextFrameFormat &basicFormat)
{
    basicFormat.setBackground(QColor(250, 250, 250));
}

void ConfigHost::novelTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    pFormatOut.setHeadingLevel(1);
    pFormatOut.setAlignment(Qt::AlignHCenter);

    wFormatOut.setBackground(QColor(250, 250, 250));
}

void ConfigHost::volumeFrameFormat(QTextFrameFormat &volumeFormatOut) const
{
    volumeFormatOut.setBackground(Qt::lightGray);
    volumeFormatOut.setMargin(0);
    volumeFormatOut.setPadding(0);
}

void ConfigHost::volumeTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{

}

void ConfigHost::chapterFrameFormat(QTextFrameFormat &pageFormatOut) const
{
    pageFormatOut.setBorderBrush(Qt::gray);
    pageFormatOut.setBorderStyle(QTextFrameFormat::BorderStyle::BorderStyle_Dotted);
    pageFormatOut.setBorder(2);
}

void ConfigHost::chapterTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{

}

void ConfigHost::chapterTextFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{

}

void ConfigHost::warringFormat(int typeIn, QTextCharFormat &formatOut) const
{

}

void ConfigHost::keywordsFormat(int typeIn, QTextCharFormat &formatOut) const
{

}
