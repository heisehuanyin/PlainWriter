#include "common.h"
#include "novelhost.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStyle>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QThreadPool>
#include <QtDebug>

using namespace NovelBase;

NovelHost::NovelHost(ConfigHost &config)
    :config_host(config),
      desp_tree(nullptr),
      outline_tree_model(new QStandardItemModel(this)),
      novel_description_present(new QTextDocument(this)),
      volume_description_present(new QTextDocument(this)),
      foreshadows_under_volume_present(new QStandardItemModel(this)),
      foreshadows_until_remain_present(new QStandardItemModel(this)),
      find_results_model(new QStandardItemModel(this)),
      chapters_navigate_model(new QStandardItemModel(this)),
      outline_under_volume_prsent(new QStandardItemModel(this)){}


NovelHost::~NovelHost(){}

void NovelHost::loadDescription(FStruct *desp)
{
    // save description structure
    this->desp_tree = desp;

    for (int volume_index = 0; volume_index < desp_tree->volumeCount(); ++volume_index) {
        FStruct::NHandle volume_node = desp->volumeAt(volume_index);
        // 在chapters-tree和outline-tree上插入卷节点
        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int keystory_count = desp->keystoryCount(volume_node);
        for (int keystory_index = 0; keystory_index < keystory_count; ++keystory_index) {
            FStruct::NHandle keystory_node = desp->keystoryAt(volume_node, keystory_index);

            // outline-tree上插入故事节点
            auto ol_keystory_item = new OutlinesItem(keystory_node);
            outline_volume_node->appendRow(ol_keystory_item);

            // outline-tree上插入point节点
            int points_count = desp->pointCount(keystory_node);
            for (int points_index = 0; points_index < points_count; ++points_index) {
                FStruct::NHandle point_node = desp->pointAt(keystory_node, points_index);

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }

            // chapters上插入chapter节点
            int chapter_count = desp->chapterCount(keystory_node);
            for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
                FStruct::NHandle chapter_node = desp->chapterAt(keystory_node, chapter_index);

                QList<QStandardItem*> node_navigate_row;
                auto node_navigate_chapter_node = new ChaptersItem(*this, chapter_node);
                node_navigate_row << node_navigate_chapter_node;
                node_navigate_row << new QStandardItem("-");
                node_navigate_volume_node->appendRow(node_navigate_row);
            }
        }
    }

    novel_description_present->setPlainText(desp_tree->novelDescription());
    novel_description_present->setModified(false);
    novel_description_present->clearUndoRedoStacks();
    connect(novel_description_present,  &QTextDocument::contentsChanged,    this,   &NovelHost::resetNovelDescription);
}

void NovelHost::save(const QString &filePath)
{
    desp_tree->save(filePath);

    for (auto vm_index=0; vm_index<chapters_navigate_model->rowCount(); ++vm_index) {
        auto item = chapters_navigate_model->item(vm_index);
        auto volume_node = static_cast<ChaptersItem*>(item);

        for (auto chp_index=0; chp_index<volume_node->rowCount(); ++chp_index) {
            auto chp_item = volume_node->child(chp_index);
            auto chapter_node = static_cast<ChaptersItem*>(chp_item);
            // 检测文件是否打开
            if(!opening_documents.contains(chapter_node))
                continue;
            auto pak = opening_documents.value(chapter_node);

            // 检测文件是否修改
            if(pak.first->isModified()){
                auto target = chapter_node->getHandleRef();
                QString file_canonical_path = desp_tree->chapterCanonicalFilePath(target);

                QFile file(file_canonical_path);
                if(!file.open(QIODevice::Text|QIODevice::WriteOnly))
                    throw new WsException("保存内容过程，目标无法打开："+ file_canonical_path);

                QTextStream txt_out(&file);
                QString file_encoding = desp_tree->chapterTextEncoding(target);
                txt_out.setCodec(file_encoding.toLocal8Bit());

                QString content = chapterTextContent(chapter_node->index());
                txt_out << content;
                txt_out.flush();
                file.flush();
                file.close();
            }
        }
    }
}

QString NovelHost::novelTitle() const
{
    return desp_tree->novelTitle();
}

void NovelHost::resetNovelTitle(const QString &title)
{
    desp_tree->resetNovelTitle(title);
}

QStandardItemModel *NovelHost::outlineTree() const
{
    return outline_tree_model;
}

QTextDocument *NovelHost::novelDescriptions() const
{
    return novel_description_present;
}

QTextDocument *NovelHost::volumeDescriptions() const
{
    return volume_description_present;
}

QStandardItemModel *NovelHost::foreshadowsUnderVolume() const
{
    return foreshadows_under_volume_present;
}

QStandardItemModel *NovelHost::foreshadowsUntilRemain() const
{
    return foreshadows_until_remain_present;
}


void NovelHost::insertVolume(int before, const QString &gName)
{
    FStruct::NHandle target = desp_tree->volumeAt(before);
    auto volume_new = desp_tree->insertVolume(target, gName, "");

    if(target.isValid()){
        insert_volume(volume_new, desp_tree->volumeCount());
    }
    else {
        insert_volume(volume_new, before);
    }
}

void NovelHost::insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName)
{
    if(!vmIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_tree_model->itemFromIndex(vmIndex);
    auto volume_tree_node = static_cast<OutlinesItem*>(node);
    auto volume_struct_node = volume_tree_node->getHandleRef();
    desp_tree->checkNandleValid(volume_struct_node, FStruct::NHandle::Type::VOLUME);

    int knode_count = desp_tree->keystoryCount(volume_tree_node->getHandleRef());
    FStruct::NHandle keystory_node = desp_tree->insertKeystory(volume_struct_node, before, kName, "");
    if(before >= knode_count)
        volume_tree_node->appendRow(new OutlinesItem(keystory_node));
    else
        volume_tree_node->insertRow(before, new OutlinesItem(keystory_node));
}

void NovelHost::insertPoint(const QModelIndex &kIndex, int before, const QString &pName)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto outline_keystory_node = static_cast<OutlinesItem*>(node);
    auto struct_keystory_node = outline_keystory_node->getHandleRef();
    desp_tree->checkNandleValid(struct_keystory_node, FStruct::NHandle::Type::KEYSTORY);

    int points_count = desp_tree->pointCount(struct_keystory_node);
    FStruct::NHandle point_node = desp_tree->insertPoint(struct_keystory_node, points_count, pName, "");

    if(before >= points_count)
        outline_keystory_node->appendRow(new OutlinesItem(point_node));
    else
        outline_keystory_node->insertRow(before, new OutlinesItem(point_node));
}

void NovelHost::appendForeshadow(const QModelIndex &kIndex, const QString &fName,
                                 const QString &desp, const QString &desp_next)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto outline_keystory_node = static_cast<OutlinesItem*>(node);
    auto struct_keystory_node = outline_keystory_node->getHandleRef();
    desp_tree->checkNandleValid(struct_keystory_node, FStruct::NHandle::Type::KEYSTORY);

    desp_tree->appendForeshadow(struct_keystory_node, fName, desp, desp_next);
}

void NovelHost::removeOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("指定modelindex无效");

    auto item = outline_tree_model->itemFromIndex(outlineNode);
    auto outline_target_node = static_cast<OutlinesItem*>(item);
    auto pnode = outline_target_node->QStandardItem::parent();

    if(!pnode){
        outline_tree_model->removeRow(outline_target_node->row());
        chapters_navigate_model->removeRow(outline_target_node->row());
    }
    else {
        pnode->removeRow(outline_target_node->row());
    }

    desp_tree->removeHandle(outline_target_node->getHandleRef());
}

int NovelHost::setCurrentOutlineNode(QString &err, const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid()){
        err = "传入的outlinemodelindex无效";
        return -1;
    }

    auto current = outline_tree_model->itemFromIndex(outlineNode);
    auto outline_node = static_cast<OutlinesItem*>(current);
    int code;
    // 设置当前卷节点描述文档模型内容，修改卷节点文档指向
    if((code = set_current_volume_node(err, outline_node)))
        return code;

    // 设置当前节点描述文档内容与相关内容
    if((code = set_current_outline_node(err, outline_node)))
        return code;

    return 0;
}
// msgList : [type](target)<keys-to-target>msg-body
void NovelHost::checkRemoveEffect(const FStruct::NHandle &target, QList<QString> &msgList) const
{
    if(target.nType() == FStruct::NHandle::Type::POINT)
        return;

    if(target.nType() == FStruct::NHandle::Type::FORESHADOW) {
        auto volume_node = desp_tree->parentHandle(target);
        auto keys_path = desp_tree->foreshadowKeysPath(target);

        FStruct::NHandle chapter_ins = desp_tree->firstChapterOfFStruct();
        while (chapter_ins.isValid()) {
            auto start_ins = desp_tree->findShadowstart(chapter_ins, keys_path);
            if(start_ins.isValid()){
                msgList << "[error](chapter)<"+desp_tree->chapterKeysPath(chapter_ins)+">指定章节作为伏笔埋设，内容将失效！";
                break;
            }

            chapter_ins = desp_tree->nextChapterOfFStruct(chapter_ins);
        }
        while (chapter_ins.isValid()) {
            auto stop_ins = desp_tree->findShadowstop(chapter_ins, keys_path);
            if(stop_ins.isValid()){
                msgList << "[error](chapter)<"+desp_tree->chapterKeysPath(chapter_ins)+">指定章节作为伏笔承接，内容将失效！";
                break;
            }

            chapter_ins = desp_tree->nextChapterOfFStruct(chapter_ins);
        }
        return;
    }
    if(target.nType() == FStruct::NHandle::Type::KEYSTORY){
        auto foreshadow_count = desp_tree->foreshadowCount(target);
        for (int var = 0; var < foreshadow_count; ++var) {
            auto foreshadow = desp_tree->foreshadowAt(target, var);
            checkRemoveEffect(foreshadow, msgList);
        }
        return;
    }
    if(target.nType() == FStruct::NHandle::Type::VOLUME){
        auto keystory_count = desp_tree->keystoryCount(target);
        for (int var = 0; var < keystory_count; ++var) {
            auto keystory = desp_tree->keystoryAt(target, var);
            checkRemoveEffect(keystory, msgList);
        }
        auto chapter_count = desp_tree->chapterCount(target);
        for (int var = 0; var < chapter_count; ++var) {
            auto chapter = desp_tree->chapterAt(target, var);
            checkRemoveEffect(chapter, msgList);
        }
    }
    if(target.nType() == FStruct::NHandle::Type::CHAPTER){
        // 校验伏笔吸附事项，校验影响
        auto start_count = desp_tree->shadowstartCount(target);
        for (int var=0; var<start_count; ++var) {
            auto start_ins = desp_tree->shadowstartAt(target, var);
            auto target_path = start_ins.attr("target");
            msgList << "[error](foreshadow)<"+target_path+">伏笔埋设将被删除，此伏笔将悬空！";

            auto chapter_ins = desp_tree->firstChapterOfFStruct();
            while (chapter_ins.isValid()) {
                auto x = desp_tree->findShadowstop(chapter_ins, target_path);
                if(x.isValid()){
                    msgList << "[error](chapter)<"+desp_tree->chapterKeysPath(chapter_ins)+">伏笔悬空，此章节作为伏笔承接，内容将失效";
                    break;
                }

                chapter_ins = desp_tree->nextChapterOfFStruct(chapter_ins);
            }
        }
        // 校验伏笔承接事项，校验影响
        auto stop_count = desp_tree->shadowstopCount(target);
        for (int var = 0; var < stop_count; ++var) {
            auto stop_ins = desp_tree->shadowstopAt(target, var);
            auto target_path = stop_ins.attr("target");
            msgList << "[warning](foreshadow)<"+target_path+">删除伏笔承接，伏笔将打开。";
        }
    }
}



// 写作界面
QStandardItemModel *NovelHost::chaptersNavigateTree() const
{
    return chapters_navigate_model;
}

QStandardItemModel *NovelHost::findResultsPresent() const
{
    return find_results_model;
}

QStandardItemModel *NovelHost::outlinesUnderVolume() const
{
    return outline_under_volume_prsent;
}


void NovelHost::insertChapter(const QModelIndex &chpVmIndex, int before, const QString &chpName)
{
    if(!chpVmIndex.isValid())
        throw new WsException("输入volumeindex：chapters无效");

    auto item = chapters_navigate_model->itemFromIndex(chpVmIndex);
    auto chapters_volume = static_cast<ChaptersItem*>(item);
    auto struct_volumme = chapters_volume->getHandleRef();
    desp_tree->checkNandleValid(struct_volumme, FStruct::NHandle::Type::VOLUME);

    auto count = desp_tree->chapterCount(struct_volumme);
    auto newnode = desp_tree->insertChapter(struct_volumme, before, chpName, "");
    if(before >= count){
        item->appendRow(new ChaptersItem(*this, newnode));
    }
    else {
        item->insertRow(before, new ChaptersItem(*this, newnode));
    }

    QString file_path = desp_tree->chapterCanonicalFilePath(newnode);

    QFile target(file_path);
    if(target.exists())
        throw new WsException("软件错误，出现重复文件名："+file_path);

    if(!target.open(QIODevice::WriteOnly|QIODevice::Text))
        throw new WsException("软件错误，指定路径文件无法打开："+file_path);

    target.close();
}

void NovelHost::appendShadowstart(const QModelIndex &chpIndex, const QString &keystory, const QString &foreshadow)
{
    if(!chpIndex.isValid())
        throw new WsException("传入的章节index非法");

    auto item = chapters_navigate_model->itemFromIndex(chpIndex);
    auto chapters_chapter_node = static_cast<ChaptersItem*>(item);
    auto struct_chapter_node = chapters_chapter_node->getHandleRef();
    desp_tree->checkNandleValid(struct_chapter_node, FStruct::NHandle::Type::CHAPTER);

    desp_tree->appendShadowstart(struct_chapter_node, keystory, foreshadow);
}

void NovelHost::appendShadowstop(const QModelIndex &chpIndex, const QString &volume,
                                 const QString &keystory, const QString &foreshadow)
{
    if(!chpIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto item = chapters_navigate_model->itemFromIndex(chpIndex);
    auto chapters_chapter_node = static_cast<ChaptersItem*>(item);
    auto struct_chapter_node = chapters_chapter_node->getHandleRef();
    desp_tree->checkNandleValid(struct_chapter_node, FStruct::NHandle::Type::CHAPTER);

    desp_tree->appendShadowstop(struct_chapter_node, volume, keystory, foreshadow);
}

void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("chaptersNodeIndex无效");

    auto item = chapters_navigate_model->itemFromIndex(chaptersNode);
    auto chapters_target = static_cast<ChaptersItem*>(item);
    desp_tree->removeHandle(chapters_target->getHandleRef());

    // 卷宗节点管理同步
    if(!item->parent()){
        chapters_navigate_model->removeRow(item->row());
        outline_tree_model->removeRow(item->row());
    }
    // 章节节点
    else {
        item->parent()->removeRow(item->row());
    }
}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_model->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_model->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
    }
}



void NovelHost::searchText(const QString &text)
{
    QRegExp exp("("+text+").*");
    find_results_model->clear();
    find_results_model->setHorizontalHeaderLabels(QStringList() << "搜索文本" << "卷宗节点" << "章节节点");

    for (int vm_index=0; vm_index<chapters_navigate_model->rowCount(); ++vm_index) {
        auto chapters_volume_node = chapters_navigate_model->item(vm_index);

        for (int chapters_chp_index=0; chapters_chp_index<chapters_volume_node->rowCount(); ++chapters_chp_index) {
            auto chapters_chp_node = chapters_volume_node->child(chapters_chp_index);
            QString content = chapterTextContent(chapters_chp_node->index());

            auto pos = -1;
            while ((pos = exp.indexIn(content, pos+1)) != -1) {
                auto word = exp.cap(1);
                auto len = word.length();

                auto text_result = content.mid(pos, 20).replace(QRegExp("\\s"), "");
                QList<QStandardItem*> row;
                QStandardItem *item;
                if(pos == 0)
                    item = new QStandardItem(text_result.length()<20?text_result+"……":text_result);
                else
                    item = new QStandardItem("……"+(text_result.length()<20?text_result+"……":text_result));

                item->setData(chapters_chp_node->index(), Qt::UserRole+1);
                item->setData(pos, Qt::UserRole + 2);
                item->setData(len, Qt::UserRole + 3);
                row << item;

                row << new QStandardItem(chapters_volume_node->text());
                row << new QStandardItem(chapters_chp_node->text());
                find_results_model->appendRow(row);
            }
        }
    }
}


QString NovelHost::chapterTextContent(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        throw new WsException("输入index无效");

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = chapters_navigate_model->itemFromIndex(index);
    auto refer_node = static_cast<ChaptersItem*>(item);

    desp_tree->checkNandleValid(refer_node->getHandleRef(), FStruct::NHandle::Type::CHAPTER);
    if(opening_documents.contains(refer_node)){
        auto pack = opening_documents.value(refer_node);
        auto doc = pack.first;
        return doc->toPlainText();
    }

    QString file_path = desp_tree->chapterCanonicalFilePath(refer_node->getHandleRef());
    QString fencoding = desp_tree->chapterTextEncoding(refer_node->getHandleRef());

    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly|QIODevice::Text))
        throw new WsException("指定文件无法打开："+file_path);

    QTextStream text_in(&file);
    text_in.setCodec(fencoding.toLocal8Bit());
    QString strOut = text_in.readAll();
    file.close();

    return strOut;
}

int NovelHost::calcValidWordsCount(const QString &content)
{
    QString newtext = content;
    QRegExp exp("[，。！？【】“”—…《》：、\\s·￥%「」]");
    return newtext.replace(exp, "").size();
}

void NovelHost::openDocument(const QModelIndex &_index)
{
    if(!_index.isValid())
        throw new WsException("index非法");

    auto index = _index;
    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = chapters_navigate_model->itemFromIndex(index);
    auto chapter_node = static_cast<ChaptersItem*>(item);
    auto bind = chapter_node->getHandleRef();

    // 确保指向正确章节节点
    desp_tree->checkNandleValid(bind, FStruct::NHandle::Type::CHAPTER);
    QString title = bind.attr("title");

    // 校验是否已经处于打开状态
    if(opening_documents.contains(chapter_node)){
        auto pak = opening_documents.value(chapter_node);
        emit documentActived(pak.first, title);
        return;
    }

    // 获取全部内容
    QString text_content = chapterTextContent(index);

    QTextFrameFormat frame_format;
    QTextBlockFormat block_format;
    QTextCharFormat char_format;
    config_host.textFrameFormat(frame_format);
    config_host.textFormat(block_format, char_format);

    auto ndoc = new QTextDocument();
    QTextCursor cur(ndoc);
    ndoc->rootFrame()->setFrameFormat(frame_format);
    cur.setBlockFormat(block_format);
    cur.setBlockCharFormat(char_format);
    cur.insertText(text_content==""?"文档内容空":text_content);
    ndoc->setModified(false);
    ndoc->clearUndoRedoStacks();
    ndoc->setUndoRedoEnabled(true);

    auto render = new KeywordsRender(ndoc, config_host);
    opening_documents.insert(chapter_node, qMakePair(ndoc, render));
    connect(ndoc, &QTextDocument::contentsChanged, chapter_node,  &ChaptersItem::calcWordsCount);
    emit documentOpened(ndoc, title);
    emit documentActived(ndoc, title);
}

void NovelHost::closeDocument(QTextDocument *doc)
{
    save();

    for (auto px : opening_documents) {
        if(px.first == doc){
            emit documentAboutToBeClosed(doc);

            auto key = opening_documents.key(px);
            delete px.second;
            delete px.first;
            opening_documents.remove(key);
        }
    }

    throw new WsException("目标文档未包含或未打开");
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const FStruct::NHandle &item, int index)
{
    auto outline_volume_node = new OutlinesItem(item);

    QList<QStandardItem*> navigate_valume_row;
    auto node_navigate_volume_node = new ChaptersItem(*this, item, true);
    navigate_valume_row << node_navigate_volume_node;
    navigate_valume_row << new QStandardItem("-");


    if(index >= outline_tree_model->rowCount()){
        outline_tree_model->appendRow(outline_volume_node);
        chapters_navigate_model->appendRow(navigate_valume_row);
    }
    else {
        outline_tree_model->insertRow(index, outline_volume_node);
        chapters_navigate_model->insertRow(index, navigate_valume_row);
    }


    return qMakePair(outline_volume_node, node_navigate_volume_node);
}

void NovelHost::resetNovelDescription()
{
    auto content = novel_description_present->toPlainText();
    desp_tree->resetNovelDescription(content);
}









ChaptersItem::ChaptersItem(NovelHost &host, const FStruct::NHandle &refer, bool isGroup)
    :host(host), fstruct_node(refer)
{
    setText(refer.attr("title"));

    if(isGroup){
        setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    }
    else {
        setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    }
}

const FStruct::NHandle ChaptersItem::getHandleRef() const
{
    return fstruct_node;
}

FStruct::NHandle::Type ChaptersItem::getType() const{
    return getHandleRef().nType();
}


void ChaptersItem::calcWordsCount()
{
    auto p = QStandardItem::parent();

    if(!p){
        int number = 0;
        for (auto index = 0; index<rowCount(); ++index) {
            auto child_item = static_cast<ChaptersItem*>(child(index));
            child_item->calcWordsCount();
            number += child(index, 1)->text().toInt();
        }

        model()->item(row(), 1)->setText(QString("%1").arg(number));
    }
    else {
        QString err,content;
        host.chapterTextContent(err, index(), content);

        auto pitem = QStandardItem::parent();
        auto cnode = pitem->child(row(), 1);
        cnode->setText(QString("%1").arg(host.calcValidWordsCount(content)));
    }
}




// highlighter collect ===========================================================================
KeywordsRender::KeywordsRender(QTextDocument *target, ConfigHost &config)
    :QSyntaxHighlighter (target), config(config){}

KeywordsRender::~KeywordsRender(){}

void KeywordsRender::highlightBlock(const QString &text)
{
    if(!text.size())
        return;

    auto warrings = config.warringWords();
    QTextCharFormat format;
    config.warringFormat(format);
    for (auto one : warrings) {
        QRegExp exp("("+one+").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(text, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            setFormat(sint, lint, format);
        }
    }

    auto keywords = config.keywordsList();
    QTextCharFormat format2;
    config.keywordsFormat(format2);
    for (auto one: keywords) {
        QRegExp exp("("+one+").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(text, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            setFormat(sint, lint, format2);
        }
    }
}


// novel-struct describe ===================================================================================
FStruct::FStruct(){}

FStruct::~FStruct(){}

void FStruct::newEmptyFile()
{
    struct_dom_store.appendChild(struct_dom_store.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\""));
    auto root = struct_dom_store.createElement("novel");
    root.setAttribute("version", "2.0");
    struct_dom_store.appendChild(root);

    auto config = struct_dom_store.createElement("config");
    root.appendChild(config);

    auto structnode = struct_dom_store.createElement("struct");
    structnode.setAttribute("title", "新建小说");
    structnode.setAttribute("desp", "小说描述为空");
    root.appendChild(structnode);
}

int FStruct::openFile(QString &errOut, const QString &filePath)
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

QString FStruct::novelDescribeFilePath() const
{
    return filepath_stored;
}

void FStruct::save(const QString &newFilepath)
{
    if(newFilepath != "")
        filepath_stored = newFilepath;

    if(filepath_stored == "")
        throw new WsException("在一个空路径上存储文件");

    QFile file(filepath_stored);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text))
        throw new WsException("写入过程指定文件打不开："+ filepath_stored);

    QTextStream textOut(&file);
    struct_dom_store.save(textOut, 2);
    textOut.flush();
    file.close();
}

QString FStruct::novelTitle() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.attribute("title");
}

void FStruct::resetNovelTitle(const QString &title)
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    struct_node.setAttribute("title", title);
}

QString FStruct::novelDescription() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.attribute("desp");
}

void FStruct::resetNovelDescription(const QString &desp)
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    struct_node.setAttribute("desp", desp);
}

int FStruct::volumeCount() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.elementsByTagName("volume").size();
}

FStruct::NHandle FStruct::volumeAt(int index) const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    QDomElement elm = find_subelm_at_index(struct_node, "volume", index);
    return NHandle(elm, NHandle::Type::VOLUME);
}

FStruct::NHandle FStruct::insertVolume(const FStruct::NHandle &before, const QString &title, const QString &description)
{
    if(before.nType() != NHandle::Type::VOLUME)
        throw new WsException("传入节点类型错误");

    QList<QString> keys;
    for (auto var=0; var<volumeCount(); ++var) {
        FStruct::NHandle volume_one = volumeAt(var);
        QString key = volume_one.attr("key");
        keys << key;
    }

    QString unique_key="volume-0";
    while (keys.contains(unique_key)) {
        unique_key = QString("volume-%1").arg(random_gen.generate64());
    }


    auto newdom = struct_dom_store.createElement("volume");
    NHandle aone(newdom, NHandle::Type::VOLUME);
    aone.setAttr("key", unique_key);
    aone.setAttr("title", title);
    aone.setAttr("desp", description);

    if(!before.isValid()){
        auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
        struct_node.appendChild(newdom);
    }
    else {
        before.dom_stored.parentNode().insertBefore(newdom, before.dom_stored);
    }

    return aone;
}

int FStruct::keystoryCount(const FStruct::NHandle &vmNode) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    auto list = vmNode.dom_stored.elementsByTagName("keystory");
    return list.size();
}

FStruct::NHandle FStruct::keystoryAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    QDomElement elm = find_subelm_at_index(vmNode.dom_stored, "keystory", index);
    return NHandle(elm, NHandle::Type::KEYSTORY);
}

FStruct::NHandle FStruct::insertKeystory(FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    int num = keystoryCount(vmNode);
    QList<QString> kkeys;
    for (int var = 0; var < num; ++var) {
        NHandle one = keystoryAt(vmNode, var);
        QString key = one.attr("key");
        kkeys << key;
    }
    QString unique_key="keystory-0";
    while (kkeys.contains(unique_key)) {
        unique_key = QString("keystory-%1").arg(random_gen.generate64());
    }

    auto ndom = struct_dom_store.createElement("keystory");
    NHandle one(ndom, NHandle::Type::KEYSTORY);
    one.setAttr("key", unique_key);
    one.setAttr("title", title);
    one.setAttr("desp", description);

    if(before >= num){
        vmNode.dom_stored.appendChild(ndom);
    }
    else {
        NHandle _before = keystoryAt(vmNode, before);
        vmNode.dom_stored.insertBefore(ndom, _before.dom_stored);
    }

    return one;
}

int FStruct::pointCount(const FStruct::NHandle &knode) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto list = knode.dom_stored.elementsByTagName("points").at(0).childNodes();
    return list.size();
}

FStruct::NHandle FStruct::pointAt(const FStruct::NHandle &knode, int index) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto points_elm = knode.dom_stored.firstChildElement("points");
    QDomElement elm = find_subelm_at_index(points_elm, "simply", index);
    return NHandle(elm, NHandle::Type::POINT);
}

FStruct::NHandle FStruct::insertPoint(FStruct::NHandle &knode, int before, const QString &title, const QString &description)
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto dom = struct_dom_store.createElement("simply");
    NHandle one(dom, NHandle::Type::POINT);
    one.setAttr("title", title);
    one.setAttr("desp", description);

    int num = pointCount(knode);
    if(before >= num){
        knode.dom_stored.firstChildElement("points").appendChild(dom);
    }
    else {
        NHandle _before = pointAt(knode, before);
        knode.dom_stored.firstChildElement("points").insertBefore(dom, _before.dom_stored);
    }

    return one;
}

int FStruct::foreshadowCount(const FStruct::NHandle &knode) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    return foreshodows_node.elementsByTagName("foreshadow").size();
}

FStruct::NHandle FStruct::foreshadowAt(const FStruct::NHandle &knode, int index) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm = find_subelm_at_index(foreshadows_node, "foreshadow", index);
    return NHandle(elm, NHandle::Type::FORESHADOW);
}

FStruct::NHandle FStruct::appendForeshadow(FStruct::NHandle &knode, const QString &title, const QString &desp, const QString &desp_next)
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    int num=foreshadowCount(knode);
    QList<QString> fkeys;
    for (auto index = 0; index<num; ++index) {
        NHandle one=foreshadowAt(knode, index);
        QString key = one.attr("key");
        fkeys << key;
    }
    QString unique_key="foreshadow-0";
    while (fkeys.contains(unique_key)) {
        unique_key = QString("foreshadow-%1").arg(random_gen.generate64());
    }

    auto elm = struct_dom_store.createElement("foreshadow");
    NHandle one(elm, NHandle::Type::FORESHADOW);
    one.setAttr("key", unique_key);
    one.setAttr("title", title);
    one.setAttr("desp", desp);
    one.setAttr("desp_next", desp_next);

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    foreshadows_node.appendChild(elm);

    return  one;
}

QString FStruct::foreshadowKeysPath(const FStruct::NHandle &foreshadow) const
{
    checkNandleValid(foreshadow, NHandle::Type::FORESHADOW);

    auto keystory_ins = parentHandle(foreshadow);
    auto volume_ins = parentHandle(keystory_ins);
    return volume_ins.attr("key")+"@"+keystory_ins.attr("key")+"@"+foreshadow.attr("key");
}

int FStruct::chapterCount(const FStruct::NHandle &vmNode) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    return vmNode.dom_stored.elementsByTagName("chapter").size();
}

FStruct::NHandle FStruct::chapterAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);
    QDomElement elm = find_subelm_at_index(vmNode.dom_stored, "chapter", index);
    return NHandle(elm, NHandle::Type::CHAPTER);
}

FStruct::NHandle FStruct::insertChapter(FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    QList<QString> ckeys;
    int num = chapterCount(vmNode);
    for (auto var=0; var<num; ++var) {
        NHandle one = chapterAt(vmNode, var);
        QString key = one.attr("key");
        ckeys << key;
    }
    QString unique_key="chapter-0";
    while (ckeys.contains(unique_key)) {
        unique_key = QString("chapter-%1").arg(random_gen.generate64());
    }

    auto elm = struct_dom_store.createElement("chapter");
    NHandle one(elm, NHandle::Type::CHAPTER);
    one.setAttr("key", unique_key);
    one.setAttr("title", title);
    one.setAttr("encoding", "utf-8");
    one.setAttr("relative", unique_key+".txt");
    one.setAttr("desp", description);

    if(before>=num){
        vmNode.dom_stored.appendChild(elm);
    }
    else {
        NHandle _before = chapterAt(vmNode, before);
        vmNode.dom_stored.insertBefore(elm, _before.dom_stored);
    }

    return one;
}

QString FStruct::chapterKeysPath(const FStruct::NHandle &chapter) const
{
    checkNandleValid(chapter, NHandle::Type::CHAPTER);

    auto volume = parentHandle(chapter);
    auto vkey = volume.attr("key");
    return vkey+"@"+chapter.attr("key");
}

QString FStruct::chapterCanonicalFilePath(const FStruct::NHandle &chapter) const
{
    checkNandleValid(chapter, FStruct::NHandle::Type::CHAPTER);

    QString relative_path;
    relative_path = chapter.attr("relative");

    return QDir(QFileInfo(this->filepath_stored).canonicalPath()).filePath(relative_path);
}

QString FStruct::chapterTextEncoding(const FStruct::NHandle &chapter) const
{
    checkNandleValid(chapter, FStruct::NHandle::Type::CHAPTER);

    return chapter.attr("encoding");
}

int FStruct::shadowstartCount(const FStruct::NHandle &chpNode) const
{
    checkNandleValid(chpNode, FStruct::NHandle::Type::CHAPTER);
    return chpNode.dom_stored.elementsByTagName("shadow-start").size();
}

FStruct::NHandle FStruct::shadowstartAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNandleValid(chpNode, FStruct::NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.dom_stored, "shadow-start", index);
    return NHandle(elm, NHandle::Type::SHADOWSTART);
}

FStruct::NHandle FStruct::findShadowstart(const FStruct::NHandle &chpNode, const QString &target) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);

    int count = shadowstartCount(chpNode);
    for (int var = 0; var < count; ++var) {
        auto start_ins = shadowstartAt(chpNode, var);
        if(start_ins.attr("target") == target)
            return start_ins;
    }

    return NHandle(QDomElement(), NHandle::Type::SHADOWSTART);
}

FStruct::NHandle FStruct::appendShadowstart(FStruct::NHandle &chpNode, const QString &keystory, const QString &foreshadow)
{
    checkNandleValid(chpNode, FStruct::NHandle::Type::CHAPTER);

    auto volume = parentHandle(chpNode);
    auto volume_key = volume.attr("key");

    auto elm = struct_dom_store.createElement("shadow-start");
    NHandle one(elm, NHandle::Type::SHADOWSTART);
    one.setAttr("target", volume_key+"@"+keystory+"@"+foreshadow);

    chpNode.dom_stored.appendChild(elm);
    return one;
}

int FStruct::shadowstopCount(const FStruct::NHandle &chpNode) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);
    return chpNode.dom_stored.elementsByTagName("shadow-stop").size();
}

FStruct::NHandle FStruct::shadowstopAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.dom_stored, "shadow-stop", index);
    return NHandle(elm, NHandle::Type::SHADOWSTOP);
}

FStruct::NHandle FStruct::findShadowstop(const FStruct::NHandle &chpNode, const QString &stopTarget) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);

    int count = shadowstopCount(chpNode);
    for (int var=0; var < count; var++) {
        auto stop_ins = shadowstopAt(chpNode, var);
        if(stop_ins.attr("target") == stopTarget)
            return stop_ins;
    }

    return NHandle(QDomElement(), NHandle::Type::SHADOWSTOP);
}

FStruct::NHandle FStruct::appendShadowstop(FStruct::NHandle &chpNode, const QString &volume,
                                           const QString &keystory, const QString &foreshadow)
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);

    auto elm = struct_dom_store.createElement("shadow-stop");
    NHandle one(elm, NHandle::Type::SHADOWSTOP);
    one.setAttr("target", volume+"@"+keystory+"@"+foreshadow);

    chpNode.dom_stored.appendChild(elm);
    return one;
}

FStruct::NHandle FStruct::parentHandle(const FStruct::NHandle &base) const
{
    if(!base.isValid())
        throw new WsException("传入无效节点");

    auto pnode = base.dom_stored.parentNode().toElement();

    switch (base.nType()) {
        case NHandle::Type::POINT:
        case NHandle::Type::FORESHADOW:
            return NHandle(pnode, NHandle::Type::KEYSTORY);
        case NHandle::Type::SHADOWSTOP:
        case NHandle::Type::SHADOWSTART:
            return NHandle(pnode, NHandle::Type::CHAPTER);
        case NHandle::Type::KEYSTORY:
        case NHandle::Type::CHAPTER:
            return NHandle(pnode, NHandle::Type::VOLUME);
        default:
            throw new WsException("无有效父节点");
    }
}

int FStruct::handleIndex(const FStruct::NHandle &node) const
{
    if(!node.isValid())
        throw new WsException("传入无效节点");

    auto dom = node.dom_stored;
    auto parent = dom.parentNode().toElement();
    auto elm = parent.firstChildElement(dom.tagName());
    int _int = 0;

    while (!elm.isNull()) {
        if(dom == elm){
            return _int;
        }

        _int++;
        elm = elm.nextSiblingElement(dom.tagName());
    }
    throw new WsException("未知错误");
}

void FStruct::removeHandle(const FStruct::NHandle &node)
{
    if(!node.isValid())
        throw new WsException("指定节点失效");

    if(node.nType() == NHandle::Type::CHAPTER){
        QString filepath = chapterCanonicalFilePath(node);

        QFile file(filepath);
        if(!file.remove())
            throw new WsException("文件系统异常，移除文件失败："+filepath);
    }

    if(node.nType() == NHandle::Type::VOLUME){
        int count = chapterCount(node);

        for(int var=0; var < count ; ++var){
            NHandle _node = chapterAt(node, var);
            removeHandle(_node);
        }
    }

    auto parent = node.dom_stored.parentNode();
    if(parent.isNull())
        throw new WsException("父节点非法");

    parent.removeChild(node.dom_stored);
}

FStruct::NHandle FStruct::firstChapterOfFStruct() const
{
    auto volume_first = volumeAt(0);
    return chapterAt(volume_first, 0);
}

FStruct::NHandle FStruct::lastChapterOfStruct() const
{
    auto volume_count = volumeCount();
    auto last_volume =volumeAt(volume_count-1);
    auto chapters_count = chapterCount(last_volume);
    return chapterAt(last_volume, chapters_count-1);
}

FStruct::NHandle FStruct::nextChapterOfFStruct(const FStruct::NHandle &chapterIns) const
{
    auto volume_ins = parentHandle(chapterIns);
    auto chapter_index = handleIndex(chapterIns);
    auto chapter_count = chapterCount(volume_ins);

    if(chapter_index == chapter_count-1){
        auto volume_index = handleIndex(volume_ins);
        auto volume_next = volumeAt(volume_index+1);
        return chapterAt(volume_next, 0);
    }
    else {
        return chapterAt(volume_ins, chapter_index+1);
    }
}

FStruct::NHandle FStruct::previousChapterOfFStruct(const FStruct::NHandle &chapterIns) const
{
    auto volume_ins = parentHandle(chapterIns);
    auto chapter_index = handleIndex(chapterIns);
    auto volume_index = handleIndex(volume_ins);

    if(chapter_index == 0){
        auto volume_previous = volumeAt(volume_index-1);
        auto chapter_count = chapterCount(volume_previous);
        return chapterAt(volume_previous, chapter_count-1);
    }
    else {
        return chapterAt(volume_ins, chapter_index-1);
    }
}

void FStruct::checkNandleValid(const FStruct::NHandle &node, FStruct::NHandle::Type type) const
{
    if(node.nType() != type)
        throw new WsException("传入节点类型错误");

    if(node.isValid())
        throw new WsException("传入节点已失效");
}

QDomElement FStruct::find_subelm_at_index(const QDomElement &pnode, const QString &tagName, int index) const
{
    auto first = pnode.firstChildElement(tagName);
    while (!first.isNull()) {
        if(!index)
            break;

        index--;
        first = first.nextSiblingElement(tagName);
    }

    return first;
}

FStruct::NHandle::NHandle():type_stored(Type::VOLUME){}

FStruct::NHandle::NHandle(QDomElement domNode, FStruct::NHandle::Type type)
    :dom_stored(domNode),type_stored(type){}

FStruct::NHandle::NHandle(const FStruct::NHandle &other)
    :dom_stored(other.dom_stored),type_stored(other.type_stored){}

FStruct::NHandle &FStruct::NHandle::operator=(const FStruct::NHandle &other)
{
    dom_stored = other.dom_stored;
    type_stored = other.type_stored;
    return *this;
}

bool FStruct::NHandle::operator==(const FStruct::NHandle &other) const
{
    return type_stored == other.type_stored &&
            dom_stored == other.dom_stored;
}

FStruct::NHandle::Type FStruct::NHandle::nType() const
{
    return type_stored;
}

bool FStruct::NHandle::isValid() const
{
    return !dom_stored.isNull();
}

QString FStruct::NHandle::attr(const QString &name) const
{
    if(dom_stored.isNull())
        throw new WsException("节点已失效");

    return dom_stored.attribute(name);
}

void FStruct::NHandle::setAttr(const QString &name, const QString &value)
{
    if(dom_stored.isNull())
        throw new WsException("节点已失效");

    dom_stored.setAttribute(name, value);
}

OutlinesItem::OutlinesItem(const FStruct::NHandle &refer):fstruct_node(refer)
{
    setText(refer.attr("title"));
}

const FStruct::NHandle OutlinesItem::getHandleRef() const
{
    return fstruct_node;
}

FStruct::NHandle::Type OutlinesItem::getType() const{
    return getHandleRef().nType();
}
