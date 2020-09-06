#include "common.h"
#include "dbaccess.h"
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
using TnType = DBAccess::StoryTreeNode::Type;
using KfvType = DBAccess::KeywordField::ValueType;

NovelHost::NovelHost(ConfigHost &config)
    :config_host(config),
      desp_ins(nullptr),
      outline_navigate_treemodel(new QStandardItemModel(this)),
      novel_outlines_present(new QTextDocument(this)),
      volume_outlines_present(new QTextDocument(this)),
      chapters_navigate_treemodel(new QStandardItemModel(this)),
      chapter_outlines_present(new QTextDocument(this)),
      desplines_fuse_source_model(new QStandardItemModel(this)),
      desplines_filter_under_volume(new DesplineFilterModel(DesplineFilterModel::Type::UNDERVOLUME, this)),
      desplines_filter_until_volume_remain(new DesplineFilterModel(DesplineFilterModel::Type::UNTILWITHVOLUME, this)),
      desplines_filter_until_chapter_remain(new DesplineFilterModel(DesplineFilterModel::Type::UNTILWITHCHAPTER, this)),
      find_results_model(new QStandardItemModel(this)),
      keywords_types_configmodel(new QStandardItemModel(this)),
      quicklook_backend_model(new QStandardItemModel(this))
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

void NovelHost::loadBase(DBAccess *desp)
{
    // save description structure
    this->desp_ins = desp;
    chapters_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "章卷名称" << "严格字数统计");
    outline_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "故事结构");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto novel_node = storytree_hdl.novelNode();

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
        DBAccess::StoryTreeNode volume_node = novel_node.childAt(TnType::VOLUME, volume_index);

        // 在chapters-tree和outline-tree上插入卷节点
        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int storyblock_count = volume_node.childCount(TnType::STORYBLOCK);
        for (int storyblock_index = 0; storyblock_index < storyblock_count; ++storyblock_index) {
            DBAccess::StoryTreeNode storyblock_node = volume_node.childAt(TnType::STORYBLOCK, storyblock_index);

            // outline-tree上插入故事节点
            auto ol_keystory_item = new OutlinesItem(storyblock_node);
            outline_volume_node->appendRow(ol_keystory_item);

            // outline-tree上插入point节点
            int points_count = storyblock_node.childCount(TnType::KEYPOINT);
            for (int points_index = 0; points_index < points_count; ++points_index) {
                DBAccess::StoryTreeNode point_node = storyblock_node.childAt(TnType::KEYPOINT, points_index);

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
            node_navigate_row.last()->setEditable(false);

            node_navigate_volume_node->appendRow(node_navigate_row);
        }
    }

    // 加载所有内容
    for(int index=0; index<chapters_navigate_treemodel->rowCount(); ++index) {
        auto column = chapters_navigate_treemodel->item(index);
        for (int chp_var = 0; chp_var < column->rowCount(); ++chp_var) {
            auto chp_item = column->child(chp_var);
            load_chapter_text_content(chp_item);
        }
    }
    refreshDesplinesSummary();
    _load_all_keywords_types_only_once();
}

void NovelHost::save()
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    for (auto vm_index=0; vm_index<chapters_navigate_treemodel->rowCount(); ++vm_index) {
        auto volume_node = static_cast<ChaptersItem*>(chapters_navigate_treemodel->item(vm_index));
        auto struct_volume_handle = storytree_hdl.novelNode().childAt(TnType::VOLUME, volume_node->row());

        for (auto chp_index=0; chp_index<volume_node->rowCount(); ++chp_index) {
            auto chapter_node = static_cast<ChaptersItem*>(volume_node->child(chp_index));

            auto pak = all_documents.value(chapter_node);
            // 检测文件是否修改
            if(all_documents.contains(chapter_node) && pak.first->isModified()){
                auto struct_chapter_handle = struct_volume_handle.childAt(TnType::CHAPTER, chapter_node->row());
                desp_ins->resetChapterText(struct_chapter_handle, pak.first->toPlainText());
                pak.first->setModified(false);
            }
        }
    }
}

QString NovelHost::novelTitle() const
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    return storytree_hdl.novelNode().title();
}

void NovelHost::resetNovelTitle(const QString &title)
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    storytree_hdl.resetTitleOf(storytree_hdl.novelNode(), title);
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
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto root = storytree_hdl.novelNode();
    auto count = root.childCount(TnType::VOLUME);

    if(index < 0 || index >= count)
        index = count;

    auto vnode = storytree_hdl.insertChildNodeBefore(root, TnType::VOLUME, index, name, description);
    insert_volume(vnode, index);
}

void NovelHost::insertStoryblock(const QModelIndex &pIndex, const QString &name, const QString &description, int index)
{
    if(!pIndex.isValid())
        throw new WsException("输入modelindex无效");
    if(indexDepth(pIndex)!=1)
        throw new WsException("输入节点索引类型错误");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    QStandardItem *item = outline_navigate_treemodel->item(pIndex.row());
    auto root = storytree_hdl.novelNode();
    auto volume_struct_node = root.childAt(TnType::VOLUME, item->row());

    int sb_node_count = volume_struct_node.childCount(TnType::STORYBLOCK);
    if(index < 0 || index >= sb_node_count){
        auto keystory_node = storytree_hdl.insertChildNodeBefore(volume_struct_node, TnType::STORYBLOCK,
                                                                      sb_node_count, name, description);
        item->appendRow(new OutlinesItem(keystory_node));
        index = sb_node_count;
    }
    else{
        auto keystory_node = storytree_hdl.insertChildNodeBefore(volume_struct_node, TnType::STORYBLOCK,
                                                                      index, name, description);
        item->insertRow(index, new OutlinesItem(keystory_node));
    }

    setCurrentOutlineNode(outline_navigate_treemodel->index(index, 0, pIndex));
}

void NovelHost::insertKeypoint(const QModelIndex &pIndex, const QString &name, const QString description, int index)
{
    if(!pIndex.isValid() || pIndex.model() != outline_navigate_treemodel)
        throw new WsException("输入modelindex无效");
    if(indexDepth(pIndex) != 2)
        throw new WsException("输入index类型错误");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto node = outline_navigate_treemodel->itemFromIndex(pIndex);          // keystory-index
    auto struct_storyblock_node = _locate_outline_handle_via(node);

    int points_count = struct_storyblock_node.childCount(TnType::KEYPOINT);
    if(index<0 || index >= points_count){
        auto point_node = storytree_hdl.insertChildNodeBefore(struct_storyblock_node, TnType::KEYPOINT,
                                                                   points_count, name, description);
        node->appendRow(new OutlinesItem(point_node));
        index = points_count;
    }
    else{
        auto point_node = storytree_hdl.insertChildNodeBefore(struct_storyblock_node, TnType::KEYPOINT,
                                                                   index, name, description);
        node->insertRow(index, new OutlinesItem(point_node));
    }

    setCurrentOutlineNode(outline_navigate_treemodel->index(index, 0, pIndex));
}

void NovelHost::appendDesplineUnder(const QModelIndex &anyVolumeIndex, const QString &name, const QString &description)
{
    if(!anyVolumeIndex.isValid())
        throw new WsException("指定index无效");
    if(indexDepth(anyVolumeIndex) != 1)
        throw new WsException("输入index类型错误");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto root = storytree_hdl.novelNode();
    auto struct_volume_node = root.childAt(TnType::VOLUME, anyVolumeIndex.row());

    auto despline_count = struct_volume_node.childCount(TnType::DESPLINE);
    storytree_hdl.insertChildNodeBefore(struct_volume_node, TnType::DESPLINE, despline_count, name, description);
}

void NovelHost::appendDesplineUnderCurrentVolume(const QString &name, const QString &description)
{
    if(!current_volume_node.isValid())
        throw new WsException("current-volume未指定");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto despline_count = current_volume_node.childCount(TnType::DESPLINE);
    storytree_hdl.insertChildNodeBefore(current_volume_node, TnType::DESPLINE, despline_count, name, description);
}

void NovelHost::removeDespline(int desplineID)
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("传入节点ID无效");

    auto points = branchattach_hdl.getPointsViaDespline(despline);
    if(points.size())
        throw new WsException("目标支线非悬空支线，无法删除！");

    storytree_hdl.removeNode(despline);
}

void NovelHost::removeOutlinesNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid() || outlineNode.model() != outline_navigate_treemodel)
        throw new WsException("指定modelindex无效");

    // 转移焦点


    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    int row = item->row();

    if(indexDepth(outlineNode) == 1){
        auto root = storytree_hdl.novelNode();
        auto struct_node = root.childAt(TnType::VOLUME, row);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        storytree_hdl.removeNode(struct_node);
    }
    else {
        auto handle = _locate_outline_handle_via(item);
        item->parent()->removeRow(row);

        storytree_hdl.removeNode(handle);
    }
}

void NovelHost::setCurrentOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid() || outlineNode.model() != outline_navigate_treemodel)
        throw new WsException("传入的outlinemodelindex无效");

    auto current_item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto struct_one = _locate_outline_handle_via(current_item);

    // 设置当前卷节点，填充卷细纲内容
    set_current_volume_outlines(struct_one);

    desplines_filter_under_volume->setFilterBase(current_volume_node);
    desplines_filter_until_volume_remain->setFilterBase(current_volume_node);
}

void NovelHost::allStoryblocksWithIDUnderCurrentVolume(QList<QPair<QString,int>> &storyblocks) const
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto keystory_num = current_volume_node.childCount(TnType::STORYBLOCK);
    for(auto kindex=0; kindex<keystory_num; kindex++){
        auto struct_keystory = storytree_hdl.childAtOf(current_volume_node,TnType::STORYBLOCK, kindex);
        storyblocks << qMakePair(struct_keystory.title(), struct_keystory.uniqueID());
    }
}

void NovelHost::checkChaptersRemoveEffect(const QModelIndex &chpsIndex, QList<QString> &msgList) const
{
    if(!chpsIndex.isValid() || chpsIndex.model()!=chapters_navigate_treemodel)
        throw new WsException("指定index无效");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::StoryTreeNode struct_node;
    switch (indexDepth(chpsIndex)) {
        case 1:
            struct_node = storytree_hdl.novelNode().childAt(TnType::VOLUME, chpsIndex.row());
            break;
        case 2:
            struct_node = storytree_hdl.novelNode().childAt(TnType::VOLUME, chpsIndex.parent().row());
            struct_node = struct_node.childAt(TnType::CHAPTER, chpsIndex.row());
            break;
    }

    _check_remove_effect(struct_node, msgList);
}

void NovelHost::checkDesplineRemoveEffect(int fsid, QList<QString> &msgList) const
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto struct_node = storytree_hdl.getNodeViaID(fsid);
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

DBAccess::StoryTreeNode NovelHost:: _locate_outline_handle_via(QStandardItem *outline_item) const
{
    QList<QStandardItem*> stack;
    while (outline_item) {
        stack.insert(0, outline_item);
        outline_item = outline_item->parent();
    }

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto root = storytree_hdl.novelNode();
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
        auto index = data->navigateIndex();
        auto title_item = outline_navigate_treemodel->itemFromIndex(index);
        if(title_block.text() == ""){
            emit errorPopup("编辑操作", "标题为空，继续删除将破坏文档结构");
        }
        if(indexDepth(index)==1)
            chapters_navigate_treemodel->item(index.row())->setText(title_block.text());

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

        DBAccess::StoryTreeController storytree_hdl(*desp_ins);
        auto index = static_cast<WsBlockData*>(title_block.userData())->navigateIndex();
        auto title_item = outline_navigate_treemodel->itemFromIndex(index);
        auto struct_node = _locate_outline_handle_via(title_item);
        storytree_hdl.resetDescriptionOf(struct_node, description);
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
    auto title_index = user_data->navigateIndex();
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
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto content = chapter_outlines_present->toPlainText();
    storytree_hdl.resetDescriptionOf(current_chapter_node, content);
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
            config_host.storyblockTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), TnType::STORYBLOCK);
            break;
        case TnType::KEYPOINT:
            config_host.keypointTitleFormat(title_block_format, title_char_format);
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
void NovelHost::_check_remove_effect(const DBAccess::StoryTreeNode &target, QList<QString> &msgList) const
{
    if(target.type() == TnType::KEYPOINT)
        return;

    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    if(target.type() == TnType::DESPLINE) {
        msgList << "[warring](foreshadow·despline)<"+target.title()+">指定伏笔[故事线]将被删除，请注意！";

        auto stopnodes = branchattach_hdl.getPointsViaDespline(target);
        for (auto dot : stopnodes) {
            auto storyblk = dot.attachedStoryblock();
            if(storyblk.isValid())
                msgList << "[error](keystory·storyblock)<"+storyblk.title()+">影响关键剧情！请重写相关内容！";
            auto chapter = dot.attachedChapter();
            if(chapter.isValid())
                msgList << "[error](chapter)<"+chapter.title()+">影响章节内容！请重写相关内容！";
        }
        return;
    }

    if(target.type() == TnType::STORYBLOCK){
        auto points = branchattach_hdl.getPointsViaStoryblock(target);
        for (auto dot : points) {
            auto foreshadownode = dot.attachedDespline();
            msgList << "[error](foreshadow·despline)<"+foreshadownode.title()+">影响指定伏笔[故事线]，请注意修改描述！";

            auto chapternode = dot.attachedChapter();
            if(chapternode.isValid())
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
        auto points = branchattach_hdl.getPointsViaChapter(target);
        for (auto dot : points) {
            auto foreshadownode = dot.attachedDespline();
            msgList << "[error](foreshadow·despline)<"+foreshadownode.title()+">影响指定伏笔[故事线]，请注意修改描述！";
            auto storyblknode = dot.attachedStoryblock();
            if(storyblknode.isValid())
                msgList << "[error](keystory·storyblock)<"+storyblknode.title()+">影响剧情内容！请注意相关内容！";
        }

    }
}

QTextDocument* NovelHost::load_chapter_text_content(QStandardItem *item)
{
    auto parent = item->parent();
    if(!parent) // 卷宗节点不可加载
        return nullptr;

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    // load text-content
    auto volume_symbo = storytree_hdl.novelNode().childAt(TnType::VOLUME, parent->row());
    auto chapter_symbo = volume_symbo.childAt(TnType::CHAPTER, item->row());
    QString content = desp_ins->chapterText(chapter_symbo);

    // 载入内存实例
    auto doc = new QTextDocument(this);
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
    auto renderer = new WordsRender(doc, *this);
    all_documents.insert(static_cast<ChaptersItem*>(item), qMakePair(doc, renderer));
    connect(doc, &QTextDocument::contentsChanged, static_cast<ChaptersItem*>(item),  &ChaptersItem::calcWordsCount);
    connect(doc, &QTextDocument::cursorPositionChanged, this,   &NovelHost::acceptEditingTextblock);

    return doc;
}

// 写作界面
QAbstractItemModel *NovelHost::chaptersNavigateTree() const
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

QAbstractItemModel *NovelHost::keywordsTypeslistModel() const
{
    return keywords_types_configmodel;
}

void NovelHost::_load_all_keywords_types_only_once()
{
    keywords_types_configmodel->setHorizontalHeaderLabels(QStringList() << "名称"<<"类型");

    DBAccess::KeywordController keywords_proc(*desp_ins);
    auto table = keywords_proc.firstTable();
    while (table.isValid()) {
        // table-row
        QList<QStandardItem*> table_row;
        table_row << new QStandardItem(table.name());
        table_row.last()->setData(table.registID());
        table_row << new QStandardItem("--------------------");
        keywords_types_configmodel->appendRow(table_row);

        for (auto item : table_row)
            item->setEditable(false);


        // field-rows
        auto child_count = table.childCount();
        for (int index=0; index < child_count; ++index) {
            auto field = table.childAt(index);

            QList<QStandardItem*> field_row;
            field_row << new QStandardItem(field.name());

            switch (field.vType()) {
                case KfvType::NUMBER:
                    field_row << new QStandardItem("[NUMBER]");
                    break;
                case KfvType::STRING:
                    field_row << new QStandardItem("[STRING]");
                    break;
                case KfvType::ENUM:
                    field_row << new QStandardItem("[ENUMERATE]"+field.supplyValue());
                    break;
                case KfvType::TABLEREF:{
                        auto nnn = keywords_proc.firstTable();
                        while (nnn.isValid()) {
                            if(nnn.supplyValue() == field.supplyValue()){
                                field_row << new QStandardItem("[TABLEREF]"+nnn.name());
                                break;
                            }
                            nnn = nnn.nextSibling();
                        }
                    }
                    break;
            }
            for (auto item : field_row)
                item->setEditable(false);

            table_row.first()->appendRow(field_row);
        }

        auto keywords_model = new QStandardItemModel(this);
        keywords_manager_group.append(qMakePair(table, keywords_model));

        table = table.nextSibling();
    }
}


QAbstractItemModel *NovelHost::keywordsModelViaTheList(const QModelIndex &mindex) const
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);

    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id)
            return pair.second;
    }
    return nullptr;
}

QAbstractItemModel *NovelHost::appendKeywordsModelToTheList(const QString &name)
{
    DBAccess::KeywordController keywords_proc(*desp_ins);
    auto newtable = keywords_proc.newTable(name);
    auto model = new QStandardItemModel(this);
    keywords_manager_group.append(qMakePair(newtable, model));

    QList<QStandardItem*> row;
    row << new QStandardItem(name);
    row.last()->setData(newtable.registID());
    row.last()->setEditable(false);
    row << new QStandardItem("--------------------");
    row.last()->setEditable(false);
    keywords_types_configmodel->appendRow(row);

    return model;
}


void NovelHost::removeKeywordsModelViaTheList(const QModelIndex &mindex)
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);

    DBAccess::KeywordController keywords_proc(*desp_ins);
    for (int index = 0; index<keywords_manager_group.size(); index++) {
        auto pair = keywords_manager_group.at(index);

        if(pair.first.registID() == table_id){
            keywords_proc.removeTable(pair.first);

            for (auto itemidx=0; itemidx<keywords_types_configmodel->rowCount(); ++itemidx) {
                auto id = keywords_types_configmodel->item(itemidx)->data().toInt();
                if(id == table_id){
                    keywords_types_configmodel->removeRow(itemidx);
                    break;
                }
            }

            delete pair.second;
            keywords_manager_group.removeAt(index);
            break;
        }
    }
}

void NovelHost::getAllKeywordsTableRefs(QList<QPair<QString, QString>> &name_ref_list) const
{
    DBAccess::KeywordController keywords_proc(*desp_ins);
    auto table = keywords_proc.firstTable();
    while (table.isValid()) {
        name_ref_list << qMakePair(table.name(), table.tableName());
        table = table.nextSibling();
    }
}

QList<QPair<int, std::tuple<QString, QString, DBAccess::KeywordField::ValueType>>>
NovelHost::customedFieldsListViaTheList(const QModelIndex &mindex) const
{
    QList<QPair<int, std::tuple<QString, QString, DBAccess::KeywordField::ValueType>>> retlist;
    auto table_id = extract_tableid_from_the_typelist_model(mindex);

    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){
            auto child_count = pair.first.childCount();
            for (auto index=0; index<child_count; ++index) {
                auto field = pair.first.childAt(index);
                retlist.append(qMakePair(
                                   field.registID(),
                                   std::make_tuple(field.name(), field.supplyValue(), field.vType())));
            }

            break;
        }
    }

    return retlist;
}

void NovelHost::adjustKeywordsFieldsViaTheList(const QModelIndex &mindex, const QList<QPair<int,std::tuple<QString, QString,
                                               DBAccess::KeywordField::ValueType> >> fields_defines)
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);
    DBAccess::KeywordController keywords_proc(*desp_ins);


    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){    // 找到指定表格
            auto table_modelindex = get_table_presentindex_via_typelist_model(mindex);
            QList<QPair<NovelBase::DBAccess::KeywordField, std::tuple<QString,
                    QString, DBAccess::KeywordField::ValueType>>> convert_peer;

            auto table_defroot = keywords_types_configmodel->item(table_modelindex.row());
            table_defroot->removeRows(0, table_defroot->rowCount());

            for (auto define : fields_defines) {
                QList<QStandardItem*> field_defrow;

                field_defrow << new QStandardItem(std::get<0>(define.second));
                switch (std::get<2>(define.second)) {
                    case KfvType::NUMBER:
                        field_defrow << new QStandardItem("[NUMBER]");
                        break;
                    case KfvType::STRING:
                        field_defrow << new QStandardItem("[STRING]");
                        break;
                    case KfvType::ENUM:
                        field_defrow << new QStandardItem("[ENUMERATE]" + std::get<1>(define.second));
                        break;
                    case KfvType::TABLEREF:{
                            QList<QPair<QString, QString>> all_tables;
                            getAllKeywordsTableRefs(all_tables);

                            bool findit = false;
                            for (auto pair : all_tables) {
                                if(pair.second == std::get<1>(define.second)){
                                    field_defrow << new QStandardItem("[TABLEREF]"+pair.first);
                                    findit = true;
                                    break;
                                }
                            }
                            if(!findit)
                                throw new WsException("指定表格不存在："+std::get<1>(define.second));
                        }
                        break;
                }
                field_defrow[0]->setEditable(false);
                field_defrow[1]->setEditable(false);

                table_defroot->appendRow(field_defrow);


                if(define.first == INT_MAX){
                    convert_peer.append(qMakePair(DBAccess::KeywordField(), define.second));
                    continue;
                }
                auto field_count = pair.first.childCount();
                for (auto index=0; index<field_count; ++index) {
                    auto field = pair.first.childAt(index);     // 轮询校对字段

                    if(define.first == field.registID()){       // 找到了指定字段
                        convert_peer.append(qMakePair(field, define.second));
                        break;
                    }
                }
            }
            keywords_proc.tablefieldsAdjust(pair.first, convert_peer);
            break;
        }
    }
}

void NovelHost::appendNewItemViaTheList(const QModelIndex &mindex, const QString &name)
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);
    DBAccess::KeywordController keywords_proc(*desp_ins);
    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){
            keywords_proc.appendEmptyItemAt(pair.first, name);
            break;
        }
    }
}

void NovelHost::removeTargetItemViaTheList(const QModelIndex &mindex, const QModelIndex &tIndex)
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);
    DBAccess::KeywordController keywords_proc(*desp_ins);
    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){
            keywords_proc.removeTargetItemAt(pair.first, tIndex);
            break;
        }
    }
}

void NovelHost::renameKeywordsTypenameViaTheList(const QModelIndex &mindex, const QString &newName)
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);
    DBAccess::KeywordController keywords_proc(*desp_ins);
    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){
            keywords_proc.resetNameOf(pair.first, newName);
            keywords_types_configmodel->setData(mindex, newName);
            break;
        }
    }
}

void NovelHost::queryKeywordsViaTheList(const QModelIndex &mindex, const QString &itemName) const
{
    auto table_id = extract_tableid_from_the_typelist_model(mindex);
    DBAccess::KeywordController keywords_proc(*desp_ins);

    for (auto pair : keywords_manager_group) {
        if(pair.first.registID() == table_id){
            keywords_proc.queryKeywordsLike(pair.second, itemName, pair.first);
            break;
        }
    }
}

QList<QPair<int, QString> > NovelHost::avaliableEnumsForIndex(const QModelIndex &index) const
{
    DBAccess::KeywordController keywords_proc(*desp_ins);
    return keywords_proc.avaliableEnumsForIndex(index);
}

QList<QPair<int, QString> > NovelHost::avaliableItemsForIndex(const QModelIndex &index) const
{
    DBAccess::KeywordController keywords_proc(*desp_ins);
    return keywords_proc.avaliableItemsForIndex(index);
}

QModelIndex NovelHost::get_table_presentindex_via_typelist_model(const QModelIndex &mindex) const
{
    if(!mindex.isValid() && mindex.model()!=keywords_types_configmodel)
        throw new WsException("传入ModelIndex非法！");

    auto table_index = mindex;
    if(indexDepth(mindex)==2)
        table_index = mindex.parent();
    if(table_index.column())
        table_index = table_index.sibling(table_index.row(), 0);

    return table_index;
}

int NovelHost::extract_tableid_from_the_typelist_model(const QModelIndex &mindex) const
{
    return get_table_presentindex_via_typelist_model(mindex).data(Qt::UserRole+1).toInt();
}

QAbstractItemModel *NovelHost::quicklookItemsModel() const
{
    return quicklook_backend_model;
}



void NovelHost::insertChapter(const QModelIndex &pIndex, const QString &name, const QString &description, int index)
{
    if(!pIndex.isValid())
        throw new WsException("输入index无效");
    if(indexDepth(pIndex) != 1)
        throw new WsException("输入index类型错误");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto volume_item = chapters_navigate_treemodel->item(pIndex.row());
    auto struct_volume = storytree_hdl.novelNode().childAt(TnType::VOLUME, volume_item->row());
    auto count = struct_volume.childCount(TnType::CHAPTER);

    QList<QStandardItem*> row;
    if(index < 0 || index >= count){
        auto newnode = storytree_hdl.insertChildNodeBefore(struct_volume, TnType::CHAPTER, count, name, description);
        desp_ins->resetChapterText(newnode, "章节内容为空");
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        volume_item->appendRow(row);
    }
    else {
        auto newnode = storytree_hdl.insertChildNodeBefore(struct_volume, TnType::CHAPTER, index, name, description);
        desp_ins->resetChapterText(newnode, "章节内容为空");
        row << new ChaptersItem(*this, newnode);
        row << new QStandardItem("-");
        volume_item->insertRow(index, row);
    }
}

void NovelHost::insertAttachpoint(int desplineID, const QString &title, const QString &desp, int index)
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(despline.type() != TnType::DESPLINE)
        throw new WsException("指定despline节点id非法");

    auto points = branchattach_hdl.getPointsViaDespline(despline);
    if(index > -1 && index < points.size())
        branchattach_hdl.insertPointBefore(despline, index, title, desp);
    else
        branchattach_hdl.insertPointBefore(despline, points.size(), title, desp);
}

void NovelHost::removeAttachpoint(int attachpointID)
{
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto point = branchattach_hdl.getPointViaID(attachpointID);
    if(point.attachedChapter().isValid() || point.attachedStoryblock().isValid())
        throw new WsException("目标驻点非悬空驻点，不可删除！");
    branchattach_hdl.removePoint(point);
}

void NovelHost::attachPointMoveup(const QModelIndex &desplineIndex)
{
    if(!desplineIndex.isValid()) return;

    auto id_index = desplineIndex.sibling(desplineIndex.row(), 1);
    auto id_integer = id_index.data(Qt::UserRole+1).toInt();

    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto point = branchattach_hdl.getPointViaID(id_integer);
    if(!branchattach_hdl.moveUpOf(point)) return;

    auto filter_model = static_cast<DesplineFilterModel*>(const_cast<QAbstractItemModel*>(desplineIndex.model()));
    auto source_model =  static_cast<QStandardItemModel*>(filter_model->sourceModel());
    auto source_mindex = filter_model->mapToSource(desplineIndex);

    auto pnode = source_model->itemFromIndex(source_mindex.parent());
    auto row_num = source_mindex.row();
    auto row = pnode->takeRow(row_num);
    pnode->insertRow(row_num-1, row);
}

void NovelHost::attachPointMovedown(const QModelIndex &desplineIndex)
{
    if(!desplineIndex.isValid()) return;

    auto id_index = desplineIndex.sibling(desplineIndex.row(), 1);
    auto id_integer = id_index.data(Qt::UserRole+1).toInt();

    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto point = branchattach_hdl.getPointViaID(id_integer);
    if(!branchattach_hdl.moveDownOf(point)) return;

    auto filter_model = static_cast<DesplineFilterModel*>(const_cast<QAbstractItemModel*>(desplineIndex.model()));
    auto source_model =  static_cast<QStandardItemModel*>(filter_model->sourceModel());
    auto source_mindex = filter_model->mapToSource(desplineIndex);

    auto pnode = source_model->itemFromIndex(source_mindex.parent());
    auto row_num = source_mindex.row();
    auto row = pnode->takeRow(row_num);
    pnode->insertRow(row_num+1, row);
}


void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid() || chaptersNode.model() != chapters_navigate_treemodel)
        throw new WsException("chaptersNodeIndex无效");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto chapter = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    int row = chapter->row();
    // 卷宗节点管理同步
    if(indexDepth(chaptersNode)==1){
        auto struct_volume = storytree_hdl.novelNode().childAt(TnType::VOLUME, row);

        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        storytree_hdl.removeNode(struct_volume);
    }
    // 章节节点
    else {
        auto volume = chapter->parent();
        auto struct_volume = storytree_hdl.novelNode().childAt(TnType::VOLUME, volume->row());
        auto struct_chapter = struct_volume.childAt(TnType::CHAPTER, row);
        volume->removeRow(row);

        storytree_hdl.removeNode(struct_chapter);
    }
}

void NovelHost::set_current_chapter_content(const QModelIndex &chaptersNode, const DBAccess::StoryTreeNode &node)
{
    if(current_chapter_node == node)
        return;
    else
        current_chapter_node = node;

    disconnect(chapter_outlines_present,    &QTextDocument::contentsChanged,this,   &NovelHost::listen_chapter_outlines_description_change);
    chapter_outlines_present->clear();

    QTextFrameFormat fformat;
    config_host.textFrameFormat(fformat);
    chapter_outlines_present->rootFrame()->setFrameFormat(fformat);
    QTextBlockFormat blockformat0;
    QTextCharFormat charformat0;
    config_host.textFormat(blockformat0, charformat0);
    QTextCursor cursor0(chapter_outlines_present);
    cursor0.setBlockFormat(blockformat0);
    cursor0.setBlockCharFormat(charformat0);
    cursor0.setCharFormat(charformat0);
    cursor0.insertText(node.description());
    chapter_outlines_present->setModified(false);
    chapter_outlines_present->clearUndoRedoStacks();
    connect(chapter_outlines_present,   &QTextDocument::contentsChanged,this,   &NovelHost::listen_chapter_outlines_description_change);

    // 打开目标章节，前置章节正文内容
    auto item = static_cast<ChaptersItem*>(chapters_navigate_treemodel->itemFromIndex(chaptersNode));
    if(!all_documents.contains(item))
        load_chapter_text_content(item);

    auto pack = all_documents.value(item);
    if(!pack.second){   // 如果打开的时候没有关联渲染器
        auto renderer = new WordsRender(pack.first, *this);
        all_documents.insert(item, qMakePair(pack.first, renderer));
    }

    emit currentChaptersActived();
    emit documentPrepared(pack.first, node.title());
}

void NovelHost::setCurrentChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid() || chaptersNode.model() != chapters_navigate_treemodel)
        throw new WsException("传入的chaptersindex无效");

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::StoryTreeNode node;
    switch (indexDepth(chaptersNode)) {
        case 1: // 卷宗
            node = storytree_hdl.novelNode().childAt(TnType::VOLUME, chaptersNode.row());
            break;
        case 2: // 章节
            auto struct_volume = storytree_hdl.novelNode().childAt(TnType::VOLUME, chaptersNode.parent().row());
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

void NovelHost::sumDesplinesUntilVolume(const QModelIndex &node, QList<QPair<QString, int> > &desplines) const
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::StoryTreeNode struct_volume;
    for (auto index=0; index <= volume_index.row(); ++index) {
        struct_volume = storytree_hdl.novelNode().childAt(TnType::VOLUME, index);
        auto despline_count = struct_volume.childCount(TnType::DESPLINE);

        for (int var = 0; var < despline_count; ++var) {
            auto despline_node = struct_volume.childAt(TnType::DESPLINE, var);
            desplines << qMakePair(despline_node.title(), despline_node.uniqueID());
        }
    }
}

void NovelHost::sumPointWithChapterSuspend(int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");

    auto points = branchattach_hdl.getPointsViaDespline(despline);
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");
    auto chapter = despline.parent().childAt(TnType::CHAPTER, chapterIndex.row());

    auto points = branchattach_hdl.getPointsViaDespline(despline);
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto chapter = storytree_hdl.novelNode().childAt(TnType::VOLUME, chapterIndex.parent().row())
                   .childAt(TnType::CHAPTER, chapterIndex.row());
    auto point = branchattach_hdl.getPointViaID(pointID);

    branchattach_hdl.resetChapterOf(point, chapter);
}

void NovelHost::chapterAttachClear(int pointID)
{
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto point = branchattach_hdl.getPointViaID(pointID);
    branchattach_hdl.resetChapterOf(point, DBAccess::StoryTreeNode());
}

void NovelHost::sumPointWithStoryblcokSuspend(int desplineID, QList<QPair<QString, int> > &suspendPoints) const
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");

    auto points = branchattach_hdl.getPointsViaDespline(despline);
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto despline = storytree_hdl.getNodeViaID(desplineID);
    if(!despline.isValid() || despline.type() != TnType::DESPLINE)
        throw new WsException("指定输入支线ID无效");
    auto storyblock = despline.parent().childAt(TnType::STORYBLOCK, outlinesIndex.row());

    auto points = branchattach_hdl.getPointsViaDespline(despline);
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto storyblock = storytree_hdl.novelNode().childAt(TnType::VOLUME, outlinesIndex.parent().row())
                      .childAt(TnType::STORYBLOCK, outlinesIndex.row());
    auto point = branchattach_hdl.getPointViaID(pointID);

    branchattach_hdl.resetStoryblockOf(point, storyblock);
}

void NovelHost::storyblockAttachClear(int pointID)
{
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto point = branchattach_hdl.getPointViaID(pointID);
    branchattach_hdl.resetStoryblockOf(point, DBAccess::StoryTreeNode());
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

                for (auto item : row) item->setEditable(false);
            }
        }
    }
}

void NovelHost::pushToQuickLook(const QTextBlock &block, const QList<QPair<QString, int> > &mixtureList_)
{
    if(current_editing_textblock != block)
        return;

    // 清洗重复项
    auto mixtureList = mixtureList_;
    for (auto base_index=0; base_index<mixtureList.size();++base_index) {
        auto base = mixtureList.at(base_index);

        for (auto cursor_index=base_index+1; cursor_index<mixtureList.size(); ++cursor_index) {
            auto cursor = mixtureList.at(cursor_index);

            if(base.first == cursor.first && base.second == cursor.second){
                mixtureList.removeAt(cursor_index);
                cursor_index--;
            }
        }
    }

    NovelBase::DBAccess::KeywordController handle(*desp_ins);
    handle.queryKeywordsViaMixtureList(mixtureList, quicklook_backend_model);
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
    if(!all_documents.contains(refer_node)) return 0;
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

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);
    auto root = storytree_hdl.novelNode();
    auto volume_count = root.childCount(TnType::VOLUME);
    for (int volume_index = 0; volume_index < volume_count; ++volume_index) {
        auto volume_one = root.childAt(TnType::VOLUME, volume_index);

        auto despline_count = volume_one.childCount(TnType::DESPLINE);
        for (int despline_index = 0; despline_index < despline_count; ++despline_index) {
            auto despline_one = volume_one.childAt(TnType::DESPLINE, despline_index);
            auto attach_points = branchattach_hdl.getPointsViaDespline(despline_one);

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

            desplines_fuse_source_model->appendRow(row);
        }
    }
}

ConfigHost &NovelHost::getConfigHost() const {
    return config_host;
}

void NovelHost::testMethod()
{
    try {
        for (auto index=0; index<desplines_filter_under_volume->rowCount(); ++index) {
            for (auto colidx=0; colidx<desplines_filter_under_volume->columnCount(); ++colidx) {
                qDebug() << desplines_filter_under_volume->flags(desplines_filter_under_volume->index(index, colidx));
            }
            qDebug() << "=================================";
        }

    } catch (WsException *e) {
        qDebug() << e->reason();
    }
}

void NovelHost::appendActiveTask(const QString &taskMask, int number)
{
    emit taskAppended(taskMask, number);
}

void NovelHost::finishActiveTask(const QString &taskMask, const QString &finalTip, int number)
{
    emit taskFinished(taskMask, finalTip, number);
}

void NovelHost::acceptEditingTextblock(const QTextCursor &cursor){
    current_editing_textblock = cursor.block();
}

void NovelHost::_listen_basic_datamodel_changed(QStandardItem *item)
{
    auto index_and_id_index = item->index().sibling(item->row(), 1);
    auto all_value_index = item->index().sibling(item->row(), 0);
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    DBAccess::BranchAttachController branchattach_hdl(*desp_ins);

    switch (item->column()) {
        case 1:
        case 3:
        case 4:
            break;
        case 0:
            if(all_value_index.data(Qt::UserRole+1)==1){
                auto despline_one = storytree_hdl.getNodeViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(despline_one.type() != TnType::DESPLINE)
                    throw new WsException("获取节点类型错误");
                storytree_hdl.resetTitleOf(despline_one, item->text());
            }
            else {
                auto attached_point = branchattach_hdl.getPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                branchattach_hdl.resetTitleOf(attached_point, item->text());
            }
            break;
        case 2:
            if(all_value_index.data(Qt::UserRole+1)==1){
                auto despline_one = storytree_hdl.getNodeViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(despline_one.type() != TnType::DESPLINE)
                    throw new WsException("获取节点类型错误");
                storytree_hdl.resetDescriptionOf(despline_one, item->text());
            }
            else {
                auto attached_point = branchattach_hdl.getPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                branchattach_hdl.resetDescriptionOf(attached_point, item->text());
            }
            break;
        case 5:{
                auto attached_point = branchattach_hdl.getPointViaID(item->model()->data(index_and_id_index, Qt::UserRole+1).toInt());
                if(item->data().isValid()){
                    auto storyblock = storytree_hdl.getNodeViaID(item->data().toInt());
                    branchattach_hdl.resetStoryblockOf(attached_point, storyblock);
                    item->setText(storyblock.title());
                }
                else {
                    branchattach_hdl.resetStoryblockOf(attached_point, DBAccess::StoryTreeNode());
                    item->setText("未吸附");
                }
            }break;
        default:
            throw new WsException("错误数据");
    }
}

void NovelHost::outlines_node_title_changed(QStandardItem *item)
{
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto struct_node = _locate_outline_handle_via(item);
    storytree_hdl.resetTitleOf(struct_node, item->text());

    auto blk = volume_outlines_present->firstBlock();
    while (blk.isValid()) {
        if(blk.userData()){
            if(blk.text() == item->text())
                break;

            auto data_key = static_cast<WsBlockData*>(blk.userData());
            if(data_key->navigateIndex() == item->index()){
                QTextCursor cur(blk);
                cur.select(QTextCursor::BlockUnderCursor);
                cur.insertText(item->text());
                break;
            }
        }
        blk = blk.next();
    }
}

void NovelHost::chapters_node_title_changed(QStandardItem *item){
    if(item->column())                      // 忽略计数节点
        return;

    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto root = storytree_hdl.novelNode();
    switch (indexDepth(item->index())) {
        case 1:{
                auto volume_struct = root.childAt(TnType::VOLUME, item->row());
                storytree_hdl.resetTitleOf(volume_struct, item->text());

                auto peer_index = outline_navigate_treemodel->index(item->row(), 0);
                auto blk = volume_outlines_present->firstBlock();
                while (blk.isValid()) {
                    if(blk.userData()){
                        if(blk.text() == item->text())
                            break;

                        auto data_key = static_cast<WsBlockData*>(blk.userData());
                        if(data_key->navigateIndex() == peer_index){
                            QTextCursor cur(blk);
                            cur.select(QTextCursor::BlockUnderCursor);
                            cur.insertText(item->text());
                            break;
                        }
                    }
                    blk = blk.next();
                }
            }
            break;
        case 2:{
                auto volume_struct = root.childAt(TnType::VOLUME, item->parent()->row());
                auto struct_chapter = volume_struct.childAt(TnType::CHAPTER, item->row());
                storytree_hdl.resetTitleOf(struct_chapter, item->text());
            }
            break;
    }
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const DBAccess::StoryTreeNode &volume_handle, int index)
{
    auto outline_volume_node = new OutlinesItem(volume_handle);

    QList<QStandardItem*> navigate_valume_row;
    auto node_navigate_volume_node = new ChaptersItem(*this, volume_handle, true);
    navigate_valume_row << node_navigate_volume_node;
    navigate_valume_row << new QStandardItem("-");
    navigate_valume_row.last()->setEditable(false);


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
    DBAccess::StoryTreeController storytree_hdl(*desp_ins);
    auto content = novel_outlines_present->toPlainText();
    storytree_hdl.resetDescriptionOf(storytree_hdl.novelNode(), content);
}

// 向卷宗细纲填充内容
void NovelHost::set_current_volume_outlines(const DBAccess::StoryTreeNode &node_under_volume){
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

ChaptersItem::ChaptersItem(NovelHost &host, const DBAccess::StoryTreeNode &refer, bool isGroup)
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
    try {
        QList<std::tuple<QString, int, QTextCharFormat, int, int>> rst;

        QTextCharFormat format;
        config_symbo.warringFormat(format);
        auto warrings = config_symbo.warringWords();
        keywords_highlighter_render(content_stored, warrings, format, rst);

        QTextCharFormat format2;
        config_symbo.keywordsFormat(format2);
        auto keywords = config_symbo.getKeywordsWithMSG();
        keywords_highlighter_render(content_stored, keywords, format2, rst);

        poster_stored->acceptRenderResult(content_stored, rst);
        emit renderFinished(placeholder);

        this->disconnect();
    } catch (std::exception *e) {
        qDebug() << "render-worker exception";
    }
}

void WordsRenderWorker::keywords_highlighter_render(const QString &text, QList<std::tuple<QString, int, QString>> words, const QTextCharFormat &format,
                                            QList<std::tuple<QString, int, QTextCharFormat, int, int> > &rst) const
{
    for (auto tuple : words) {
        QRegExp exp("("+ std::get<2>(tuple) +").*");
        exp.setMinimal(true);
        int pos = -1;

        while ((pos = exp.indexIn(text, pos+1)) != -1) {
            auto sint = pos;
            auto wstr = exp.cap(1);
            auto lint = wstr.length();

            rst.append(std::make_tuple(std::get<0>(tuple), std::get<1>(tuple), format, sint, lint));
        }
    }
}


WordsRender::WordsRender(QTextDocument *target, NovelHost &config)
    :QSyntaxHighlighter (target), novel_host(config){}

WordsRender::~WordsRender(){}

void WordsRender::acceptRenderResult(const QString &content, const QList<std::tuple<QString, int, QTextCharFormat, int, int>> &rst)
{
    QMutexLocker lock(&mutex);

    _result_store.insert(content, rst);
}

ConfigHost &WordsRender::configBase() const
{
    return novel_host.getConfigHost();
}

bool WordsRender::_check_extract_render_result(const QString &text, QList<std::tuple<QString, int, QTextCharFormat, int, int>> &rst)
{
    QMutexLocker lock(&mutex);

    rst = _result_store.value(text);
    return _result_store.remove(text);
}

void WordsRender::highlightBlock(const QString &text)
{
    auto blk = currentBlock();
    if(!blk.isValid())
        return;

    QList<std::tuple<QString, int, QTextCharFormat, int, int>> rst;
    if(!_check_extract_render_result(text, rst)){
        auto worker = new WordsRenderWorker(this, blk, text);
        connect(worker, &WordsRenderWorker::renderFinished,   this,   &QSyntaxHighlighter::rehighlightBlock);
        connect(worker, &WordsRenderWorker::renderFinished,     [&]{
            novel_host.finishActiveTask("关键字渲染", "关键字渲染结束");
        });
        novel_host.appendActiveTask("关键字渲染");
        QThreadPool::globalInstance()->start(worker);
        return;
    }

    QList<QPair<QString, int>> keywords_ids;
    for (auto tuple:rst) {
        auto table_realname = std::get<0>(tuple);
        auto word_id = std::get<1>(tuple);

        auto format = std::get<2>(tuple);
        auto sint = std::get<3>(tuple);
        auto lint = std::get<4>(tuple);

        setFormat(sint, lint, format);

        if(table_realname!="" && word_id != INT_MAX)
            keywords_ids << qMakePair(table_realname, word_id);
    }

    novel_host.pushToQuickLook(blk, keywords_ids);
}

OutlinesItem::OutlinesItem(const DBAccess::StoryTreeNode &refer)
{
    setText(refer.title());
    switch (refer.type()) {
        case DBAccess::StoryTreeNode::Type::KEYPOINT:
            setIcon(QIcon(":/outlines/icon/点.png"));
            break;
        case DBAccess::StoryTreeNode::Type::VOLUME:
            setIcon(QIcon(":/outlines/icon/卷.png"));
            break;
        case DBAccess::StoryTreeNode::Type::CHAPTER:
            setIcon(QIcon(":/outlines/icon/章.png"));
            break;
        case DBAccess::StoryTreeNode::Type::STORYBLOCK:
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

QModelIndex WsBlockData::navigateIndex() const
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
            config.storyblockTitleFormat(bformat, cformat);
            break;
        case WsBlockData::Type::KEYPOINT:
            config.keypointTitleFormat(bformat, cformat);
            break;
        default:
            return;
    }

    setFormat(0, text.length(), cformat);
}






DesplineFilterModel::DesplineFilterModel(DesplineFilterModel::Type operateType, QObject *parent)
    :QSortFilterProxyModel (parent), operate_type_store(operateType),
      volume_filter_index(INT_MAX), chapter_filter_id(INT_MAX){}

void DesplineFilterModel::setFilterBase(const DBAccess::StoryTreeNode &volume_node, const DBAccess::StoryTreeNode & chapter_node)
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
