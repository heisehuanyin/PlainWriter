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
     * @param kwfPath 关键字集合配置文件
     * @param wfPath 敏感字集合配置文件
     * @return 0 成功
     */
    int loadBaseFile(QString &err, const QString &kwfPath, const QString &wfPath);

    void volumeTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;
    void keystoryTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;
    void pointTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const;

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

    QList<QString> warringWords() const;
    QList<QString> keywordsList() const;

private:
    QList<QString> warring_words;
    QList<QString> keywords_list;
};

#endif // CONFIGHOST_H
