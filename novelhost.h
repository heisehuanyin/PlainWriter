#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTextDocument>


class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() = default;

    QTextDocument *presentModel() const;

    QStandardItemModel *navigateModel() const;
    void appendVolume(const QString& gName);
    void appendChapter(const QString& aName, const QModelIndex &index);
    void removeNode(const QModelIndex &index);

    QStandardItemModel *searchModel() const;
    void searchText(const QString& text);

private:
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

    void navigate_node_midify(QStandardItem *item);
    void remove_node_recursive(const QModelIndex &one);
};

class HidenVerify : public QSyntaxHighlighter
{
public:
    HidenVerify(QTextDocument *target);
    virtual ~HidenVerify() override = default;
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
