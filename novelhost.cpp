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

    connect(outline_navigate_treemodel, &QStandardItemModel::itemChanged,   this,   &NovelHost::outlines_node_title_changed);
    connect(chapters_navigate_treemodel,&QStandardItemModel::itemChanged,   this,   &NovelHost::chapters_node_title_changed);
}

NovelHost::~NovelHost(){}

void NovelHost::loadDescription(FStruct *desp)
{
    // save description structure
    this->desp_tree = desp;
    chapters_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "章卷名称" << "严格字数统计");
    outline_navigate_treemodel->setHorizontalHeaderLabels(QStringList() << "故事结构");

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
        }

        // chapters上插入chapter节点
        int chapter_count = desp->chapterCount(volume_node);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            FStruct::NHandle chapter_node = desp->chapterAt(volume_node, chapter_index);

            QList<QStandardItem*> node_navigate_row;
            node_navigate_row << new ChaptersItem(*this, chapter_node);
            node_navigate_row << new QStandardItem("-");
            node_navigate_volume_node->appendRow(node_navigate_row);
        }
    }

    QTextBlockFormat blockformat;
    QTextCharFormat charformat;
    config_host.textFormat(blockformat, charformat);
    QTextCursor cursor(novel_outlines_present);
    cursor.setBlockFormat(blockformat);
    cursor.setBlockCharFormat(charformat);
    cursor.insertText(desp_tree->novelDescription());
    novel_outlines_present->setModified(false);
    novel_outlines_present->clearUndoRedoStacks();
    connect(novel_outlines_present,  &QTextDocument::contentsChanged,    this,   &NovelHost::listen_novel_description_change);
}

void NovelHost::save(const QString &filePath)
{
    desp_tree->save(filePath);

    for (auto vm_index=0; vm_index<chapters_navigate_treemodel->rowCount(); ++vm_index) {
        auto item = chapters_navigate_treemodel->item(vm_index);
        auto volume_node = static_cast<ChaptersItem*>(item);
        auto struct_volume_handle = desp_tree->volumeAt(vm_index);

        for (auto chp_index=0; chp_index<volume_node->rowCount(); ++chp_index) {
            auto chp_item = volume_node->child(chp_index);
            auto chapter_node = static_cast<ChaptersItem*>(chp_item);

            // 检测文件是否打开
            if(!opening_documents.contains(chapter_node))
                continue;

            auto pak = opening_documents.value(chapter_node);
            // 检测文件是否修改
            if(pak.first->isModified()){
                auto struct_chapter_node = desp_tree->chapterAt(struct_volume_handle, chp_index);
                QString file_canonical_path = desp_tree->chapterCanonicalFilePath(struct_chapter_node);

                QFile file(file_canonical_path);
                if(!file.open(QIODevice::Text|QIODevice::WriteOnly))
                    throw new WsException("保存内容过程，目标无法打开："+ file_canonical_path);

                QTextStream txt_out(&file);
                QString file_encoding = desp_tree->chapterTextEncoding(struct_chapter_node);
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
    try {
        FStruct::NHandle target = desp_tree->volumeAt(before);
        auto volume_new = desp_tree->insertVolume(target, gName, "");
        insert_volume(volume_new, before);
    } catch (WsException *) {
        auto volume_new = desp_tree->insertVolume(FStruct::NHandle(), gName, "");
        insert_volume(volume_new, desp_tree->volumeCount());
    }
}

void NovelHost::insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName)
{
    if(!vmIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(vmIndex);
    auto outline_volume_node = static_cast<OutlinesItem*>(node);
    auto volume_struct_node = desp_tree->volumeAt(node->row());

    int knode_count = desp_tree->keystoryCount(volume_struct_node);
    FStruct::NHandle keystory_node = desp_tree->insertKeystory(volume_struct_node, before, kName, "");
    if(before >= knode_count)
        outline_volume_node->appendRow(new OutlinesItem(keystory_node));
    else
        outline_volume_node->insertRow(before, new OutlinesItem(keystory_node));
}

void NovelHost::insertPoint(const QModelIndex &kIndex, int before, const QString &pName)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(kIndex);          // keystory-index
    auto struct_keystory_node = _locate_outline_handle_via(node);
    FStruct::NHandle point_node = desp_tree->insertPoint(struct_keystory_node, before, pName, "");

    int points_count = desp_tree->pointCount(struct_keystory_node);
    if(before >= points_count)
        node->appendRow(new OutlinesItem(point_node));
    else
        node->insertRow(before, new OutlinesItem(point_node));
}

void NovelHost::appendForeshadow(const QModelIndex &kIndex, const QString &fName,
                                 const QString &desp, const QString &desp_next)
{
    if(!kIndex.isValid())
        throw new WsException("输入modelindex无效");

    auto node = outline_navigate_treemodel->itemFromIndex(kIndex);          // keystory
    auto parent = node->parent();                                   // volume
    auto struct_volume_node = desp_tree->volumeAt(parent->row());
    auto struct_keystory_node = desp_tree->keystoryAt(struct_volume_node, node->row());

    desp_tree->appendForeshadow(struct_keystory_node, fName, desp, desp_next);
}

void NovelHost::removeOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("指定modelindex无效");

    auto item = outline_navigate_treemodel->itemFromIndex(outlineNode);
    auto pnode = item->parent();

    if(!pnode){
        auto struct_node = desp_tree->volumeAt(item->row());
        desp_tree->removeHandle(struct_node);

        outline_navigate_treemodel->removeRow(item->row());
        chapters_navigate_treemodel->removeRow(item->row());
    }
    else {
        auto handle = _locate_outline_handle_via(item);
        desp_tree->removeHandle(handle);

        pnode->removeRow(item->row());
    }
}

void NovelHost::setCurrentOutlineNode(const QModelIndex &outlineNode)
{
    if(!outlineNode.isValid())
        throw new WsException("传入的outlinemodelindex无效");

    auto current = outline_navigate_treemodel->itemFromIndex(outlineNode);
    FStruct::NHandle struct_one = _locate_outline_handle_via(current);

    // 设置当前卷节点，填充卷细纲内容
    set_current_volume_outlines(struct_one);
    emit currentVolumeActived();

    // 统计本卷宗下所有构建伏笔及其状态  名称，吸附状态，前描述，后描述，吸附章节、源剧情
    sum_foreshadows_under_volume(current_volume_node);

    // 统计至此卷宗前未闭合伏笔及本卷闭合状态  名称、闭合状态、前描述、后描述、闭合章节、源剧情、源卷宗
    sum_foreshadows_until_volume_remains(current_volume_node);
}

QList<QPair<QString, QModelIndex>> NovelHost::chaptersKeystorySum(const QModelIndex &chaptersNode) const
{
    if(!chaptersNode.isValid())
        return QList<QPair<QString, QModelIndex>>();

    QList<QPair<QString, QModelIndex>> hash;
    auto item = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    auto volume = item;
    if(item->parent()){ // chapter-node
        volume = item->parent();
    }
    auto outlines_volume_item = outline_navigate_treemodel->item(item->row());
    for (int var = 0; var < outlines_volume_item->rowCount(); ++var) {
        auto one_item = outlines_volume_item->child(var);
        hash << qMakePair(one_item->text(), one_item->index());
    }
    return hash;
}

QList<QPair<QString, QModelIndex> > NovelHost::outlinesKeystorySum(const QModelIndex &outlinesNode) const
{
    if(!outlinesNode.isValid())
        return QList<QPair<QString,QModelIndex>>();

    auto selected_item = outline_navigate_treemodel->itemFromIndex(outlinesNode);
    auto struct_node = _locate_outline_handle_via(selected_item);
    QList<QPair<QString,QModelIndex>> result;

    if(struct_node.nType() == FStruct::NHandle::Type::VOLUME){
        for (int var = 0; var < selected_item->rowCount(); ++var) {
            auto one = selected_item->child(var);
            result<< qMakePair(one->text(), one->index());
        }
        return result;
    }

    QStandardItem *keystory_item = selected_item;
    if (struct_node.nType() == FStruct::NHandle::Type::POINT) {
        keystory_item = selected_item->parent();
    }

    result << qMakePair(keystory_item->text(), keystory_item->index());
    return result;
}

void NovelHost::checkChaptersRemoveEffect(const QModelIndex &chpsIndex, QList<QString> &msgList) const
{
    if(!chpsIndex.isValid())
        return;

    auto item = chapters_navigate_treemodel->itemFromIndex(chpsIndex);
    auto struct_node = _locate_outline_handle_via(item);
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


FStruct::NHandle NovelHost:: _locate_outline_handle_via(QStandardItem *outline_item) const
{
    QList<QStandardItem*> stack;
    while (outline_item) {
        stack.insert(0, outline_item);
        outline_item = outline_item->parent();
    }

    auto volume_node = desp_tree->volumeAt(stack.at(0)->row());
    if(stack.size() == 1){
        return volume_node;
    }

    auto keystory_node = desp_tree->keystoryAt(volume_node, stack.at(1)->row());
    if(stack.size() == 2){
        return keystory_node;
    }

    auto point_node = desp_tree->pointAt(keystory_node, stack.at(2)->row());
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
        desp_tree->setAttr(struct_node, "desp", description);
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
    int volume_index = desp_tree->handleIndex(current_volume_node);
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
    desp_tree->setAttr(current_chapter_node, "desp", content);
}


void NovelHost::insert_content_at_document(QTextCursor cursor, OutlinesItem *outline_node)
{
    auto struct_node = _locate_outline_handle_via(outline_node);

    QTextBlockFormat title_block_format;
    QTextCharFormat title_char_format;
    WsBlockData *data = nullptr;

    switch (struct_node.nType()) {
        case FStruct::NHandle::Type::VOLUME:
            config_host.volumeTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), FStruct::NHandle::Type::VOLUME);
            break;
        case FStruct::NHandle::Type::KEYSTORY:
            config_host.keystoryTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), FStruct::NHandle::Type::KEYSTORY);
            break;
        case FStruct::NHandle::Type::POINT:
            config_host.pointTitleFormat(title_block_format, title_char_format);
            data = new WsBlockData(outline_node->index(), FStruct::NHandle::Type::POINT);
            break;
        default:
            break;
    }
    cursor.setBlockFormat(title_block_format);
    cursor.setBlockCharFormat(title_char_format);
    cursor.insertText(struct_node.attr("title"));
    cursor.block().setUserData(data);

    cursor.insertBlock();
    QTextBlockFormat text_block_format;
    QTextCharFormat text_char_format;
    config_host.textFormat(text_block_format, text_char_format);
    cursor.setBlockFormat(text_block_format);
    cursor.setBlockCharFormat(text_char_format);
    cursor.insertText(struct_node.attr("desp"));
    cursor.insertBlock();

    for (int var=0; var < outline_node->rowCount(); ++var) {
        auto child = outline_node->child(var);
        insert_content_at_document(cursor, static_cast<OutlinesItem*>(child));
    }
}

void NovelHost::sum_foreshadows_under_volume(const FStruct::NHandle &volume_node)
{
    desp_tree->checkNandleValid(volume_node, FStruct::NHandle::Type::VOLUME);
    foreshadows_under_volume_present->clear();
    foreshadows_under_volume_present->setHorizontalHeaderLabels(
                QStringList() << "伏笔名称" << "吸附？" << "描述1" << "描述2" << "吸附章节" << "剧情起点");

    QList<FStruct::NHandle> foreshadows_sum;
    // 获取所有伏笔节点
    auto keystory_count = desp_tree->keystoryCount(volume_node);
    for (int var = 0; var < keystory_count; ++var) {
        auto keystory_one = desp_tree->keystoryAt(volume_node, var);

        int foreshadow_count = desp_tree->foreshadowCount(keystory_one);
        for (int var2 = 0; var2 < foreshadow_count; ++var2) {
            auto foreshadow_one = desp_tree->foreshadowAt(keystory_one, var2);
            foreshadows_sum << foreshadow_one;
        }
    }

    // 填充伏笔表格模型数据
    for(int var=0; var<foreshadows_sum.size(); ++var){
        auto foreshadow_one = foreshadows_sum.at(var);
        QList<QStandardItem*> row;
        row << new QStandardItem(foreshadow_one.attr( "title"));
        row << new QStandardItem("悬空");
        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));
        row << new QStandardItem("无");

        auto keystory = desp_tree->parentHandle(foreshadow_one);
        row << new QStandardItem(keystory.attr( "title"));

        foreshadows_under_volume_present->appendRow(row);
    }

    // 汇聚伏笔吸附信息
    QList<FStruct::NHandle> shadowstart_list;
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
                foreshadows_under_volume_present->item(var, 1)->setText("吸附");

                auto chapter_one = desp_tree->parentHandle(start_one);
                foreshadows_under_volume_present->item(var, 4)->setText(chapter_one.attr( "title"));
            }
        }
    }
}

void NovelHost::sum_foreshadows_until_volume_remains(const FStruct::NHandle &volume_node)
{
    desp_tree->checkNandleValid(volume_node, FStruct::NHandle::Type::VOLUME);
    foreshadows_until_volume_remain_present->clear();
    foreshadows_until_volume_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"闭合章节"<<"剧情源"<<"卷宗名");

    QList<FStruct::NHandle> shadowstart_list;
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

    QList<FStruct::NHandle> shadowstop_list;
    // 不包含本卷，所有伏笔承接信息统计
    for (int volume_index_tmp = 0; volume_index_tmp < volume_index; ++volume_index_tmp) {
        auto volume_one = desp_tree->volumeAt(volume_index_tmp);

        auto chapter_count = desp_tree->chapterCount(volume_one);
        for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
            auto chapter_one = desp_tree->chapterAt(volume_one, chapter_index);

            auto stop_count = desp_tree->shadowstartCount(chapter_one);
            for (int stop_index = 0; stop_index < stop_count; ++stop_index) {
                shadowstop_list << desp_tree->shadowstopAt(chapter_one, stop_index);
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

        QList<QStandardItem*> row;
        row << new QStandardItem(foreshadow_one.attr( "title"));
        row << new QStandardItem("开启");
        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));
        row << new QStandardItem("无");
        row << new QStandardItem(keystory_one.attr( "title"));
        row << new QStandardItem(volume_one.attr( "title"));

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
                foreshadows_until_volume_remain_present->item(var, 1)->setText("闭合");
                auto chapter = desp_tree->parentHandle(stop_one);
                foreshadows_until_volume_remain_present->item(var, 4)->setText(chapter.attr( "title"));
            }
        }
    }
}

void NovelHost::sum_foreshadows_until_chapter_remains(const FStruct::NHandle &chapter_node)
{
    // 累积所有打开伏笔
    // 累积本章节前关闭伏笔
    desp_tree->checkNandleValid(chapter_node, FStruct::NHandle::Type::CHAPTER);
    foreshadows_until_chapter_remain_present->clear();
    foreshadows_until_chapter_remain_present->setHorizontalHeaderLabels(
                QStringList() << "名称"<<"闭合？"<<"描述1"<<"描述2"<<"闭合章节"<<"剧情源"<<"卷宗名");
    QList<FStruct::NHandle> shadowstart_list;
    QList<FStruct::NHandle> shadowstop_list;
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

        QList<QStandardItem*> row;
        row << new QStandardItem(foreshadow_one.attr( "title"));
        row << new QStandardItem("开启");
        row << new QStandardItem(foreshadow_one.attr( "desp"));
        row << new QStandardItem(foreshadow_one.attr( "desp_next"));
        row << new QStandardItem("无");
        row << new QStandardItem(keystory_one.attr( "title"));
        row << new QStandardItem(volume_one.attr( "title"));

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
            auto close_path = close_one.attr( "title");

            if(open_path == close_path){
                foreshadows_until_chapter_remain_present->item(row_index, 1)->setText("闭合");
                auto chapter = desp_tree->parentHandle(close_one);
                foreshadows_until_chapter_remain_present->item(row_index, 4)->setText(chapter.attr( "title"));
            }
        }
    }
}


// msgList : [type](target)<keys-to-target>msg-body
void NovelHost::_check_remove_effect(const FStruct::NHandle &target, QList<QString> &msgList) const
{
    if(target.nType() == FStruct::NHandle::Type::POINT){
        return;
    }

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
            _check_remove_effect(foreshadow, msgList);
        }
        return;
    }

    if(target.nType() == FStruct::NHandle::Type::VOLUME){
        auto keystory_count = desp_tree->keystoryCount(target);
        for (int var = 0; var < keystory_count; ++var) {
            auto keystory = desp_tree->keystoryAt(target, var);
            _check_remove_effect(keystory, msgList);
        }

        auto chapter_count = desp_tree->chapterCount(target);
        for (int var = 0; var < chapter_count; ++var) {
            auto chapter = desp_tree->chapterAt(target, var);
            _check_remove_effect(chapter, msgList);
        }
    }

    if(target.nType() == FStruct::NHandle::Type::SHADOWSTART){
        auto target_path = target.attr("target");
        auto struct_chapter = desp_tree->parentHandle(target);

        msgList << "[warning](foreshadow)<" + target_path + ">指定伏笔埋设将移除，伏笔将悬空，可能影响后续伏笔内容承接";
        msgList << "[error](chapter)<"+desp_tree->chapterKeysPath(target)+">伏笔埋设将移除，章节内容可能失效";

        while (struct_chapter.isValid()) {
            auto one = desp_tree->findShadowstop(struct_chapter, target_path);
            if(one.isValid()){
                msgList << "[error](chapter)<"+desp_tree->chapterKeysPath(struct_chapter)+">指定章节作为伏笔承接，内容可能失效.";
                break;
            }

            struct_chapter = desp_tree->nextChapterOfFStruct(struct_chapter);
        }
        return;
    }

    if(target.nType() == FStruct::NHandle::Type::SHADOWSTOP){
        auto target_path = target.attr("target");
        msgList << "[warning](foreshadow)<"+target_path+">指定伏笔承接将被移除，伏笔成打开状态，影响伏笔接续。";

        auto struct_chapter = desp_tree->parentHandle(target);
        msgList << "(error)[chapter]<"+desp_tree->chapterKeysPath(struct_chapter)+">指定章节作为伏笔承接，可能失效。";
        return;
    }

    if(target.nType() == FStruct::NHandle::Type::CHAPTER){
        // 校验伏笔吸附事项，校验影响
        auto start_count = desp_tree->shadowstartCount(target);
        for (int var=0; var<start_count; ++var) {
            auto start_ins = desp_tree->shadowstartAt(target, var);
            _check_remove_effect(start_ins, msgList);
        }
        // 校验伏笔承接事项，校验影响
        auto stop_count = desp_tree->shadowstopCount(target);
        for (int var = 0; var < stop_count; ++var) {
            auto stop_ins = desp_tree->shadowstopAt(target, var);
            _check_remove_effect(stop_ins, msgList);
        }
    }
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

void NovelHost::removeChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("chaptersNodeIndex无效");

    auto chapter = chapters_navigate_treemodel->itemFromIndex(chaptersNode);

    // 卷宗节点管理同步
    if(!chapter->parent()){
        int row = chapter->row();
        auto struct_volume = desp_tree->volumeAt(row);
        outline_navigate_treemodel->removeRow(row);
        chapters_navigate_treemodel->removeRow(row);

        desp_tree->removeHandle(struct_volume);
    }
    // 章节节点
    else {
        auto volume = chapter->parent();
        auto struct_volume = desp_tree->volumeAt(volume->row());
        auto struct_chapter = desp_tree->chapterAt(struct_volume, chapter->row());

        volume->removeRow(chapter->row());
        desp_tree->removeHandle(struct_chapter);
    }
}

void NovelHost::setCurrentChaptersNode(const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid())
        throw new WsException("传入的chaptersindex无效");

    auto item = chapters_navigate_treemodel->itemFromIndex(chaptersNode);
    FStruct::NHandle node;
    if(item->parent()){     // 选中章节节点
        auto struct_volume = desp_tree->volumeAt(item->parent()->row());
        node = desp_tree->chapterAt(struct_volume, item->row());
    }
    else {                  // 选中卷宗节点
        node = desp_tree->volumeAt(item->row());
    }

    set_current_volume_outlines(node);
    emit currentVolumeActived();

    // 统计本卷宗下所有构建伏笔及其状态  名称，吸附状态，前描述，后描述，吸附章节、源剧情
    sum_foreshadows_under_volume(current_volume_node);
    sum_foreshadows_until_volume_remains(current_volume_node);

    if(node.nType() != FStruct::NHandle::Type::CHAPTER){
        return;
    }

    current_chapter_node = node;
    emit currentChaptersActived();
    // 统计至此章节前未闭合伏笔及本章闭合状态  名称、闭合状态、前描述、后描述、闭合章节、源剧情、源卷宗
    sum_foreshadows_until_chapter_remains(current_chapter_node);
    disconnect(chapter_outlines_present,    &QTextDocument::contentsChanged,
               this,   &NovelHost::listen_chapter_outlines_description_change);
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
    if(opening_documents.contains(static_cast<ChaptersItem*>(item))){
        auto doc = opening_documents.value(static_cast<ChaptersItem*>(item)).first;
        auto title = node.attr( "title");
        emit documentPrepared(doc, title);
        return;
    }

    auto content = chapterTextContent(item->index());
    auto doc = new QTextDocument();
    QTextBlockFormat blockformat;
    QTextCharFormat charformat;
    QTextFrameFormat frameformat;
    config_host.textFrameFormat(frameformat);
    config_host.textFormat(blockformat, charformat);
    doc->rootFrame()->setFrameFormat(frameformat);

    QTextCursor cursor(doc);
    cursor.setBlockFormat(blockformat);
    cursor.setBlockCharFormat(charformat);
    cursor.insertText(content==""?"章节内容为空":content);

    doc->setModified(false);
    doc->clearUndoRedoStacks();
    doc->setUndoRedoEnabled(true);
    auto render = new WordsRender(doc, config_host);
    opening_documents.insert(static_cast<ChaptersItem*>(item), qMakePair(doc, render));
    connect(doc, &QTextDocument::contentsChanged, static_cast<ChaptersItem*>(item),  &ChaptersItem::calcWordsCount);
    emit documentPrepared(doc, node.attr( "title"));
}

void NovelHost::refreshWordsCount()
{
    for (int num=0; num < chapters_navigate_treemodel->rowCount(); ++num) {
        auto volume_title_node = chapters_navigate_treemodel->item(num);
        static_cast<ChaptersItem*>(volume_title_node)->calcWordsCount();
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

    auto item = chapters_navigate_treemodel->itemFromIndex(index);
    auto parent = item->parent();
    if(!parent) // 卷节点不获取内容
        return "";

    auto refer_node = static_cast<ChaptersItem*>(item);
    if(opening_documents.contains(refer_node)){
        auto pack = opening_documents.value(refer_node);
        auto doc = pack.first;
        return doc->toPlainText();
    }

    auto volume_symbo = desp_tree->volumeAt(parent->row());
    auto chapter_symbo = desp_tree->chapterAt(volume_symbo, refer_node->row());
    QString file_path = desp_tree->chapterCanonicalFilePath(chapter_symbo);
    QString fencoding = desp_tree->chapterTextEncoding(chapter_symbo);

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

void NovelHost::outlines_node_title_changed(QStandardItem *item){
    auto struct_node = _locate_outline_handle_via(item);
    desp_tree->setAttr(struct_node, "title", item->text());
}

void NovelHost::chapters_node_title_changed(QStandardItem *item){
    if(item->parent() && !item->column() )  // chapter-node 而且 不是计数节点
    {
        auto volume_struct = desp_tree->volumeAt(item->parent()->row());
        auto struct_chapter = desp_tree->chapterAt(volume_struct, item->row());
        desp_tree->setAttr(struct_chapter, "title", item->text());
    }
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const FStruct::NHandle &volume_handle, int index)
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
    desp_tree->resetNovelDescription(content);
}

// 向卷宗细纲填充内容
void NovelHost::set_current_volume_outlines(const FStruct::NHandle &node_under_volume){
    if(!node_under_volume.isValid())
        throw new WsException("传入节点无效");

    if(node_under_volume.nType() == FStruct::NHandle::Type::VOLUME){
        current_volume_node = node_under_volume;

        disconnect(volume_outlines_present,  &QTextDocument::contentsChange,
                   this,   &NovelHost::listen_volume_outlines_description_change);
        disconnect(volume_outlines_present,  &QTextDocument::blockCountChanged,
                   this,    &NovelHost::listen_volume_outlines_structure_changed);

        volume_outlines_present->clear();
        QTextCursor cursor(volume_outlines_present);

        int volume_index = desp_tree->handleIndex(node_under_volume);
        auto volume_node = outline_navigate_treemodel->item(volume_index);
        insert_content_at_document(cursor, static_cast<OutlinesItem*>(volume_node));
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

ChaptersItem::ChaptersItem(NovelHost &host, const FStruct::NHandle &refer, bool isGroup)
    :host(host)
{
    setText(refer.attr("title"));

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
        QString content = host.chapterTextContent(index());

        auto pitem = QStandardItem::parent();
        auto cnode = pitem->child(row(), 1);
        cnode->setText(QString("%1").arg(host.calcValidWordsCount(content)));
    }
}

// highlighter collect ===========================================================================
WordsRender::WordsRender(QTextDocument *target, ConfigHost &config)
    :QSyntaxHighlighter (target), config(config){}

WordsRender::~WordsRender(){}

void WordsRender::highlightBlock(const QString &text)
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

void FStruct::openFile(const QString &filePath)
{
    filepath_stored = filePath;

    QFile file(filePath);
    if(!file.exists())
        throw new WsException("读取过程指定文件路径不存在:"+filePath);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw new WsException("读取过程指定文件打不开："+filePath);

    int rown,coln;
    QString temp;
    if(!struct_dom_store.setContent(&file, false, &temp, &rown, &coln))
        throw new WsException(QString(temp+"(r:%1,c:%2)").arg(rown, coln));
}


void FStruct::setAttr(FStruct::NHandle &handle, const QString &name, const QString &value)
{
    handle.setAttr(name, value);
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
    auto struct_dom = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    auto elm = find_subelm_at_index(struct_dom, "volume", index);
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
        before.elm_stored.parentNode().insertBefore(newdom, before.elm_stored);
    }

    return aone;
}

int FStruct::keystoryCount(const FStruct::NHandle &vmNode) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    auto list = vmNode.elm_stored.elementsByTagName("keystory");
    return list.size();
}

FStruct::NHandle FStruct::keystoryAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    QDomElement elm = find_subelm_at_index(vmNode.elm_stored, "keystory", index);
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
    auto points_elm = struct_dom_store.createElement("points");
    auto keystory_elm = struct_dom_store.createElement("foreshadows");
    ndom.appendChild(points_elm);
    ndom.appendChild(keystory_elm);
    NHandle one(ndom, NHandle::Type::KEYSTORY);
    one.setAttr("key", unique_key);
    one.setAttr("title", title);
    one.setAttr("desp", description);

    if(before >= num){
        vmNode.elm_stored.appendChild(ndom);
    }
    else {
        NHandle _before = keystoryAt(vmNode, before);
        vmNode.elm_stored.insertBefore(ndom, _before.elm_stored);
    }

    return one;
}

int FStruct::pointCount(const FStruct::NHandle &knode) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto list = knode.elm_stored.elementsByTagName("points").at(0).childNodes();
    return list.size();
}

FStruct::NHandle FStruct::pointAt(const FStruct::NHandle &knode, int index) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto points_elm = knode.elm_stored.firstChildElement("points");
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
        knode.elm_stored.firstChildElement("points").appendChild(dom);
    }
    else {
        NHandle _before = pointAt(knode, before);
        knode.elm_stored.firstChildElement("points").insertBefore(dom, _before.elm_stored);
    }

    return one;
}

int FStruct::foreshadowCount(const FStruct::NHandle &knode) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshodows_node = knode.elm_stored.firstChildElement("foreshadows");
    return foreshodows_node.elementsByTagName("foreshadow").size();
}

FStruct::NHandle FStruct::foreshadowAt(const FStruct::NHandle &knode, int index) const
{
    checkNandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshadows_node = knode.elm_stored.firstChildElement("foreshadows");
    QDomElement elm = find_subelm_at_index(foreshadows_node, "foreshadow", index);
    return NHandle(elm, NHandle::Type::FORESHADOW);
}

FStruct::NHandle FStruct::findForeshadow(const QString &keysPath) const
{
    int volume_count = volumeCount();
    for (int vindex = 0; vindex < volume_count; ++vindex) {
        auto volume_one = volumeAt(vindex);

        // 找到指定卷宗节点
        int keystory_count = keystoryCount(volume_one);
        for (int kindex = 0; kindex < keystory_count; ++kindex) {
            auto keystory_one = keystoryAt(volume_one, kindex);

            int foreshadow_count = foreshadowCount(keystory_one);
            for (int findex = 0; findex < foreshadow_count; ++findex) {
                auto foreshadow_one = foreshadowAt(keystory_one, findex);

                if(foreshadowKeysPath(foreshadow_one) == keysPath)
                    return foreshadow_one;
            }
        }
    }

    return NHandle();
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

    auto foreshadows_node = knode.elm_stored.firstChildElement("foreshadows");
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

    return vmNode.elm_stored.elementsByTagName("chapter").size();
}

FStruct::NHandle FStruct::chapterAt(const FStruct::NHandle &vmNode, int index) const
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);
    QDomElement elm = find_subelm_at_index(vmNode.elm_stored, "chapter", index);
    return NHandle(elm, NHandle::Type::CHAPTER);
}

FStruct::NHandle FStruct::insertChapter(FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkNandleValid(vmNode, NHandle::Type::VOLUME);

    QList<QString> ckeys;
    auto handle = firstChapterOfFStruct();
    while (handle.isValid()) {
        ckeys << handle.attr("key");
        handle = nextChapterOfFStruct(handle);
    }

    int num = chapterCount(vmNode);
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
        vmNode.elm_stored.appendChild(elm);
    }
    else {
        NHandle _before = chapterAt(vmNode, before);
        vmNode.elm_stored.insertBefore(elm, _before.elm_stored);
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
    return chpNode.elm_stored.elementsByTagName("shadow-start").size();
}

FStruct::NHandle FStruct::shadowstartAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNandleValid(chpNode, FStruct::NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.elm_stored, "shadow-start", index);
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

    chpNode.elm_stored.appendChild(elm);
    return one;
}

int FStruct::shadowstopCount(const FStruct::NHandle &chpNode) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);
    return chpNode.elm_stored.elementsByTagName("shadow-stop").size();
}

FStruct::NHandle FStruct::shadowstopAt(const FStruct::NHandle &chpNode, int index) const
{
    checkNandleValid(chpNode, NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.elm_stored, "shadow-stop", index);
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

    chpNode.elm_stored.appendChild(elm);
    return one;
}

FStruct::NHandle FStruct::parentHandle(const FStruct::NHandle &base) const
{
    if(!base.isValid())
        throw new WsException("传入无效节点");

    switch (base.nType()) {
        case NHandle::Type::SHADOWSTOP:
        case NHandle::Type::SHADOWSTART:
            return NHandle(base.elm_stored.parentNode().toElement(), NHandle::Type::CHAPTER);
        case NHandle::Type::POINT:
        case NHandle::Type::FORESHADOW:
            return NHandle(base.elm_stored.parentNode().parentNode().toElement(),
                           NHandle::Type::KEYSTORY);
        case NHandle::Type::KEYSTORY:
        case NHandle::Type::CHAPTER:
            return NHandle(base.elm_stored.parentNode().toElement(), NHandle::Type::VOLUME);
        default:
            throw new WsException("无有效父节点");
    }
}

int FStruct::handleIndex(const FStruct::NHandle &node) const
{
    if(!node.isValid())
        throw new WsException("传入无效节点");

    auto dom = node.elm_stored;
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
        if(file.exists() &&!file.remove())
            throw new WsException("文件系统异常，移除文件失败："+filepath);
    }

    if(node.nType() == NHandle::Type::VOLUME){
        int count = chapterCount(node);

        for(int var=0; var < count ; ++var){
            NHandle _node = chapterAt(node, var);
            removeHandle(_node);
        }
    }

    auto parent = node.elm_stored.parentNode();
    if(parent.isNull())
        throw new WsException("父节点非法");

    parent.removeChild(node.elm_stored);
}

FStruct::NHandle FStruct::firstChapterOfFStruct() const
{
    int volume_count = volumeCount();
    for (int v_index=0; v_index<volume_count; ++v_index) {
        auto volume_one = volumeAt(v_index);
        if(chapterCount(volume_one))    // 第一个chapter不为零的volume节点
            return chapterAt(volume_one, 0);
    }

    return NHandle();
}

FStruct::NHandle FStruct::lastChapterOfStruct() const
{
    auto volume_count = volumeCount();
    for (int v_index = volume_count-1; v_index >= 0; --v_index) {
        auto volume_one = volumeAt(v_index);
        int chapter_count = 0;
        if((chapter_count = chapterCount(volume_one))){
            return chapterAt(volume_one, chapter_count-1);
        }
    }

    return NHandle();
}

FStruct::NHandle FStruct::nextChapterOfFStruct(const FStruct::NHandle &chapterIns) const
{
    auto volume_this = parentHandle(chapterIns);
    auto chapter_index_this = handleIndex(chapterIns);
    auto chapter_count_this = chapterCount(volume_this);

    if(chapter_index_this == chapter_count_this-1){                     // 指向本卷宗最后一个节点
        auto volume_index_this = handleIndex(volume_this);              // 本卷宗索引
        auto volume_count = volumeCount();

        // 转到下一个volume
        for (volume_index_this += 1; volume_index_this < volume_count; ++volume_index_this) {
            auto volume_one = volumeAt(volume_index_this);

            if(chapterCount(volume_one))
                return chapterAt(volume_one, 0);
        }

        return NHandle();
    }
    else {
        return chapterAt(volume_this, chapter_index_this+1);
    }
}

FStruct::NHandle FStruct::previousChapterOfFStruct(const FStruct::NHandle &chapterIns) const
{
    auto volume_this = parentHandle(chapterIns);
    auto chapter_index_this = handleIndex(chapterIns);

    if(!chapter_index_this) {    // chapter位于卷首位置
        auto volume_index_this = handleIndex(volume_this);
        for ( volume_index_this-=1; volume_index_this >= 0; --volume_index_this) {
            auto volume_one = volumeAt(volume_index_this);

            auto chapter_count = 0;
            if((chapter_count = chapterCount(volume_one)))
                return chapterAt(volume_one, chapter_count-1);
        }
        return NHandle();
    }
    else {
        return chapterAt(volume_this, chapter_index_this-1);
    }
}

void FStruct::checkNandleValid(const FStruct::NHandle &node, FStruct::NHandle::Type type) const
{
    if(node.nType() != type)
        throw new WsException("传入节点类型错误");

    if(!node.isValid())
        throw new WsException("传入节点已失效");
}

QDomElement FStruct::find_subelm_at_index(const QDomElement &pnode, const QString &tagName, int index) const
{
    if(index >= 0){
        auto elm = pnode.firstChildElement(tagName);
        while (!elm.isNull()) {
            if(!index){
                return elm;
            }

            index--;
            elm = elm.nextSiblingElement(tagName);
        }
    }

    throw new WsException(QString("在" + pnode.tagName() + "元素中查找"+ tagName+"，指定index超界：%1").arg(index));
}



FStruct::NHandle::NHandle()
    :type_stored(Type::VOLUME){}

FStruct::NHandle::NHandle(const FStruct::NHandle &other)
    :elm_stored(other.elm_stored), type_stored(other.type_stored){}


FStruct::NHandle &FStruct::NHandle::operator=(const FStruct::NHandle &other)
{
    elm_stored = other.elm_stored;
    type_stored = other.type_stored;
    return *this;
}

bool FStruct::NHandle::operator==(const FStruct::NHandle &other) const
{
    return elm_stored == other.elm_stored && type_stored == other.type_stored;
}

FStruct::NHandle::Type FStruct::NHandle::nType() const
{
    return type_stored;
}

bool FStruct::NHandle::isValid() const
{
    return !elm_stored.isNull();
}

QString FStruct::NHandle::attr(const QString &name) const{
    return elm_stored.attribute(name);
}

void FStruct::NHandle::setAttr(const QString &name, const QString &value)
{
    elm_stored.setAttribute(name, value);
}


FStruct::NHandle::NHandle(QDomElement elm, FStruct::NHandle::Type type)
    :elm_stored(elm), type_stored(type){}

OutlinesItem::OutlinesItem(const FStruct::NHandle &refer)
{
    setText(refer.attr("title"));
    switch (refer.nType()) {
        case FStruct::NHandle::Type::POINT:
            setIcon(QIcon(":/outlines/icon/点.png"));
            break;
        case FStruct::NHandle::Type::VOLUME:
            setIcon(QIcon(":/outlines/icon/卷.png"));
            break;
        case FStruct::NHandle::Type::CHAPTER:
            setIcon(QIcon(":/outlines/icon/章.png"));
            break;
        case FStruct::NHandle::Type::KEYSTORY:
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
        case WsBlockData::Type::KEYSTORY:
            config.keystoryTitleFormat(bformat, cformat);
            break;
        case WsBlockData::Type::POINT:
            config.pointTitleFormat(bformat, cformat);
            break;
        default:
            return;
    }

    setFormat(0, text.length(), cformat);
}
