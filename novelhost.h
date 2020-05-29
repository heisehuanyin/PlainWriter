#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"
#include "common.h"

#include <QDir>
#include <QDomDocument>
#include <QException>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSemaphore>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QTextStream>
#include <QThreadPool>
#include <QWaitCondition>

class ReferenceItem;
class StructureDescription;
class BlockHidenVerify;
class KeywordsRender;
class GlobalFormatRender;

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    int loadDescription(QString &err, StructureDescription *desp);
    int save(QString &errorOut, const QString &filePath=QString());

    QTextDocument *presentDocument() const;

    QStandardItemModel *navigateTree() const;
    int appendVolume(QString &errOut, const QString& gName);
    int appendChapter(QString &errOut, const QString& aName, const QModelIndex &volume_navigate_index);
    int removeNode(QString &errOut, const QModelIndex &index);
    void refreshWordsCount();

    QStandardItemModel *searchResultPresent() const;
    void searchText(const QString& text);

    QString chapterTextContent(const QModelIndex& index);
    int calcValidWordsCount(const QString &content);

    void reHighlightDocument();

private:
    ConfigHost &config_host;
    StructureDescription * struct_discrib;
    /**
     * @brief 整个小说融合成一个文档
     * |-[textframe]novellabel
     * | |-[textblock]noveltitle
     * |
     * |-[textframe]==================volume
     * | |-[textframe]volumelabel
     * | |  |-[textblock]volumetitle
     * | |
     * | |-[textframe]================chapter
     * | | |-[textframe]chapterlabel
     * | | |  |-[textblock]chaptertitle
     * | | |
     * | | |-[textframe]==============content
     * | |    |-[textblock]text
     * | |    |-[textblock]text
     * | |
     * | |-[textframe]================chapter
     * |   |-[textframe]chapterlabel
     * |   |  |-[textblock]chaptertitle
     * |   |
     * |   |-[textframe]==============content
     * |      |-[textblock]text
     * |      |-[textblock]text
     * |
     * |-[textframe]==================volume
     * | |-[textframe]volumelabel
     * | |  |-[textblock]volumetitle
     * | |
     * | |-[textframe]================chapter
     * |   |-[textframe]chapterlabel
     * |   |  |-[textblock]chaptertitle
     * |   |
     * |   |-[textframe]==============content
     * |      |-[textblock]text
     * |      |-[textblock]text
     */
    QTextDocument*const content_presentation;
    QStandardItemModel*const node_navigate_model;
    QStandardItemModel*const result_enter_model;
    BlockHidenVerify *const hiden_formater;
    KeywordsRender *const keywords_formater;
    GlobalFormatRender *const global_formater;

    void insert_bigtitle(QTextDocument *doc, const QString &title, ConfigHost &config_host);
    QTextFrame* append_volume(QTextDocument *doc, const QString &title, ConfigHost &config_host);
    QTextCursor append_chapter(QTextFrame *volume, const QString &title, ConfigHost &config_host);

    void navigate_title_midify(QStandardItem *item);
    int remove_node_recursive(QString &errOut, const QModelIndex &one);

};

class RenderWorker : public QThread
{
    Q_OBJECT
public:
    RenderWorker(const ConfigHost &config);

    void pushRenderRequest(const QTextBlock &pholder, const QString &text);
    QPair<QTextBlock, QList<std::tuple<QTextCharFormat, QString, int, int> > > topResult();
    void discardTopResult();

    // QRunnable interface
public:
    virtual void run() override;

private:
    const ConfigHost &config;
    QList<QPair<QTextBlock, QList<std::tuple<QTextCharFormat, QString, int, int>>>> result_stored;
    QMutex result_protect;

    QList<QPair<QTextBlock, QString>> request_stored;
    QMutex req_protect;
    QSemaphore req_sgl;

    void _render_warrings(const QString &content, QList<std::tuple<QTextCharFormat, QString, int, int> > &one_set);
    void _render_keywords(const QString &content, QList<std::tuple<QTextCharFormat, QString, int, int> > &one_set);

    QPair<QTextBlock, QString> take_render_request();
    void push_render_result(const QTextBlock &pholder, const QList<std::tuple<QTextCharFormat, QString, int, int>> formats);


signals:
    void renderFinish(const QTextBlock &holder);
};

class KeywordsRender : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    KeywordsRender(QTextDocument *target, ConfigHost &config);
    virtual ~KeywordsRender() override;

    // QSyntaxHighlighter interface
protected:
    virtual void highlightBlock(const QString &text) override;

private:
    ConfigHost &config;
    RenderWorker *const thread;
};

class BlockHidenVerify : public QSyntaxHighlighter
{
public:
    BlockHidenVerify(QTextDocument *target);
    virtual ~BlockHidenVerify() override = default;
    // QSyntaxHighlighter interface
protected:
    virtual void highlightBlock(const QString &text) override;
};

class GlobalFormatRender : public QSyntaxHighlighter
{
public:
    GlobalFormatRender(QTextDocument *target, ConfigHost &config);
    virtual ~GlobalFormatRender() override = default;

    // QSyntaxHighlighter interface
protected:
    virtual void highlightBlock(const QString &text) override;

private:
    ConfigHost &host;
};

class ReferenceItem : public QStandardItem
{
public:
    ReferenceItem(const QString &disp, QTextFrame *anchor);
    virtual ~ReferenceItem() override = default;

    QTextFrame *getAnchorItem();

    bool modified() const;
    void resetModified(bool value);

private:
    QTextFrame *const anchor_item;
    bool modify_flag;
};

class StructureDescription
{
public:
    StructureDescription();
    virtual ~StructureDescription();

    void newDescription();
    int openDescription(QString &errOut, const QString &filePath);

    QString novelDescribeFilePath() const;
    int save(QString &errOut, const QString &newFilepath);

    QString novelTitle() const;

    int volumeCount() const;
    int volumeTitle(QString &errOut, int volumeIndex, QString &titleOut) const;
    int insertVolume(QString &errOut, int volumeIndexBefore, const QString &volumeTitle);
    int removeVolume(QString &errOut, int volumeIndex);
    int resetVolumeTitle(QString &errOut, int volumeIndex, const QString &volumeTitle);

    int chapterCount(QString &errOut, int volumeIndex, int &numOut) const;
    int insertChapter(QString &errOut, int volumeIndexAt, int chapterIndexBefore,
                      const QString &chapterTitle, const QString &encoding="utf-8");
    int removeChapter(QString &errOut, int volumeIndexAt, int chapterIndex);
    int resetChapterTitle(QString &errOut, int volumeIndexAt, int chapterIndex, const QString &title);
    int chapterTitle(QString &errOut, int volumeIndex, int chapterIndex, QString &titleOut) const;
    int chapterCanonicalFilepath(QString &errOut, int volumeIndex, int chapterIndex, QString &pathOut) const;
    int chapterTextEncoding(QString &errOut, int volumeIndex, int chapterIndex, QString &encodingOut) const;

private:
    /*
     *[root version='1.0' title='novel-title']
     *  |---[config]
     *  |     |---[item]
     *  |
     *  |---[struct]
     *        |---[volume title='volume-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |
     *        |---[volume title='volume-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     *        |     |---[chapter relative='./filepath' encoding='encoding' title='chapter-title']
     */
    QDomDocument struct_dom_store;
    QString filepath_stored;
    QRandomGenerator gen;

    /**
     * @brief 通过index获取 volume dom节点
     * @param index
     * @return
     * @throw 索引超界
     */
    int find_volume_domnode_by_index(QString &errO, int index, QDomElement &domOut) const;

    int find_chapter_domnode_ty_index(QString &errO, const QDomElement &volumeNode, int index, QDomElement &domOut) const;
};


#endif // NOVELHOST_H
