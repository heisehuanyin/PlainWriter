#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QThreadPool>

class ReferenceItem;

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
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
    QThreadPool *const work_ground;
    ConfigHost &host;
    /**
     * @brief 整个小说融合成一个文档
     * -[textblock]noveltitle
     * |-[textframe]==================
     * | |-[textblock]volumetitle
     * | |-[textframe]=====chapterpage
     * |   |-[textblock]chaptertitle
     * |   |-[textblock]chaptertext
     * |
     * |-[textframe]==================
     *   |-[textblock]volumetitle
     *   |-[textframe]=====chapterpage
     *     |-[textblock]chaptertitle
     *     |-[textblock]chaptertext
     */
    QTextDocument* content_presentation;
    QStandardItemModel* node_navigate_model;
    QStandardItemModel* result_enter_model;

    void insert_bigtitle(QTextDocument *doc, const QString &title, ConfigHost &host);
    QTextFrame* append_volume(QTextDocument *doc, const QString &title, ConfigHost &host);
    QTextCursor append_chapter(QTextFrame *volume, const QString &title, ConfigHost &host);

    void navigate_title_midify(QStandardItem *item);
    void remove_node_recursive(const QModelIndex &one);


};

/*

class RenderWorker : public QRunnable
{
public:
    RenderWorker(QTextBlock *holder, const QString &content, ConfigHost &host);

    // QRunnable interface
public:
    virtual void run() override;
};

class KeywordsRender : public QSyntaxHighlighter
{
    Q_OBJECT
    friend RenderWorker;
public:
    KeywordsRender(QTextDocument *target);

    void refreshHightlightRecord(QTextBlock target){
        if(hightlight_records.contains(target))
            hightlight_records.remove(target);

        QSyntaxHighlighter::rehighlightBlock(target);
    }
private:
    QHash<QTextBlock, QList<std::tuple<QTextCharFormat, QString, int, int>>> hightlight_records;

    void resetBlockHightlightFormat(QTextBlock holder, QList<std::tuple<QTextCharFormat, QString, int, int> > &record);

    // QSyntaxHighlighter interface
protected:
    virtual void highlightBlock(const QString &text) override{
        if(!text.size())
            return;

    }
};*/





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
