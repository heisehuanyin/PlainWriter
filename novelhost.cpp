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
      desplines_fuse_source_model(new QStandardItemModel(this)),
      desplines_filter_under_volume(new DesplineFilterModel(DesplineFilterModel::Type::UNDERVOLUME, this)),
      desplines_filter_until_volume_remain(new DesplineFilterModel(DesplineFilterModel::Type::UNTILWITHVOLUME, this)),
      desplines_filter_until_chapter_remain(new DesplineFilterModel(DesplineFilterModel::Type::UNTILWITHCHAPTER, this)),
      find_results_model(new QStandardItemModel(this)),
      chapters_navigate_treemodel(new QStandardItemModel(this)),
      chapter_outlines_present(new QTextDocument(this))
{
    new OutlinesRender(volume_outlines_present, config);

    connect(outline_navigate_treemodel, &QStandardItemModel::itemChanged,
            this,   &NovelHost::outlines_node_title_changed);
    connect(chapters_navigate_treemodel,&QStandardItemModel::itemChanged,
            this,   &NovelHost::chapters_node_title_changed);

    desplines_filter_under_volume->setSourceModel(desplines_fuse_source_model);
    desplines_filter_until_volume_remain->setSourceModel(desplines_fuse_source_model);
    desplines_filter_until_chapter_remain->setSourceModel(desplines_fuse_source_model);

    connect(desplines_fuse_source_model,    &QStandardItemModel::itemChanged,
            this,                           &NovelHost::_listen_basic_datamodel_changed);
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
                                                            root.childCount(DBAccess::TreeNode::Type::VOLUME),
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
                                                                     dbvnode.childCount(DBAccess::TreeNode::Type::DESPLINE),
                                                                     foreshadownode.attr("title"), "无整体描述");

                    auto headnode = dbtool.insertAttachPointBefore(dbfsnode, 0, "阶段0", foreshadownode.attr("desp"));
                    dbtool.resetStoryblockOfAttachPoint(headnode, dbkstorynode);
                    dbtool.insertAttachPointBefore(dbfsnode, 1, "阶段1", foreshadownode.attr("desp_next"));
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
                auto dbdespline_node = dbvolume_node.childAt(TnType::DESPLINE, index_acc);
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

                auto dbvolume_node = dbtool.novelTreeNode().childAt(TnType::VOLUME, volume_index);
                auto dbchapter_node = dbvolume_node.childAt(TnType::CHAPTER, chapter_index);
                auto dbdespline_node = dbvolume_node.childAt(TnType::DESPLINE, index_acc);
                auto points = dbtool.getAttachPointsViaDespline(dbdespline_node);
                dbtool.resetChapterOfAttachPoint(points[1], dbchapter_node);
            }
            firstchapter_node = desp_tree->nextChapterOfFStruct(firstchapter_node);
        }

        delete desp_tree;
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

    auto volume_num = novel_node.childCount(TnType::VOLUME);
    for (int volume_index = 0; volume_index < volume_num; ++volume_index) {
        DBAccess::TreeNode volume_node = novel_node.childAt(TnType::VOLUME, volume_index);

        // 在chapters-tree和outline-tree上插入卷节点
        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int storyblock_count = volume_node.childCount(TnType::STORYBLOCK);
        for (int storyblock_index = 0; storyblock_index < storyblock_count; ++storyblock_index) {
            DBAccess::TreeNode storyblock_node = volume_node.childAt(TnType::STORYBLOCK, storyblock_index);

            // outline-tree上插入故事节点
            auto ol_keystory_item = new OutlinesItem(storyblock_node);
            outline_volume_node->appendRow(ol_keystory_item);

            // outline-tree上插入point节点
            int points_count = storyblock_node.childCount(TnType::KEYPOINT);
            for (int points_index = 0; points_index < points_count; ++points_index) {
                DBAccess::TreeNode point_node = storyblock_node.childAt(TnType::KEYPOINT, points_index);

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }
        }

        // chapters上插入chapter节点
        int chapter_count = volume_node.childCount(TnType::CHAPTER);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto chapter_node = volume_node.childAt(TnType::CHAPTER, chapter_index);

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
    refreshDesplinesSummary();
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

int NovelHost::indexDepth(const QModelIndex &node) const
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

QAbstractItemModel *NovelHost::desplinesUnderVolume() const
{
    return desplines_filter_under_volume;
}

QAbstractItemModel *NovelHost::desplinesUntilVolumeRemain() const
{
    return desplines_filter_until_volume_remain;
}

QAbstractItemModel *NovelHost::desplinesUntilChapterRemain() const
{
    return desplines_filter_until_chapter_remain;
}


void NovelHost::insertVolume(const QString &name, const QString &description, int index)
{
    auto root = desp_ins->novelTreeNode();
    auto count = root.childCount(TnType::VOLUME);
    QList<QStandardItem*> row;
    row << new QStandardItem(name);
    if(index < 0 || index >= count){
        auto vnode = desp_ins->insertChildTreeNodeBefore(root, TnType::VOLUME, count, name, description);
        insert_volume(vnode, count);
    }
    else {
        auto vnode = desp_ins->insertChildTreeNodeBefore(root, TnType::VOLUME, index, name, description);
        insert_volume(vnode, index);
    }
}

void NovelHost::insertStoryblock(const QModelIndex &pIndex, const QString &name, const QString &description, int index)
{
    if(!pIndex.isValid())
        throw new WsException("输入modelindex无效");
    if(indexDepth(pIndex)!=1)
        throw new WsException("输入节点索引类型错误");

    QStandardItem *item = outline_navigate_treemodel->item(pIndex.row());
    auto root = desp_ins->novelTreeNode();
    auto volume_struct_node = root.childAt(TnType::VOLUME, item->row());

    int sb_node_count = volume_struct_node.childCount(TnType::STORYBLOCK);
    if(index < 0 || index >= sb_node_count){
        auto keystory_node = desp_ins->insertChildTreeNodeBefore(volume_struct_node, TnType::STORYBLOCK,
                                                                 sb_node_count, name, description);
        item->appendRow(new OutlinesItem(keystory_node));
    }
    else{
        auto keystory_node = desp_ins->insertChildTreeNodeBefore(volume_struct_node, TnType::STORYBLOCK,
                                                                 index, name, description);
        item->insertRow(index, new OutlinesItem(keystory_node));
    }
}

void NovelHost::insertKeypoint(const QModelIndex &pIndex, const QString &name, const QString description, int index)
{
    if(!pIndex.isValid() || pIndex.model() != outline_navigate_treemodel)
        throw new WsException("输入modelindex无效");
    if(indexDepth(pIndex) != 2)
        throw new WsException("输入index类型错误");

    auto node = outline_navigate_treemodel->itemFromIndex(pIndex);          // keystory-index
    auto struct_storyblock_node = _locate_outline_handle_via(node);

    int points_count = struct_storyblock_node.childCount(TnType::KEYPOINT);
    if(index<0 || index >= points_count){
        auto point_node = desp_ins->insertChildTreeNodeBefore(struct_storyblock_node, TnType::KEYPOINT, points_count, name, description);
        node->appendRow(new OutlinesItem(point_node));
    }
    else{
        auto point_node = desp_ins->insertChildTreeNodeBefore(struct_storyblock_node, TnType::KEYPOINT, index, name, description);
        node->insertRow(index, new OutlinesItem(point_node));
    }
}

void NovelHost::appendDespline(const QModelIndex &pIndex, const QString &name, const QString &description)
{
    if(!pIndex.isValid())
        throw new WsException("指定index无效");
    if(indexDepth(pIndex) != 1)
        throw new WsException("输入index类型错误");

    auto root = desp_ins->novelTreeNode();
    auto struct_volume_node = root.childAt(TnType::VOLUME, pIndex.row());

    auto despline_count = struct_volume_node.childCount(TnType::DESPLINE);
    desp_ins->insertChildTreeNodeBefore(struct_volume_node, TnType::DESPLINE, despline_count, name, description);
}

void NovelHost::appendDesplineUnderCurrentVolume(const QString &name, const QString &description)
{
    if(!current_volume_node.isValid())
        throw new WsException("current-volume未指定");

    auto despline_count = current_volume_node.childCount(TnType::DESPLINE);
    desp_ins->insertChildTreeNodeBefore(current_volume_node, TnType::DESPLINE, despline_count, name, description);
}

void NovelHost::removeDespline(int desplineID)
{
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("传入节点ID无效");

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    if(points.size())
        throw new WsException("目标支线非悬空支线，无法删除！");

    desp_ins->removeTreeNode(despline);
}

void NovelHost::removeOutlinesNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid() || outlineNode.model() != outline_navigate_treemodel)
        throw new WsException("指定modelindex无效");

    auto item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    int row = item->row();

    if(indexDepth(outlineNode) == 1){
        auto root = desp_ins->novelTreeNode();
        auto struct_node = root.childAt(TnType::VOLUME, row);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        desp_ins->removeTreeNode(struct_node);
    }
    else {
        auto handle = _locate_outline_handle_via(item);
        item->parent()->removeRow(row);

        desp_ins->removeTreeNode(handle);
    }
}

void NovelHost::setCurrentOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid() || outlineNode.model() != outline_navigate_treemodel)
        throw new WsException("传入的outlinemodelindex无效");

    auto current = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto struct_one = _locate_outline_handle_via(current);

    // 设置当前卷节点，填充卷细纲内容
    set_current_volume_outlines(struct_one);

    desplines_filter_under_volume->setFilterBase(current_volume_node);
    desplines_filter_until_volume_remain->setFilterBase(current_volume_node);
}

void NovelHost::allStoryblocksUnderCurrentVolume(QList<QPair<QString,int>> &keystories) const
{
    auto keystory_num = current_volume_node.childCount(TnType::STORYBLOCK);
    for(auto kindex=0; kindex<keystory_num; kindex++){
        auto struct_keystory = desp_ins->childAtOfTreeNode(current_volume_node,TnType::STORYBLOCK, kindex);
        keystories << qMakePair(struct_keystory.title(), struct_keystory.uniqueID());
    }
}

QList<QPair<QString, QModelIndex>> NovelHost::sumStoryblockIndexViaChapters(const QModelIndex &chaptersNode) const
{
    if(!chaptersNode.isValid())
        return QList<QPair<QString, QModelIndex>>();

    QList<QPair<QString, QModelIndex>> hash;
    auto item = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    auto volume = item;
    if(indexDepth(chaptersNode) == 2)
        volume = item->parent();

    auto outlines_volume_item = outline_navigate_treemodel->item(volume->row());
    for (int var = 0; var < outlines_volume_item->rowCount(); ++var) {
        auto one_item = outlines_volume_item->child(var);
        hash << qMakePair(one_item->text(), one_item->index());
    }
    return hash;
}

QList<QPair<QString, QModelIndex> > NovelHost::sumStoryblockIndexViaOutlines(const QModelIndex &outlinesNode) const
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
    if(!chpsIndex.isValid() || chpsIndex.model()!=chapters_navigate_treemodel)
        throw new WsException("指定index无效");

    DBAccess::TreeNode struct_node;
    switch (indexDepth(chpsIndex)) {
        case 1:
            struct_node = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chpsIndex.row());
            break;
        case 2:
            struct_node = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chpsIndex.parent().row());
            struct_node = struct_node.childAt(TnType::CHAPTER, chpsIndex.row());
            break;
    }

    _check_remove_effect(struct_node, msgList);
}

void NovelHost::checkDesplineRemoveEffect(int fsid, QList<QString> &msgList) const
{
    auto struct_node = desp_ins->getTreeNodeViaID(fsid);
    if(struct_node.type() != TnType::DESPLINE)
        throw new WsException("传入的ID不属于伏笔[故事线]");

    _check_remove_effect(struct_node, msgList);
}

void NovelHost::checkOutlinesRemoveEffect(const QModelIndex &outlinesIndex, QList<QString> &msgList) const
{
    if(!outlinesIndex.isValid() || outlinesIndex.model() != outline_navigate_treemodel)
        throw new WsException("指定index无效");

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
    auto volume_node = root.childAt(TnType::VOLUME, stack.at(0)->row());
    if(stack.size() == 1){
        return volume_node;
    }

    auto keystory_node = volume_node.childAt(TnType::STORYBLOCK, stack.at(1)->row());
    if(stack.size() == 2){
        return keystory_node;
    }

    auto point_node = keystory_node.childAt(TnType::KEYPOINT, stack.at(2)->row());
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
    int volume_index = current_volume_node.index();
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
        auto despline_count = target.childCount(TnType::DESPLINE);
        for (int var = 0; var < despline_count; ++var) {
            _check_remove_effect(target.childAt(TnType::DESPLINE,var), msgList);
        }

        auto storyblock_count = target.childCount(TnType::STORYBLOCK);
        for (int var = 0; var < storyblock_count; ++var) {
            _check_remove_effect(target.childAt(TnType::STORYBLOCK, var), msgList);
        }

        auto chapter_count = target.childCount(TnType::CHAPTER);
        for (int var = 0; var < chapter_count; ++var) {
            _check_remove_effect(target.childAt(TnType::CHAPTER, var), msgList);
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
    auto volume_symbo = desp_ins->novelTreeNode().childAt(TnType::VOLUME, parent->row());
    auto chapter_symbo = volume_symbo.childAt(TnType::CHAPTER, item->row());
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

QStandardItemModel *NovelHost::findResultTable() const
{
    return find_results_model;
}

QTextDocument *NovelHost::chapterOutlinePresent() const
{
    return chapter_outlines_present;
}

void NovelHost::insertChapter(const QModelIndex &pIndex, const QString &name, const QString &description, int index)
{
    if(!pIndex.isValid())
        throw new WsException("输入index无效");
    if(indexDepth(pIndex) != 1)
        throw new WsException("输入index类型错误");

    auto volume_item = chapters_navigate_treemodel->item(pIndex.row());
    auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, volume_item->row());
    auto count = struct_volume.childCount(TnType::CHAPTER);

    if(index < 0 || index >= count){
        QList<QStandardItem*> row;
        auto newnode = desp_ins->insertChildTreeNodeBefore(struct_volume, TnType::CHAPTER, count, name, description);
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        volume_item->appendRow(row);
    }
    else {
        QList<QStandardItem*> row;
        auto newnode = desp_ins->insertChildTreeNodeBefore(struct_volume, TnType::CHAPTER, index, name, description);
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        volume_item->insertRow(index, row);
    }
}

void NovelHost::insertAttachpoint(int desplineID, const QString &title, const QString &desp, int index)
{
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(despline.type() != TnType::DESPLINE)
        throw new WsException("指定despline节点id非法");

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    if(index > -1 && index < points.size())
        desp_ins->insertAttachPointBefore(despline, index, title, desp);
    else
        desp_ins->insertAttachPointBefore(despline, points.size(), title, desp);
}

void NovelHost::removeAttachpoint(int attachpointID)
{
    auto point = desp_ins->getAttachPointViaID(attachpointID);
    if(point.attachedChapter().isValid() || point.attachedStoryblock().isValid())
        throw new WsException("目标驻点非悬空驻点，不可删除！");
    desp_ins->removeAttachPoint(point);
}

void NovelHost::attachPointMoveup(int pointID) const
{
    auto point = desp_ins->getAttachPointViaID(pointID);
    if(point.index()==0)
        return;

    auto despline = point.attachedDespline();
    auto node = desp_ins->insertAttachPointBefore(despline, point.index()-1, point.title(), point.description());
    desp_ins->resetChapterOfAttachPoint(node, point.attachedChapter());
    desp_ins->resetStoryblockOfAttachPoint(node, point.attachedStoryblock());

    desp_ins->removeAttachPoint(point);
}

void NovelHost::attachPointMovedown(int pointID) const
{
    auto point = desp_ins->getAttachPointViaID(pointID);
    auto despline = point.attachedDespline();
    auto points = desp_ins->getAttachPointsViaDespline(despline);

    if(point == points.last())
        return;

    auto node = desp_ins->insertAttachPointBefore(despline, point.index()+2, point.title(), point.description());
    desp_ins->resetChapterOfAttachPoint(node, point.attachedChapter());
    desp_ins->resetStoryblockOfAttachPoint(node, point.attachedStoryblock());

    desp_ins->removeAttachPoint(point);
}


void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid() || chaptersNode.model() != chapters_navigate_treemodel)
        throw new WsException("chaptersNodeIndex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    int row = chapter->row();
    // 卷宗节点管理同步
    if(indexDepth(chaptersNode)==1){
        auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, row);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        desp_ins->removeTreeNode(struct_volume);
    }
    // 章节节点
    else {
        auto volume = chapter->parent();
        auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, volume->row());
        auto struct_chapter = struct_volume.childAt(TnType::CHAPTER, row);
        volume->removeRow(row);

        desp_ins->removeTreeNode(struct_chapter);
    }
}

void NovelHost::set_current_chapter_content(const QModelIndex &chaptersNode, const DBAccess::TreeNode &node)
{
    if(current_chapter_node == node)
        return;
    else
        current_chapter_node = node;

    disconnect(chapter_outlines_present,    &QTextDocument::contentsChanged,this,   &NovelHost::listen_chapter_outlines_description_change);
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
    connect(chapter_outlines_present,   &QTextDocument::contentsChanged,this,   &NovelHost::listen_chapter_outlines_description_change);

    // 打开目标章节，前置章节正文内容
    auto item = static_cast<ChaptersItem*>(chapters_navigate_treemodel->itemFromIndex(chaptersNode));
    if(!all_documents.contains(item))
        _load_chapter_text_content(item);

    auto pack = all_documents.value(item);
    if(!pack.second){   // 如果打开的时候没有关联渲染器
        auto renderer = new WordsRender(pack.first, config_host);
        all_documents.insert(item, qMakePair(pack.first, renderer));
    }

    emit currentChaptersActived();
    emit documentPrepared(pack.first, node.title());
}

void NovelHost::setCurrentChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid() || chaptersNode.model() != chapters_navigate_treemodel)
        throw new WsException("传入的chaptersindex无效");

    DBAccess::TreeNode node;
    switch (indexDepth(chaptersNode)) {
        case 1: // 卷宗
            node = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chaptersNode.row());
            break;
        case 2: // 章节
            auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chaptersNode.parent().row());
            node = struct_volume.childAt(TnType::CHAPTER, chaptersNode.row());
            break;
    }

    set_current_volume_outlines(node);

    if(node.type() != TnType::CHAPTER){ // volume
        desplines_filter_under_volume->setFilterBase(current_volume_node);
        desplines_filter_until_volume_remain->setFilterBase(current_volume_node);
    }
    else {
        set_current_chapter_content(chaptersNode, node);

        desplines_filter_under_volume->setFilterBase(current_volume_node);
        desplines_filter_until_volume_remain->setFilterBase(current_volume_node);
        desplines_filter_until_chapter_remain->setFilterBase(current_volume_node, current_chapter_node);
    }

}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_treemodel->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_treemodel->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
    }
}

DBAccess::TreeNode NovelHost::sumDesplinesUnderVolume(const QModelIndex &node, QList<QPair<QString, int> > &desplines) const
{
    if(!node.isValid())
        throw new WsException("输入index无效");

    QModelIndex temp_node = node;
    QList<QModelIndex> stack;
    while (temp_node.isValid()) {
        stack.insert(0, temp_node);
        temp_node = temp_node.parent();
    }
    auto volume_index = stack[0];

    auto struct_volume = desp_ins->novelTreeNode().childAt(TnType::VOLUME, volume_index.row());
    auto despline_count = struct_volume.childCount(TnType::DESPLINE);
    for (int var = 0; var < despline_count; ++var) {
        auto despline_node = struct_volume.childAt(TnType::DESPLINE, var);
        desplines << qMakePair(despline_node.title(), despline_node.uniqueID());
    }

    return struct_volume;
}

void NovelHost::sumPointWithChapterSuspend(int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    for (auto point : points) {
        if(point.attachedChapter().isValid())
            continue;
        suspendPoints << qMakePair(point.title(), point.uniqueID());
    }
}

void NovelHost::sumPointWithChapterAttached(const QModelIndex &chapterIndex, int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    if(!chapterIndex.isValid() || chapterIndex.model() != chapters_navigate_treemodel)
        throw new WsException("指定index无效");
    if(indexDepth(chapterIndex)!=2)
        throw new WsException("指定index类型错误");

    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");
    auto chapter = despline.parent().childAt(TnType::CHAPTER, chapterIndex.row());

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    for (auto point : points) {
        if(point.attachedChapter()==chapter)
            suspendPoints << qMakePair(point.title(), point.uniqueID());
    }
}

void NovelHost::chapterAttachSet(const QModelIndex &chapterIndex, int pointID)
{
    if(!chapterIndex.isValid() || chapterIndex.model() != chapters_navigate_treemodel)
        throw new WsException("指定index无效");
    if(indexDepth(chapterIndex)!=2)
        throw new WsException("指定index类型错误");

    auto chapter = desp_ins->novelTreeNode().childAt(TnType::VOLUME, chapterIndex.parent().row())
                   .childAt(TnType::CHAPTER, chapterIndex.row());
    auto point = desp_ins->getAttachPointViaID(pointID);

    desp_ins->resetChapterOfAttachPoint(point, chapter);
}

void NovelHost::chapterAttachClear(int pointID)
{
    auto point = desp_ins->getAttachPointViaID(pointID);
    desp_ins->resetChapterOfAttachPoint(point, DBAccess::TreeNode());
}

void NovelHost::sumPointWithStoryblcokSuspend(int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    for (auto point : points) {
        if(point.attachedStoryblock().isValid())
            continue;
        suspendPoints << qMakePair(point.title(), point.uniqueID());
    }
}

void NovelHost::sumPointWithStoryblockAttached(const QModelIndex &outlinesIndex, int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    if(!outlinesIndex.isValid() || outlinesIndex.model() != outline_navigate_treemodel)
        throw new WsException("指定index无效");
    if(indexDepth(outlinesIndex)!=2)
        throw new WsException("指定index类型错误");

    auto despline = desp_ins->getTreeNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");
    auto storyblock = despline.parent().childAt(TnType::STORYBLOCK, outlinesIndex.row());

    auto points = desp_ins->getAttachPointsViaDespline(despline);
    for (auto point : points) {
        if(point.attachedStoryblock()==storyblock)
            suspendPoints << qMakePair(point.title(), point.uniqueID());
    }
}

void NovelHost::storyblockAttachSet(const QModelIndex &outlinesIndex, int pointID)
{
    if(!outlinesIndex.isValid() || outlinesIndex.model() != outline_navigate_treemodel)
        throw new WsException("指定index无效");
    if(indexDepth(outlinesIndex)!=2)
        throw new WsException("指定index类型错误");

    auto storyblock = desp_ins->novelTreeNode().childAt(TnType::VOLUME, outlinesIndex.parent().row())
                   .childAt(TnType::STORYBLOCK, outlinesIndex.row());
    auto point = desp_ins->getAttachPointViaID(pointID);

    desp_ins->resetStoryblockOfAttachPoint(point, storyblock);
}

void NovelHost::storyblockAttachClear(int pointID)
{
    auto point = desp_ins->getAttachPointViaID(pointID);
    desp_ins->resetStoryblockOfAttachPoint(point, DBAccess::TreeNode());
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

void NovelHost::refreshDesplinesSummary()
{
    desplines_fuse_source_model->clear();
    desplines_fuse_source_model->setHorizontalHeaderLabels(
                QStringList()<<"名称"<<"索引"<<"描述"<<"所属卷"<<"所属章"<<"关联剧情");

    auto root = desp_ins->novelTreeNode();
    auto volume_count = root.childCount(TnType::VOLUME);
    for (int volume_index = 0; volume_index < volume_count; ++volume_index) {
        auto volume_one = root.childAt(TnType::VOLUME, volume_index);

        auto despline_count = volume_one.childCount(TnType::DESPLINE);
        for (int despline_index = 0; despline_index < despline_count; ++despline_index) {
            auto despline_one = volume_one.childAt(TnType::DESPLINE, despline_index);
            auto attach_points = desp_ins->getAttachPointsViaDespline(despline_one);

            QList<QStandardItem*> row;
            row << new QStandardItem(despline_one.title());                     // displayrole  ：显示文字
            row.last()->setData(1, Qt::UserRole+1);                             // userrole+1   ：despline-mk
            row.last()->setData(despline_one.parent().index(), Qt::UserRole+2); // userrole+2   ：起始卷宗

            if(!attach_points.size()){
                row.last()->setIcon(QIcon(":/outlines/icon/云朵.png"));
            }
            else {
                row.last()->setIcon(QIcon(":/outlines/icon/okpic.png"));

                // append-attachpoint
                for (auto point : attach_points) {
                    auto chpnode = point.attachedChapter();

                    QList<QStandardItem*> points_row;
                    points_row << new QStandardItem(point.title());
                    points_row.last()->setData(2, Qt::UserRole+1);
                    if(!chpnode.isValid()){
                        row.last()->setIcon(QIcon(":/outlines/icon/曲别针.png"));
                        points_row.last()->setData(QVariant(), Qt::UserRole+2);
                        points_row.last()->setData(QVariant(), Qt::UserRole+3);
                    }
                    else {
                        points_row.last()->setData(chpnode.parent().index(), Qt::UserRole+2);
                        points_row.last()->setData(chpnode.uniqueID(), Qt::UserRole+3);
                    }

                    points_row << new QStandardItem(QString("%1").arg(point.index()));
                    points_row.last()->setData(point.uniqueID());
                    points_row.last()->setEditable(false);

                    points_row << new QStandardItem(point.description());

                    if(chpnode.isValid()){
                        points_row << new QStandardItem(chpnode.parent().title());
                        points_row.last()->setEditable(false);

                        points_row << new QStandardItem(chpnode.title());
                        points_row.last()->setEditable(false);
                    }
                    else {
                        points_row << new QStandardItem("未吸附");
                        points_row.last()->setEditable(false);

                        points_row << new QStandardItem("未吸附");
                        points_row.last()->setEditable(false);
                    }
                    auto attached_b = point.attachedStoryblock();
                    points_row << new QStandardItem(attached_b.isValid()?
                                                        attached_b.title():"未吸附");

                    if(chpnode.isValid() && attached_b.isValid())
                        points_row.first()->setIcon(QIcon(":/outlines/icon/okpic.png"));
                    else
                        points_row.first()->setIcon(QIcon(":/outlines/icon/cyclepic.png"));

                    for (auto one : points_row) {
                        QBrush b;
                        b.setColor(Qt::lightGray);
                        b.setStyle(Qt::Dense7Pattern);
                        one->setData(b, Qt::BackgroundRole);
                    }

                    row.last()->appendRow(points_row);
                }
            }


            row << new QStandardItem(QString("%1").arg(despline_one.index()));
            row.last()->setData(despline_one.uniqueID());
            row.last()->setEditable(false);

            row << new QStandardItem(despline_one.description());

            row << new QStandardItem(despline_one.parent().title());
            row.last()->setEditable(false);

            row << new QStandardItem("———————————————");
            row.last()->setEditable(false);
            row << new QStandardItem("———————————————");
            row.last()->setEditable(false);
            for (auto one : row) {
                QLinearGradient g(0,0,0,20);
                g.setColorAt(0, Qt::white);
                g.setColorAt(0.9, QColor(0xfe, 0xfe, 0xfe));
                g.setColorAt(1, Qt::lightGray);
                QBrush b(g);
                b.setStyle(Qt::LinearGradientPattern);
                one->setData(b, Qt::BackgroundRole);
            }

            desplines_fuse_source_model->appendRow(row);
        }
    }
}

void NovelHost::_listen_basic_datamodel_changed(QStandardItem *item)
{
    auto index_and_id_index = item->index().sibling(item->row(), 1);
    auto all_value_index = item->index().sibling(item->row(), 0);

    switch (item->column()) {
        case 1:
        case 3:
        case 4:
            break;
        case 0:
            if(all_value_index.data(Qt::UserRole+1)==1){
                auto despline_one = desp_ins->getTreeNodeViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(despline_one.type() != TnType::DESPLINE)
                    throw new WsException("获取节点类型错误");
                desp_ins->resetTitleOfTreeNode(despline_one, item->text());
            }
            else {
                auto attached_point = desp_ins->getAttachPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                desp_ins->resetTitleOfAttachPoint(attached_point, item->text());
            }
            break;
        case 2:
            if(all_value_index.data(Qt::UserRole+1)==1){
                auto despline_one = desp_ins->getTreeNodeViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(despline_one.type() != TnType::DESPLINE)
                    throw new WsException("获取节点类型错误");
                desp_ins->resetDescriptionOfTreeNode(despline_one, item->text());
            }
            else {
                auto attached_point = desp_ins->getAttachPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                desp_ins->resetDescriptionOfAttachPoint(attached_point, item->text());
            }
            break;
        case 5:{
                auto attached_point = desp_ins->getAttachPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(item->data().isValid()){
                    auto storyblock = desp_ins->getTreeNodeViaID(item->data().toInt());
                    desp_ins->resetStoryblockOfAttachPoint(attached_point, storyblock);
                    item->setText(storyblock.title());
                }
                else {
                    desp_ins->resetStoryblockOfAttachPoint(attached_point, DBAccess::TreeNode());
                    item->setText("未吸附");
                }
            }break;
        default:
            throw new WsException("错误数据");
    }
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
        auto volume_struct = root.childAt(TnType::VOLUME, item->parent()->row());
        auto struct_chapter = volume_struct.childAt(TnType::CHAPTER, item->row());
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
        if(current_volume_node == node_under_volume)
            return;
        else
            current_volume_node = node_under_volume;

        disconnect(volume_outlines_present,  &QTextDocument::contentsChange,
                   this,   &NovelHost::listen_volume_outlines_description_change);
        disconnect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                   this,    &NovelHost::listen_volume_outlines_structure_changed);

        volume_outlines_present->clear();
        QTextCursor cursor(volume_outlines_present);

        int volume_index = node_under_volume.index();
        auto volume_node = outline_navigate_treemodel->item(volume_index);
        insert_description_at_volume_outlines_doc(cursor, static_cast<OutlinesItem*>(volume_node));
        volume_outlines_present->setModified(false);
        volume_outlines_present->clearUndoRedoStacks();

        connect(volume_outlines_present, &QTextDocument::contentsChange,
                this,   &NovelHost::listen_volume_outlines_description_change);
        connect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                this,    &NovelHost::listen_volume_outlines_structure_changed);

        emit currentVolumeActived();
        return;
    }

    set_current_volume_outlines(node_under_volume.parent());
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

DesplineRedirect::DesplineRedirect(NovelHost *const host)
    :host(host){}

QWidget *DesplineRedirect::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    return new QComboBox(parent);
}

void DesplineRedirect::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    QList<QPair<QString,int>> key_stories;
    host->allStoryblocksUnderCurrentVolume(key_stories);
    for (auto xpair : key_stories) {
        cedit->addItem(xpair.first, xpair.second);
    }
    cedit->insertItem(0, "未吸附", QVariant());
    cedit->setCurrentText(index.data().toString());
}

void DesplineRedirect::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    model->setData(index, cedit->currentData(), Qt::UserRole+1);
}

void DesplineRedirect::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}









DesplineFilterModel::DesplineFilterModel(DesplineFilterModel::Type operateType, QObject *parent)
    :QSortFilterProxyModel (parent), operate_type_store(operateType),
      volume_filter_index(INT_MAX), chapter_filter_id(INT_MAX){}

void DesplineFilterModel::setFilterBase(const DBAccess::TreeNode &volume_node, const DBAccess::TreeNode & chapter_node)
{
    volume_filter_index = volume_node.index();
    chapter_filter_id = chapter_node.isValid()?chapter_node.uniqueID():QVariant();
    invalidateFilter();
}

bool DesplineFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    auto target_cell_index = sourceModel()->index(source_row, 0, source_parent);

    if(sourceModel()->data(target_cell_index, Qt::UserRole+1) == 2)
        return true; // 接受所有驻点

    auto parent_volume_index = sourceModel()->data(target_cell_index, Qt::UserRole+2).toInt();  // start-volume index


    switch (operate_type_store) {
        case UNDERVOLUME:
                return volume_filter_index == parent_volume_index;
        case UNTILWITHVOLUME:{
                if(parent_volume_index > volume_filter_index)
                    return false;

                int suspend_point_remains = 0;
                auto attach_point_count = sourceModel()->rowCount(target_cell_index);
                for (auto point_index=0;point_index<attach_point_count;++point_index) {
                    auto attach_point_model_index = sourceModel()->index(point_index, 0, target_cell_index);

                    auto attach_chapter_id = sourceModel()->data(attach_point_model_index, Qt::UserRole+3);
                    if(!attach_chapter_id.isValid())
                        suspend_point_remains+=1;
                    else {
                        auto attach_volume_index = sourceModel()->data(attach_point_model_index, Qt::UserRole+2).toInt();
                        if(attach_volume_index == volume_filter_index)
                            return true;
                    }
                }

                if(suspend_point_remains)
                    return true;

                return false;
            }
        case UNTILWITHCHAPTER:{
                if(parent_volume_index > volume_filter_index)
                    return false;

                int suspend_point_remains = 0;
                auto attach_point_count = sourceModel()->rowCount(target_cell_index);
                for (auto point_index=0; point_index<attach_point_count; point_index++) {
                    auto attach_point_model_index = sourceModel()->index(point_index, 0, target_cell_index);
                    auto attach_chapter_id = sourceModel()->data(attach_point_model_index, Qt::UserRole+3);

                    if(!attach_chapter_id.isValid())
                        suspend_point_remains+=1;
                    else if(attach_chapter_id == chapter_filter_id)
                        return true;
                }

                if(suspend_point_remains)
                    return true;

                return false;
            }
    }
}
