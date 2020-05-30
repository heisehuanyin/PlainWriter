#include "novelhost.h"

#include <QTextCodec>
#include <QTextCursor>
#include <QTextFrame>
#include <QtDebug>

NovelHost::NovelHost(ConfigHost &config)
    :config_host(config),
      struct_discrib(nullptr),
      content_presentation(new QTextDocument(this)),
      node_navigate_model(new QStandardItemModel(this)),
      result_enter_model(new QStandardItemModel(this)),
      hiden_formater(new BlockHidenVerify(content_presentation)),
      keywords_formater(new KeywordsRender(content_presentation, config)),
      global_formater(new GlobalFormatRender(content_presentation, config))
{
    QTextFrameFormat novel_frame_format;
    config.novelFrameFormat(novel_frame_format);
    content_presentation->rootFrame()->setFrameFormat(novel_frame_format);

    connect(node_navigate_model,   &QStandardItemModel::itemChanged,  this,  &NovelHost::navigate_title_midify);
    node_navigate_model->setHorizontalHeaderLabels(QStringList() << "章节标题" << "字数统计");
}

NovelHost::~NovelHost()
{
    delete hiden_formater;
    delete keywords_formater;
    delete global_formater;
}

int NovelHost::loadDescription(QString &err, StructDescription *desp)
{
    // save description
    this->struct_discrib = desp;

    // insert novel title
    auto title = struct_discrib->novelTitle();
    insert_bigtitle(content_presentation, title, config_host);

    for (int vm_index=0; vm_index<struct_discrib->volumeCount(); ++vm_index) {
        int code, chpr_count;
        QString vm_title;

        if((code = struct_discrib->volumeTitle(err, vm_index, vm_title)))
            return code;

        // append volume
        auto volume = append_volume(content_presentation, vm_title, config_host);
        if((code = struct_discrib->chapterCount(err, vm_index, chpr_count)))
            return code;

        for (int chpr_index=0; chpr_index<chpr_count; ++chpr_index) {
            QString chpr_title;
            if((code = struct_discrib->chapterTitle(err, vm_index, chpr_index, chpr_title)))
                return code;

            // append chapter
            auto chapter_cursor = append_chapter(volume, chpr_title, config_host);

            QString file_path, file_encoding;
            if((code = struct_discrib->chapterCanonicalFilepath(err, vm_index, chpr_index, file_path)))
                return code;

            if((code = struct_discrib->chapterTextEncoding(err, vm_index, chpr_index, file_encoding)))
                return code;

            QFile file(file_path);
            if(!file.exists()){
                err = "加载内容过程，指定路径文件不存在："+file_path;
                return -1;
            }

            if(!file.open(QIODevice::Text|QIODevice::ReadOnly)){
                err = "加载内容过程，指定路径文件无法打开："+file_path;
                return -1;
            }

            QTextStream tin(&file);
            tin.setCodec(file_encoding.toLocal8Bit());
            chapter_cursor.insertText(tin.readAll());
        }
    }

    content_presentation->clearUndoRedoStacks();
    return 0;
}


int NovelHost::save(QString &errorOut, const QString &filePath)
{
    auto xret = struct_discrib->save(errorOut, filePath);
    if(xret) return xret;

    for (auto vm_index=0; vm_index<node_navigate_model->rowCount(); ++vm_index) {
        auto item = node_navigate_model->item(vm_index);
        auto xitem = static_cast<ReferenceItem*>(item);

        if(xitem->modified()){
            for (auto chp_index=0; chp_index<xitem->rowCount(); ++chp_index) {
                auto chapter = xitem->child(chp_index);
                auto xchapter = static_cast<ReferenceItem*>(chapter);

                if(xchapter->modified()){
                    QString file_canonical_path;
                    if((xret = struct_discrib->chapterCanonicalFilepath(errorOut, vm_index, chp_index, file_canonical_path)))
                        return xret;

                    QFile file(file_canonical_path);
                    if(!file.open(QIODevice::Text|QIODevice::WriteOnly)){
                        errorOut = "保存内容过程，目标无法打开："+ file_canonical_path;
                        return -1;
                    }

                    QTextStream txt_out(&file);
                    QString file_encoding;
                    if((xret = struct_discrib->chapterTextEncoding(errorOut, vm_index, chp_index, file_encoding)))
                        return xret;

                    txt_out.setCodec(file_encoding.toLocal8Bit());
                    txt_out << chapterTextContent(xchapter->index());

                    txt_out.flush();
                    file.flush();
                    file.close();
                }
            }
        }
    }

    return 0;
}

QTextDocument *NovelHost::presentDocument() const
{
    return content_presentation;
}

QStandardItemModel *NovelHost::navigateTree() const
{
    return node_navigate_model;
}

int NovelHost::appendVolume(QString &errOut, const QString &gName)
{
    append_volume(content_presentation, gName, config_host);

    int code;
    if((code = struct_discrib->insertVolume(errOut, struct_discrib->volumeCount()+1, gName)))
        return code;

    return 0;
}

int NovelHost::appendChapter(QString &errOut, const QString &aName, const QModelIndex &volume_navigate_index)
{
    if(!volume_navigate_index.isValid()){
        errOut = "appendChapter: 非法modelindex";
        return -1;
    }

    int code;

    // 选中了卷节点
    auto item = node_navigate_model->itemFromIndex(volume_navigate_index);
    for (int vm_index=0; vm_index < node_navigate_model->rowCount(); ++vm_index) {
        auto volume_node = node_navigate_model->item(vm_index);
        if(volume_node == item){
            auto volume_item = static_cast<ReferenceItem*>(item);
            append_chapter(volume_item->getAnchorItem(), aName, config_host);

            if((code = struct_discrib->insertChapter(errOut, vm_index, volume_item->rowCount()+1, aName)))
                return code;
            return 0;
        }
    }

    // 选中了章节节点
    auto pitem = item->parent();
    append_chapter(static_cast<ReferenceItem*>(pitem)->getAnchorItem(), aName, config_host);
    if((code = struct_discrib->insertChapter(errOut, pitem->index().row(), pitem->rowCount()+1, aName)))
        return code;
    return 0;
}

int NovelHost::removeNode(QString &errOut, const QModelIndex &index)
{
    if(!index.isValid()){
        errOut = "index无效";
        return -1;
    }

    int code;
    if((code = remove_node_recursive(errOut, index)))
        return code;
    return 0;
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

QStandardItemModel *NovelHost::searchResultPresent() const
{
    return result_enter_model;
}

void NovelHost::searchText(const QString &text)
{
    QRegExp exp("("+text+").*");
    result_enter_model->clear();
    result_enter_model->setHorizontalHeaderLabels(QStringList() << "搜索文本");

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
            item->setData(len, Qt::UserRole+2);
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

void NovelHost::reHighlightDocument()
{
    keywords_formater->rehighlight();
    global_formater->rehighlight();
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
    cur.setBlockCharFormat(title_text_format);
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
    volume_pos.setBlockCharFormat(volume_title_text_format);
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
    chapter_pos.setBlockCharFormat(chapter_title_text_format);
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
    chapter_text_pos.setBlockCharFormat(chapter_text_format);

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

int NovelHost::remove_node_recursive(QString &errOut, const QModelIndex &one)
{
    auto item = node_navigate_model->itemFromIndex(one);
    auto xitem = static_cast<ReferenceItem*>(item);

    auto anchor = xitem->getAnchorItem();

    QTextCursor cursor(anchor);
    cursor.setPosition(anchor->firstPosition()-1);
    cursor.setPosition(anchor->lastPosition(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    auto parent = item->parent();
    if(parent){
        parent->removeRow(item->row());

        int code;
        if((struct_discrib->removeChapter(errOut, parent->row(), item->row())))
            return code;
    }
    else{
        node_navigate_model->removeRow(item->row());

        int code;
        if((struct_discrib->removeVolume(errOut, item->row())))
            return code;
    }

    return 0;
}




ReferenceItem::ReferenceItem(const QString &disp, QTextFrame *anchor)
    :QStandardItem (disp),anchor_item(anchor),modify_flag(false){}

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





// highlighter collect ===========================================================================
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
    thread->terminate();
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


// novel-struct describe ===================================================================================
StructDescription::StructDescription(){}

StructDescription::~StructDescription(){}

void StructDescription::newDescription()
{
    struct_dom_store.appendChild(struct_dom_store.createProcessingInstruction("xml", "version='1.0' encoding='utf-8'"));
    auto root = struct_dom_store.createElement("root");
    root.setAttribute("version", "1.0");
    root.setAttribute("title", "新建小说");
    struct_dom_store.appendChild(root);

    auto config = struct_dom_store.createElement("config");
    root.appendChild(config);

    auto structnode = struct_dom_store.createElement("struct");
    root.appendChild(structnode);
}

int StructDescription::openDescription(QString &errOut, const QString &filePath)
{
    filepath_stored = filePath;

    QFile file(filePath);
    if(!file.exists()){
        errOut = "读取过程指定文件路径不存在:"+filePath;
        return -1;
    }

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        errOut = "读取过程指定文件打不开："+filePath;
        return -1;
    }

    int rown,coln;
    QString temp;
    if(!struct_dom_store.setContent(&file, false, &temp, &rown, &coln)){
        errOut = QString(temp+"(r:%1,c:%2)").arg(rown, coln);
        return -1;
    }
    return 0;
}

QString StructDescription::novelDescribeFilePath() const
{
    return filepath_stored;
}

int StructDescription::save(QString &errOut, const QString &newFilepath)
{
    if(newFilepath != "")
        filepath_stored = newFilepath;

    if(filepath_stored == ""){
        errOut = "在一个空路径上存储文件";
        return -1;
    }

    QFile file(filepath_stored);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){
        errOut = "写入过程指定文件打不开："+ filepath_stored;
        filepath_stored = "";
        return -1;
    }

    QTextStream textOut(&file);
    struct_dom_store.save(textOut, 2);
    textOut.flush();
    file.close();

    return 0;
}

QString StructDescription::novelTitle() const
{
    auto root = struct_dom_store.documentElement();
    return root.attribute("title");
}

int StructDescription::volumeCount() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
    return struct_node.childNodes().size();
}

int StructDescription::volumeTitle(QString &errOut, int volumeIndex, QString &titleOut) const
{
    QDomElement volume_node;
    auto x = find_volume_domnode_by_index(errOut, volumeIndex, volume_node);
    if(x) return x;

    titleOut = volume_node.attribute("title");
    return 0;
}

int StructDescription::insertVolume(QString &errOut, int volumeIndexBefore, const QString &volumeTitle)
{
    auto newv = struct_dom_store.createElement("volume");
    newv.setAttribute("title", volumeTitle);

    int ret;
    QDomElement volume_node;
    if((ret = find_volume_domnode_by_index(errOut, volumeIndexBefore, volume_node))){
        auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
        struct_node.appendChild(newv);
    }
    else {
        struct_dom_store.insertBefore(newv, volume_node);
    }

    return 0;
}

int StructDescription::removeVolume(QString &errOut, int volumeIndex)
{
    int ret;
    QDomElement volume_node;

    if((ret = find_volume_domnode_by_index(errOut, volumeIndex, volume_node)))
        return ret;

    volume_node.parentNode().removeChild(volume_node);
    return 0;
}

int StructDescription::resetVolumeTitle(QString &errOut, int volumeIndex, const QString &volumeTitle)
{
    int ret_code;
    QDomElement volume_node;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndex, volume_node)))
        return ret_code;

    volume_node.setAttribute("title", volumeTitle);
    return 0;
}

int StructDescription::chapterCount(QString &errOut, int volumeIndex, int &numOut) const
{
    int ret_code;
    QDomElement volume_dom;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return ret_code;

    numOut = volume_dom.childNodes().size();
    return 0;
}

int StructDescription::insertChapter(QString &errOut, int volumeIndexAt, int chapterIndexBefore,
                               const QString &chapterTitle, const QString &encoding)
{
    int ret_code;
    QDomElement volume_dom;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return ret_code;

    QList<QString> paths;
    // 新建章节节点
    auto all_chapter = struct_dom_store.elementsByTagName("chapter");
    for (int index=0; index<all_chapter.size(); ++index) {
        auto dom_item = all_chapter.at(index).toElement();

        auto relative_path = dom_item.attribute("relative");
        paths.append(relative_path);
    }

    QString new_relative_path = "chapter_0000000000.txt";
    while (paths.contains(new_relative_path)) {
        new_relative_path = QString("chapter_%1.txt").arg(gen.generate64());
    }

    auto newdom = struct_dom_store.createElement("chapter");
    newdom.setAttribute("title", chapterTitle);
    newdom.setAttribute("relative", new_relative_path);
    newdom.setAttribute("encoding", encoding);


    QDomElement chapter_dom;
    if((ret_code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndexBefore, chapter_dom))){
        volume_dom.appendChild(newdom);
    }
    else {
        volume_dom.insertBefore(newdom, chapter_dom);
    }

    return 0;
}

int StructDescription::removeChapter(QString &errOut, int volumeIndexAt, int chapterIndex)
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    volume_dom.removeChild(chapter_dom);
    return 0;
}

int StructDescription::resetChapterTitle(QString &errOut, int volumeIndexAt, int chapterIndex, const QString &title)
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    chapter_dom.setAttribute("title", title);
    return 0;
}

int StructDescription::chapterTitle(QString &errOut, int volumeIndex, int chapterIndex, QString &titleOut) const
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    titleOut =  chapter_dom.attribute("title");
    return 0;
}

int StructDescription::chapterCanonicalFilepath(QString &errOut, int volumeIndex, int chapterIndex, QString &pathOut) const
{
    int code;
    QDomElement volume_dom, chapter_dom;

    if((code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return code;

    if((code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return code;

    auto relative_path = chapter_dom.attribute("relative");
    auto dir_path = QFileInfo(filepath_stored).canonicalPath();

    pathOut = QDir(dir_path).filePath(relative_path);
    return 0;
}

int StructDescription::chapterTextEncoding(QString &errOut, int volumeIndex, int chapterIndex, QString &encodingOut) const
{
    int code;
    QDomElement volume_dom, chapter_dom;

    if((code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return code;

    if((code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return code;

    encodingOut = chapter_dom.attribute("encoding");
    return 0;
}

int StructDescription::find_volume_domnode_by_index(QString &errO, int index, QDomElement &domOut) const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
    auto node = struct_node.firstChildElement("volume");

    while (!node.isNull()) {
        if(!index){
            domOut = node.toElement();
            return 0;
        }

        node = node.nextSiblingElement("volume");
        index--;
    }

    errO = QString("volumeIndex超界：%1").arg(index);
    return -1;
}

int StructDescription::find_chapter_domnode_ty_index(QString &errO, const QDomElement &volumeNode,
                                               int index, QDomElement &domOut) const
{
    auto node = volumeNode.firstChildElement("chapter");

    while (!node.isNull()) {
        if(!index){
            domOut = node.toElement();
            return 0;
        }

        node = node.nextSiblingElement("chapter");
        index--;
    }

    errO = QString("chapterIndex超界：%1").arg(index);
    return -1;
}

GlobalFormatRender::GlobalFormatRender(QTextDocument *target, ConfigHost &config)
    :QSyntaxHighlighter(target), host(config){}

void GlobalFormatRender::highlightBlock(const QString &text)
{
    if(!text.size())
        return;

    auto blk = currentBlock();
    if(!blk.isValid())
        return;

    QTextCursor cursor(blk);
    auto blkaround = cursor.currentFrame();
    if(!blkaround) return;

    auto blkgroup = blkaround->parentFrame();
    auto doc = blk.document();
    // 小说标题
    if(blkgroup == doc->rootFrame()){
        QTextCharFormat charformat;
        QTextBlockFormat blkformat;

        host.novelTitleFormat(blkformat, charformat);
        cursor.setBlockFormat(blkformat);
        cursor.setBlockCharFormat(charformat);
        return;
    }

    // 卷宗标题
    blkgroup = blkgroup->parentFrame();
    if(blkgroup == doc->rootFrame()){
        QTextCharFormat charformat;
        QTextBlockFormat blkformat;

        host.volumeTitleFormat(blkformat, charformat);
        cursor.setBlockFormat(blkformat);
        cursor.setBlockCharFormat(charformat);
        return;
    }

    // 章节标题
    blkgroup = blkgroup->parentFrame();
    if(blkgroup == doc->rootFrame()){
        auto chapterframe = blkaround->parentFrame();
        for (auto it=chapterframe->begin(); !it.atEnd(); ++it) {
            if(it.currentFrame()){
                QTextCharFormat charformat;
                QTextBlockFormat blkformat;

                if(it.currentFrame() == blkaround)
                    host.chapterTitleFormat(blkformat, charformat);
                else
                    host.chapterTextFormat(blkformat, charformat);

                cursor.setBlockFormat(blkformat);
                cursor.setBlockCharFormat(charformat);
                break;
            }
        }
    }
}
