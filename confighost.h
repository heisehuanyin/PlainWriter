#ifndef CONFIGHOST_H
#define CONFIGHOST_H

#include <QTextCharFormat>



class ConfigHost
{
public:
    ConfigHost();
    virtual ~ConfigHost() = default;

    void basicFrameFormat(QTextFrameFormat &basicFormat);
    /**
     * @brief 小说标题格式输出
     * @param pFormatOut
     * @param wFormatOut
     */
    void novelTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;

    void volumeFrameFormat(QTextFrameFormat &volumeFormatOut) const;
    void volumeTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;

    void chapterFrameFormat(QTextFrameFormat &pageFormatOut) const;
    void chapterTitleFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;
    void chapterTextFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const;

    /**
     * @brief 获取指定类型的警告字符格式
     * @param typeIn
     * @param formatOut
     */
    void warringFormat(int typeIn, QTextCharFormat &formatOut) const;

    /**
     * @brief 获取指定类型的关键字字符格式
     * @param typeIn
     * @param formatOut
     */
    void keywordsFormat(int typeIn, QTextCharFormat &formatOut) const;
};

#endif // CONFIGHOST_H
