#include "common.h"
#include "dataaccess.h"
#include "novelhost.h"
#include "_x_deprecated.h"

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
using TnType = DataAccess::TreeNode::Type;

NovelHost::NovelHost(ConfigHost &config)
    :config_host(config),
      desp_ins(nullptr),
      outline_navigate_treemodel(new QStandardItemModel(this)),
      novel_outlines_present(new QTextDocument(this)),
      volume_outlines_present(new QTextDocument(this)),
      foreshadows_under_volume_present(new QStandardItemModel(this)),
      foreshadows_until_volume_remain_present(new QStandardItemModel(this)),
      foreshadows_until_chapter_remain_present(new QStandardItemModel(this)),
      find_results_model(new QStandardItemModel(this)),
      chapters_navigate_treemodel(new QStandardItemModel(this)),
      chapter_outlines_present(new QTextDocument(this))
{
    new OutlinesRender(volume_outlines_present, config);

    connect(outline_navigate_treemodel, &QStandardItemModel::itemChanged,
            this,   &NovelHost::outlines_node_title_changed);
    connect(chapters_navigate_treemodel,&QStandardItemModel::itemChanged,
            this,   &NovelHost::chapters_node_title_changed);

    connect(foreshadows_under_volume_present,   &QStandardItemModel::itemChanged,
            this,   &NovelHost::listen_foreshadows_volume_changed);
    connect(foreshadows_until_volume_remain_present,    &QStandardItemModel::itemChanged,
            this,       &NovelHost::listen_foreshadows_until_volume_changed);
    connect(foreshadows_until_chapter_remain_present,   &QStandardItemModel::itemChanged,
            this,       &NovelHost::listen_foreshadows_until_chapter_changed);
}

NovelHost::~NovelHost(){}

void NovelHost::convert20_21(const QString &validPath)
{
    DataAccess dbtool;

    try {
        dbtool.createEmptyDB(validPath);
        auto root = dbtool.novelRoot();
        root.descriptionReset(desp_tree->novelDescription());
        root.titleReset(desp_tree->novelTitle());

        auto vnum = desp_tree->volumeCount();
        for (int vindex = 0; vindex < vnum; ++vindex) {
            auto vmnode = desp_tree->volumeAt(vindex);
            auto dbvnode = dbtool.insertChildBefore(root, DataAccess::TreeNode::Type::VOLUME,
                                     dbtool.childNodeCount(root, DataAccess::TreeNode::Type::VOLUME),
                                     vmnode.attr("title"),
                                     vmnode.attr("desp"));

            // chapters
            auto chpnum = desp_tree->chapterCount(vmnode);
            for (int chpindex = 0; chpindex < chpnum; ++chpindex) {
                auto chpnode = desp_tree->chapterAt(vmnode, chpindex);
                auto dbchpnode = dbtool.insertChildBefore(dbvnode, DataAccess::TreeNode::Type::CHAPTER,
                                                          chpindex, chpnode.attr("title"), chpnode.attr("desp"));
                auto fpath = desp_tree->chapterCanonicalFilePath(chpnode);
                QFile file(fpath);
                if(!file.open(QIODevice::Text|QIODevice::ReadOnly))
                    throw new WsException("指定文件无法打开");
                QTextStream tin(&file);
                tin.setCodec(desp_tree->chapterTextEncoding(chpnode).toLocal8Bit());
                dbtool.resetChapterText(dbchpnode, tin.readAll());
            }

            // storyblock
            auto keystorynum = desp_tree->keystoryCount(vmnode);
            for (int ksindex = 0; ksindex < keystorynum; ++ksindex) {
                auto kstorynode = desp_tree->keystoryAt(vmnode, ksindex);
                auto dbkstorynode = dbtool.insertChildBefore(dbvnode, DataAccess::TreeNode::Type::STORYBLOCK,
                                                         ksindex, kstorynode.attr("title"), kstorynode.attr("desp"));

                // points
                auto pointnum = desp_tree->pointCount(kstorynode);
                for (int pindex = 0; pindex < pointnum; ++pindex) {
                    auto pointnode = desp_tree->pointAt(kstorynode, pindex);
                    dbtool.insertChildBefore(dbkstorynode, DataAccess::TreeNode::Type::KEYPOINT,
                                             pindex, pointnode.attr("title"), pointnode.attr("desp"));
                }

                // foreshadows
                auto foreshadownum = desp_tree->foreshadowCount(kstorynode);
                for (int findex = 0; findex < foreshadownum; ++findex) {
                    auto foreshadownode = desp_tree->foreshadowAt(kstorynode, findex);
                    auto dbfsnode = dbtool.insertChildBefore(dbvnode, DataAccess::TreeNode::Type::DESPLINE,
                                             dbtool.childNodeCount(dbvnode, DataAccess::TreeNode::Type::DESPLINE),
                                             foreshadownode.attr("title"), "无整体描述");

                    auto headnode = dbtool.insertAttachpointBefore(dbfsnode, 0, false, "阶段0", foreshadownode.attr("desp"));
                    headnode.storyAttachedReset(dbkstorynode);
                    auto tailnode = dbtool.insertAttachpointBefore(dbfsnode, 1, false, "阶段1", foreshadownode.attr("desp_next"));

                    auto chpnum = desp_tree->chapterCount(vmnode);
                    auto fskeyspath = desp_tree->foreshadowKeysPath(foreshadownode);
                    for (int chpindex = 0; chpindex < chpnum; ++chpindex){
                        auto chpnode = desp_tree->chapterAt(vmnode, chpindex);
                        auto start = desp_tree->findShadowstart(chpnode, fskeyspath);
                        if(start.isValid()){
                            auto dbchpnode = dbtool.childNodeAt(dbvnode, DataAccess::TreeNode::Type::CHAPTER, chpindex);
                            headnode.chapterAttachedReset(dbchpnode);
                            break;
                        }
                    }
                    for (int chpindex = 0; chpindex < chpnum; ++chpindex){
                        auto chpnode = desp_tree->chapterAt(vmnode, chpindex);
                        auto stop = desp_tree->findShadowstop(chpnode, fskeyspath);
                        if(stop.isValid()){
                            auto dbchpnode = dbtool.childNodeAt(dbvnode, DataAccess::TreeNode::Type::CHAPTER, chpindex);
                            tailnode.chapterAttachedReset(dbchpnode);
                            break;
                        }
                    }
                }
            }
        }
    } catch (WsException *e) {
        qDebug() << e->reason();
    }

}

void NovelHost::loadDescription(DataAccess *desp)
{
    // save description structure
    this->desp_ins = desp;
    chapters_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "章卷名称" << "严格字数统计");
    outline_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "故事结构");

    auto novel_node = desp->novelRoot();

    QTextBlockFormat blockformat;
    QTextCharFormat charformat;
    config_host.textFormat(blockformat, charformat);
    QTextCursor cursor(novel_outlines_present);
    cursor.setBlockFormat(blockformat);
    cursor.setBlockCharFormat(charformat);
    cursor.insertText(novel_node.description());
    novel_outlines_present->setModified(false);
    novel_outlines_present->clearUndoRedoStacks();
    connect(novel_outlines_present,  &QTextDocument::contentsChanged,    this,   &NovelHost::listen_novel_description_change);

    using TnType = DataAccess::TreeNode::Type;
    auto volume_num = desp->childNodeCount(novel_node, TnType::VOLUME);
    for (int volume_index = 0; volume_index < volume_num; ++volume_index) {
        DataAccess::TreeNode volume_node = desp->childNodeAt(novel_node, TnType::VOLUME, volume_index);

        // 在chapters-tree和outline-tree上插入卷节点
        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int storyblock_count = desp->childNodeCount(volume_node, TnType::STORYBLOCK);
        for (int storyblock_index = 0; storyblock_index < storyblock_count; ++storyblock_index) {
            DataAccess::TreeNode storyblock_node = desp->childNodeAt(volume_node, TnType::STORYBLOCK, storyblock_index);

            // outline-tree上插入故事节点
            auto ol_keystory_item = new OutlinesItem(storyblock_node);
            outline_volume_node->appendRow(ol_keystory_item);

            // outline-tree上插入point节点
            int points_count = desp->childNodeCount(storyblock_node, TnType::KEYPOINT);
            for (int points_index = 0; points_index < points_count; ++points_index) {
                DataAccess::TreeNode point_node = desp->childNodeAt(storyblock_node, TnType::KEYPOINT, points_index);

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }
        }

        // chapters上插入chapter节点
        int chapter_count = desp->childNodeCount(volume_node, TnType::CHAPTER);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto chapter_node = desp->childNodeAt(volume_node, TnType::CHAPTER, chapter_index);

            QList<QStandardItem*> node_navigate_row;
            node_navigate_row << new ChaptersItem(*this, chapter_node);
            node_navigate_row << new QStandardItem("-");
            node_navigate_volume_node->appendRow(node_navigate_row);
        }
    }

    // 加载所有内容
    for(int index=0; index<chapters_navigate_treemodel->rowCount(); ++index) {
        auto column = chapters_navigate_treemodel->item(index);
        for (int chp_var = 0; chp_var < column->rowCount(); ++chp_var) {
            auto chp_item = column->child(chp_var);
            _load_chapter_text_content(chp_item);
        }
    }
}

void NovelHost::save(const QString &filePath)
{
    TODO rebase
}

QString NovelHost::novelTitle() const
{
    return desp_ins->novelRoot().title();
}

void NovelHost::resetNovelTitle(const QString &title)
{
    desp_ins->novelRoot().titleReset(title);
}

int NovelHost::treeNodeLevel(const QModelIndex &node) const
{
    auto index = node;
    QList<QModelIndex> level_stack;
    while (index.isValid()) {
        level_stack << index;
        index = index.parent();
    }

    return level_stack.size();
}

QStandardItemModel *NovelHost::outlineNavigateTree() const
{
    return outline_navigate_treemodel;
}

QTextDocument *NovelHost::novelOutlinesPresent() const
{
    return novel_outlines_present;
}

QTextDocument *NovelHost::volumeOutlinesPresent() const
{
    return volume_outlines_present;
}

QStandardItemModel *NovelHost::foreshadowsUnderVolume() const
{
    return foreshadows_under_volume_present;
}

QStandardItemModel *NovelHost::foreshadowsUntilVolumeRemain() const
{
    return foreshadows_until_volume_remain_present;
}

QStandardItemModel *NovelHost::foreshadowsUntilChapterRemain() const
{
    return foreshadows_until_chapter_remain_present;
}


void NovelHost::insertVolume(int before, const QString &gName)
{
    using TnType = DataAccess::TreeNode::Type;

    auto root = desp_ins->novelRoot();
    auto count = desp_ins->childNodeCount(root, TnType::VOLUME);
    if(before >= count){
        desp_ins->insertChildBefore(root, TnType::VOLUME, count, gName, "无描述");
    }
    else {
        desp_ins->insertChildBefore(root, TnType::VOLUME, before, gName, "无描述");
    }
}

void NovelHost::insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName)
{
    if(!vmIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(vmIndex);
    auto outline_volume_node = static_cast<OutlinesItem*>(node);
    auto root = desp_ins->novelRoot();
    auto volume_struct_node = desp_ins->childNodeAt(root, TnType::VOLUME, node->row());

    int sb_node_count = desp_ins->childNodeCount(volume_struct_node, TnType::STORYBLOCK);
    if(before >= sb_node_count){
        auto keystory_node = desp_ins->insertChildBefore(volume_struct_node, TnType::STORYBLOCK, sb_node_count, kName, "无描述");
        outline_volume_node->appendRow(new OutlinesItem(keystory_node));
    }
    else{
        auto keystory_node = desp_ins->insertChildBefore(volume_struct_node, TnType::STORYBLOCK, before, kName, "无描述");
        outline_volume_node->insertRow(before, new OutlinesItem(keystory_node));
    }
}

void NovelHost::insertPoint(const QModelIndex &kIndex, int before, const QString &pName)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(kIndex);          // keystory-index
    auto struct_keystory_node = _locate_outline_handle_via(node);

    int points_count = desp_ins->childNodeCount(struct_keystory_node, TnType::KEYPOINT);
    if(before >= points_count){
        auto point_node = desp_ins->insertChildBefore(struct_keystory_node, TnType::KEYPOINT, points_count, pName, "无描述");
        node->appendRow(new OutlinesItem(point_node));
    }
    else{
        auto point_node = desp_ins->insertChildBefore(struct_keystory_node, TnType::KEYPOINT, before, pName, "无描述");
        node->insertRow(before, new OutlinesItem(point_node));
    }
}

void NovelHost::appendForeshadow(const QModelIndex &kIndex, const QString &fName,
                                 const QString &desp, const QString &desp_next)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto storyblock_node = outline_navigate_treemodel->itemFromIndex(kIndex);          // storyblock
    auto volume_node = storyblock_node->parent();                                      // volume

    auto root = desp_ins->novelRoot();
    auto struct_volume_node = desp_ins->childNodeAt(root, TnType::VOLUME, volume_node->row());
    auto struct_storyblock_node = desp_ins->childNodeAt(struct_volume_node, TnType::STORYBLOCK, storyblock_node->row());

    auto despline_count = desp_ins->childNodeCount(struct_volume_node, TnType::DESPLINE);
    auto despline = desp_ins->insertChildBefore(struct_volume_node, TnType::DESPLINE, despline_count, fName, "无整体描述");

    auto stop0 = desp_ins->insertAttachpointBefore(despline, 0, false, "描述0", desp);
    stop0.storyAttachedReset(struct_storyblock_node);
    desp_ins->insertAttachpointBefore(despline, 1, false, "描述1", desp_next);
}

void NovelHost::removeOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("指定modelindex无效");

    auto item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto pnode = item->parent();

    int row = item->row();
    if(!pnode){
        auto root = desp_ins->novelRoot();

        auto struct_node = desp_ins->childNodeAt(root, TnType::VOLUME, row);
        desp_ins->removeNode(struct_node);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);
    }
    else {
        auto handle = _locate_outline_handle_via(item);
        desp_ins->removeNode(handle);

        pnode->removeRow(row);
    }
}

void NovelHost::setCurrentOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("传入的outlinemodelindex无效");

    auto current = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto struct_one = _locate_outline_handle_via(current);

    // 设置当前卷节点，填充卷细纲内容
    set_current_volume_outlines(struct_one);
    emit currentVolumeActived();

    // 统计本卷宗下所有构建伏笔及其状态  名称，吸附状态，前描述，后描述，吸附章节、源剧情
    sum_foreshadows_under_volume(current_volume_node);

    // 统计至此卷宗前未闭合伏笔及本卷闭合状态  名称、闭合状态、前描述、后描述、闭合章节、源剧情、源卷宗
    sum_foreshadows_until_volume_remains(current_volume_node);
}

void NovelHost::allKeystoriesUnderCurrentVolume(QList<QPair<QString,int>> &keystories) const
{
    auto keystory_num = desp_ins->childNodeCount(current_volume_node, TnType::STORYBLOCK);
    for(auto kindex=0; kindex<keystory_num; kindex++){
        auto struct_keystory = desp_ins->childNodeAt(current_volume_node,TnType::STORYBLOCK, kindex);
        keystories << qMakePair(struct_keystory.title(), struct_keystory.uniqueID());
    }
}

QList<QPair<QString, QModelIndex>> NovelHost::keystorySumViaChapters(const QModelIndex &chaptersNode) const
{
    if(!chaptersNode.isValid())
        return QList<QPair<QString, QModelIndex>>();

    QList<QPair<QString, QModelIndex>> hash;
    auto item = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    auto volume = item;
    if(treeNodeLevel(chaptersNode) == 2)
        volume = item->parent();

    auto outlines_volume_item = outline_navigate_treemodel->item(volume->row());
    for (int var = 0; var < outlines_volume_item->rowCount(); ++var) {
        auto one_item = outlines_volume_item->child(var);
        hash << qMakePair(one_item->text(), one_item->index());
    }
    return hash;
}

QList<QPair<QString, QModelIndex> > NovelHost::keystorySumViaOutlines(const QModelIndex &outlinesNode) const
{
    if(!outlinesNode.isValid())
        return QList<QPair<QString,QModelIndex>>();

    auto selected_item = outline_navigate_treemodel->itemFromIndex(outlinesNode);
    auto struct_node = _locate_outline_handle_via(selected_item);
    QList<QPair<QString,QModelIndex>> result;

    if(struct_node.type() == TnType::VOLUME){
        for (int var = 0; var < selected_item->rowCount(); ++var) {
            auto one = selected_item->child(var);
            result<< qMakePair(one->text(), one->index());
        }
        return result;
    }

    QStandardItem *keystory_item = selected_item;
    if (struct_node.type() == TnType::KEYPOINT)
        keystory_item = selected_item->parent();

    result << qMakePair(keystory_item->text(), keystory_item->index());
    return result;
}

void NovelHost::checkChaptersRemoveEffect(const QModelIndex &chpsIndex, QList<QString> &msgList) const
{
    if(!chpsIndex.isValid())
        return;

    DataAccess::TreeNode struct_node;
    switch (treeNodeLevel(chpsIndex)) {
        case 1:
            struct_node = desp_ins->childNodeAt(desp_ins->novelRoot(), TnType::VOLUME, chpsIndex.row());
            break;
        case 2:
            struct_node = desp_ins->childNodeAt(desp_ins->novelRoot(), TnType::VOLUME, chpsIndex.parent().row());
            struct_node = desp_ins->childNodeAt(struct_node, TnType::CHAPTER, chpsIndex.row());
            break;
    }

    _check_remove_effect(struct_node, msgList);
}

void NovelHost::checkForeshadowRemoveEffect(int fsid, QList<QString> &msgList) const
{
    auto struct_node = desp_ins->getTreenodeViaID(fsid);
    if(struct_node.type() != TnType::DESPLINE)
        throw new WsException("传入的ID不属于伏笔[故事线]");

    _check_remove_effect(struct_node, msgList);
}


void NovelHost::checkOutlinesRemoveEffect(const QModelIndex &outlinesIndex, QList<QString> &msgList) const
{
    if(!outlinesIndex.isValid())
        return;

    auto item = outline_navigate_treemodel->itemFromIndex(outlinesIndex);
    auto struct_node = _locate_outline_handle_via(item);
    _check_remove_effect(struct_node, msgList);
}

DataAccess::TreeNode NovelHost:: _locate_outline_handle_via(QStandardItem *outline_item) const
{
    QList<QStandardItem*> stack;
    while (outline_item) {
        stack.insert(0, outline_item);
        outline_item = outline_item->parent();
    }

    auto root = desp_ins->novelRoot();
    auto volume_node = desp_ins->childNodeAt(root, TnType::VOLUME, stack.at(0)->row());
    if(stack.size() == 1){
        return volume_node;
    }

    auto keystory_node = desp_ins->childNodeAt(volume_node, TnType::STORYBLOCK, stack.at(1)->row());
    if(stack.size() == 2){
        return keystory_node;
    }

    auto point_node = desp_ins->childNodeAt(keystory_node, TnType::KEYPOINT, stack.at(2)->row());
    return point_node;
}

void NovelHost::listen_volume_outlines_description_change(int pos, int removed, int added)
{
    // 输入法更新期间，数据无用
    if(removed == added)
        return;

    // 查询内容修改
    QTextCursor cursor(volume_outlines_present);
    cursor.setPosition(pos);

    auto current_block = cursor.block();
    auto title_block = current_block;
    while (title_block.isValid()) {
        if(title_block.userData())
            break;
        title_block = title_block.previous();
    }

    if(current_block == title_block){
        auto data = static_cast<WsBlockData*>(title_block.userData());
        auto index = data->outlineTarget();
        auto title_item = outline_navigate_treemodel->itemFromIndex(index);
        if(title_block.text() == ""){
            emit errorPopup("编辑操作", "标题为空，继续删除将破坏文档结构");
        }
        title_item->setText(title_block.text());
    }
    else {
        QString description = "";
        auto block = title_block.next();
        while (block.isValid()) {
            if(block.userData())
                break;

            auto line = block.text();
            if(line.size())
                description += line + "\n";

            block = block.next();
        }

        auto index = static_cast<WsBlockData*>(title_block.userData())->outlineTarget();
        auto title_item = outline_navigate_treemodel->itemFromIndex(index);
        auto struct_node = _locate_outline_handle_via(title_item);
        struct_node.descriptionReset(description);
    }
}

bool NovelHost::check_volume_structure_diff(const OutlinesItem *base_node, QTextBlock &blk) const {
    auto target_index = base_node->index();
    while (blk.isValid()) {
        if(blk.userData())
            break;
        blk = blk.next();
    }
    if(!blk.isValid())
        return false;

    auto user_data = static_cast<WsBlockData*>(blk.userData());
    auto title_index = user_data->outlineTarget();
    if(target_index != title_index)
        return true;

    blk = blk.next();
    for (int var = 0; var < base_node->rowCount(); ++var) {
        auto item = base_node->child(var);
        if(check_volume_structure_diff(static_cast<OutlinesItem*>(item), blk))
            return true;
    }

    return false;
}

void NovelHost::listen_volume_outlines_structure_changed()
{
    int volume_index = desp_ins->nodeIndex(current_volume_node);
    auto volume_item = outline_navigate_treemodel->item(volume_index);
    auto blk = volume_outlines_present->firstBlock();
    auto outline_volume_item = static_cast<OutlinesItem*>(volume_item);

    // 循环递归校验文档结构
    if(check_volume_structure_diff(outline_volume_item, blk))
        emit errorPopup("文档编辑错误", "操作导致文档结构被破坏，请从故事树重新开始");
}

void NovelHost::listen_chapter_outlines_description_change()
{
    auto content = chapter_outlines_present->toPlainText();
    current_chapter_node.descriptionReset(content);
}

void NovelHost::insert_description_at_volume_outlines_doc(QTextCursor cursor, OutlinesItem *outline_node)
{
    auto struct_node = _locate_outline_handle_via(outline_node);

    QTextBlockFormat title_block_format;
    QTextCharFormat title_char_format;
    WsBlockData *data = nullptr;

    switch (struct_node.type()) {
        case TnType::VOLUME:
            config_host.volumeTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), TnType::VOLUME);
            break;
        case TnType::STORYBLOCK:
            config_host.keystoryTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), TnType::STORYBLOCK);
            break;
        case TnType::KEYPOINT:
            config_host.pointTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), TnType::KEYPOINT);
            break;
        default:
            break;
    }
    cursor.setBlockFormat(title_block_format);
    cursor.setBlockCharFormat(title_char_format);
    cursor.insertText(struct_node.title());
    cursor.block().setUserData(data);

    cursor.insertBlock();
    QTextBlockFormat text_block_format;
    QTextCharFormat text_char_format;
    config_host.textFormat(text_block_format, text_char_format);
    cursor.setBlockFormat(text_block_format);
    cursor.setBlockCharFormat(text_char_format);
    cursor.insertText(struct_node.description());
    cursor.insertBlock();

    for (int var=0; var < outline_node->rowCount(); ++var) {
        auto child = outline_node->child(var);
        insert_description_at_volume_outlines_doc(cursor, static_cast<OutlinesItem*>(child));
    }
}

void NovelHost::sum_foreshadows_under_volume(const DataAccess::TreeNode &volume_node)
{
    desp_tree->checkHandleValid(volume_node, _X_FStruct::NHandle::Type::VOLUME);
    foreshadows_under_volume_present->clear();
    foreshadows_under_volume_present->setHorizontalHeaderLabels(
                QStringList() << "伏笔名称" << "吸附？" << "描述1" << "描述2" << "吸附章节" << "剧情起点");

    QList<_X_FStruct::NHandle> foreshadows_sum;
    QList<QPair<int,int>> indexes;
    // 获取所有伏笔节点
    auto keystory_count = desp_tree->keystoryCount(volume_node);
    for (int keystory_index = 0; keystory_index < keystory_count; ++keystory_index) {
        auto keystory_one = desp_tree->keystoryAt(volume_node, keystory_index);

        int foreshadow_count = desp_tree->foreshadowCount(keystory_one);
        for (int foreshadow_index = 0; foreshadow_index < foreshadow_count; ++foreshadow_index) {
            auto foreshadow_one = desp_tree->foreshadowAt(keystory_one, foreshadow_index);
            foreshadows_sum << foreshadow_one;
            indexes << qMakePair(keystory_index, foreshadow_index);
        }
    }

    // 填充伏笔表格模型数据
    for(int var=0; var<foreshadows_sum.size(); ++var){
        auto foreshadow_one = foreshadows_sum.at(var);
        QList<QStandardItem*> row;
        auto node = new QStandardItem(foreshadow_one.attr( "title"));
        node->setData(indexes.at(var).first, Qt::UserRole+1);   // keystory-index
        node->setData(indexes.at(var).second, Qt::UserRole+2);  // foreshadow-index
        row << node;

        node = new QStandardItem("☁️悬空");
        node->setEditable(false);
        row << node;

        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));

        node = new QStandardItem("无");
        node->setEditable(false);
        row << node;

        auto keystory = desp_tree->parentHandle(foreshadow_one);
        node = new QStandardItem(keystory.attr( "title"));
        node->setData(current_volume_node.attr("key") + "@" + keystory.attr("key"));
        row << node;

        foreshadows_under_volume_present->appendRow(row);
    }

    // 汇聚伏笔吸附信息
    QList<_X_FStruct::NHandle> shadowstart_list;
    auto chapter_count = desp_tree->chapterCount(volume_node);
    for (int var = 0; var < chapter_count; ++var) {
        auto chapter_one = desp_tree->chapterAt(volume_node, var);

        auto shadowstart_count = desp_tree->shadowstartCount(chapter_one);
        for (int start_index = 0; start_index < shadowstart_count; ++start_index) {
            auto shadowstart_one = desp_tree->shadowstartAt(chapter_one, start_index);
            shadowstart_list << shadowstart_one;
        }
    }

    // 修改相关数据
    for (auto var=0; var<foreshadows_sum.size(); ++var) {
        auto foreshadow_one = foreshadows_sum.at(var);
        auto foreshadow_path = desp_tree->foreshadowKeysPath(foreshadow_one);

        for (int var2 = 0; var2 < shadowstart_list.size(); ++var2) {
            auto start_one = shadowstart_list.at(var2);
            auto start_target_path = start_one.attr( "target");
            if(foreshadow_path == start_target_path){
                foreshadows_under_volume_present->item(var, 1)->setText("📎吸附");

                auto chapter_one = desp_tree->parentHandle(start_one);
                foreshadows_under_volume_present->item(var, 4)->setText(chapter_one.attr( "title"));
            }
        }
    }
}

void NovelHost::listen_foreshadows_volume_changed(QStandardItem *item)
{
    auto item_important = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto keystory_index = item_important->data(Qt::UserRole+1).toInt();
    auto foreshadow_index = item_important->data(Qt::UserRole+2).toInt();
    auto struct_keystory = desp_tree->keystoryAt(current_volume_node, keystory_index);
    auto struct_foreshadow = desp_tree->foreshadowAt(struct_keystory, foreshadow_index);

    switch (item->column()) {
        case 1:
        case 4:
            break;
        case 0:
            desp_tree->setAttr(struct_foreshadow, "title", item->text());
            break;
        case 2:
            desp_tree->setAttr(struct_foreshadow, "desp", item->text());
            break;
        case 3:
            desp_tree->setAttr(struct_foreshadow, "desp_next", item->text());
            break;
        case 5:{
                auto path = item->data().toString();
                auto keystory_key = path.split("@").at(1);
                _X_FStruct::NHandle newTkeystory;

                auto keystory_count = desp_tree->keystoryCount(current_volume_node);
                for (int index = 0; index < keystory_count; ++index) {
                    auto item = desp_tree->keystoryAt(current_volume_node, index);
                    if(item.attr("key") == keystory_key)
                        newTkeystory = item;
                }
                if(!newTkeystory.isValid())
                    throw new WsException("找不到指定剧情节点");

                item->setText(newTkeystory.attr("title"));
                auto new_foreshadow = desp_tree->appendForeshadow(newTkeystory,
                                                                  struct_foreshadow.attr("title"),
                                                                  struct_foreshadow.attr("desp"),
                                                                  struct_foreshadow.attr("desp_next"));

                auto first_chapter = desp_tree->firstChapterOfFStruct();
                while (first_chapter.isValid()) {
                    auto start = desp_tree->findShadowstart(first_chapter,
                                                            desp_tree->foreshadowKeysPath(struct_foreshadow));
                    if(start.isValid()){
                        desp_tree->setAttr(start, "target", desp_tree->foreshadowKeysPath(new_foreshadow));
                        qDebug() << "修改一个吸附";
                    }

                    auto stop = desp_tree->findShadowstop(first_chapter,
                                                          desp_tree->foreshadowKeysPath(struct_foreshadow));
                    if(stop.isValid()){
                        desp_tree->setAttr(stop, "target", desp_tree->foreshadowKeysPath(new_foreshadow));
                        qDebug() << "修改一个终结";
                    }

                    first_chapter = desp_tree->nextChapterOfFStruct(first_chapter);
                }

                desp_tree->removeHandle(struct_foreshadow);
            }
            break;
    }
}

void NovelHost::sum_foreshadows_until_volume_remains(const DataAccess::TreeNode &volume_node)
{
    desp_tree->checkHandleValid(volume_node, _X_FStruct::NHandle::Type::VOLUME);
    foreshadows_until_volume_remain_present->clear();
    foreshadows_until_volume_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"闭合章节"<<"剧情源"<<"卷宗名");

    QList<_X_FStruct::NHandle> shadowstart_list;
    int volume_index = desp_tree->handleIndex(volume_node);
    // 包含本卷所有伏笔埋设信息统计
    for (int volume_index_tmp = 0; volume_index_tmp <= volume_index; ++volume_index_tmp) {
        auto volume_one = desp_tree->volumeAt(volume_index_tmp);

        auto chapter_count = desp_tree->chapterCount(volume_one);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto chapter_one = desp_tree->chapterAt(volume_one, chapter_index);

            auto start_count = desp_tree->shadowstartCount(chapter_one);
            for (int start_index = 0; start_index < start_count; ++start_index) {
                shadowstart_list << desp_tree->shadowstartAt(chapter_one, start_index);
            }
        }
    }

    QList<_X_FStruct::NHandle> shadowstop_list;
    // 不包含本卷，所有伏笔承接信息统计
    for (int index_tmp = 0; index_tmp < volume_index; ++index_tmp) {
        auto struct_volume = desp_tree->volumeAt(index_tmp);

        auto chapter_count = desp_tree->chapterCount(struct_volume);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto struct_chapter = desp_tree->chapterAt(struct_volume, chapter_index);

            auto stop_count = desp_tree->shadowstopCount(struct_chapter);
            for (int stop_index = 0; stop_index < stop_count; ++stop_index) {
                shadowstop_list << desp_tree->shadowstopAt(struct_chapter, stop_index);
            }
        }
    }

    // 过滤已关闭伏笔
    for (int start_index = 0; start_index < shadowstart_list.size(); ++start_index) {
        auto start_one = shadowstart_list.at(start_index);
        auto open_path = start_one.attr( "target");

        // 检查伏笔关闭性
        bool item_removed = false;
        for (int stop_index = 0; stop_index < shadowstop_list.size(); ++stop_index) {
            auto stop_one = shadowstop_list.at(stop_index);
            auto close_path = stop_one.attr( "target");

            if(open_path == close_path){
                item_removed = true;
                shadowstop_list.removeAt(stop_index);
                break;
            }
        }
        // 移除已关闭伏笔条目，游标前置消除操作影响
        if(item_removed){
            shadowstart_list.removeAt(start_index);
            start_index--;
        }
    }

    // 未关闭伏笔填充模型
    for (int open_index = 0; open_index < shadowstart_list.size(); ++open_index) {
        auto open_one = shadowstart_list.at(open_index);
        auto foreshadow_one = desp_tree->findForeshadow(open_one.attr( "target"));
        auto keystory_one = desp_tree->parentHandle(foreshadow_one);
        auto volume_one = desp_tree->parentHandle(keystory_one);

        auto foreshadow_index = desp_tree->handleIndex(foreshadow_one);
        auto keystory_index = desp_tree->handleIndex(keystory_one);
        auto volume_index = desp_tree->handleIndex(volume_one);

        QList<QStandardItem*> row;
        auto item = new QStandardItem(foreshadow_one.attr( "title"));
        item->setData(volume_index, Qt::UserRole+1);
        item->setData(keystory_index, Qt::UserRole+2);
        item->setData(foreshadow_index, Qt::UserRole+3);
        row << item;

        item = new QStandardItem("✅开启");
        item->setEditable(false);
        row << item;

        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));

        item = new QStandardItem("无");
        item->setEditable(false);
        row << item;

        item = new QStandardItem(keystory_one.attr( "title"));
        item->setEditable(false);
        row << item;

        item = new QStandardItem(volume_one.attr( "title"));
        item->setEditable(false);
        row << item;

        foreshadows_until_volume_remain_present->appendRow(row);
    }

    // 本卷伏笔关闭信息
    shadowstop_list.clear();
    int chapter_count = desp_tree->chapterCount(volume_node);
    for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
        auto chapter_one = desp_tree->chapterAt(volume_node, chapter_index);

        int stop_count = desp_tree->shadowstopCount(chapter_one);
        for (int stop_index = 0; stop_index < stop_count; ++stop_index) {
            auto stop_one = desp_tree->shadowstopAt(chapter_one, stop_index);
            shadowstop_list << stop_one;
        }
    }

    // 校验伏笔数据
    for (int var = 0; var < shadowstart_list.size(); ++var) {
        auto start_one = shadowstart_list.at(var);
        auto open_path = start_one.attr( "target");

        for (int s2 = 0; s2 < shadowstop_list.size(); ++s2) {
            auto stop_one = shadowstop_list.at(s2);
            auto close_path = stop_one.attr( "target");

            if(open_path == close_path){
                foreshadows_until_volume_remain_present->item(var, 1)->setText("🔒闭合");
                auto chapter = desp_tree->parentHandle(stop_one);
                foreshadows_until_volume_remain_present->item(var, 4)->setText(chapter.attr( "title"));
            }
        }
    }
}

void NovelHost::listen_foreshadows_until_volume_changed(QStandardItem *item)
{
    auto important_item = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto volume_index = important_item->data(Qt::UserRole+1).toInt();
    auto keystory_index = important_item->data(Qt::UserRole+2).toInt();
    auto foreshadow_index = important_item->data(Qt::UserRole+3).toInt();

    auto tvolume = desp_tree->volumeAt(volume_index);
    auto tkeystory = desp_tree->keystoryAt(tvolume, keystory_index);
    auto tforeshadow = desp_tree->foreshadowAt(tkeystory, foreshadow_index);

    switch (item->column()) {
        case 1:
        case 4:
        case 5:
        case 6:
            break;
        case 0:
            desp_tree->setAttr(tforeshadow, "title", item->text());
            break;
        case 2:
            desp_tree->setAttr(tforeshadow, "desp", item->text());
            break;
        case 3:
            desp_tree->setAttr(tforeshadow, "desp_next", item->text());
            break;
    }
}

void NovelHost::sum_foreshadows_until_chapter_remains(const DataAccess::TreeNode &chapter_node)
{
    // 累积所有打开伏笔
    // 累积本章节前关闭伏笔
    desp_tree->checkHandleValid(chapter_node, _X_FStruct::NHandle::Type::CHAPTER);
    foreshadows_until_chapter_remain_present->clear();
    foreshadows_until_chapter_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"起始章节"<<"剧情源"<<"卷宗名");
    QList<_X_FStruct::NHandle> shadowstart_list;
    QList<_X_FStruct::NHandle> shadowstop_list;
    int this_start_count = desp_tree->shadowstartCount(chapter_node);
    for (int start_index = 0; start_index < this_start_count; ++start_index) {
        shadowstart_list << desp_tree->shadowstartAt(chapter_node, start_index);
    }

    auto chapter_previous = desp_tree->previousChapterOfFStruct(chapter_node);
    while (chapter_previous.isValid()) {
        int start_count = desp_tree->shadowstartCount(chapter_previous);
        for (int start_index = 0; start_index < start_count; ++start_index) {
            shadowstart_list << desp_tree->shadowstartAt(chapter_previous, start_index);
        }

        int stop_count = desp_tree->shadowstopCount(chapter_previous);
        for (int stop_index = 0; stop_index < stop_count; ++stop_index) {
            shadowstop_list << desp_tree->shadowstopAt(chapter_previous, stop_index);
        }

        chapter_previous = desp_tree->previousChapterOfFStruct(chapter_previous);
    }

    // 清洗已关闭伏笔
    for (int var = 0; var < shadowstart_list.size(); ++var) {
        auto open_one = shadowstart_list.at(var);
        auto open_path = open_one.attr( "target");

        bool item_remove = false;
        for (int cindex = 0; cindex < shadowstop_list.size(); ++cindex) {
            auto close_one = shadowstop_list.at(cindex);
            auto close_path = close_one.attr( "target");

            if(open_path == close_path){
                shadowstop_list.removeAt(cindex);
                item_remove = true;
                break;
            }
        }
        if(item_remove){
            shadowstart_list.removeAt(var);
            var--;
        }
    }

    // 未关闭列表填充列表
    for (int row_index = 0; row_index < shadowstart_list.size(); ++row_index) {
        auto item_one = shadowstart_list.at(row_index);
        auto foreshadow_one = desp_tree->findForeshadow(item_one.attr( "target"));
        auto keystory_one = desp_tree->parentHandle(foreshadow_one);
        auto volume_one = desp_tree->parentHandle(keystory_one);

        auto foreshadow_index = desp_tree->handleIndex(foreshadow_one);
        auto keystory_index = desp_tree->handleIndex(keystory_one);
        auto volume_index = desp_tree->handleIndex(volume_one);

        QList<QStandardItem*> row;
        auto node = new QStandardItem(foreshadow_one.attr( "title"));
        node->setData(volume_index, Qt::UserRole+1);
        node->setData(keystory_index, Qt::UserRole+2);
        node->setData(foreshadow_index, Qt::UserRole+3);
        row << node;

        node = new QStandardItem("✅开启");
        node->setEditable(false);
        row << node;

        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));

        node = new QStandardItem("无");
        node->setEditable(false);
        row << node;

        node = new QStandardItem(keystory_one.attr( "title"));
        node->setEditable(false);
        row << node;

        node = new QStandardItem(volume_one.attr( "title"));
        node->setEditable(false);
        row << node;

        foreshadows_until_chapter_remain_present->appendRow(row);
    }

    // 获取本章节所有关闭伏笔
    shadowstop_list.clear();
    int this_stop_count = desp_tree->shadowstopCount(chapter_node);
    for (int stop_index = 0; stop_index < this_stop_count; ++stop_index) {
        shadowstop_list << desp_tree->shadowstopAt(chapter_node, stop_index);
    }

    for (int row_index = 0; row_index < shadowstart_list.size(); ++row_index) {
        auto open_one = shadowstart_list.at(row_index);
        auto open_path = open_one.attr( "target");

        for (int cycindex = 0; cycindex < shadowstop_list.size(); ++cycindex) {
            auto close_one = shadowstop_list.at(cycindex);
            auto close_path = close_one.attr( "target");

            if(open_path == close_path){
                foreshadows_until_chapter_remain_present->item(row_index, 1)->setText("🔒闭合");
                auto chapter = desp_tree->parentHandle(open_one);
                auto volume = desp_tree->parentHandle(chapter);
                foreshadows_until_chapter_remain_present->item(row_index, 4)->setText(volume.attr("title") + "·"
                                                                                      + chapter.attr( "title"));
            }
        }
    }
}

void NovelHost::listen_foreshadows_until_chapter_changed(QStandardItem *item)
{
    auto important = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto volume_index = important->data(Qt::UserRole+1).toInt();
    auto keystory_index = important->data(Qt::UserRole+2).toInt();
    auto foreshadow_index = important->data(Qt::UserRole+3).toInt();

    auto tvolume = desp_tree->volumeAt(volume_index);
    auto tkeystory = desp_tree->keystoryAt(tvolume, keystory_index);
    auto tforeshadow = desp_tree->foreshadowAt(tkeystory, foreshadow_index);

    switch (item->column()) {
        case 1:
        case 4:
        case 5:
        case 6:
            break;
        case 0:
            desp_tree->setAttr(tforeshadow, "title", item->text());
            break;
        case 2:
            desp_tree->setAttr(tforeshadow, "desp", item->text());
            break;
        case 3:
            desp_tree->setAttr(tforeshadow, "desp_next", item->text());
            break;
    }
}


// msgList : [type](target)<keys-to-target>msg-body
void NovelHost::_check_remove_effect(const DataAccess::TreeNode &target, QList<QString> &msgList) const
{
    if(target.type() == TnType::KEYPOINT)
        return;

    if(target.type() == TnType::DESPLINE) {
        msgList << "[warring](foreshadow·despline)<"+target.title()+">指定伏笔[故事线]将被删除，请注意！";

        auto stopnodes = desp_ins->getAttachedPointsViaDespline(target);
        for (auto dot : stopnodes) {
            auto storyblk = dot.storyAttached();
            auto chapter = dot.chapterAttached();
            msgList << "[error](keystory·storyblock)<"+storyblk.title()+">影响关键剧情！请重写相关内容！";
            msgList << "[error](chapter)<"+chapter.title()+">影响章节内容！请重写相关内容！";
        }
        return;
    }

    if(target.type() == TnType::STORYBLOCK){
        auto points = desp_ins->getAttachedPointsViaStoryblock(target);
        for (auto dot : points) {
            auto foreshadownode = dot.desplineReference();
            auto chapternode = dot.chapterAttached();
            msgList << "[error](foreshadow·despline)<"+foreshadownode.title()+">影响指定伏笔[故事线]，请注意修改描述！";
            msgList << "[error](chapter)<"+chapternode.title()+">影响章节内容！请重写相关内容！";
        }
    }

    if(target.type() == TnType::VOLUME){
        auto despline_count = desp_ins->childNodeCount(target, TnType::DESPLINE);
        for (int var = 0; var < despline_count; ++var) {
            _check_remove_effect(desp_ins->childNodeAt(target, TnType::DESPLINE, var), msgList);
        }

        auto storyblock_count = desp_ins->childNodeCount(target, TnType::STORYBLOCK);
        for (int var = 0; var < storyblock_count; ++var) {
            _check_remove_effect(desp_ins->childNodeAt(target, TnType::STORYBLOCK, var), msgList);
        }

        auto chapter_count = desp_ins->childNodeCount(target, TnType::CHAPTER);
        for (int var = 0; var < chapter_count; ++var) {
            _check_remove_effect(desp_ins->childNodeAt(target, TnType::CHAPTER, var), msgList);
        }
    }

    if(target.type() == TnType::CHAPTER){
        auto points = desp_ins->getAttachedPointsViaChapter(target);
        for (auto dot : points) {
            auto foreshadownode = dot.desplineReference();
            auto storyblknode = dot.storyAttached();
            msgList << "[error](foreshadow·despline)<"+foreshadownode.title()+">影响指定伏笔[故事线]，请注意修改描述！";
            msgList << "[error](keystory·storyblock)<"+storyblknode.title()+">影响剧情内容！请注意相关内容！";
        }

    }
}

QTextDocument* NovelHost::_load_chapter_text_content(QStandardItem *item)
{
    auto parent = item->parent();
    if(!parent) // 卷宗节点不可加载
        return nullptr;

    // load text-content
    auto volume_symbo = desp_ins->childNodeAt(desp_ins->novelRoot(), TnType::VOLUME, parent->row());
    auto chapter_symbo = desp_ins->childNodeAt(volume_symbo, TnType::CHAPTER, item->row());
    QString content = desp_ins->chapterText(chapter_symbo);

    // 载入内存实例
    auto doc = new QTextDocument();
    doc->setPlainText(content==""?"章节内容为空":content);

    QTextFrameFormat frameformat;
    config_host.textFrameFormat(frameformat);
    doc->rootFrame()->setFrameFormat(frameformat);

    QTextBlockFormat blockformat;
    QTextCharFormat charformat;
    config_host.textFormat(blockformat, charformat);
    QTextCursor cursor(doc);
    cursor.select(QTextCursor::Document);
    cursor.setBlockFormat(blockformat);
    cursor.setBlockCharFormat(charformat);
    cursor.setCharFormat(charformat);

    doc->clearUndoRedoStacks();
    doc->setUndoRedoEnabled(true);
    doc->setModified(false);

    // 纳入管理机制
    all_documents.insert(static_cast<ChaptersItem*>(item), qMakePair(doc, nullptr));
    connect(doc, &QTextDocument::contentsChanged, static_cast<ChaptersItem*>(item),  &ChaptersItem::calcWordsCount);

    return doc;
}

// 写作界面
QStandardItemModel *NovelHost::chaptersNavigateTree() const
{
    return chapters_navigate_treemodel;
}

QStandardItemModel *NovelHost::findResultsPresent() const
{
    return find_results_model;
}

QTextDocument *NovelHost::chapterOutlinePresent() const
{
    return chapter_outlines_present;
}

void NovelHost::insertChapter(const QModelIndex &chpsVmIndex, int before, const QString &chpName)
{
    if(!chpsVmIndex.isValid())
        throw new WsException("输入volumeindex：chapters无效");

    auto item = chapters_navigate_treemodel->itemFromIndex(chpsVmIndex);
    auto parent = item->parent();
    if(parent) // 选中的是章节节点
        return;

    auto struct_volumme = desp_tree->volumeAt(item->row());
    auto count = desp_tree->chapterCount(struct_volumme);
    auto newnode = desp_tree->insertChapter(struct_volumme, before, chpName, "");

    QList<QStandardItem*> row;
    row << new ChaptersItem(*this, newnode);
    row << new QStandardItem("-");
    if(before >= count){
        item->appendRow(row);
    }
    else {
        item->insertRow(before, row);
    }

    QString file_path = desp_tree->chapterCanonicalFilePath(newnode);
    QFile target(file_path);
    if(target.exists()){
        if(!target.remove())
            throw new WsException("指定路径文件已存在，重建失败！");
    }

    if(!target.open(QIODevice::WriteOnly|QIODevice::Text))
        throw new WsException("软件错误，指定路径文件无法打开："+file_path);

    target.close();
}

void NovelHost::appendShadowstart(const QModelIndex &chpIndex, const QString &keystory, const QString &foreshadow)
{
    if(!chpIndex.isValid())
        throw new WsException("传入的章节index非法");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chpIndex);       // 章节节点
    auto volume = chapter->parent();                                       // 卷宗节点

    auto struct_volume_node = desp_tree->volumeAt(volume->row());
    auto struct_chapter_node = desp_tree->chapterAt(struct_volume_node, chapter->row());

    desp_tree->appendShadowstart(struct_chapter_node, keystory, foreshadow);
}

void NovelHost::removeShadowstart(const QModelIndex &chpIndex, const QString &targetPath)
{
    if(treeNodeLevel(chpIndex) != 2)
        throw new WsException("传入index非章节index");

    auto struct_volume = desp_tree->volumeAt(chpIndex.parent().row());
    auto struct_chapter = desp_tree->chapterAt(struct_volume, chpIndex.row());
    auto start_count = desp_tree->shadowstartCount(struct_chapter);
    for (int index = 0; index < start_count; ++index) {
        auto struct_start = desp_tree->shadowstartAt( struct_chapter, index);
        if(struct_start.attr("target") == targetPath){
            desp_tree->removeHandle(struct_start);
            return;
        }
    }

    throw new WsException("章节节点未找到指定targetpath的shadowstart节点："+targetPath);
}

void NovelHost::appendShadowstop(const QModelIndex &chpIndex, const QString &volume,
                                 const QString &keystory, const QString &foreshadow)
{
    if(!chpIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chpIndex);
    auto volume_ = chapter->parent();                                       // 卷宗节点

    auto struct_volume_node = desp_tree->volumeAt(volume_->row());
    auto struct_chapter_node = desp_tree->chapterAt(struct_volume_node, chapter->row());

    desp_tree->appendShadowstop(struct_chapter_node, volume, keystory, foreshadow);
}

void NovelHost::removeShadowstop(const QModelIndex &chpIndex, const QString &targetPath)
{
    if(treeNodeLevel(chpIndex) != 2)
        throw new WsException("传入index非章节index");

    auto struct_volume = desp_tree->volumeAt(chpIndex.parent().row());
    auto struct_chapter = desp_tree->chapterAt(struct_volume, chpIndex.row());
    auto stop_count = desp_tree->shadowstopCount(struct_chapter);
    for (int index = 0; index < stop_count; ++index) {
        auto struct_stop = desp_tree->shadowstopAt( struct_chapter, index);
        if(struct_stop.attr("target") == targetPath){
            desp_tree->removeHandle(struct_stop);
            return;
        }
    }

    throw new WsException("章节节点未找到指定targetpath的shadowstop节点："+targetPath);
}

void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("chaptersNodeIndex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chaptersNode);

    int row = chapter->row();
    // 卷宗节点管理同步
    if(!chapter->parent()){
        auto struct_volume = desp_tree->volumeAt(row);
        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        desp_tree->removeHandle(struct_volume);
    }
    // 章节节点
    else {
        auto volume = chapter->parent();
        auto struct_volume = desp_tree->volumeAt(volume->row());
        auto struct_chapter = desp_tree->chapterAt(struct_volume, row);

        volume->removeRow(row);
        desp_tree->removeHandle(struct_chapter);
    }
}

void NovelHost::removeForeshadowNode(int fsid)
{
    auto node = desp_ins->getTreenodeViaID(fsid);
    if(node.type() != TnType::DESPLINE)
        throw new WsException("指定传入id不属于伏笔[故事线]");
    desp_ins->removeNode(node);
}

void NovelHost::setCurrentChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("传入的chaptersindex无效");

    _X_FStruct::NHandle node;
    switch (treeNodeLevel(chaptersNode)) {
        case 1: // 卷宗
            node = desp_tree->volumeAt(chaptersNode.row());
            break;
        case 2: // 章节
            auto struct_volume = desp_tree->volumeAt(chaptersNode.parent().row());
            node = desp_tree->chapterAt(struct_volume, chaptersNode.row());
            break;
    }

    set_current_volume_outlines(node);
    emit currentVolumeActived();

    // 统计本卷宗下所有构建伏笔及其状态  名称，吸附状态，前描述，后描述，吸附章节、源剧情
    sum_foreshadows_under_volume(current_volume_node);
    sum_foreshadows_until_volume_remains(current_volume_node);

    if(node.nType() != _X_FStruct::NHandle::Type::CHAPTER)
        return;

    current_chapter_node = node;
    emit currentChaptersActived();

    // 统计至此章节前未闭合伏笔及本章闭合状态  名称、闭合状态、前描述、后描述、闭合章节、源剧情、源卷宗
    sum_foreshadows_until_chapter_remains(current_chapter_node);
    disconnect(chapter_outlines_present,    &QTextDocument::contentsChanged,
               this,   &NovelHost::listen_chapter_outlines_description_change);
    chapter_outlines_present->clear();
    auto chapters_outlines_str = current_chapter_node.attr( "desp");
    QTextBlockFormat blockformat0;
    QTextCharFormat charformat0;
    config_host.textFormat(blockformat0, charformat0);
    QTextCursor cursor0(chapter_outlines_present);
    cursor0.setBlockFormat(blockformat0);
    cursor0.setBlockCharFormat(charformat0);
    cursor0.insertText(chapters_outlines_str);
    chapter_outlines_present->setModified(false);
    chapter_outlines_present->clearUndoRedoStacks();
    connect(chapter_outlines_present,   &QTextDocument::contentsChanged,
            this,   &NovelHost::listen_chapter_outlines_description_change);

    // 打开目标章节，前置章节正文内容
    auto item = static_cast<ChaptersItem*>(chapters_navigate_treemodel->itemFromIndex(chaptersNode));
    if(!all_documents.contains(item))
        _load_chapter_text_content(item);

    auto pack = all_documents.value(item);
    if(!pack.second){   // 如果打开的时候没有关联渲染器
        auto renderer = new WordsRender(pack.first, config_host);
        all_documents.insert(item, qMakePair(pack.first, renderer));
    }

    auto title = node.attr( "title");
    emit documentPrepared(pack.first, title);
}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_treemodel->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_treemodel->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
    }
}

DataAccess::TreeNode NovelHost::sumForeshadowsUnderVolumeAll(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    QModelIndex volume_index = chpsNode;
    auto level = treeNodeLevel(chpsNode);
    if(level==2)
        volume_index = chpsNode.parent();

    auto struct_volume = desp_tree->volumeAt(volume_index.row());
    auto keystory_count = desp_tree->keystoryCount(struct_volume);
    for (auto keystory_index = 0; keystory_index<keystory_count; ++keystory_index) {
        auto struct_keystory = desp_tree->keystoryAt(struct_volume, keystory_index);
        auto foreshadows_count = desp_tree->foreshadowCount(struct_keystory);

        for (auto foreshadow_index=0; foreshadow_index<foreshadows_count; ++foreshadow_index) {
            auto struct_foreshadow = desp_tree->foreshadowAt(struct_keystory, foreshadow_index);
            foreshadows << qMakePair(QString("[%1]%2").arg(struct_keystory.attr("title")).arg(struct_foreshadow.attr("title")),
                                     desp_tree->foreshadowKeysPath(struct_foreshadow));
        }
    }

    return struct_volume;
}

void NovelHost::sumForeshadowsUnderVolumeHanging(const QModelIndex &chpsNode, QList<QPair<QString, QString> > &foreshadows) const
{
    // 汇总所有伏笔
    auto struct_volume = sumForeshadowsUnderVolumeAll(chpsNode, foreshadows);

    // 清洗所有吸附伏笔信息
    for (int index = 0; index < foreshadows.size(); ++index) {
        auto one = foreshadows.at(index);

        auto chapter_count = desp_tree->chapterCount(struct_volume);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto struct_chapter = desp_tree->chapterAt(struct_volume, chapter_index);
            auto struct_start = desp_tree->findShadowstart(struct_chapter, one.second);
            if(struct_start.isValid()) {
                foreshadows.removeAt(index);
                index--;
                break;
            }
        }
    }
}

void NovelHost::sumForeshadowsAbsorbedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString, QString> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto struct_volume = desp_tree->volumeAt(chpsNode.parent().row());
    auto struct_chapter = desp_tree->chapterAt(struct_volume, chpsNode.row());
    auto start_count = desp_tree->shadowstartCount(struct_chapter);
    for (int index = 0; index < start_count; ++index) {
        auto struct_start = desp_tree->shadowstartAt(struct_chapter, index);
        auto path = struct_start.attr("target");

        auto struct_foreshadow = desp_tree->findForeshadow(path);
        auto struct_keystory = desp_tree->parentHandle(struct_foreshadow);
        foreshadows << qMakePair(QString("[%1*%2]%3").arg(struct_volume.attr("title"))
                                 .arg(struct_keystory.attr("title"))
                                 .arg(struct_foreshadow.attr("title")),
                                 path);
    }
}

void NovelHost::sumForeshadowsOpeningUntilChapter(const QModelIndex &chpsNode, QList<QPair<QString, QString> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto struct_volume = desp_tree->volumeAt(chpsNode.parent().row());
    auto struct_chapter= desp_tree->chapterAt(struct_volume, chpsNode.row());
    QList<_X_FStruct::NHandle> startlist, stoplist;
    while (struct_chapter.isValid()) {
        auto start_count = desp_tree->shadowstartCount(struct_chapter);
        for (int index = 0; index < start_count; ++index) {
            startlist << desp_tree->shadowstartAt(struct_chapter, index);
        }

        auto stop_count = desp_tree->shadowstopCount(struct_chapter);
        for (int index = 0; index < stop_count; ++index) {
            stoplist << desp_tree->shadowstopAt(struct_chapter, index);
        }

        struct_chapter = desp_tree->previousChapterOfFStruct(struct_chapter);
    }

    for (auto stop_one : stoplist) {
        for (auto index=0; index<startlist.size(); ++index) {
            auto start_one = startlist.at(index);
            if(stop_one.attr("target") == start_one.attr("target")){
                startlist.removeAt(index);
                break;
            }
        }
    }

    for (auto one : startlist) {
        auto fullpath = one.attr("target");
        auto struct_foreshadow = desp_tree->findForeshadow(fullpath);
        auto struct_keystory = desp_tree->parentHandle(struct_foreshadow);
        auto struct_volume = desp_tree->parentHandle(struct_keystory);

        foreshadows << qMakePair(
                           QString("[%1*%2]%3").arg(struct_volume.attr("title"))
                           .arg(struct_keystory.attr("title"))
                           .arg(struct_foreshadow.attr("title")),
                           fullpath);
    }
}

void NovelHost::sumForeshadowsClosedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString, QString> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto struct_volume = desp_tree->volumeAt(chpsNode.parent().row());
    auto struct_chapter= desp_tree->chapterAt(struct_volume, chpsNode.row());
    auto stop_count = desp_tree->shadowstopCount(struct_chapter);
    for (int index = 0; index < stop_count; ++index) {
        auto struct_stop = desp_tree->shadowstopAt(struct_chapter, index);
        auto fullpath = struct_stop.attr("target");

        auto struct_foreshadow = desp_tree->findForeshadow(fullpath);
        auto struct_keystory = desp_tree->parentHandle(struct_foreshadow);
        auto struct_volume = desp_tree->parentHandle(struct_keystory);

        foreshadows << qMakePair(
                           QString("[%1*%2]%3").arg(struct_volume.attr("title"))
                           .arg(struct_keystory.attr("title"))
                           .arg(struct_foreshadow.attr("title")),
                           fullpath);
    }
}



void NovelHost::searchText(const QString &text)
{
    QRegExp exp("("+text+").*");
    find_results_model->clear();
    find_results_model->setHorizontalHeaderLabels(QStringList() << "搜索文本" << "卷宗节点" << "章节节点");

    for (int vm_index=0; vm_index<chapters_navigate_treemodel->rowCount(); ++vm_index) {
        auto chapters_volume_node = chapters_navigate_treemodel->item(vm_index);

        for (int chapters_chp_index=0; chapters_chp_index<chapters_volume_node->rowCount(); ++chapters_chp_index) {
            auto chapters_chp_node = chapters_volume_node->child(chapters_chp_index);
            QString content = chapterActiveText(chapters_chp_node->index());

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

QString NovelHost::chapterActiveText(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        throw new WsException("输入index无效");

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = chapters_navigate_treemodel->itemFromIndex(index);
    auto parent = item->parent();
    if(!parent) // 卷节点不获取内容
        return "";

    auto refer_node = static_cast<ChaptersItem*>(item);
    return all_documents.value(refer_node).first->toPlainText();
}

int NovelHost::calcValidWordsCount(const QString &content)
{
    QString newtext = content;
    QRegExp exp("[，。！？【】“”—…《》：、\\s·￥%「」]");
    return newtext.replace(exp, "").size();
}

void NovelHost::outlines_node_title_changed(QStandardItem *item)
{
    auto struct_node = _locate_outline_handle_via(item);
    struct_node.titleReset(item->text());
}

void NovelHost::chapters_node_title_changed(QStandardItem *item){
    if(item->parent() && !item->column() )  // chapter-node 而且 不是计数节点
    {
        auto root = desp_ins->novelRoot();
        auto volume_struct = desp_ins->childNodeAt(root, TnType::VOLUME, item->parent()->row());
        auto struct_chapter = desp_ins->childNodeAt(volume_struct, TnType::CHAPTER, item->row());
        struct_chapter.titleReset(item->text());
    }
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const DataAccess::TreeNode &volume_handle, int index)
{
    auto outline_volume_node = new OutlinesItem(volume_handle);

    QList<QStandardItem*> navigate_valume_row;
    auto node_navigate_volume_node = new ChaptersItem(*this, volume_handle, true);
    navigate_valume_row << node_navigate_volume_node;
    navigate_valume_row << new QStandardItem("-");


    if(index >= outline_navigate_treemodel->rowCount()){
        outline_navigate_treemodel->appendRow(outline_volume_node);
        chapters_navigate_treemodel->appendRow(navigate_valume_row);
    }
    else {
        outline_navigate_treemodel->insertRow(index, outline_volume_node);
        chapters_navigate_treemodel->insertRow(index, navigate_valume_row);
    }


    return qMakePair(outline_volume_node, node_navigate_volume_node);
}

void NovelHost::listen_novel_description_change()
{
    auto content = novel_outlines_present->toPlainText();
    desp_ins->novelRoot().descriptionReset(content);
}

// 向卷宗细纲填充内容
void NovelHost::set_current_volume_outlines(const DataAccess::TreeNode &node_under_volume){
    if(!node_under_volume.isValid())
        throw new WsException("传入节点无效");

    if(node_under_volume.nType() == _X_FStruct::NHandle::Type::VOLUME){
        current_volume_node = node_under_volume;

        disconnect(volume_outlines_present,  &QTextDocument::contentsChange,
                   this,   &NovelHost::listen_volume_outlines_description_change);
        disconnect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                   this,    &NovelHost::listen_volume_outlines_structure_changed);

        volume_outlines_present->clear();
        QTextCursor cursor(volume_outlines_present);

        int volume_index = desp_tree->handleIndex(node_under_volume);
        auto volume_node = outline_navigate_treemodel->item(volume_index);
        insert_description_at_volume_outlines_doc(cursor, static_cast<OutlinesItem*>(volume_node));
        volume_outlines_present->setModified(false);
        volume_outlines_present->clearUndoRedoStacks();

        connect(volume_outlines_present, &QTextDocument::contentsChange,
                this,   &NovelHost::listen_volume_outlines_description_change);
        connect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                   this,    &NovelHost::listen_volume_outlines_structure_changed);
        return;
    }

    auto node = desp_tree->parentHandle(node_under_volume);
    set_current_volume_outlines(node);
}

ChaptersItem::ChaptersItem(NovelHost &host, const DataAccess::TreeNode &refer, bool isGroup)
    :host(host)
{
    setText(refer.title());

    if(isGroup){
        setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    }
    else {
        setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    }
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
        QString content = host.chapterActiveText(index());

        auto pitem = QStandardItem::parent();
        auto cnode = pitem->child(row(), 1);
        cnode->setText(QString("%1").arg(host.calcValidWordsCount(content)));
    }
}

// highlighter collect ===========================================================================

WordsRenderWorker::WordsRenderWorker(WordsRender *poster, const QTextBlock pholder, const QString &content)
    :poster_stored(poster),config_symbo(poster->configBase()),placeholder(pholder), content_stored(content)
{
    setAutoDelete(true);
}

void WordsRenderWorker::run()
{
    QList<std::tuple<QTextCharFormat, QString, int, int>> rst;

    QTextCharFormat format;
    config_symbo.warringFormat(format);
    auto warrings = config_symbo.warringWords();
    _highlighter_render(content_stored, warrings, format, rst);

    QTextCharFormat format2;
    config_symbo.keywordsFormat(format2);
    auto keywords = config_symbo.keywordsList();
    _highlighter_render(content_stored, keywords, format2, rst);

    poster_stored->acceptRenderResult(content_stored, rst);
    emit renderFinished(placeholder);
}

void WordsRenderWorker::_highlighter_render(const QString &text, QList<QString> words, const QTextCharFormat &format,
                                       QList<std::tuple<QTextCharFormat, QString, int, int> > &rst) const
{
    for (auto one : words) {
        QRegExp exp("("+one+").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(text, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            rst.append(std::make_tuple(format, wstr, sint, lint));
        }
    }
}


WordsRender::WordsRender(QTextDocument *target, ConfigHost &config)
    :QSyntaxHighlighter (target), config(config){}

WordsRender::~WordsRender(){}

void WordsRender::acceptRenderResult(const QString &content, const QList<std::tuple<QTextCharFormat, QString, int, int> > &rst)
{
    QMutexLocker lock(&mutex);

    _result_store.insert(content, rst);
}

ConfigHost &WordsRender::configBase() const
{
    return config;
}

bool WordsRender::_check_extract_render_result(const QString &text, QList<std::tuple<QTextCharFormat, QString, int, int>> &rst)
{
    QMutexLocker lock(&mutex);

    if(!_result_store.contains(text))
        return false;

    rst = _result_store.value(text);
    return _result_store.remove(text);
}

void WordsRender::highlightBlock(const QString &text)
{
    auto blk = currentBlock();
    if(!blk.isValid())
        return;

    QList<std::tuple<QTextCharFormat, QString, int, int>> rst;
    if(!_check_extract_render_result(text, rst)){
        auto worker = new WordsRenderWorker(this, blk, text);
        connect(worker, &WordsRenderWorker::renderFinished,   this,   &QSyntaxHighlighter::rehighlightBlock);
        QThreadPool::globalInstance()->start(worker);
        return;
    }

    for (auto tuple:rst) {
        auto format = std::get<0>(tuple);
        auto sint = std::get<2>(tuple);
        auto lint = std::get<3>(tuple);

        setFormat(sint, lint, format);
    }
}

OutlinesItem::OutlinesItem(const DataAccess::TreeNode &refer)
{
    setText(refer.title());
    switch (refer.type()) {
        case DataAccess::TreeNode::Type::KEYPOINT:
            setIcon(QIcon(":/outlines/icon/点.png"));
            break;
        case DataAccess::TreeNode::Type::VOLUME:
            setIcon(QIcon(":/outlines/icon/卷.png"));
            break;
        case DataAccess::TreeNode::Type::CHAPTER:
            setIcon(QIcon(":/outlines/icon/章.png"));
            break;
        case DataAccess::TreeNode::Type::STORYBLOCK:
            setIcon(QIcon(":/outlines/icon/情.png"));
            break;
        default:
            break;
    }
}



WsBlockData::WsBlockData(const QModelIndex &target, WsBlockData::Type blockType)
    :outline_index(target), block_type(blockType){}

bool WsBlockData::operator==(const WsBlockData &other) const
{
    return outline_index == other.outline_index;
}

QModelIndex WsBlockData::outlineTarget() const
{
    return outline_index;
}

WsBlockData::Type WsBlockData::blockType() const
{
    return block_type;
}

OutlinesRender::OutlinesRender(QTextDocument *doc, ConfigHost &config)
    :QSyntaxHighlighter (doc), config(config){}

void OutlinesRender::highlightBlock(const QString &text)
{
    auto block = currentBlock();
    if(!block.userData())
        return;

    QTextBlockFormat bformat;
    QTextCharFormat  cformat;
    auto typedate = static_cast<WsBlockData*>(block.userData());
    switch (typedate->blockType()) {
        case WsBlockData::Type::VOLUME:
            config.volumeTitleFormat(bformat, cformat);
            break;
        case WsBlockData::Type::STORYBLOCK:
            config.keystoryTitleFormat(bformat, cformat);
            break;
        case WsBlockData::Type::KEYPOINT:
            config.pointTitleFormat(bformat, cformat);
            break;
        default:
            return;
    }

    setFormat(0, text.length(), cformat);
}

ForeshadowRedirectDelegate::ForeshadowRedirectDelegate(NovelHost *const host)
    :host(host){}

QWidget *ForeshadowRedirectDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    return new QComboBox(parent);
}

void ForeshadowRedirectDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    QList<QPair<QString,int>> key_stories;
    host->allKeystoriesUnderCurrentVolume(key_stories);
    for (auto xpair : key_stories) {
        cedit->addItem(xpair.first, xpair.second);
    }
    cedit->setCurrentText(index.data().toString());
}

void ForeshadowRedirectDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    model->setData(index, cedit->currentData(), Qt::UserRole+1);
}

void ForeshadowRedirectDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
