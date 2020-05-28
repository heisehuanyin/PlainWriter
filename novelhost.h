#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QSemaphore>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QThreadPool>
#include <QWaitCondition>

class ReferenceItem;

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config, const QString &filePath);
    virtual ~NovelHost() = default;

    QTextDocument *presentModel() const;

    QStandardItemModel *navigateTree() const;
    void appendVolume(const QString& gName);
    void appendChapter(const QString& aName, const QModelIndex &index);
    void removeNode(const QModelIndex &index);
    void refreshWordsCount();

    QStandardItemModel *searchModel() const;
    void searchText(const QString& text);


    QString chapterTextContent(const QModelIndex& index);
    int calcValidWordsCount(const QString &content);

private:
    ConfigHost &config_host;
    const QString novel_config_file_path;
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
    QTextDocument* content_presentation;
    QStandardItemModel* node_navigate_model;
    QStandardItemModel* result_enter_model;

    void insert_bigtitle(QTextDocument *doc, const QString &title, ConfigHost &config_host);
    QTextFrame* append_volume(QTextDocument *doc, const QString &title, ConfigHost &config_host);
    QTextCursor append_chapter(QTextFrame *volume, const QString &title, ConfigHost &config_host);

    void navigate_title_midify(QStandardItem *item);
    void remove_node_recursive(const QModelIndex &one);

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

class ReferenceItem : public QStandardItem
{
public:
    ReferenceItem(const QString &text, QTextFrame *frame);
    virtual ~ReferenceItem() override = default;

    QTextFrame *getAnchorItem();

    bool modified() const;
    void resetModified(bool value);

private:
    QTextFrame *const anchor_item;
    bool modify_flag;
};


#endif // NOVELHOST_H
