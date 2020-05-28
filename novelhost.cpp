#include "novelhost.h"

#include <QTextCursor>
#include <QTextFrame>
#include <QtDebug>

NovelHost::NovelHost(ConfigHost &config, const QString &filePath)
    : config_host(config), novel_config_file_path(filePath)
{
    content_presentation = new QTextDocument();
    node_navigate_model = new QStandardItemModel;
    result_enter_model = new QStandardItemModel;

    new BlockHidenVerify(content_presentation);
    new KeywordsRender(content_presentation, config);

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
    append_volume(content_presentation, gName, config_host);
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
            append_chapter(volume_item->getAnchorItem(), aName, config_host);
            return;
        }
    }

    // 选中了章节节点
    item = item->parent();
    append_chapter(static_cast<ReferenceItem*>(item)->getAnchorItem(), aName, config_host);
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

            auto text_content = chapterTextContent(chapter_title_node->index());
            auto vc = calcValidWordsCount(text_content);
            chapter_words_count->setText(QString("%1").arg(vc));

            v_temp += vc;
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
    QRegExp exp("("+text+").*");

    auto blk = content_presentation->begin();
    while (blk.isValid()) {
        int pos = -1;
        auto ttext = blk.text();
        while ((pos = exp.indexIn(ttext, pos+1)) != -1) {
            auto word = exp.cap(1);
            auto doc_position = blk.position() + pos;
            auto len = word.length();

            auto text_result = ttext.mid(pos, 20);
            QStandardItem *item;
            if(pos == 0){
                item = new QStandardItem(text_result.length()==20?text_result+"……":text_result);
            }
            else {
                item = new QStandardItem("……"+(text_result.length()==20?text_result+"……":text_result));
            }

            item->setData(doc_position, Qt::UserRole + 1);
            item->setData(doc_position + len, Qt::UserRole+2);
            result_enter_model->appendRow(item);
        }

        blk = blk.next();
    }
}


QString NovelHost::chapterTextContent(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        return "";

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto xxxitem = node_navigate_model->itemFromIndex(index);

    auto xchapter_frame = static_cast<ReferenceItem*>(xxxitem)->getAnchorItem();
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

            return cursor.selectedText();
        }
    }

    return "";
}

int NovelHost::calcValidWordsCount(const QString &content)
{
    QString newtext = content;
    QRegExp exp("[，。！？【】“”—…《》：、\\s]");
    return newtext.replace(exp, "").size();
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




RenderWorker::RenderWorker(const ConfigHost &config)
    :config(config)
{
}

void RenderWorker::pushRenderRequest(const QTextBlock &pholder, const QString &text)
{
    QMutexLocker lock(&req_protect);

    request_stored.insert(0, qMakePair(pholder, text));
    req_sgl.release();
}

QPair<QTextBlock, QList<std::tuple<QTextCharFormat, QString, int, int>>> RenderWorker::topResult()
{
    QMutexLocker lock(&result_protect);

    for (int index=0; index < result_stored.size();) {
        auto pair = result_stored.at(index);
        if(!pair.first.isValid()){
            result_stored.removeAt(index);
            continue;
        }
        index++;
    }

    if(!result_stored.size())
        return QPair<QTextBlock, QList<std::tuple<QTextCharFormat, QString, int, int>>>();

    return result_stored.at(result_stored.size()-1);
}

void RenderWorker::discardTopResult()
{
    QMutexLocker lock(&result_protect);

    result_stored.removeAt(result_stored.size()-1);
}

void RenderWorker::run(){
    while (1) {
        auto request_one = take_render_request();
        QList<std::tuple<QTextCharFormat, QString, int, int> > one_set;

        _render_warrings(request_one.second, one_set);
        _render_keywords(request_one.second, one_set);

        push_render_result(request_one.first, one_set);
        emit renderFinish(request_one.first);
    }
}

void RenderWorker::_render_warrings(const QString &content, QList<std::tuple<QTextCharFormat, QString, int, int> > &one_set)
{
    auto warrings = config.warringWords();
    QTextCharFormat format;
    config.warringFormat(format);


    for (auto one : warrings) {
        QRegExp exp("("+one+").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(content, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            one_set.append(std::make_tuple(format, wstr, sint, lint));
        }
    }
}

void RenderWorker::_render_keywords(const QString &content, QList<std::tuple<QTextCharFormat, QString, int, int> > &one_set)
{
    auto keywords = config.keywordsList();
    QTextCharFormat format2;
    config.keywordsFormat(format2);

    for (auto one: keywords) {
        QRegExp exp("("+one+").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(content, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            one_set.append(std::make_tuple(format2, wstr, sint, lint));
        }
    }
}

QPair<QTextBlock, QString> RenderWorker::take_render_request()
{
    req_sgl.acquire();
    QMutexLocker lock(&req_protect);

    return request_stored.takeAt(request_stored.size()-1);
}

void RenderWorker::push_render_result(const QTextBlock &pholder, const QList<std::tuple<QTextCharFormat, QString, int, int> > formats)
{
    QMutexLocker lock(&result_protect);

    result_stored.insert(0, qMakePair(pholder, formats));
}



KeywordsRender::KeywordsRender(QTextDocument *target, ConfigHost &config)
    :QSyntaxHighlighter (target), config(config), thread(new RenderWorker(config))
{
    thread->start();
    connect(thread, &RenderWorker::renderFinish,this,   &QSyntaxHighlighter::rehighlightBlock);
    connect(thread, &QThread::finished,         this,   &QThread::deleteLater);
}

KeywordsRender::~KeywordsRender() {
    thread->quit();
    thread->wait();
}

void KeywordsRender::highlightBlock(const QString &text){
    if(!text.size())
        return;

    auto blk = currentBlock();
    if(!blk.isValid()){
        thread->topResult();
        return;
    }

    auto format_set = thread->topResult();
    if(format_set.first != blk){
        thread->pushRenderRequest(blk, text);
        return;
    }

    auto formats = format_set.second;
    for (auto item : formats) {
        auto charformat = std::get<0>(item);
        auto word = std::get<1>(item);
        auto start = std::get<2>(item);
        auto len = std::get<3>(item);

        setFormat(start, len, charformat);
    }
    thread->discardTopResult();
}
