#include "novelhost.h"

#include <QTextCursor>
#include <QTextFrame>
#include <QtDebug>

NovelHost::NovelHost(ConfigHost &config)
    :host(config)
{
    content_presentation = new QTextDocument();
    node_navigate_model = new QStandardItemModel;
    result_enter_model = new QStandardItemModel;

    new BlockHidenVerify(content_presentation);

    QTextFrameFormat novel_frame_format;
    config.novelFrameFormat(novel_frame_format);
    content_presentation->rootFrame()->setFrameFormat(novel_frame_format);

    insert_bigtitle(content_presentation, "小说标题", config);

    auto nframe = append_volume(content_presentation, "分卷标题", config);
    auto currr = append_chapter(nframe, "章节标题", config);
    currr.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");
    auto currr2 = append_chapter(nframe, "章节标题", config);
    currr2.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");
    auto currr3 = append_chapter(nframe, "章节标题", config);
    currr3.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                      "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");

    content_presentation->clearUndoRedoStacks();

    connect(node_navigate_model,   &QStandardItemModel::itemChanged,
            this,                  &NovelHost::navigate_title_midify);
    node_navigate_model->setHorizontalHeaderLabels(QStringList() << "章节标题" << "字数统计");
}

QTextDocument *NovelHost::presentModel() const
{
    return content_presentation;
}

QStandardItemModel *NovelHost::navigateTree() const
{
    return node_navigate_model;
}

void NovelHost::appendVolume(const QString &gName)
{
    append_volume(content_presentation, gName, host);
}

void NovelHost::appendChapter(const QString &aName, const QModelIndex &index)
{
    if(!index.isValid()){
        qDebug() << "appendArticle:非法index";
        return;
    }

    // 选中了卷节点
    auto item = node_navigate_model->itemFromIndex(index);
    for (int iii=0; iii < node_navigate_model->rowCount(); ++iii) {
        auto volume_node = node_navigate_model->item(iii);
        if(volume_node == item){
            auto volume_item = static_cast<ReferenceItem*>(item);
            append_chapter(volume_item->getAnchorItem(), aName, host);
            return;
        }
    }

    // 选中了章节节点
    item = item->parent();
    append_chapter(static_cast<ReferenceItem*>(item)->getAnchorItem(), aName, host);
}

void NovelHost::removeNode(const QModelIndex &index)
{
    if(!index.isValid())
        return;

    remove_node_recursive(index);
}



void NovelHost::refreshWordsCount()
{
    for (int num=0; num < node_navigate_model->rowCount(); ++num) {
        auto volume_title_node = node_navigate_model->item(num);
        auto volume_words_count= node_navigate_model->item(num, 1);
        auto v_temp = 0;

        for (int num2=0; num2 < volume_title_node->rowCount(); ++num2) {
            auto chapter_title_node = volume_title_node->child(num2);
            auto chapter_words_count= volume_title_node->child(num2, 1);

            auto xhrefn = static_cast<ReferenceItem*>(chapter_title_node);
            auto count = chapterWordsCount(xhrefn);
            chapter_words_count->setText(QString("%1").arg(count));

            v_temp += count;
        }

        volume_words_count->setText(QString("%1").arg(v_temp));
    }
}

QStandardItemModel *NovelHost::searchModel() const
{
    return result_enter_model;
}

void NovelHost::searchText(const QString &text)
{

}

int NovelHost::chapterWordsCount(ReferenceItem *chapterNode)
{
    auto xchapter_frame = chapterNode->getAnchorItem();
    bool title_found = false;
    for (auto it=xchapter_frame->begin(); !it.atEnd(); ++it) {
        auto frame_one = it.currentFrame();

        if(!title_found && frame_one){
            title_found = true;
            continue;
        }

        if(title_found && frame_one){
            QTextCursor cursor(frame_one);
            cursor.setPosition(frame_one->firstPosition(), QTextCursor::MoveAnchor);
            cursor.setPosition(frame_one->lastPosition(), QTextCursor::KeepAnchor);

            return cursor.selectedText().size();
        }
    }

    return 0;
}

void NovelHost::insert_bigtitle(QTextDocument *doc, const QString &title, ConfigHost &host)
{
    /*
     * 插入全局标题标签
     */
    QTextCursor cur(doc);
    QTextFrameFormat novel_label_frame_format;
    host.novelLabelFrameFormat(novel_label_frame_format);
    cur.insertFrame(novel_label_frame_format);

    /*
     *插入全局标题文本
     */
    QTextBlockFormat title_block_format;
    QTextCharFormat title_text_format;
    host.novelTitleFormat(title_block_format, title_text_format);
    cur.setBlockFormat(title_block_format);
    cur.setCharFormat(title_text_format);
    cur.insertText(title);
}

QTextFrame *NovelHost::append_volume(QTextDocument *doc, const QString &title, ConfigHost &host)
{
    QTextCursor volume_pos(doc);
    volume_pos.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    /**
     * 插入卷宗基底
     */
    QTextFrameFormat volumn_frame_format;
    host.volumeFrameFormat(volumn_frame_format);
    auto volume_group =  volume_pos.insertFrame(volumn_frame_format);
    QList<QStandardItem*> row;
    row.append(new ReferenceItem(title, volume_group));
    row.append(new QStandardItem("-"));
    node_navigate_model->appendRow(row);

    /**
     * 插入卷宗标题标签
     */
    QTextFrameFormat volumn_label_frame_format;
    QTextCharFormat volume_title_text_format;
    QTextBlockFormat volume_title_block_format;
    host.volumeLabelFrameFormat(volumn_label_frame_format);
    host.volumeTitleFormat(volume_title_block_format, volume_title_text_format);
    volume_pos.insertFrame(volumn_label_frame_format);
    volume_pos.setBlockFormat(volume_title_block_format);
    volume_pos.setCharFormat(volume_title_text_format);
    volume_pos.insertText(title);

    return volume_group;
}

QTextCursor NovelHost::append_chapter(QTextFrame *volume, const QString &title, ConfigHost &host)
{
    QTextCursor chapter_pos = volume->lastCursorPosition();

    /**
     * 插入章节基底
     */
    QTextFrameFormat chapter_frame_format;
    host.chapterFrameFormat(chapter_frame_format);
    auto chapter_group = chapter_pos.insertFrame(chapter_frame_format);


    for (int index = 0; index < node_navigate_model->rowCount(); ++index) {
        auto item = node_navigate_model->item(index);
        auto xitem = static_cast<ReferenceItem*>(item);
        if(xitem->getAnchorItem() == volume){
            QList<QStandardItem*> row;
            row.append(new ReferenceItem(title, chapter_group));
            row.append(new QStandardItem("-"));
            xitem->appendRow(row);
        }
    }


    /**
      * 插入章节标题标签
      */
    QTextFrameFormat chapter_label_frame_format;
    host.chapterLabelFrameFormat(chapter_label_frame_format);
    chapter_pos.insertFrame(chapter_label_frame_format);
    QTextBlockFormat chapter_title_block_format;
    QTextCharFormat chapter_title_text_format;
    host.chapterTitleFormat(chapter_title_block_format, chapter_title_text_format);
    chapter_pos.setBlockFormat(chapter_title_block_format);
    chapter_pos.setCharFormat(chapter_title_text_format);
    chapter_pos.insertText(title);

    /**
     * 插入章节正文区域
     */
    QTextCursor chapter_text_pos = chapter_group->lastCursorPosition();
    QTextFrameFormat chapter_text_frame_format;
    host.chapterTextFrameFormat(chapter_text_frame_format);
    chapter_text_pos.insertFrame(chapter_text_frame_format);

    QTextBlockFormat chapter_text_block_format;
    QTextCharFormat chapter_text_format;
    host.chapterTextFormat(chapter_text_block_format, chapter_text_format);
    chapter_text_pos.setBlockFormat(chapter_text_block_format);
    chapter_text_pos.setCharFormat(chapter_text_format);

    return chapter_text_pos;
}

void NovelHost::navigate_title_midify(QStandardItem *item)
{
    if(item->column() != 0)
        return;

    auto xitem = static_cast<ReferenceItem*>(item);
    auto enter_point = xitem->getAnchorItem();

    QTextFrame::Iterator it;
    for (it = enter_point->begin(); !it.atEnd() ;it++) {
        auto label_frame = it.currentFrame();
        if(label_frame){
            auto text_block = label_frame->firstCursorPosition();
            text_block.clearSelection();
            text_block.movePosition(QTextCursor::StartOfBlock);
            text_block.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            text_block.insertText(item->text());
            break;
        }
    }
}

void NovelHost::remove_node_recursive(const QModelIndex &one)
{
    auto item = node_navigate_model->itemFromIndex(one);
    auto xitem = static_cast<ReferenceItem*>(item);

    auto anchor = xitem->getAnchorItem();
    qDebug() << anchor->firstPosition() << anchor->lastPosition();

    QTextCursor cursor(anchor);
    cursor.setPosition(anchor->firstPosition()-1);
    cursor.setPosition(anchor->lastPosition(), QTextCursor::KeepAnchor);

    cursor.removeSelectedText();

    auto parent = item->parent();
    if(parent)
        parent->removeRow(item->row());
    else
        node_navigate_model->removeRow(item->row());
}

BlockHidenVerify::BlockHidenVerify(QTextDocument *target)
    :QSyntaxHighlighter (target){}

void BlockHidenVerify::highlightBlock(const QString &text){
    if(!text.length()){
        auto blk = currentBlock();
        QTextCursor cur(blk);
        auto frame = cur.currentFrame();
        if(frame->childFrames().size())
            blk.setVisible(false);
    }
}

ReferenceItem::ReferenceItem(const QString &text, QTextFrame *frame)
    :QStandardItem (text),
      anchor_item(frame){}

QTextFrame *ReferenceItem::getAnchorItem(){
    return anchor_item;
}

bool ReferenceItem::modified() const
{
    return modify_flag;
}

void ReferenceItem::resetModified(bool value)
{
    modify_flag = value;
}
