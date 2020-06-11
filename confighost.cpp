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
        line = line.trimmed();
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
        line = line.trimmed();
        if (line.startsWith("#"))
            continue;

        if(!warring_words.contains(line))
            warring_words.append(line);
    }
    warrings.close();

    return 0;
}


void ConfigHost::volumeTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(3);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(25);
    cFormat.setFontWeight(200);
}

void ConfigHost::keystoryTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(23);
    cFormat.setFontWeight(200);
}

void ConfigHost::pointTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(21);
    cFormat.setFontWeight(200);
}


void ConfigHost::textFrameFormat(QTextFrameFormat &formatOut) const
{
    formatOut.setTopMargin(10);
    formatOut.setBottomMargin(10);
    formatOut.setBackground(QColor(250,250,250));
}


void ConfigHost::textFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    wFormatOut.setFontPointSize(18);

    pFormatOut.setTopMargin(1);
    pFormatOut.setBottomMargin(1);
    pFormatOut.setLeftMargin(9);
    pFormatOut.setRightMargin(9);
    pFormatOut.setTextIndent(18*2);
    pFormatOut.setBackground(QColor(250,250,250));
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
