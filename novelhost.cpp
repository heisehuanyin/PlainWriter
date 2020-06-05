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
                auto target = chapter_node->getRefer();
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

QStandardItemModel *NovelHost::outlineTree() const{
    return outline_tree_model;
}


void NovelHost::insertVolume(int before, const QString &gName)
{
    FStruct::NHandle volume_new, target;
    try {
        target = desp_tree->volumeAt(before);
    } catch (WsException *) {
        target = FStruct::NHandle();
    }

    volume_new = desp_tree->insertVolume(target, gName, "");
    insert_volume(volume_new, desp_tree->volumeCount());
}

void NovelHost::insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName)
{
    if(!vmIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_tree_model->itemFromIndex(vmIndex);
    auto volume_tree_node = static_cast<OutlinesItem*>(node);
    auto volume_struct_node = volume_tree_node->getRefer();
    desp_tree->checkNValid(volume_struct_node, FStruct::NHandle::Type::VOLUME);

    int knode_count = desp_tree->keystoryCount(volume_tree_node->getRefer());
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
    auto struct_keystory_node = outline_keystory_node->getRefer();
    desp_tree->checkNValid(struct_keystory_node, FStruct::NHandle::Type::KEYSTORY);

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
    auto struct_keystory_node = outline_keystory_node->getRefer();
    desp_tree->checkNValid(struct_keystory_node, FStruct::NHandle::Type::KEYSTORY);

    desp_tree->appendForeshadow(struct_keystory_node, fName, desp, desp_next);
}

void NovelHost::appendShadowstart(const QModelIndex &chpIndex, const QString &keystory, const QString &foreshadow)
{
f
}

int NovelHost::appendShadowstop(QString &err, const QModelIndex &kIndex, const QString &vKey,
                                const QString &kKey, const QString &fKey)
{
    if(!kIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto outline_keystory_node = static_cast<OutlinesItem*>(node);
    auto refer_keystory_node = outline_keystory_node->getRefer();
    int code;
    desp_tree->checkNValid(refer_keystory_node, FStruct::NHandle::Type::KEYSTORY);
    int shadowstop_count;
    if((code = desp_tree->shadowstopCount(err, refer_keystory_node, shadowstop_count)))
        return code;
    FStruct::NHandle shadowstop_node;
    if((code = desp_tree->insertShadowstop(err, refer_keystory_node, shadowstop_count,
                                           vKey, kKey, fKey, shadowstop_node)))
        return code;
    return 0;
}



int NovelHost::removeOutlineNode(QString &err, const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    int code;
    auto item = outline_tree_model->itemFromIndex(outlineNode);
    auto outline_struct_node = static_cast<OutlinesItem*>(item);
    auto pnode = outline_struct_node->QStandardItem::parent();

    if(!pnode){
        outline_tree_model->removeRow(outline_struct_node->row());
        chapters_navigate_model->removeRow(outline_struct_node->row());
    }
    else {
        pnode->removeRow(outline_struct_node->row());

        if(outline_struct_node->getRefer().nType() == FStruct::NHandle::Type::KEYSTORY){
            FStruct::NHandle volume_node;
            if((code = desp_tree->parentHandle(err, outline_struct_node->getRefer(), volume_node)))
                return code;
            int chapter_count_will_be_remove;
            if((code = desp_tree->chapterCount(err, outline_struct_node->getRefer(), chapter_count_will_be_remove)))
                return code;

            int view_volume_index;
            if((code = desp_tree->handleIndex(err, volume_node, view_volume_index)))
                return code;
            auto view_volume_entry = chapters_navigate_model->item(view_volume_index);
            for (auto chapter_index=0; chapter_index<chapter_count_will_be_remove; ++chapter_index) {
                FStruct::NHandle chapter_will_be_remove;
                if((code = desp_tree->chapterAt(err, outline_struct_node->getRefer(), chapter_index, chapter_will_be_remove)))
                    return code;

                for (auto view_chapter_index=0; view_chapter_index<view_volume_entry->rowCount(); ++view_chapter_index) {
                    auto chapter_entry2 = static_cast<ChaptersItem*>(view_volume_entry->child(view_chapter_index));

                    if(chapter_entry2->getRefer() == chapter_will_be_remove){
                        view_volume_entry->removeRow(view_chapter_index);
                        break;
                    }
                }
            }
        }
    }

    if((code = desp_tree->removeNodeHandle(err, outline_struct_node->getRefer())))
        return code;
    return 0;
}


/*

int NovelHost::outlineNodeTitle(QString &err, const QModelIndex &nodeIndex, QString &title) const
{
    if(!nodeIndex.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(nodeIndex);
    auto outline_node = static_cast<OutlinesItem*>(item);
    int code;
    if((code = outline_node->getRefer().attr(err, "title", title)))
        return code;
    return 0;
}

int NovelHost::outlineNodeDescription(QString &err, const QModelIndex &nodeIndex, QString &desp) const
{
    if(!nodeIndex.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(nodeIndex);
    auto outline_node = static_cast<OutlinesItem*>(item);
    int code;
    if((code = outline_node->getRefer().attr(err, "desp", desp)))
        return code;
    return 0;
}

int NovelHost::outlineForshadowNextDescription(QString &err, const QModelIndex &nodeIndex, QString &desp) const
{
    if(!nodeIndex.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(nodeIndex);
    auto outline_node = static_cast<OutlinesItem*>(item);

    int code;
    if((code = desp_node->checkNodeValid(err, outline_node->getRefer(), FStruct::NodeHandle::Type::FORESHADOW)))
        return code;
    if((code = outline_node->getRefer().attr(err, "desp_next", desp)))
        return code;
    return 0;
}

int NovelHost::resetOutlineNodeTitle(QString &err, const QModelIndex &outlineNode, const QString &title)
{
    if(!outlineNode.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(outlineNode);
    auto outline_node = static_cast<OutlinesItem*>(item);

    int code;
    FStruct::NodeHandle one = outline_node->getRefer();
    one.setAttr("title", title);
    return 0;
}

int NovelHost::resetOutlineNodeDescription(QString &err, const QModelIndex &outlineNode, const QString &desp)
{
    if(!outlineNode.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(outlineNode);
    auto outline_node = static_cast<OutlinesItem*>(item);
    auto x = outline_node->getRefer();
    int code;
    if((code = x.setAttr("desp", desp)))
        return code;
    return 0;
}

int NovelHost::resetOutlineForshadowNextDescription(QString &err, const QModelIndex &nodeIndex, const QString &desp) const
{
    if(!nodeIndex.isValid()){
        err = "指定modelindex无效";
        return -1;
    }

    auto item = outline_tree_model->itemFromIndex(nodeIndex);
    auto outline_node = static_cast<OutlinesItem*>(item);

    int code;
    if((code = desp_node->checkNodeValid(err, outline_node->getRefer(), FStruct::NodeHandle::Type::FORESHADOW)))
        return code;
    auto x = outline_node->getRefer();
    if((code = x.setAttr("desp_next", desp)))
        return code;
    return 0;
}*/

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





void NovelHost::insertChapter(const QModelIndex &chpVmIndex, int before, const QString &chpName)
{
    if(!chpVmIndex.isValid())
        throw new WsException("输入volumeindex：chapters无效");

    auto item = chaptersNavigateTree()->itemFromIndex(chpVmIndex);
    auto chapters_volume = static_cast<ChaptersItem*>(item);
    auto struct_volumme = chapters_volume->getRefer();
    desp_tree->checkNValid(struct_volumme, FStruct::NHandle::Type::VOLUME);

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



int NovelHost::removeChaptersNode(QString &err, const QModelIndex &chaptersNode)
{
    int code;
    if(!chaptersNode.isValid()){
        err = "chaptersNodeIndex无效";
        return -1;
    }

    auto item = chapters_navigate_model->itemFromIndex(chaptersNode);
    if(!item->parent()){
        chapters_navigate_model->removeRow(item->row());
        outline_tree_model->removeRow(item->row());
    }

    if((code = desp_tree->removeNodeHandle(err, static_cast<ChaptersItem*>(item)->getRefer())))
        return code;

    return 0;
}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_model->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_model->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
    }
}

QStandardItemModel *NovelHost::searchResultPresent() const
{
    return find_results_model;
}

void NovelHost::searchText(const QString &text)
{
    QRegExp exp("("+text+").*");
    find_results_model->clear();
    find_results_model->setHorizontalHeaderLabels(QStringList() << "搜索文本" << "卷宗节点" << "章节节点");

    for (int vm_index=0; vm_index<chapters_navigate_model->rowCount(); ++vm_index) {
        auto volume_node = chapters_navigate_model->item(vm_index);

        for (int chp_index=0; chp_index<volume_node->rowCount(); ++chp_index) {
            auto chp_node = volume_node->child(chp_index);
            QString content, err;
            auto pos = -1;
            if(chapterTextContent(err, chp_node->index(), content))
                return;

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

                item->setData(chp_node->index(), Qt::UserRole+1);
                item->setData(pos, Qt::UserRole + 2);
                item->setData(len, Qt::UserRole + 3);
                row << item;

                auto path = static_cast<ChaptersItem*>(chp_node)->getRefer();
                QString temp,err;
                desp_tree->volumeTitle(err, path.first, temp);
                row << new QStandardItem(temp);

                desp_tree->chapterTitle(err, path.first, path.second, temp);
                row << new QStandardItem(temp);

                find_results_model->appendRow(row);
            }
        }
    }
}


QTextDocument *NovelHost::novelDescriptionPresent() const
{
    return novel_description_present;
}

QTextDocument *NovelHost::volumeDescriptionPresent() const
{
    return volume_description_present;
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

    desp_tree->checkNValid(refer_node->getRefer(), FStruct::NHandle::Type::CHAPTER);
    if(opening_documents.contains(refer_node)){
        auto pack = opening_documents.value(refer_node);
        auto doc = pack.first;
        return doc->toPlainText();
    }

    QString file_path = desp_tree->chapterCanonicalFilePath(refer_node->getRefer());
    QString fencoding = desp_tree->chapterTextEncoding(refer_node->getRefer());

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
    QRegExp exp("[，。！？【】“”—…《》：、\\s]");
    return newtext.replace(exp, "").size();
}

int NovelHost::openDocument(QString &err, const QModelIndex &_index)
{
    if(!_index.isValid()){
        err = "index非法";
        return -1;
    }

    auto index = _index;
    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = chapters_navigate_model->itemFromIndex(index);
    auto chapter_node = static_cast<ChaptersItem*>(item);
    auto bind = chapter_node->getRefer();
    // 确保指向正确章节节点
    if(bind.first < 0){
        err = "传入错误目标index，试图打开卷宗节点";
        return -1;
    }

    QString title;
    int code;
    if((code = desp_tree->chapterTitle(err, bind.first, bind.second, title)))
        return code;

    // 校验是否已经处于打开状态
    if(opening_documents.contains(chapter_node)){
        auto pak = opening_documents.value(chapter_node);
        emit documentActived(pak.first, title);
        return 0;
    }

    // 获取全部内容
    QString text_content;
    if((code = chapterTextContent(err, index, text_content)))
        return code;

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
    return 0;
}

int NovelHost::closeDocument(QString &err, QTextDocument *doc)
{
    int code;
    if((code = save(err)))
        return code;

    for (auto px : opening_documents) {
        if(px.first == doc){
            emit documentAboutToBeClosed(doc);

            auto key = opening_documents.key(px);
            delete px.second;
            delete px.first;
            opening_documents.remove(key);

            return 0;
        }
    }

    err = "目标文档未包含或未打开";
    return -1;
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


void NovelHost::chapters_navigate_title_midify(QStandardItem *item)
{
    if(item->column())
        return;

    auto xitem = static_cast<ChaptersItem*>(item);
    auto struct_node = xitem->getRefer();
    QString err;
    struct_node.setAttr("title", item->text());
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

const FStruct::NHandle ChaptersItem::getRefer() const
{
    return fstruct_node;
}

FStruct::NHandle::Type ChaptersItem::getType() const{
    return getRefer().nType();
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
    QDomElement elm = find_direct_subdom_at_index(struct_node, "volume", index);

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
        unique_key = QString("volume-%1").arg(gen.generate64());
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
    checkNValid(vmNode, NHandle::Type::VOLUME);

    auto list = vmNode.dom_stored.elementsByTagName("keystory");
    return list.size();
}

FStruct::NHandle FStruct::keystoryAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNValid(vmNode, NHandle::Type::VOLUME);

    QDomElement elm = find_direct_subdom_at_index(vmNode.dom_stored, "keystory", index);
    return NHandle(elm, NHandle::Type::KEYSTORY);
}

FStruct::NHandle FStruct::insertKeystory(FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkNValid(vmNode, NHandle::Type::VOLUME);

    int num = keystoryCount(vmNode);
    QList<QString> kkeys;
    for (int var = 0; var < num; ++var) {
        NHandle one = keystoryAt(vmNode, var);
        QString key = one.attr("key");
        kkeys << key;
    }
    QString unique_key="keystory-0";
    while (kkeys.contains(unique_key)) {
        unique_key = QString("keystory-%1").arg(gen.generate64());
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
    checkNValid(knode, NHandle::Type::KEYSTORY);

    auto list = knode.dom_stored.elementsByTagName("points").at(0).childNodes();
    return list.size();
}

FStruct::NHandle FStruct::pointAt(const FStruct::NHandle &knode, int index) const
{
    checkNValid(knode, NHandle::Type::KEYSTORY);


    auto points_elm = knode.dom_stored.firstChildElement("points");
    QDomElement elm = find_direct_subdom_at_index(points_elm, "simply", index);

    return NHandle(elm, NHandle::Type::POINT);
}

FStruct::NHandle FStruct::insertPoint(FStruct::NHandle &knode, int before, const QString &title,
                         const QString &description)
{
    checkNValid(knode, NHandle::Type::KEYSTORY);

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
    checkNValid(knode, NHandle::Type::KEYSTORY);

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    return foreshodows_node.elementsByTagName("foreshadow").size();
}

FStruct::NHandle FStruct::foreshadowAt(const FStruct::NHandle &knode, int index) const
{
    checkNValid(knode, NHandle::Type::KEYSTORY);

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm = find_direct_subdom_at_index(foreshadows_node, "foreshadow", index);

    return NHandle(elm, NHandle::Type::FORESHADOW);
}

FStruct::NHandle FStruct::appendForeshadow(FStruct::NHandle &knode, const QString &title,
                              const QString &desp, const QString &desp_next)
{
    checkNValid(knode, NHandle::Type::KEYSTORY);

    int num=foreshadowCount(knode);
    QList<QString> fkeys;
    for (auto index = 0; index<num; ++index) {
        NHandle one=foreshadowAt(knode, index);
        QString key = one.attr("key");
        fkeys << key;
    }
    QString unique_key="foreshadow-0";
    while (fkeys.contains(unique_key)) {
        unique_key = QString("foreshadow-%1").arg(gen.generate64());
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

int FStruct::chapterCount(const FStruct::NHandle &vmNode) const
{
    checkNValid(vmNode, NHandle::Type::VOLUME);

    return vmNode.dom_stored.elementsByTagName("chapter").size();
}

FStruct::NHandle FStruct::chapterAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNValid(vmNode, NHandle::Type::VOLUME);

    QDomElement elm = find_direct_subdom_at_index(vmNode.dom_stored, "chapter", index);
    return NHandle(elm, NHandle::Type::CHAPTER);
}

FStruct::NHandle FStruct::insertChapter(FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkNValid(vmNode, NHandle::Type::VOLUME);

    QList<QString> ckeys;
    int num = chapterCount(vmNode);
    for (auto var=0; var<num; ++var) {
        NHandle one = chapterAt(vmNode, var);
        QString key = one.attr("key");
        ckeys << key;
    }
    QString unique_key="chapter-0";
    while (ckeys.contains(unique_key)) {
        unique_key = QString("chapter-%1").arg(gen.generate64());
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

QString FStruct::chapterCanonicalFilePath(const FStruct::NHandle &chapter) const
{
    checkNValid(chapter, FStruct::NHandle::Type::CHAPTER);

    QString relative_path;
     relative_path = chapter.attr("relative");

    return QDir(QFileInfo(this->filepath_stored).canonicalPath()).filePath(relative_path);
}

QString FStruct::chapterTextEncoding(const FStruct::NHandle &chapter) const
{
    checkNValid(chapter, FStruct::NHandle::Type::CHAPTER);

     return chapter.attr("encoding");
}

int FStruct::shadowstartCount(const FStruct::NHandle &chpNode) const
{
    checkNValid(chpNode, FStruct::NHandle::Type::CHAPTER);
    return chpNode.dom_stored.elementsByTagName("shadow-start").size();
}

FStruct::NHandle FStruct::shadowstartAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNValid(chpNode, FStruct::NHandle::Type::CHAPTER);

    QDomElement elm = find_direct_subdom_at_index(chpNode.dom_stored, "shadow-start", index);
    return NHandle(elm, NHandle::Type::SHADOWSTART);
}

FStruct::NHandle FStruct::appendShadowstart(FStruct::NHandle &chpNode, const QString &keystory, const QString &foreshadow)
{
    checkNValid(chpNode, FStruct::NHandle::Type::CHAPTER);

    auto elm = struct_dom_store.createElement("shadow-start");
    NHandle one(elm, NHandle::Type::SHADOWSTART);
    one.setAttr("target", keystory+"@"+foreshadow);

    chpNode.dom_stored.appendChild(elm);
    return one;
}

int FStruct::shadowstopCount(const FStruct::NHandle &chpNode) const
{
    checkNValid(chpNode, NHandle::Type::CHAPTER);
    return chpNode.dom_stored.elementsByTagName("shadow-stop").size();
}

FStruct::NHandle FStruct::shadowstopAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNValid(chpNode, NHandle::Type::CHAPTER);

    QDomElement elm = find_direct_subdom_at_index(chpNode.dom_stored, "shadow-stop", index);
    return NHandle(elm, NHandle::Type::SHADOWSTOP);
}

FStruct::NHandle FStruct::appendShadowstop(FStruct::NHandle &chpNode, const QString &volume,
                                           const QString &keystory, const QString &foreshadow)
{
    checkNValid(chpNode, NHandle::Type::CHAPTER);

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

void FStruct::removeNodeHandle(const FStruct::NHandle &node)
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
            removeNodeHandle(_node);
        }
    }

    auto parent = node.dom_stored.parentNode();
    if(parent.isNull())
        throw new WsException("父节点非法");

    parent.removeChild(node.dom_stored);
}

void FStruct::checkNValid(const FStruct::NHandle &node, FStruct::NHandle::Type type) const
{
    if(node.nType() != type)
        throw new WsException("传入节点类型错误");

    if(node.isValid())
        throw new WsException("传入节点已失效");
}

QDomElement FStruct::find_direct_subdom_at_index(const QDomElement &pnode, const QString &tagName, int index) const
{
    auto first = pnode.firstChildElement(tagName);
    while (!first.isNull()) {
        if(!index){
            return first;
        }

        index--;
        first = first.nextSiblingElement(tagName);
    }

    throw new WsException("无效index指定");
}

FStruct::NHandle::NHandle()
    :type_stored(Type::VOLUME){}

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

const FStruct::NHandle OutlinesItem::getRefer() const
{
    return fstruct_node;
}

FStruct::NHandle::Type OutlinesItem::getType() const{
    return getRefer().nType();
}
