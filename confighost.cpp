#include "confighost.h"

#include <QTextFrame>
#include <QTextStream>

ConfigHost::ConfigHost()
{
    qRegisterMetaType<QTextBlock>("QTextBlock");
}

int ConfigHost::loadWarrings(QString &err, const QString &wfPath)
{
    warrings_filepath = wfPath;

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

    warring_words.clear();

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

        if(!warring_words.contains(std::make_tuple("", INT_MAX, line)))
            warring_words.append(std::make_tuple("", INT_MAX, line));
    }
    warrings.close();

    return 0;
}


void ConfigHost::volumeTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(3);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(40);
    cFormat.setFontWeight(200);
}

void ConfigHost::storyblockTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(30);
    cFormat.setFontWeight(200);
}

void ConfigHost::keypointTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(20);
    cFormat.setFontWeight(200);
}


void ConfigHost::textFrameFormat(QTextFrameFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setTopMargin(10);
    formatOut.setBottomMargin(10);
    formatOut.setBackground(QColor(250,250,250));
}


void ConfigHost::textFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    wFormatOut.setFontPointSize(15);

    pFormatOut.setTopMargin(1);
    pFormatOut.setBottomMargin(1);
    pFormatOut.setLeftMargin(9);
    pFormatOut.setRightMargin(9);
    pFormatOut.setTextIndent(wFormatOut.fontPointSize()*2);
    pFormatOut.setLineHeight(110, QTextBlockFormat::ProportionalHeight);
    pFormatOut.setBackground(QColor(250,250,250));
}

void ConfigHost::warringFormat(QTextCharFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setFontUnderline(true);
    formatOut.setForeground(Qt::red);
}

void ConfigHost::keywordsFormat(QTextCharFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setFontItalic(true);
    formatOut.setForeground(Qt::blue);
}

QList<std::tuple<QString, int, QString>> ConfigHost::warringWords() const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    return warring_words;
}

QList<std::tuple<QString, int, QString>> ConfigHost::getKeywordsWithMSG() const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    return keywords_list;
}

QString ConfigHost::warringsFilePath() const{
    return warrings_filepath;
}

void ConfigHost::appendKeyword(QString tableRealname, int uniqueID, const QString &words)
{
    QMutexLocker locker((&mutex));

    for (int index=0; index < keywords_list.size(); ++index) {
        auto tuple = keywords_list.at(index);
        auto ref_tablename = std::get<0>(tuple);
        auto unique_id = std::get<1>(tuple);

        if(ref_tablename == tableRealname && uniqueID == unique_id){
            keywords_list.insert(index, std::make_tuple(tableRealname, uniqueID, words));
            keywords_list.removeAt(index+1);
            return;
        }
    }

    keywords_list.append(std::make_tuple(tableRealname, uniqueID, words));
}

void ConfigHost::removeKeyword(QString tableRealname, int uniqueID)
{
    QMutexLocker locker(&mutex);

    for (int index=0; index < keywords_list.size(); ++index) {
        auto tuple = keywords_list.at(index);
        auto ref_tablename = std::get<0>(tuple);
        auto unique_id = std::get<1>(tuple);

        if(ref_tablename == tableRealname && uniqueID == unique_id){
            keywords_list.removeAt(index+1);
        }
    }
}
