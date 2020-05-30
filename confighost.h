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


    /**
     * @brief 全局基底格式
     * @param basicFormatOut
     */
    void novelFrameFormat(QTextFrameFormat &basicFormatOut) const;
    /**
     * @brief 全局标签基底格式
     * @param labelFormatOut
     */
    void novelLabelFrameFormat(QTextFrameFormat &labelFormatOut) const ;
    /**
     * @brief 小说标题文本格式
     * @param pFormatOut
     * @param wFormatOut
     */
    void novelTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;


    /**
     * @brief 卷宗范围基底格式
     * @param volumeFormatOut
     */
    void volumeFrameFormat(QTextFrameFormat &volumeFormatOut) const;
    /**
     * @brief 卷宗标签基底格式
     * @param labelFormatOut
     */
    void volumeLabelFrameFormat(QTextFrameFormat &labelFormatOut) const;
    /**
     * @brief 卷宗标题文本格式
     * @param pFormatOut
     * @param wFormatOut
     */
    void volumeTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;


    /**
     * @brief 章节范围基底格式
     * @param chapterFormatOut
     */
    void chapterFrameFormat(QTextFrameFormat &chapterFormatOut) const;
    /**
     * @brief 章节标签基底格式
     * @param labelFormatOut
     */
    void chapterLabelFrameFormat(QTextFrameFormat &labelFormatOut) const;
    /**
     * @brief 章节标题文本格式
     * @param pFormatOut
     * @param wFormatOut
     */
    void chapterTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;
    void chapterTextFrameFormat(QTextFrameFormat &formatOut) const;
    void chapterTextFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;

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
