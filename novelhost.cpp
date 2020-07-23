#include "common.h"
#include "dbaccess.h"
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
using TnType = DBAccess::TreeNode::Type;

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

void NovelHost::convert20_21(const QString &destPath, const QString &fromPath)
{
    try {
        _X_FStruct *desp_tree = new _X_FStruct();
        desp_tree->openFile(fromPath);
        DBAccess dbtool;

        dbtool.createEmptyDB(destPath);
        auto root = dbtool.novelTreeNode();
        dbtool.resetDescriptionOfTreeNode(root, desp_tree->novelDescription());
        dbtool.resetTitleOfTreeNode(root, desp_tree->novelTitle());

        auto vnum = desp_tree->volumeCount();
        // 导入所有条目
        for (int vindex = 0; vindex < vnum; ++vindex) {
            auto vmnode = desp_tree->volumeAt(vindex);
            auto dbvnode = dbtool.insertChildTreeNodeBefore(root, DBAccess::TreeNode::Type::VOLUME,
                                     dbtool.childCountOfTreeNode(root, DBAccess::TreeNode::Type::VOLUME),
                                     vmnode.attr("title"),
                                     vmnode.attr("desp"));

            // chapters
            auto chpnum = desp_tree->chapterCount(vmnode);
            for (int chpindex = 0; chpindex < chpnum; ++chpindex) {
                auto chpnode = desp_tree->chapterAt(vmnode, chpindex);
                auto dbchpnode = dbtool.insertChildTreeNodeBefore(dbvnode, DBAccess::TreeNode::Type::CHAPTER,
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
                auto dbkstorynode = dbtool.insertChildTreeNodeBefore(dbvnode, DBAccess::TreeNode::Type::STORYBLOCK,
                                                         ksindex, kstorynode.attr("title"), kstorynode.attr("desp"));

                // points
                auto pointnum = desp_tree->pointCount(kstorynode);
                for (int pindex = 0; pindex < pointnum; ++pindex) {
                    auto pointnode = desp_tree->pointAt(kstorynode, pindex);
                    dbtool.insertChildTreeNodeBefore(dbkstorynode, DBAccess::TreeNode::Type::KEYPOINT,
                                             pindex, pointnode.attr("title"), pointnode.attr("desp"));
                }

                // foreshadows
                auto foreshadownum = desp_tree->foreshadowCount(kstorynode);
                for (int findex = 0; findex < foreshadownum; ++findex) {
                    auto foreshadownode = desp_tree->foreshadowAt(kstorynode, findex);
                    auto dbfsnode = dbtool.insertChildTreeNodeBefore(dbvnode, DBAccess::TreeNode::Type::DESPLINE,
                                             dbtool.childCountOfTreeNode(dbvnode, DBAccess::TreeNode::Type::DESPLINE),
                                             foreshadownode.attr("title"), "无整体描述");

                    auto headnode = dbtool.insertAttachPointBefore(dbfsnode, 0, false, "阶段0", foreshadownode.attr("desp"));
                    dbtool.resetStoryblockOfAttachPoint(headnode, dbkstorynode);
                    dbtool.insertAttachPointBefore(dbfsnode, 1, false, "阶段1", foreshadownode.attr("desp_next"));
                }
            }
        }

        // 校验关联伏笔吸附等情况
        auto firstchapter_node = desp_tree->firstChapterOfFStruct();
        while (firstchapter_node.isValid()) {
            auto chapter_index = desp_tree->handleIndex(firstchapter_node);
            auto startcount = desp_tree->shadowstartCount(firstchapter_node);
            for (int var = 0; var < startcount; ++var) {
                auto start_one = desp_tree->shadowstartAt(firstchapter_node, var);
                auto fskeyspath = start_one.attr("target");

                auto foreshadow_node = desp_tree->findForeshadow(fskeyspath);
                auto foreshadow_index = desp_tree->handleIndex(foreshadow_node);
                auto keystory_node = desp_tree->parentHandle(foreshadow_node);
                auto keystory_index = desp_tree->handleIndex(keystory_node);
                auto volume_node = desp_tree->parentHandle(keystory_node);
                auto volume_index = desp_tree->handleIndex(volume_node);
                auto index_acc = foreshadow_index;

                for (int var = 0; var < keystory_index; ++var) {
                    auto keystory_one = desp_tree->keystoryAt(volume_node, var);
                    index_acc += desp_tree->foreshadowCount(keystory_one);
                }

                auto dbvolume_node = dbtool.novelTreeNode().childAt(TnType::VOLUME, volume_index);
                auto dbchapter_node = dbvolume_node.childAt(TnType::CHAPTER, chapter_index);
                auto dbdespline_node = dbtool.childAtOfTreeNode(dbvolume_node, TnType::DESPLINE, index_acc);
                auto points = dbtool.getAttachPointsViaDespline(dbdespline_node);
                dbtool.resetChapterOfAttachPoint(points[0], dbchapter_node);
            }

            auto stopcount = desp_tree->shadowstopCount(firstchapter_node);
            for (int var = 0; var < stopcount; ++var) {
                auto stop_one = desp_tree->shadowstopAt(firstchapter_node, var);
                auto fskeyspath = stop_one.attr("target");

                auto foreshadow_node = desp_tree->findForeshadow(fskeyspath);
                auto foreshadow_index = desp_tree->handleIndex(foreshadow_node);
                auto keystory_node = desp_tree->parentHandle(foreshadow_node);
                auto keystory_index = desp_tree->handleIndex(keystory_node);
                auto volume_node = desp_tree->parentHandle(keystory_node);
                auto volume_index = desp_tree->handleIndex(volume_node);
                auto index_acc = foreshadow_index;

                for (int var = 0; var < keystory_index; ++var) {
                    auto keystory_one = desp_tree->keystoryAt(volume_node, var);
                    index_acc += desp_tree->foreshadowCount(keystory_one);
                }

                auto dbvolume_node = dbtool.childAtOfTreeNode(dbtool.novelTreeNode(), TnType::VOLUME, volume_index);
                auto dbchapter_node = dbtool.childAtOfTreeNode(dbvolume_node, TnType::CHAPTER, chapter_index);
                auto dbdespline_node = dbtool.childAtOfTreeNode(dbvolume_node, TnType::DESPLINE, index_acc);
                auto points = dbtool.getAttachPointsViaDespline(dbdespline_node);
                dbtool.resetChapterOfAttachPoint(points[1], dbchapter_node);
                dbtool.resetCloseStateOfAttachPoint(points[1], true);
            }
            firstchapter_node = desp_tree->nextChapterOfFStruct(firstchapter_node);
        }

    } catch (WsException *e) {
        qDebug() << e->reason();
    }

}

void NovelHost::loadDescription(DBAccess *desp)
{
    // save description structure
    this->desp_ins = desp;
    chapters_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "章卷名称" << "严格字数统计");
    outline_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "故事结构");

    auto novel_node = desp->novelTreeNode();

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

    using TnType = DBAccess::TreeNode::Type;
    auto volume_num = desp->childCountOfTreeNode(novel_node, TnType::VOLUME);
    for (int volume_index = 0; volume_index < volume_num; ++volume_index) {
        DBAccess::TreeNode volume_node = desp->childAtOfTreeNode(novel_node, TnType::VOLUME, volume_index);

        // 在chapters-tree和outline-tree上插入卷节点
        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int storyblock_count = desp->childCountOfTreeNode(volume_node, TnType::STORYBLOCK);
        for (int storyblock_index = 0; storyblock_index < storyblock_count; ++storyblock_index) {
            DBAccess::TreeNode storyblock_node = desp->childAtOfTreeNode(volume_node, TnType::STORYBLOCK, storyblock_index);

            // outline-tree上插入故事节点
            auto ol_keystory_item = new OutlinesItem(storyblock_node);
            outline_volume_node->appendRow(ol_keystory_item);

            // outline-tree上插入point节点
            int points_count = desp->childCountOfTreeNode(storyblock_node, TnType::KEYPOINT);
            for (int points_index = 0; points_index < points_count; ++points_index) {
                DBAccess::TreeNode point_node = desp->childAtOfTreeNode(storyblock_node, TnType::KEYPOINT, points_index);

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }
        }

        // chapters上插入chapter节点
        int chapter_count = desp->childCountOfTreeNode(volume_node, TnType::CHAPTER);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto chapter_node = desp->childAtOfTreeNode(volume_node, TnType::CHAPTER, chapter_index);

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

void NovelHost::save()
{
    for (auto vm_index=0; vm_index<chapters_navigate_treemodel->rowCount(); ++vm_index) {
        auto volume_node = static_cast<ChaptersItem*>(chapters_navigate_treemodel->item(vm_index));
        auto struct_volume_handle = desp_ins->novelTreeNode().childAt(TnType::VOLUME, volume_node->row());

        for (auto chp_index=0; chp_index<volume_node->rowCount(); ++chp_index) {
            auto chapter_node = static_cast<ChaptersItem*>(volume_node->child(chp_index));

            auto pak = all_documents.value(chapter_node);
            // 检测文件是否修改
            if(pak.first->isModified()){
                auto struct_chapter_handle = struct_volume_handle.childAt(TnType::CHAPTER, chapter_node->row());
                desp_ins->resetChapterText(struct_chapter_handle, pak.first->toPlainText());
                pak.first->setModified(false);
            }
        }
    }
}

QString NovelHost::novelTitle() const
{
    return desp_ins->novelTreeNode().title();
}

void NovelHost::resetNovelTitle(const QString &title)
{
    desp_ins->resetTitleOfTreeNode(desp_ins->novelTreeNode(), title);
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
    using TnType = DBAccess::TreeNode::Type;

    auto root = desp_ins->novelTreeNode();
    auto count = desp_ins->childCountOfTreeNode(root, TnType::VOLUME);
    if(before >= count){
        desp_ins->insertChildTreeNodeBefore(root, TnType::VOLUME, count, gName, "无描述");
    }
    else {
        desp_ins->insertChildTreeNodeBefore(root, TnType::VOLUME, before, gName, "无描述");
    }
}

void NovelHost::insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName)
{
    if(!vmIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(vmIndex);
    auto outline_volume_node = static_cast<OutlinesItem*>(node);
    auto root = desp_ins->novelTreeNode();
    auto volume_struct_node = desp_ins->childAtOfTreeNode(root, TnType::VOLUME, node->row());

    int sb_node_count = desp_ins->childCountOfTreeNode(volume_struct_node, TnType::STORYBLOCK);
    if(before >= sb_node_count){
        auto keystory_node = desp_ins->insertChildTreeNodeBefore(volume_struct_node, TnType::STORYBLOCK, sb_node_count, kName, "无描述");
        outline_volume_node->appendRow(new OutlinesItem(keystory_node));
    }
    else{
        auto keystory_node = desp_ins->insertChildTreeNodeBefore(volume_struct_node, TnType::STORYBLOCK, before, kName, "无描述");
        outline_volume_node->insertRow(before, new OutlinesItem(keystory_node));
    }
}

void NovelHost::insertPoint(const QModelIndex &kIndex, int before, const QString &pName)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(kIndex);          // keystory-index
    auto struct_keystory_node = _locate_outline_handle_via(node);

    int points_count = desp_ins->childCountOfTreeNode(struct_keystory_node, TnType::KEYPOINT);
    if(before >= points_count){
        auto point_node = desp_ins->insertChildTreeNodeBefore(struct_keystory_node, TnType::KEYPOINT, points_count, pName, "无描述");
        node->appendRow(new OutlinesItem(point_node));
    }
    else{
        auto point_node = desp_ins->insertChildTreeNodeBefore(struct_keystory_node, TnType::KEYPOINT, before, pName, "无描述");
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

    auto root = desp_ins->novelTreeNode();
    auto struct_volume_node = desp_ins->childAtOfTreeNode(root, TnType::VOLUME, volume_node->row());
    auto struct_storyblock_node = desp_ins->childAtOfTreeNode(struct_volume_node, TnType::STORYBLOCK, storyblock_node->row());

    auto despline_count = desp_ins->childCountOfTreeNode(struct_volume_node, TnType::DESPLINE);
    auto despline = desp_ins->insertChildTreeNodeBefore(struct_volume_node, TnType::DESPLINE, despline_count, fName, "无整体描述");

    auto stop0 = desp_ins->insertAttachPointBefore(despline, 0, false, "描述0", desp);
    desp_ins->resetStoryblockOfAttachPoint(stop0, struct_storyblock_node);
    desp_ins->insertAttachPointBefore(despline, 1, false, "描述1", desp_next);
}

void NovelHost::removeOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("指定modelindex无效");

    auto item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto pnode = item->parent();

    int row = item->row();
    if(!pnode){
        auto root = desp_ins->novelTreeNode();

        auto struct_node = desp_ins->childAtOfTreeNode(root, TnType::VOLUME, row);
        desp_ins->removeTreeNode(struct_node);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);
    }
    else {
        auto handle = _locate_outline_handle_via(item);
        desp_ins->removeTreeNode(handle);

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
    auto keystory_num = desp_ins->childCountOfTreeNode(current_volume_node, TnType::STORYBLOCK);
    for(auto kindex=0; kindex<keystory_num; kindex++){
        auto struct_keystory = desp_ins->childAtOfTreeNode(current_volume_node,TnType::STORYBLOCK, kindex);
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

    DBAccess::TreeNode struct_node;
    switch (treeNodeLevel(chpsIndex)) {
        case 1:
            struct_node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, chpsIndex.row());
            break;
        case 2:
            struct_node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, chpsIndex.parent().row());
            struct_node = desp_ins->childAtOfTreeNode(struct_node, TnType::CHAPTER, chpsIndex.row());
            break;
    }

    _check_remove_effect(struct_node, msgList);
}

void NovelHost::checkForeshadowRemoveEffect(int fsid, QList<QString> &msgList) const
{
    auto struct_node = desp_ins->getTreeNodeViaID(fsid);
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

DBAccess::TreeNode NovelHost:: _locate_outline_handle_via(QStandardItem *outline_item) const
{
    QList<QStandardItem*> stack;
    while (outline_item) {
        stack.insert(0, outline_item);
        outline_item = outline_item->parent();
    }

    auto root = desp_ins->novelTreeNode();
    auto volume_node = desp_ins->childAtOfTreeNode(root, TnType::VOLUME, stack.at(0)->row());
    if(stack.size() == 1){
        return volume_node;
    }

    auto keystory_node = desp_ins->childAtOfTreeNode(volume_node, TnType::STORYBLOCK, stack.at(1)->row());
    if(stack.size() == 2){
        return keystory_node;
    }

    auto point_node = desp_ins->childAtOfTreeNode(keystory_node, TnType::KEYPOINT, stack.at(2)->row());
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
        desp_ins->resetDescriptionOfTreeNode(struct_node, description);
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
    int volume_index = desp_ins->indexOfTreeNode(current_volume_node);
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
    desp_ins->resetDescriptionOfTreeNode(current_chapter_node, content);
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

void NovelHost::sum_foreshadows_under_volume(const DBAccess::TreeNode &volume_node)
{
    foreshadows_under_volume_present->clear();
    foreshadows_under_volume_present->setHorizontalHeaderLabels(
                QStringList() << "伏笔名称" << "吸附？" << "描述1" << "描述2" << "吸附章节" << "剧情起点");

    QList<DBAccess::TreeNode> foreshadows_sum;
    QList<QPair<int,int>> indexes;  // volume-index : despline-index
    // 获取所有伏笔节点
    auto despline_count = desp_ins->childCountOfTreeNode(volume_node, TnType::DESPLINE);
    for (int despindex = 0; despindex < despline_count; ++despindex) {
        auto despline_one = desp_ins->childAtOfTreeNode(volume_node, TnType::DESPLINE, despindex);
        foreshadows_sum << despline_one;
        indexes << qMakePair(desp_ins->indexOfTreeNode(volume_node), despindex);
    }

    // 填充伏笔表格模型数据
    for(int var=0; var<foreshadows_sum.size(); ++var){
        auto foreshadow_one = foreshadows_sum.at(var);
        QList<QStandardItem*> row;
        auto node = new QStandardItem(foreshadow_one.title());
        node->setData(indexes.at(var).first, Qt::UserRole+1);   // volume-index
        node->setData(indexes.at(var).second, Qt::UserRole+2);  // despline-index
        row << node;

        auto pss = desp_ins->getAttachPointsViaDespline(foreshadow_one);
        if(!pss.at(0).attachedChapter().isValid())
            node = new QStandardItem("☁️悬空");
        else
            node = new QStandardItem("📎吸附");
        node->setEditable(false);
        row << node;

        row << new QStandardItem(pss[0].description());
        row << new QStandardItem(pss[1].description());

        node = new QStandardItem(pss.at(0).attachedChapter().isValid()? pss.at(0).attachedChapter().title():"无");
        node->setEditable(false);
        row << node;

        node = new QStandardItem(pss.at(0).attachedStoryblock().title());
        node->setData(pss.at(0).attachedStoryblock().uniqueID());
        row << node;

        foreshadows_under_volume_present->appendRow(row);
    }
}

void NovelHost::listen_foreshadows_volume_changed(QStandardItem *item)
{
    auto item_important = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto volume_index = item_important->data(Qt::UserRole+1).toInt();
    auto despline_index = item_important->data(Qt::UserRole+2).toInt();
    auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume_index);
    auto struct_despline = desp_ins->childAtOfTreeNode(struct_volume, TnType::DESPLINE, despline_index);
    auto stops = desp_ins->getAttachPointsViaDespline(struct_despline);

    switch (item->column()) {
        case 1:
        case 4:
            break;
        case 0:
            desp_ins->resetTitleOfTreeNode(struct_despline, item->text());
            break;
        case 2:
            desp_ins->resetDescriptionOfAttachPoint(stops[0], item->text());
            break;
        case 3:
            desp_ins->resetDescriptionOfAttachPoint(stops[1], item->text());
            break;
        case 5:{
                auto storyblockID = item->data().toInt();
                auto storyblock_node = desp_ins->getTreeNodeViaID(storyblockID);
                desp_ins->resetStoryblockOfAttachPoint(stops[0], storyblock_node);
            }
            break;
    }
}

void NovelHost::sum_foreshadows_until_volume_remains(const DBAccess::TreeNode &volume_node)
{
    foreshadows_until_volume_remain_present->clear();
    foreshadows_until_volume_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"闭合章节"<<"剧情源"<<"卷宗名");

    QList<DBAccess::TreeNode> desplinelist;
    // 累积所有伏笔【故事线】
    auto volume_index = desp_ins->indexOfTreeNode(volume_node);
    for (int var = 0; var <= volume_index; ++var) {
        auto xvolume_node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, var);

        auto despline_count = desp_ins->childCountOfTreeNode(xvolume_node, TnType::DESPLINE);
        for (int var = 0; var < despline_count; ++var) {
            desplinelist << desp_ins->childAtOfTreeNode(xvolume_node, TnType::DESPLINE, var);
        }
    }

    // 清洗故事线
    for (int var = 0; var < desplinelist.size(); ++var) {
        auto desp_node = desplinelist.at(var);
        auto points = desp_ins->getAttachPointsViaDespline(desp_node);
        // 悬空移除
        if(!points[0].attachedChapter().isValid()){
            desplinelist.removeAt(var);
            var--;
            continue;
        }

        // 保留敞开故事线【伏笔】
        if(!points[1].attachedChapter().isValid())
            continue;

        // 本卷前闭合伏笔【故事线】移除
        if(points[1].attachedChapter().parent().index() < volume_node.index()){
            desplinelist.removeAt(var);
            var--;
        }
    }

    // 未关闭伏笔填充模型
    for (int open_index = 0; open_index < desplinelist.size(); ++open_index) {
        auto despline_one = desplinelist.at(open_index);

        QList<QStandardItem*> row;
        auto item = new QStandardItem(despline_one.title());
        item->setData(volume_index, Qt::UserRole+1);
        item->setData(desp_ins->indexOfTreeNode(despline_one), Qt::UserRole+2);
        row << item;

        auto stops = desp_ins->getAttachPointsViaDespline(despline_one);
        if(stops[1].attachedChapter().isValid())
            item = new QStandardItem("🔒闭合");
        else
            item = new QStandardItem("✅开启");
        item->setEditable(false);
        row << item;

        row << new QStandardItem(stops[0].description());
        row << new QStandardItem(stops[1].description());

        item = new QStandardItem(stops[1].attachedChapter().isValid()?stops[1].attachedChapter().title():"无");
        item->setEditable(false);
        row << item;

        item = new QStandardItem(stops[0].attachedStoryblock().title());
        item->setEditable(false);
        row << item;

        item = new QStandardItem(desp_ins->parentOfTreeNode(despline_one).title());
        item->setEditable(false);
        row << item;

        foreshadows_until_volume_remain_present->appendRow(row);
    }
}

void NovelHost::listen_foreshadows_until_volume_changed(QStandardItem *item)
{
    auto important_item = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto volume_index = important_item->data(Qt::UserRole+1).toInt();
    auto despline_index = important_item->data(Qt::UserRole+2).toInt();

    auto tvolume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume_index);
    auto tdespline = desp_ins->childAtOfTreeNode(tvolume, TnType::DESPLINE, despline_index);
    auto stops = desp_ins->getAttachPointsViaDespline(tdespline);

    switch (item->column()) {
        case 1:
        case 4:
        case 5:
        case 6:
            break;
        case 0:
            desp_ins->resetTitleOfTreeNode(tdespline, item->text());
            break;
        case 2:
            desp_ins->resetDescriptionOfAttachPoint(stops[0], item->text());
            break;
        case 3:
            desp_ins->resetDescriptionOfAttachPoint(stops[1], item->text());
            break;
    }
}

void NovelHost::sum_foreshadows_until_chapter_remains(const DBAccess::TreeNode &chapter_node)
{
    // 累积所有打开伏笔
    // 累积本章节前关闭伏笔
    foreshadows_until_chapter_remain_present->clear();
    foreshadows_until_chapter_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"起始章节"<<"剧情源"<<"卷宗名");

    auto chpindex = chapter_node.index();
    auto vmindex = chapter_node.parent().index();
    auto item = chapters_navigate_treemodel->item(vmindex)->child(chpindex);

    QList<QPair<QString,int>> foreshadows_list;
    sumForeshadowsOpeningUntilChapter(item->index(), foreshadows_list);

    for (auto fs_href : foreshadows_list) {
        auto foreshadow_one = desp_ins->getTreeNodeViaID(fs_href.second);

        QList<QStandardItem*> row;
        auto node = new QStandardItem(foreshadow_one.title());
        node->setData(foreshadow_one.parent().index(), Qt::UserRole+1); // volume-index
        node->setData(foreshadow_one.index(), Qt::UserRole+2);          // foreshadow-index
        row << node;

        auto points = desp_ins->getAttachPointsViaDespline(foreshadow_one);
        if(points[1].attachedChapter().isValid())
            node = new QStandardItem("🔒闭合");
        else
            node = new QStandardItem("✅开启");
        node->setEditable(false);
        row << node;

        row << new QStandardItem(points[0].description());
        row << new QStandardItem(points[1].description());

        row << new QStandardItem(points[0].attachedChapter().title());
        row.last()->setEditable(false);

        row << new QStandardItem(points[0].attachedStoryblock().title());
        row.last()->setEditable(false);

        row << new QStandardItem(points[0].attachedChapter().parent().title());
        row.last()->setEditable(false);

        foreshadows_until_chapter_remain_present->appendRow(row);
    }
}

void NovelHost::listen_foreshadows_until_chapter_changed(QStandardItem *item)
{
    auto important = item->model()->itemFromIndex(item->index().sibling(item->row(), 0));
    auto volume_index = important->data(Qt::UserRole+1).toInt();
    auto foreshadow_index = important->data(Qt::UserRole+2).toInt();

    auto foreshadow_one = desp_ins->novelTreeNode().childAt(TnType::VOLUME, volume_index)
                          .childAt(TnType::DESPLINE, foreshadow_index);

    auto points = desp_ins->getAttachPointsViaDespline(foreshadow_one);
    switch (item->column()) {
        case 1:
        case 4:
        case 5:
        case 6:
            break;
        case 0:
            desp_ins->resetTitleOfTreeNode(foreshadow_one, item->text());
            break;
        case 2:
            desp_ins->resetDescriptionOfAttachPoint(points[0], item->text());
            break;
        case 3:
            desp_ins->resetDescriptionOfAttachPoint(points[1], item->text());
            break;
    }
}


// msgList : [type](target)<keys-to-target>msg-body
void NovelHost::_check_remove_effect(const DBAccess::TreeNode &target, QList<QString> &msgList) const
{
    if(target.type() == TnType::KEYPOINT)
        return;

    if(target.type() == TnType::DESPLINE) {
        msgList << "[warring](foreshadow·despline)<"+target.title()+">指定伏笔[故事线]将被删除，请注意！";

        auto stopnodes = desp_ins->getAttachPointsViaDespline(target);
        for (auto dot : stopnodes) {
            auto storyblk = dot.attachedStoryblock();
            auto chapter = dot.attachedChapter();
            msgList << "[error](keystory·storyblock)<"+storyblk.title()+">影响关键剧情！请重写相关内容！";
            msgList << "[error](chapter)<"+chapter.title()+">影响章节内容！请重写相关内容！";
        }
        return;
    }

    if(target.type() == TnType::STORYBLOCK){
        auto points = desp_ins->getAttachPointsViaStoryblock(target);
        for (auto dot : points) {
            auto foreshadownode = dot.attachedDespline();
            auto chapternode = dot.attachedChapter();
            msgList << "[error](foreshadow·despline)<"+foreshadownode.title()+">影响指定伏笔[故事线]，请注意修改描述！";
            msgList << "[error](chapter)<"+chapternode.title()+">影响章节内容！请重写相关内容！";
        }
    }

    if(target.type() == TnType::VOLUME){
        auto despline_count = desp_ins->childCountOfTreeNode(target, TnType::DESPLINE);
        for (int var = 0; var < despline_count; ++var) {
            _check_remove_effect(desp_ins->childAtOfTreeNode(target, TnType::DESPLINE, var), msgList);
        }

        auto storyblock_count = desp_ins->childCountOfTreeNode(target, TnType::STORYBLOCK);
        for (int var = 0; var < storyblock_count; ++var) {
            _check_remove_effect(desp_ins->childAtOfTreeNode(target, TnType::STORYBLOCK, var), msgList);
        }

        auto chapter_count = desp_ins->childCountOfTreeNode(target, TnType::CHAPTER);
        for (int var = 0; var < chapter_count; ++var) {
            _check_remove_effect(desp_ins->childAtOfTreeNode(target, TnType::CHAPTER, var), msgList);
        }
    }

    if(target.type() == TnType::CHAPTER){
        auto points = desp_ins->getAttachPointsViaChapter(target);
        for (auto dot : points) {
            auto foreshadownode = dot.attachedDespline();
            auto storyblknode = dot.attachedStoryblock();
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
    auto volume_symbo = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, parent->row());
    auto chapter_symbo = desp_ins->childAtOfTreeNode(volume_symbo, TnType::CHAPTER, item->row());
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

    auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, item->row());
    auto count = desp_ins->childCountOfTreeNode(struct_volume, TnType::CHAPTER);

    QList<QStandardItem*> row;
    if(before >= count){
        auto newnode = desp_ins->insertChildTreeNodeBefore(struct_volume, TnType::CHAPTER, count, chpName, "无章节描述");
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        item->appendRow(row);
    }
    else {
        auto newnode = desp_ins->insertChildTreeNodeBefore(struct_volume, TnType::CHAPTER, before, chpName, "无章节描述");
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        item->insertRow(before, row);
    }
}

void NovelHost::appendShadowstart(const QModelIndex &chpIndex, int desplineID)
{
    if(!chpIndex.isValid())
        throw new WsException("传入的章节index非法");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chpIndex);        // 章节节点
    auto volume = chapter->parent();                                            // 卷宗节点

    auto struct_volume_node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume->row());
    auto struct_chapter_node = desp_ins->childAtOfTreeNode(struct_volume_node, TnType::CHAPTER, chapter->row());

    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(despline.type() != TnType::DESPLINE)
        throw new WsException("指定despline节点id或者storyblock节点ID非法");

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    desp_ins->resetChapterOfAttachPoint(points[0], struct_chapter_node);
}

void NovelHost::removeShadowstart(int desplineID)
{
    auto despline_node = desp_ins->getTreeNodeViaID(desplineID);
    auto attached = desp_ins->getAttachPointsViaDespline(despline_node);
    desp_ins->resetChapterOfAttachPoint(attached[0], DBAccess::TreeNode());
}

void NovelHost::appendShadowstop(const QModelIndex &chpIndex, int desplineID)
{
    if(!chpIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chpIndex);
    auto volume_ = chapter->parent();                                       // 卷宗节点

    auto struct_volume_node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume_->row());
    auto struct_chapter_node = desp_ins->childAtOfTreeNode(struct_volume_node, TnType::CHAPTER, chapter->row());
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    auto attached = desp_ins->getAttachPointsViaDespline(despline);

    desp_ins->resetChapterOfAttachPoint(attached[1], struct_chapter_node);
}

void NovelHost::removeShadowstop(int desplineID)
{
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    auto attached = desp_ins->getAttachPointsViaDespline(despline);
    desp_ins->resetChapterOfAttachPoint(attached[1], DBAccess::TreeNode());
}

void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("chaptersNodeIndex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    int row = chapter->row();
    // 卷宗节点管理同步
    if(!chapter->parent()){
        auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, row);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        desp_ins->removeTreeNode(struct_volume);
    }
    // 章节节点
    else {
        auto volume = chapter->parent();
        auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume->row());
        auto struct_chapter = desp_ins->childAtOfTreeNode(struct_volume, TnType::CHAPTER, row);
        volume->removeRow(row);

        desp_ins->removeTreeNode(struct_chapter);
    }
}

void NovelHost::removeForeshadowNode(int desplineID)
{
    auto node = desp_ins->getTreeNodeViaID(desplineID);
    if(node.type() != TnType::DESPLINE)
        throw new WsException("指定传入id不属于伏笔[故事线]");
    desp_ins->removeTreeNode(node);
}

void NovelHost::setCurrentChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("传入的chaptersindex无效");

    DBAccess::TreeNode node;
    switch (treeNodeLevel(chaptersNode)) {
        case 1: // 卷宗
            node = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, chaptersNode.row());
            break;
        case 2: // 章节
            auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, chaptersNode.parent().row());
            node = desp_ins->childAtOfTreeNode(struct_volume, TnType::CHAPTER, chaptersNode.row());
            break;
    }

    set_current_volume_outlines(node);
    emit currentVolumeActived();

    // 统计本卷宗下所有构建伏笔及其状态  名称，吸附状态，前描述，后描述，吸附章节、源剧情
    sum_foreshadows_under_volume(current_volume_node);
    sum_foreshadows_until_volume_remains(current_volume_node);

    if(node.type() != TnType::CHAPTER)
        return;

    current_chapter_node = node;
    emit currentChaptersActived();

    // 统计至此章节前未闭合伏笔及本章闭合状态  名称、闭合状态、前描述、后描述、闭合章节、源剧情、源卷宗
    sum_foreshadows_until_chapter_remains(node);
    disconnect(chapter_outlines_present,    &QTextDocument::contentsChanged,
               this,   &NovelHost::listen_chapter_outlines_description_change);
    chapter_outlines_present->clear();
    QTextBlockFormat blockformat0;
    QTextCharFormat charformat0;
    config_host.textFormat(blockformat0, charformat0);
    QTextCursor cursor0(chapter_outlines_present);
    cursor0.setBlockFormat(blockformat0);
    cursor0.setBlockCharFormat(charformat0);
    cursor0.insertText(node.description());
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

    emit documentPrepared(pack.first, node.title());
}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_treemodel->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_treemodel->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
    }
}

DBAccess::TreeNode NovelHost::sumForeshadowsUnderVolumeAll(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    QModelIndex volume_index = chpsNode;
    auto level = treeNodeLevel(chpsNode);
    if(level==2)
        volume_index = chpsNode.parent();

    auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, volume_index.row());
    auto despline_count = desp_ins->childCountOfTreeNode(struct_volume, TnType::DESPLINE);
    for (int var = 0; var < despline_count; ++var) {
        auto despline_node = desp_ins->childAtOfTreeNode(struct_volume, TnType::DESPLINE, var);
        auto attached = desp_ins->getAttachPointsViaDespline(despline_node);

        foreshadows << qMakePair(QString("%1[%2]").arg(despline_node.title()).arg(attached[0].title()), despline_node.uniqueID());
    }

    return struct_volume;
}

void NovelHost::sumForeshadowsUnderVolumeHanging(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    // 汇总所有伏笔
    sumForeshadowsUnderVolumeAll(chpsNode, foreshadows);

    // 清洗所有吸附伏笔信息
    for (int index = 0; index < foreshadows.size(); ++index) {
        auto despline_href = foreshadows.at(index);
        auto despline_one = desp_ins->getTreeNodeViaID(despline_href.second);
        auto attached = desp_ins->getAttachPointsViaDespline(despline_one);

        if(attached[0].attachedChapter().isValid()){
            foreshadows.removeAt(index);
            index--;
        }
    }
}

void NovelHost::sumForeshadowsAbsorbedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto struct_volume = desp_ins->childAtOfTreeNode(desp_ins->novelTreeNode(), TnType::VOLUME, chpsNode.parent().row());
    auto struct_chapter = desp_ins->childAtOfTreeNode(struct_volume, TnType::CHAPTER, chpsNode.row());
    auto attached = desp_ins->getAttachPointsViaChapter(struct_chapter);
    for (int var = 0; var < attached.size(); ++var) {
        if(attached[var].index() != 0){
            attached.removeAt(var);
            var--;
        }
    }

    for (int var = 0; var < attached.size(); ++var) {
        foreshadows << qMakePair(QString("%1[%2]").arg(attached[var].attachedDespline().title())
                                 .arg(attached[var].attachedStoryblock().title()), attached[var].attachedDespline().uniqueID());
    }
}

void NovelHost::sumForeshadowsOpeningUntilChapter(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto dbvolume_node = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chpsNode.parent().row());
    auto dbchapter_node = dbvolume_node.childAt(TnType::CHAPTER, chpsNode.row());

    QList<DBAccess::TreeNode> despline_list;
    // 累积前文所有故事线
    for (int var = 0; var < dbvolume_node.index(); ++var) {
        auto tmvmnode = desp_ins->novelTreeNode().childAt(TnType::VOLUME, var);
        auto despcount = tmvmnode.childCount(TnType::DESPLINE);
        for (int xvar = 0; xvar < despcount; ++xvar) {
            despline_list << tmvmnode.childAt(TnType::DESPLINE, xvar);
        }
    }
    // 累积本卷故事线
    for(int chpindex=0; chpindex<=dbchapter_node.index(); ++chpindex){
        auto chpone = dbvolume_node.childAt(TnType::CHAPTER, chpindex);
        auto stops = desp_ins->getAttachPointsViaChapter(chpone);
        for (auto one : stops) {
            if(despline_list.contains(one.attachedDespline()))
                continue;
            despline_list << one.attachedDespline();
        }
    }

    // 清洗故事线【伏笔】
    for(int index=0; index<despline_list.size(); ++index){
        auto despline_one = despline_list[index];
        auto stops = desp_ins->getAttachPointsViaDespline(despline_one);
        // 悬空伏笔移除，保证都吸附
        if(!stops[0].attachedChapter().isValid()){
            despline_list.removeAt(index);
            index--;
            continue;
        }
        // 敞开伏笔保留
        if(!stops[1].attachedChapter().isValid())
            continue;

        // 移除本卷前闭合伏笔
        if(stops[1].attachedChapter().parent().index() < dbvolume_node.index()){
            despline_list.removeAt(index);
            index--;
            continue;
        }
        if(stops[1].attachedChapter().index() < dbchapter_node.index()){
            despline_list.removeAt(index);
            index--;
            continue;
        }
    }

    for (auto one : despline_list) {
        foreshadows << qMakePair(QString("%1[%2]").arg(one.title()).
                                 arg(desp_ins->getAttachPointsViaDespline(one)[0].attachedStoryblock().title()),
                one.uniqueID());
    }
}

void NovelHost::sumForeshadowsClosedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString, int> > &foreshadows) const
{
    auto level = treeNodeLevel(chpsNode);
    if(level != 2)
        throw new WsException("传入节点类别错误");

    auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chpsNode.parent().row());
    auto struct_chapter = struct_volume.childAt(TnType::CHAPTER, chpsNode.row());
    auto points = desp_ins->getAttachPointsViaChapter(struct_chapter);
    for(int index=0; index<points.size(); ++index){
        if(points[index].index() != 1){
            points.removeAt(index);
            index--;
        }
    }

    for (auto one : points) {
        foreshadows << qMakePair(QString("%1[%2]").arg(one.attachedDespline().title())
                                 .arg(one.attachedStoryblock().title()), one.attachedDespline().uniqueID());
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
    desp_ins->resetTitleOfTreeNode(struct_node, item->text());
}

void NovelHost::chapters_node_title_changed(QStandardItem *item){
    if(item->parent() && !item->column() )  // chapter-node 而且 不是计数节点
    {
        auto root = desp_ins->novelTreeNode();
        auto volume_struct = desp_ins->childAtOfTreeNode(root, TnType::VOLUME, item->parent()->row());
        auto struct_chapter = desp_ins->childAtOfTreeNode(volume_struct, TnType::CHAPTER, item->row());
        desp_ins->resetTitleOfTreeNode(struct_chapter, item->text());
    }
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const DBAccess::TreeNode &volume_handle, int index)
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
    desp_ins->resetDescriptionOfTreeNode(desp_ins->novelTreeNode(), content);
}

// 向卷宗细纲填充内容
void NovelHost::set_current_volume_outlines(const DBAccess::TreeNode &node_under_volume){
    if(!node_under_volume.isValid())
        throw new WsException("传入节点无效");

    if(node_under_volume.type() == TnType::VOLUME){
        current_volume_node = node_under_volume;

        disconnect(volume_outlines_present,  &QTextDocument::contentsChange,
                   this,   &NovelHost::listen_volume_outlines_description_change);
        disconnect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                   this,    &NovelHost::listen_volume_outlines_structure_changed);

        volume_outlines_present->clear();
        QTextCursor cursor(volume_outlines_present);

        int volume_index = desp_ins->indexOfTreeNode(node_under_volume);
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

    auto node = desp_ins->parentOfTreeNode(node_under_volume);
    set_current_volume_outlines(node);
}

ChaptersItem::ChaptersItem(NovelHost &host, const DBAccess::TreeNode &refer, bool isGroup)
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
    auto parent = QStandardItem::parent();

    if(!parent){    // 卷宗节点
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

OutlinesItem::OutlinesItem(const DBAccess::TreeNode &refer)
{
    setText(refer.title());
    switch (refer.type()) {
        case DBAccess::TreeNode::Type::KEYPOINT:
            setIcon(QIcon(":/outlines/icon/点.png"));
            break;
        case DBAccess::TreeNode::Type::VOLUME:
            setIcon(QIcon(":/outlines/icon/卷.png"));
            break;
        case DBAccess::TreeNode::Type::CHAPTER:
            setIcon(QIcon(":/outlines/icon/章.png"));
            break;
        case DBAccess::TreeNode::Type::STORYBLOCK:
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
