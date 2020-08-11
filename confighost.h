#ifndef CONFIGHOST_H
#define CONFIGHOST_H

#include <QTextCharFormat>


class ConfigHost
{
public:
    ConfigHost();
    virtual ~ConfigHost() = default;

    /**
     * @brief 载入配置文件
     * @param wfPath 敏感字集合配置文件
     * @return 0 成功
     */
    int loadWarrings(QString &err, const QString &wfPath);

    void volumeTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;
    void storyblockTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;
    void keypointTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;

    void textFrameFormat(QTextFrameFormat &formatOut) const;
    void textFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;

    /**
     * @brief 获取指定类型的警告字符格式
     * @param formatOut
     */
    void warringFormat(QTextCharFormat &formatOut) const;

    /**
     * @brief 获取指定类型的关键字字符格式
     * @param formatOut
     */
    void keywordsFormat(QTextCharFormat &formatOut) const;

    QList<std::tuple<QString, int, QString> > warringWords() const;

    QList<std::tuple<QString, int, QString>> getKeywordsWithMSG() const;

    QString warringsFilePath() const;

public slots:
    void appendKeyword(QString tableRealname, int uniqueID, const QString &words);
    void removeKeyword(QString tableRealname, int uniqueID);

private:
    QMutex mutex;
    QString warrings_filepath;

    // tableRealname-string : unique_id-int : keyword-string
    QList<std::tuple<QString, int, QString>> warring_words;
    QList<std::tuple<QString, int, QString>> keywords_list;
};

#endif // CONFIGHOST_H
