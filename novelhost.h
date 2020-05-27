#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QStandardItemModel>
#include <QString>
#include <QTextDocument>


class NovelHost
{
public:
    NovelHost(ConfigHost &config);
    virtual ~NovelHost() = default;

    QTextDocument* presentModel() const;

    QStandardItemModel* navigateModel() const;
    void appendGroup(const QString& gName);
    void appendArticle(const QString& aName);

    QStandardItemModel* searchModel() const;
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

    QTextFrame* appendVolume(QTextDocument *doc, const QString &title, ConfigHost &host);
    QTextCursor appendChapter(QTextFrame *volume, const QString &title, ConfigHost &host);
};

class CustomItem : public QStandardItem
{
public:
    CustomItem(const QString &text, QTextBlock &block);


private:
    QTextBlock &block_item;
};


class NavigateItemsModel : public QStandardItemModel
{

};


#endif // NOVELHOST_H
