#include "confighost.h"

#include <QTextFrame>
#include <QTextStream>

ConfigHost::ConfigHost()
{
    qRegisterMetaType<QTextBlock>("QTextBlock");
}

int ConfigHost::loadBaseFile(QString &err, const QString &kwfPath, const QString &wfPath)
{
    QFile keywords(kwfPath);
    if(!keywords.exists()){
        if(!keywords.open(QIODevice::Text|QIODevice::WriteOnly)){
            err = "指定文件不存在，新建过程中指定文件无法打开："+kwfPath;
            return -1;
        }
        QTextStream tout(&keywords);
        tout.setCodec("UTF-8");
        tout << QObject::tr("# 关键字配置文件") << endl;
        tout << QObject::tr("# 每一行视为一个关键字，请注意暂不支持正则表达式") << endl;
        tout << QObject::tr("# 每一行以“#”开头的字符意味着注释，不会被软件收录") << endl;
        tout << QObject::tr("# 删除本文件，将重建本文件并附带示例关键字，不代表本软件观点") << endl;
        tout << QObject::tr("主角") << endl;
        tout << QObject::tr("武器") << endl;
        tout << QObject::tr("丹药") << endl;
        tout.flush();
        keywords.close();
    }
    QFile warrings(wfPath);
    if(!warrings.exists()){
        if(!warrings.open(QIODevice::Text|QIODevice::WriteOnly)){
            err = "指定文件不存在，新建过程中指定文件无法打开："+wfPath;
            return -1;
        }
        QTextStream tout(&warrings);
        tout.setCodec("UTF-8");
        tout << QObject::tr("# 敏感字配置文件") << endl;
        tout << QObject::tr("# 每一行视为一个敏感字，请注意暂不支持正则表达式") << endl;
        tout << QObject::tr("# 每一行以“#”开头的字符意味着注释，不会被软件收录") << endl;
        tout << QObject::tr("# 删除本文件，将重建本文件并附带示例敏感字，不代表本软件观点") << endl;
        tout << QObject::tr("八九") << endl;
        tout << QObject::tr("乳交") << endl;
        tout << QObject::tr("鸡鸡") << endl;
        tout.flush();
        warrings.close();
    }

    keywords_list.clear();
    warring_words.clear();

    if(!keywords.open(QIODevice::Text|QIODevice::ReadOnly)){
        err = "指定文件无法打开："+kwfPath;
        return -1;
    }
    QTextStream tin(&keywords);
    tin.setCodec("UTF-8");
    while (!tin.atEnd()) {
        auto line = tin.readLine();
        if(line.startsWith("#"))
            continue;

        if(!keywords_list.contains(line))
            keywords_list.append(line);
    }
    keywords.close();

    if(!warrings.open(QIODevice::ReadOnly|QIODevice::Text)){
        err = "指定文件无法打开："+wfPath;
        return -1;
    }
    QTextStream wtin(&warrings);
    wtin.setCodec("UTF-8");
    while (!wtin.atEnd()) {
        auto line = wtin.readLine();
        if (line.startsWith("#"))
            continue;

        if(!warring_words.contains(line))
            warring_words.append(line);
    }
    warrings.close();

    return 0;
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
    return warring_words;
}

QList<QString> ConfigHost::keywordsList() const{
    return keywords_list;
}
