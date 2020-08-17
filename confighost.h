#ifndef CONFIGHOST_H
#define CONFIGHOST_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextCharFormat>
#include <QSqlError>

#include "common.h"


class ConfigHost
{
public:
    class ViewConfigController;
    class ViewConfig
    {
    public:
        enum Type{
            UICONFIG=-1,
            MODEINDICATOR = 0,
            VIEWSPLITTER = 1,
            VIEWSELECTOR = 2
        };

        ViewConfig();
        ViewConfig(const ViewConfig &other);

        int uniqueID() const;
        bool isValid() const;
        Type configType() const;
        int index() const;
        QString supply() const;

        ViewConfig parent() const;
        int childCount() const;
        ViewConfig childAt(int index) const;
        ViewConfig nextSibling() const;
        ViewConfig previousSibling() const;

        bool operator==(const ViewConfig &other) const;

    private:
        friend ViewConfigController;

        int id_store;
        bool valid_state;
        ConfigHost *config;

        ViewConfig(int id, ConfigHost *config);
    };

    class ViewConfigController
    {
    public:
        ViewConfigController(ConfigHost &config);

        ViewConfig firstModeConfig() const;
        ViewConfig modeConfigAt(int index) const;
        ViewConfig::Type typeOf(const ViewConfig& node) const;
        int indexOf(const ViewConfig &node) const;
        QString supplyOf(const ViewConfig &node) const;
        void resetSupplyOf(const ViewConfig &node, const QString &value);

        ViewConfig parentOf(const ViewConfig &node) const;
        int childCountOf(const ViewConfig &pnode) const;
        ViewConfig childAtOf(const ViewConfig &pnode, int index) const;
        ViewConfig nextSibling(const ViewConfig &item) const;
        ViewConfig previousSibling(const ViewConfig &item) const;

        void configMove(const ViewConfig &destParent, int destIndex, const ViewConfig &targetConfig);

        ViewConfig insertBefore(const ViewConfig &pnode, ViewConfig::Type type, int index, QString supply);
        void remove(const ViewConfig &item);
    private:
        ConfigHost &host;
    };



    ConfigHost(const QString &wfPath);
    virtual ~ConfigHost() = default;

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
    QSqlDatabase dbins;

    // tableRealname-string : unique_id-int : keyword-string
    QList<std::tuple<QString, int, QString>> warring_words;
    QList<std::tuple<QString, int, QString>> keywords_list;
};

#endif // CONFIGHOST_H
