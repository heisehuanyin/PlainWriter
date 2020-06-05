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
      desp_node(nullptr),
      current_outline_node(nullptr),
      outline_tree_model(new QStandardItemModel(this)),
      foreshadows_present(new QStandardItemModel(this)),
      result_enter_model(new QStandardItemModel(this)),
      chapters_navigate_model(new QStandardItemModel(this)),
      keystory_points_model(new QStandardItemModel(this)),
      novel_description_present(new QTextDocument(this)),
      volume_description_present(new QTextDocument(this)),
      node_description_present(new QTextDocument(this))
{

}







NovelHost::~NovelHost()
{

}

int NovelHost::loadDescription(QString &err, FStruct *desp)
{
    // save description structure
    this->desp_node = desp;

    int code;
    for (int volume_index = 0; volume_index < desp_node->volumeCount(); ++volume_index) {
        FStruct::NHandle volume_node;
        if((code = desp->volumeAt(err, volume_index, volume_node)))
            return code;

        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int keystory_count = 0;
        if((code = desp->keystoryCount(err, volume_node, keystory_count)))
            return keystory_count;
        for (int keystory_index = 0; keystory_index < keystory_count; ++keystory_index) {
            FStruct::NHandle keystory_node;
            if((code = desp->keystoryAt(err, volume_node, keystory_index, keystory_node)))
                return code;

            auto ol_keystory_item = new OutlinesItem(keystory_node);
            outline_volume_node->appendRow(ol_keystory_item);

            int points_count=0;
            if((code = desp->pointCount(err, keystory_node, points_count)))
                return code;
            for (int points_index = 0; points_index < points_count; ++points_index) {
                FStruct::NHandle point_node;
                if((code = desp->pointAt(err, keystory_node, points_index, point_node)))
                    return code;

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }

            int chapter_count =0;
            if((code = desp->chapterCount(err, keystory_node, chapter_count)))
                return code;
            for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
                FStruct::NHandle chapter_node;
                if((code = desp->chapterAt(err, keystory_node, chapter_index, chapter_node)))
                    return code;

                QList<QStandardItem*> node_navigate_row;
                auto node_navigate_chapter_node = new ChaptersItem(*this, chapter_node);
                node_navigate_row << node_navigate_chapter_node;
                node_navigate_row << new QStandardItem("-");
                node_navigate_volume_node->appendRow(node_navigate_row);
            }
        }
    }

    novel_description_present->setPlainText(desp_node->novelDescription());
    novel_description_present->clearUndoRedoStacks();
    connect(novel_description_present,  &QTextDocument::contentsChanged,    this,   &NovelHost::resetNovelDescription);

    return 0;
}

int NovelHost::save(QString &err, const QString &filePath)
{
    int xret;
    if((xret = desp_node->save(err, filePath)))
        return xret;

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
                QString file_canonical_path;
                auto target = chapter_node->getRefer();
                if((xret = desp_node->chapterCanonicalFilePath(err, target, file_canonical_path)))
                    return xret;

                QFile file(file_canonical_path);
                if(!file.open(QIODevice::Text|QIODevice::WriteOnly)){
                    err = "保存内容过程，目标无法打开："+ file_canonical_path;
                    return -1;
                }

                QTextStream txt_out(&file);
                QString file_encoding;
                if((xret = desp_node->chapterTextEncoding(err, target, file_encoding)))
                    return xret;
                txt_out.setCodec(file_encoding.toLocal8Bit());

                QString content;
                if((xret = chapterTextContent(err, chapter_node->index(), content)))
                    return xret;

                txt_out << content;
                txt_out.flush();
                file.flush();
                file.close();
            }

        }
    }

    return 0;
}

QString NovelHost::novelTitle() const
{
    return desp_node->novelTitle();
}

void NovelHost::resetNovelTitle(const QString &title)
{
    desp_node->resetNovelTitle(title);
}

QStandardItemModel *NovelHost::outlineTree() const
{
    return this->outline_tree_model;
}

int NovelHost::appendVolume(QString &err, const QString &gName)
{
    int code;
    FStruct::NHandle volume_new;
    if((code = desp_node->insertVolume(err, FStruct::NHandle(), gName, "", volume_new)))
        return code;

    insert_volume(volume_new, desp_node->volumeCount());
    return 0;
}

int NovelHost::appendKeystory(QString &err, const QModelIndex &vmIndex, const QString &kName)
{
    if(!vmIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(vmIndex);
    auto volume_tree_node = static_cast<OutlinesItem*>(node);
    auto volume_struct_node = volume_tree_node->getRefer();
    int code;
    if((code = desp_node->checkNValid(err, volume_struct_node, FStruct::NHandle::Type::VOLUME)))
        return code;

    int knode_count=0;
    if((code = desp_node->keystoryCount(err, volume_tree_node->getRefer(), knode_count)))
        return code;

    FStruct::NHandle keystory_node;
    if((code = desp_node->insertKeystory(err, volume_struct_node, knode_count, kName, "", keystory_node)))
        return code;

    volume_tree_node->appendRow(new OutlinesItem(keystory_node));
    return 0;
}

int NovelHost::appendPoint(QString &err, const QModelIndex &kIndex, const QString &pName)
{
    if(!kIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto keystruct_node_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystruct_node_node->getRefer();
    int code;
    if((code = desp_node->checkNValid(err, keystory_struct_node, FStruct::NHandle::Type::KEYSTORY)))
        return code;
    int points_count;
    if((code = desp_node->pointCount(err, keystory_struct_node, points_count)))
        return code;
    FStruct::NHandle point_node;
    if((code = desp_node->insertPoint(err, keystory_struct_node, points_count, pName, "", point_node)))
        return code;

    keystruct_node_node->appendRow(new OutlinesItem(point_node));
    return 0;
}

int NovelHost::appendForeshadow(QString &err, const QModelIndex &kIndex, const QString &fName,
                                const QString &desp, const QString &desp_next)
{
    if(!kIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto keystruct_node_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystruct_node_node->getRefer();
    int code;
    if((code = desp_node->checkNValid(err, keystory_struct_node, FStruct::NHandle::Type::KEYSTORY)))
        return code;
    int foreshadows_count;
    if((code = desp_node->foreshadowCount(err, keystory_struct_node, foreshadows_count)))
        return code;
    FStruct::NHandle foreshadow_node;
    if((code = desp_node->appendForeshadow(err, keystory_struct_node, foreshadows_count,
                                           fName, desp, desp_next, foreshadow_node)))
        return code;
    return 0;
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
    if((code = desp_node->checkNValid(err, refer_keystory_node, FStruct::NHandle::Type::KEYSTORY)))
        return code;
    int shadowstop_count;
    if((code = desp_node->shadowstopCount(err, refer_keystory_node, shadowstop_count)))
        return code;
    FStruct::NHandle shadowstop_node;
    if((code = desp_node->insertShadowstop(err, refer_keystory_node, shadowstop_count,
                                           vKey, kKey, fKey, shadowstop_node)))
        return code;
    return 0;
}

int NovelHost::appendChapter(QString &err, const QModelIndex &outlineKeystoryIndex, const QString &aName)
{
    if(!outlineKeystoryIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(outlineKeystoryIndex);
    auto outline_keystory_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = outline_keystory_node->getRefer();
    int code;
    if((code = desp_node->checkNValid(err, keystory_struct_node, FStruct::NHandle::Type::KEYSTORY)))
        return code;
    int chapter_count;
    if((code = desp_node->chapterCount(err, keystory_struct_node, chapter_count)))
        return code;
    FStruct::NHandle chapter_node;
    if((code = desp_node->insertChapter(err, keystory_struct_node, chapter_count, aName, "", chapter_node)))
        return code;

    auto outline_volume_node = outline_keystory_node->QStandardItem::parent();
    auto chapters_volume_node = chapters_navigate_model->item(outline_volume_node->row());
    chapters_volume_node->appendRow(new ChaptersItem(*this, chapter_node));

    QString file_path;
    if((code = desp_node->chapterCanonicalFilePath(err, chapter_node, file_path)))
        return code;

    QFile target(file_path);
    if(target.exists()){
        err = "软件错误，出现重复文件名："+file_path;
        return -1;
    }

    if(!target.open(QIODevice::WriteOnly|QIODevice::Text)){
        err = "软件错误，指定路径文件无法打开："+file_path;
        return -1;
    }
    target.close();
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
            if((code = desp_node->parentHandle(err, outline_struct_node->getRefer(), volume_node)))
                return code;
            int chapter_count_will_be_remove;
            if((code = desp_node->chapterCount(err, outline_struct_node->getRefer(), chapter_count_will_be_remove)))
                return code;

            int view_volume_index;
            if((code = desp_node->handleIndex(err, volume_node, view_volume_index)))
                return code;
            auto view_volume_entry = chapters_navigate_model->item(view_volume_index);
            for (auto chapter_index=0; chapter_index<chapter_count_will_be_remove; ++chapter_index) {
                FStruct::NHandle chapter_will_be_remove;
                if((code = desp_node->chapterAt(err, outline_struct_node->getRefer(), chapter_index, chapter_will_be_remove)))
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

    if((code = desp_node->removeNodeHandle(err, outline_struct_node->getRefer())))
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
    if((code = one.setAttr("title", title)))
        return code;
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

int NovelHost::get_foreshadows_under_keystory(QString &err, const FStruct::NHandle keystoryNode,
                                              QList<FStruct::NHandle> &resultSum) const
{
    int code;
    int struct_forshadows_count;
    if((code = desp_node->checkNValid(err, keystoryNode, FStruct::NHandle::Type::KEYSTORY)))
        return code;

    if((code = desp_node->foreshadowCount(err, keystoryNode, struct_forshadows_count)))
        return code;
    for (auto var_index=0; var_index<struct_forshadows_count; ++var_index) {
        FStruct::NHandle foreshadow_at;
        if((code = desp_node->foreshadowAt(err, keystoryNode, var_index, foreshadow_at)))
            return code;
        resultSum.append(foreshadow_at);
    }
    return 0;
}

int NovelHost::get_foreshadows_until_this(QString &err, const FStruct::NHandle keystoryOrVolumeNode,
                                          QList<FStruct::NHandle> &resultSum) const
{
    int code;
    if(keystoryOrVolumeNode.nType() == FStruct::NHandle::Type::VOLUME){
        int struct_keystory_count;
        if((code = desp_node->keystoryCount(err, keystoryOrVolumeNode, struct_keystory_count)))
            return code;

        FStruct::NHandle lastKeystory;
        if((code = desp_node->keystoryAt(err, keystoryOrVolumeNode, struct_keystory_count-1, lastKeystory)))
            return code;

        if((code = get_foreshadows_until_this(err, lastKeystory, resultSum)))
            return code;
    }
    else if(keystoryOrVolumeNode.nType() == FStruct::NHandle::Type::KEYSTORY) {
        // 装载本节点下所有伏笔
        if((code = get_foreshadows_under_keystory(err, keystoryOrVolumeNode, resultSum)))
            return code;


        int struct_keystory_index;
        if((code = desp_node->handleIndex(err, keystoryOrVolumeNode, struct_keystory_index)))
            return code;
        FStruct::NHandle struct_volume_node;
        if((code = desp_node->parentHandle(err, keystoryOrVolumeNode, struct_volume_node)))
            return code;

        // 本节点是卷宗节点的首节点
        if(!struct_keystory_index){
            int struct_volume_index;
            if((code = desp_node->handleIndex(err, struct_volume_node, struct_volume_index)))
                return code;

            // 卷宗节点是全书首节点
            if(struct_volume_index){
                if((code = desp_node->volumeAt(err, struct_volume_index-1, struct_volume_node)))
                    return code;
                if((code = get_foreshadows_until_this(err, struct_volume_node, resultSum)))
                    return code;
            }
            else
                return 0;
        }
        else {
            // 向前爬行一个节点
            FStruct::NHandle struct_keystory_node;
            if((code = desp_node->keystoryAt(err, struct_volume_node, struct_keystory_index-1, struct_keystory_node)))
                return code;

            if((code = get_foreshadows_until_this(err, struct_keystory_node, resultSum)))
                return code;
        }
    }

    return 0;
}


int NovelHost::get_shadowstops_under_keystory(QString &err, const FStruct::NHandle keystoryNode,
                                              QList<FStruct::NHandle> &resultSum) const
{
    int code ;
    // 装载本节点下所有伏笔
    int struct_shadowstops_count;
    if((code = desp_node->shadowstopCount(err, keystoryNode, struct_shadowstops_count)))
        return code;
    for (auto var_index=0; var_index<struct_shadowstops_count; ++var_index) {
        FStruct::NHandle shadowstop_at;
        if((code = desp_node->shadowstopAt(err, keystoryNode, var_index, shadowstop_at)))
            return code;
        resultSum.append(shadowstop_at);
    }

    return 0;
}

int NovelHost::get_shadowstops_until_this(QString &err, const FStruct::NHandle keystoryOrVolumeNode,
                                          QList<FStruct::NHandle> &resultSum) const
{
    int code;
    if(keystoryOrVolumeNode.nType() == FStruct::NHandle::Type::VOLUME){
        int struct_keystory_count;
        if((code = desp_node->keystoryCount(err, keystoryOrVolumeNode, struct_keystory_count)))
            return code;

        FStruct::NHandle lastKeystory;
        if((code = desp_node->keystoryAt(err, keystoryOrVolumeNode, struct_keystory_count-1, lastKeystory)))
            return code;

        if((code = get_shadowstops_until_this(err, lastKeystory, resultSum)))
            return code;
    }
    else if(keystoryOrVolumeNode.nType() == FStruct::NHandle::Type::KEYSTORY) {
        // 装载本节点下所有伏笔
        if((code = get_shadowstops_under_keystory(err, keystoryOrVolumeNode, resultSum)))
            return code;


        int struct_keystory_index;
        if((code = desp_node->handleIndex(err, keystoryOrVolumeNode, struct_keystory_index)))
            return code;

        FStruct::NHandle struct_volume_node;
        if((code = desp_node->parentHandle(err, keystoryOrVolumeNode, struct_volume_node)))
            return code;

        // 本节点是卷宗节点的首节点
        if(!struct_keystory_index){
            int struct_volume_index;
            if((code = desp_node->handleIndex(err, struct_volume_node, struct_volume_index)))
                return code;

            // 卷宗节点是全书首节点
            if(struct_volume_index){
                if((code = desp_node->volumeAt(err, struct_volume_index-1, struct_volume_node)))
                    return code;
                if((code = get_shadowstops_until_this(err, struct_volume_node, resultSum)))
                    return code;
            }
            else
                return 0;
        }
        else {
            // 向前爬行一个节点
            FStruct::NHandle struct_keystory_node;
            if((code = desp_node->keystoryAt(err, struct_volume_node, struct_keystory_index-1, struct_keystory_node)))
                return code;

            if((code = get_shadowstops_until_this(err, struct_keystory_node, resultSum)))
                return code;
        }
    }

    return 0;
}


int NovelHost::set_current_outline_node(QString &err, OutlinesItem *node)
{
    int code;
    // 设置当前节点描述文档内容
    disconnect(node_description_present,&QTextDocument::contentsChanged,    this,   &NovelHost::resetCurrentOutlineNodeDescription);
    this->current_outline_node = node;
    auto nn = node->getRefer();
    QString text;
    if((code = nn.attr(err, "desp", text)))
        return code;
    node_description_present->setPlainText(text);
    node_description_present->clearUndoRedoStacks();
    connect(node_description_present,   &QTextDocument::contentsChanged,    this,   &NovelHost::resetCurrentOutlineNodeDescription);

    // 设置伏笔埋设与合并视图内容
    {
        auto _bool = node->getRefer().nType() == FStruct::NHandle::Type::KEYSTORY;
        QList<FStruct::NHandle> resultForeshadows;
        if((code = get_foreshadows_until_this(err, nn, resultForeshadows)))
            return code;

        QList<FStruct::NHandle> resultShadowStops;
        if(_bool){
            int this_node_index;
            if((code = desp_node->handleIndex(err, node->getRefer(), this_node_index)))
                return code;
            FStruct::NHandle volume_node;
            if((code = desp_node->parentHandle(err, node->getRefer(), volume_node)))
                return code;

            if(!this_node_index){

            }
            else {
                FStruct::NHandle next_keystory_node;
                if((code = desp_node->keystoryAt(err, volume_node, )))
            }

        }
        if((code = get_shadowstops_until_this(err, nn, resultShadowStops)))
            return code;

        // 清楚闭合伏笔
        for (int foreshadow_index = 0; foreshadow_index < resultForeshadows.size();) {
            auto foreshadow_one = resultForeshadows.at(foreshadow_index);
            FStruct::NHandle keystory_one, volume_one;
            if((code = desp_node->parentHandle(err, foreshadow_one, keystory_one)))
                return code;
            if((code = desp_node->parentHandle(err, keystory_one, volume_one)))
                return code;
            QString foreshadow_key, keystory_key, volume_key;
            if((code = foreshadow_one.attr(err, "key", foreshadow_key)))
                return code;
            if((code = keystory_one.attr(err, "key", keystory_key)))
                return code;
            if((code = volume_one.attr(err, "key", volume_key)))
                return code;

            bool item_finded = false;
            for (int shadowstop_index = 0; shadowstop_index < resultShadowStops.size(); shadowstop_index++) {
                auto shadowstop_one = resultForeshadows.at(shadowstop_index);
                QString fkey, kkey, vkey;
                if((code = shadowstop_one.attr(err, "connect", fkey)))
                    return code;
                if((code = shadowstop_one.attr(err, "kfrom", kkey)))
                    return code;
                if((code = shadowstop_one.attr(err, "vfrom", vkey)))
                    return code;

                if(foreshadow_key == fkey && keystory_key == kkey && volume_key==vkey){
                    item_finded = true;
                    break;
                }
            }

            if(item_finded)
                resultForeshadows.removeAt(foreshadow_index);
            else
                foreshadow_index++;
        }

        for (auto& foreshadow : resultForeshadows) {
            QList<QStandardItem*> foreshadow_row;
            auto item = new OutlinesItem(foreshadow);
            item->setCheckable(_bool);


            foreshadow_row << item;

            keystory_points_model->appendRow(foreshadow_row);
        }
    }
    return 0;
}

int NovelHost::set_current_volume_node(QString &err, const OutlinesItem *node)
{
    int code;
    auto in_struct = node->getRefer();
    if((code = in_struct.nType() == FStruct::NHandle::Type::VOLUME)){
        disconnect(volume_description_present, &QTextDocument::contentsChanged,    this,   &NovelHost::resetCurrentVolumeDescription);
        QString content;
        if((code = in_struct.attr(err, "desp", content)))
            return code;
        volume_description_present->setPlainText(content);
        volume_description_present->clearUndoRedoStacks();
        connect(volume_description_present, &QTextDocument::contentsChanged,    this,   &NovelHost::resetCurrentVolumeDescription);
        return 0;
    }
    else {
        auto parent = node->QStandardItem::parent();
        if(!parent){
            err = "输入节点无效，该节点父节点为nullptr";
            return -1;
        }
        if((code = set_current_volume_node(err, static_cast<OutlinesItem*>(parent))))
            return code;
    }

    return 0;
}

int NovelHost::get_current_volume_node(QString &err, const OutlinesItem **node) const
{
    auto current_node = currentOutlineNode();
    while (current_node) {
        if(current_node->getRefer().nType() == FStruct::NHandle::Type::VOLUME){
            *node = current_node;
            return 0;
        }

        auto parent = current_node->QStandardItem::parent();
        if(!parent){
            err = "指定节点错误，向上搜索未找到卷节点";
            return -1;
        }

        current_node = static_cast<OutlinesItem*>(parent);
    }
    err = "未找到当前合法卷节点";
    return -1;
}

void NovelHost::resetNovelDescription()
{
    auto text = novel_description_present->toPlainText();
    desp_node->resetNovelDescription(text);
}

OutlinesItem *NovelHost::currentOutlineNode() const
{
    return current_outline_node;
}

QStandardItemModel *NovelHost::navigateTree() const
{
    return chapters_navigate_model;
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

    if((code = desp_node->removeNodeHandle(err, static_cast<ChaptersItem*>(item)->getRefer())))
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
    return result_enter_model;
}

void NovelHost::searchText(const QString &text)
{
    QRegExp exp("("+text+").*");
    result_enter_model->clear();
    result_enter_model->setHorizontalHeaderLabels(QStringList() << "搜索文本" << "卷宗节点" << "章节节点");

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
                desp_node->volumeTitle(err, path.first, temp);
                row << new QStandardItem(temp);

                desp_node->chapterTitle(err, path.first, path.second, temp);
                row << new QStandardItem(temp);

                result_enter_model->appendRow(row);
            }
        }
    }
}

QStandardItemModel *NovelHost::keystoryPointsPresent() const{
    return keystory_points_model;
}

QTextDocument *NovelHost::novelDescriptionPresent() const
{
    return novel_description_present;
}

QTextDocument *NovelHost::volumeDescriptionPresent() const
{
    return volume_description_present;
}

QTextDocument *NovelHost::currentNodeDescriptionPresent() const
{
    return node_description_present;
}

int NovelHost::chapterTextContent(QString &err, const QModelIndex &index0, QString &strOut)
{
    QModelIndex index = index0;
    if(!index.isValid()){
        err = "输入index无效";
        return -1;
    }

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = chapters_navigate_model->itemFromIndex(index);
    auto refer_node = static_cast<ChaptersItem*>(item);
    if(refer_node->getRefer().first < 0) {
        err = "输入了卷宗节点，非法数据";
        return -1;
    }

    if(opening_documents.contains(refer_node)){
        auto pack = opening_documents.value(refer_node);
        auto doc = pack.first;
        strOut = doc->toPlainText();
        return 0;
    }

    QString file_path, fencoding;
    desp_node->chapterCanonicalFilepath(err, refer_node->getRefer().first,
                                        refer_node->getRefer().second, file_path);
    desp_node->chapterTextEncoding(err, refer_node->getRefer().first,
                                   refer_node->getRefer().second, fencoding);

    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){
        err = "指定文件无法打开："+file_path;
        return -1;
    }
    QTextStream text_in(&file);
    text_in.setCodec(fencoding.toLocal8Bit());
    strOut = text_in.readAll();
    file.close();

    return 0;
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
    if((code = desp_node->chapterTitle(err, bind.first, bind.second, title)))
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
    QString title,err;
    refer.attr(err, "title", title);
    setText(title);

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

int FStruct::save(QString &errOut, const QString &newFilepath)
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

int FStruct::volumeAt(QString err, int index, FStruct::NHandle &node) const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();

    int code;
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, struct_node, "volume", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::VOLUME);
    return 0;
}

int FStruct::insertVolume(QString &err, const FStruct::NHandle &before, const QString &title,
                          const QString &description, FStruct::NHandle &node)
{
    if(before.nType() != NHandle::Type::VOLUME){
        err = "传入节点类型错误";
        return -1;
    }

    int code;
    QList<QString> keys;
    for (auto var=0; var<volumeCount(); ++var) {
        FStruct::NHandle volume_one;
        if((code = volumeAt(err, var, volume_one)))
            return code;
        QString key;
        if((code = volume_one.attr(err, "key", key)))
            return code;

        keys << key;
    }

    QString unique_key="volume-0";
    while (keys.contains(unique_key)) {
        unique_key = QString("volume-%1").arg(gen.generate64());
    }


    auto newdom = struct_dom_store.createElement("volume");
    NHandle aone(newdom, NHandle::Type::VOLUME);

    if((code = aone.setAttr("key", unique_key)))
        return code;

    if((code = aone.setAttr("title", title)))
        return code;

    if((code = aone.setAttr("desp", description)))
        return code;

    if(!before.isValid()){
        auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
        struct_node.appendChild(newdom);
    }
    else {
        before.dom_stored.parentNode().insertBefore(newdom, before.dom_stored);
    }

    node = aone;
    return 0;
}

int FStruct::keystoryCount(QString &err, const FStruct::NHandle &vmNode, int &num) const
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    auto list = vmNode.dom_stored.elementsByTagName("keystory");
    num = list.size();
    return 0;
}

int FStruct::keystoryAt(QString &err, const FStruct::NHandle &vmNode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, vmNode.dom_stored, "keystory", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::KEYSTORY);
    return 0;
}

int FStruct::insertKeystory(QString &err, FStruct::NHandle &vmNode, int before, const QString &title,
                            const QString &description, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    int num;
    if((code = keystoryCount(err, vmNode, num)))
        return code;
    QList<QString> kkeys;
    for (int var = 0; var < num; ++var) {
        NHandle one;
        keystoryAt(err, vmNode, var, one);
        QString key;
        one.attr(err, "key", key);
        kkeys << key;
    }
    QString unique_key="keystory-0";
    while (kkeys.contains(unique_key)) {
        unique_key = QString("keystory-%1").arg(gen.generate64());
    }

    auto ndom = struct_dom_store.createElement("keystory");
    NHandle one(ndom, NHandle::Type::KEYSTORY);
    if((code = one.setAttr("key", unique_key)))
        return code;
    if((code = one.setAttr("title", title)))
        return code;
    if((code = one.setAttr("desp", description)))
        return code;

    if(before >= num){
        vmNode.dom_stored.appendChild(ndom);
    }
    else {
        NHandle _before;
        if((code = keystoryAt(err, vmNode, before, _before)))
            return code;
        vmNode.dom_stored.insertBefore(ndom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::pointCount(QString &err, const FStruct::NHandle &knode, int &num) const
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    auto list = knode.dom_stored.elementsByTagName("points").at(0).childNodes();
    num = list.size();
    return 0;
}

int FStruct::pointAt(QString &err, const FStruct::NHandle &knode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    QDomElement elm;
    auto points_elm = knode.dom_stored.firstChildElement("points");
    if((code = find_direct_subdom_at_index(err, points_elm, "simply", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::POINT);
    return 0;
}

int FStruct::insertPoint(QString &err, FStruct::NHandle &knode, int before, const QString &title,
                         const QString &description, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    auto dom = struct_dom_store.createElement("simply");
    NHandle one(dom, NHandle::Type::POINT);
    if((code = one.setAttr("title", title)))
        return code;
    if((code = one.setAttr("desp", description)))
        return code;

    int num;
    if((code = pointCount(err, knode, num)))
        return code;
    if(before >= num){
        knode.dom_stored.firstChildElement("points").appendChild(dom);
    }
    else {
        NHandle _before;
        if((code = pointAt(err, knode, before, _before)))
            return code;
        knode.dom_stored.firstChildElement("points").insertBefore(dom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::foreshadowCount(QString &err, const FStruct::NHandle &knode, int &num) const
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    num = foreshodows_node.elementsByTagName("foreshadow").size();
    return 0;
}

int FStruct::foreshadowAt(QString &err, const FStruct::NHandle &knode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, foreshadows_node, "foreshadow", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::FORESHADOW);
    return 0;
}

int FStruct::appendForeshadow(QString &err, FStruct::NHandle &knode, const QString &title,
                              const QString &desp, const QString &desp_next, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, knode, NHandle::Type::KEYSTORY)))
        return code;

    int num;
    if((code = foreshadowCount(err, knode, num)))
        return code;
    QList<QString> fkeys;
    for (auto index = 0; index<num; ++index) {
        NHandle one;
        foreshadowAt(err, knode, index, one);
        QString key;
        one.attr(err, "key", key);
        fkeys << key;
    }
    QString unique_key="foreshadow-0";
    while (fkeys.contains(unique_key)) {
        unique_key = QString("foreshadow-%1").arg(gen.generate64());
    }

    auto elm = struct_dom_store.createElement("foreshadow");
    NHandle one(elm, NHandle::Type::FORESHADOW);
    if((code = one.setAttr("key", unique_key)))
        return code;
    if((code = one.setAttr("title", title)))
        return code;
    if((code = one.setAttr("desp", desp)))
        return code;
    if((code = one.setAttr("desp_next", desp_next)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    foreshadows_node.appendChild(elm);

    node = one;
    return 0;
}

int FStruct::chapterCount(QString &err, const FStruct::NHandle &vmNode, int &num) const
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    num = vmNode.dom_stored.elementsByTagName("chapter").size();
    return 0;
}

int FStruct::chapterAt(QString &err, const FStruct::NHandle &vmNode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, vmNode.dom_stored, "chapter", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::CHAPTER);
    return 0;
}

int FStruct::insertChapter(QString &err, FStruct::NHandle &vmNode, int before, const QString &title,
                           const QString &description, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, vmNode, NHandle::Type::VOLUME)))
        return code;

    int num;
    if((code = chapterCount(err, vmNode, num)))
        return code;
    QList<QString> ckeys;
    for (auto var=0; var<num; ++var) {
        NHandle one;
        chapterAt(err, vmNode, var, one);
        QString key;
        ckeys << key;
    }
    QString unique_key="chapter-0";
    while (ckeys.contains(unique_key)) {
        unique_key = QString("chapter-%1").arg(gen.generate64());
    }

    auto elm = struct_dom_store.createElement("chapter");
    NHandle one(elm, NHandle::Type::CHAPTER);
    if((code = one.setAttr("key", unique_key)))
        return code;
    if((code = one.setAttr("title", title)))
        return code;
    if((code = one.setAttr("encoding", "utf-8")))
        return code;
    if((code = one.setAttr("relative", unique_key+".txt")))
        return code;
    if((code = one.setAttr("desp", description)))
        return code;

    if(before>=num){
        vmNode.dom_stored.appendChild(elm);
    }
    else {
        NHandle _before;
        if((code = chapterAt(err, vmNode, before, _before)))
            return code;
        vmNode.dom_stored.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::chapterCanonicalFilePath(QString &err, const FStruct::NHandle &chapter, QString &filePath) const
{
    int code;
    if((code = checkNValid(err, chapter, FStruct::NHandle::Type::CHAPTER)))
        return code;

    QString relative_path;
    if((code = chapter.attr(err, "relative", relative_path)))
        return code;

    filePath = QDir(QFileInfo(this->filepath_stored).canonicalPath()).filePath(relative_path);
    return 0;
}

int FStruct::chapterTextEncoding(QString &err, const FStruct::NHandle &chapter, QString &encoding) const
{
    int code;
    if((code = checkNValid(err, chapter, FStruct::NHandle::Type::CHAPTER)))
        return code;

    if((code = chapter.attr(err, "encoding", encoding)))
        return code;

    return 0;
}

int FStruct::shadowstartCount(QString &err, const FStruct::NHandle &chpNode, int &num) const
{
    int code;
    if((code = checkNValid(err, chpNode, FStruct::NHandle::Type::CHAPTER)))
        return code;

    num = chpNode.dom_stored.elementsByTagName("shadow-start").size();
    return 0;
}

int FStruct::shadowstartAt(QString &err, const FStruct::NHandle &chpNode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, chpNode, FStruct::NHandle::Type::CHAPTER)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, chpNode.dom_stored, "shadow-start", index, elm)))
        return code;
    node = NHandle(elm, NHandle::Type::SHADOWSTART);
    return 0;
}

int FStruct::appendShadowstart(QString &err, FStruct::NHandle &chpNode, const QString &keystory,
                               const QString &foreshadow, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, chpNode, FStruct::NHandle::Type::CHAPTER)))
        return code;

    auto elm = struct_dom_store.createElement("shadow-start");
    NHandle one(elm, NHandle::Type::SHADOWSTART);
    if((code = one.setAttr("target", keystory+"@"+foreshadow)))
        return code;

    chpNode.dom_stored.appendChild(elm);
    node = one;
    return 0;
}

int FStruct::shadowstopCount(QString &err, const FStruct::NHandle &chpNode, int &num) const
{
    int code;
    if((code = checkNValid(err, chpNode, NHandle::Type::CHAPTER)))
        return code;

    num = chpNode.dom_stored.elementsByTagName("shadow-stop").size();
    return 0;
}

int FStruct::shadowstopAt(QString &err, const FStruct::NHandle &chpNode, int index, FStruct::NHandle &node) const
{
    int code;
    if((code = checkNValid(err, chpNode, NHandle::Type::CHAPTER)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, chpNode.dom_stored, "shadow-stop", index, elm)))
        return code;

    node = NHandle(elm, NHandle::Type::SHADOWSTOP);
    return 0;
}

int FStruct::appendShadowstop(QString &err, FStruct::NHandle &chpNode, const QString &volume, const QString &keystory,
                              const QString &foreshadow, FStruct::NHandle &node)
{
    int code;
    if((code = checkNValid(err, chpNode, NHandle::Type::CHAPTER)))
        return code;

    auto elm = struct_dom_store.createElement("shadow-stop");
    NHandle one(elm, NHandle::Type::SHADOWSTOP);
    if((code = one.setAttr("target", volume+"@"+keystory+"@"+foreshadow)))
        return code;

    int num;
    if((code = shadowstopCount(err, chpNode, num)))
        return code;

    chpNode.dom_stored.appendChild(elm);

    node = one;
    return 0;
}

int FStruct::parentHandle(QString &err, const FStruct::NHandle &base, FStruct::NHandle &parent) const
{
    if(!base.isValid()){
        err = "传入无效节点";
        return -1;
    }

    auto pnode = base.dom_stored.parentNode().toElement();

    switch (base.nType()) {
        case NHandle::Type::POINT:
        case NHandle::Type::FORESHADOW:
            parent = NHandle(pnode, NHandle::Type::KEYSTORY);
            return 0;
        case NHandle::Type::SHADOWSTOP:
        case NHandle::Type::SHADOWSTART:
            parent = NHandle(pnode, NHandle::Type::CHAPTER);
            return 0;
        case NHandle::Type::KEYSTORY:
        case NHandle::Type::CHAPTER:
            parent = NHandle(pnode, NHandle::Type::VOLUME);
            return 0;
        default:
            parent = NHandle();
            err = "无有效父节点";
            return -1;
    }
}

int FStruct::handleIndex(QString &err, const FStruct::NHandle &node, int &index) const
{
    if(!node.isValid()){
        err = "传入无效节点";
        return -1;
    }

    auto dom = node.dom_stored;
    auto parent = dom.parentNode().toElement();
    auto elm = parent.firstChildElement(dom.tagName());
    int _int = 0;

    while (!elm.isNull()) {
        if(dom == elm){
            index = _int;
            return 0;
        }

        _int++;
        elm = elm.nextSiblingElement(dom.tagName());
    }
    err = "未知错误";
    return -1;
}

int FStruct::removeNodeHandle(QString &err, const FStruct::NHandle &node)
{
    if(!node.isValid()){
        err = "指定节点失效";
        return -1;
    }

    int code;
    if(node.nType() == NHandle::Type::CHAPTER){
        QString filepath;
        if((code = chapterCanonicalFilePath(err, node, filepath)))
            return code;

        QFile file(filepath);
        if(!file.remove()){
            err = "文件系统异常，移除文件失败："+filepath;
            return -1;
        }
    }

    if(node.nType() == NHandle::Type::VOLUME){
        int count;
        if((code = chapterCount(err, node, count)))
            return code;

        for(int var=0; var < count ; ++var){
            NHandle _node;
            if((code = chapterAt(err, node, var, _node)))
                return code;

            if((code = removeNodeHandle(err, _node)))
                return code;
        }
    }

    auto parent = node.dom_stored.parentNode();
    if(parent.isNull()){
        err = "父节点非法";
        return -1;
    }

    parent.removeChild(node.dom_stored);
    return 0;
}

int FStruct::checkNValid(QString &err, const FStruct::NHandle &node, FStruct::NHandle::Type type) const
{
    if(node.nType() != type){
        err = "传入节点类型错误";
        return -1;
    }

    if(node.isValid()){
        err = "传入节点已失效";
        return -1;
    }

    return 0;
}

int FStruct::find_direct_subdom_at_index(QString &err, const QDomElement &pnode, const QString &tagName,
                                         int index, QDomElement &node) const
{
    auto first = pnode.firstChildElement(tagName);
    while (!first.isNull()) {
        if(!index){
            node = first;
            return 0;
        }

        index--;
        first = first.nextSiblingElement(tagName);
    }

    err = "无效index指定";
    return -1;
}

FStruct::NHandle::NHandle()
    :type_stored(Type::VOLUME){}

FStruct::NHandle::NHandle(QDomElement domNode, FStruct::NHandle::Type type)
    :dom_stored(domNode),
      type_stored(type){}

FStruct::NHandle::NHandle(const FStruct::NHandle &other)
    :dom_stored(other.dom_stored),
      type_stored(other.type_stored){}

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
    if(dom_stored.isNull()){
        throw new WsException("节点已失效");
    }

    return dom_stored.attribute(name);
}

void FStruct::NHandle::setAttr(const QString &name, const QString &value)
{
    if(dom_stored.isNull()){
        throw new WsException("节点已失效");
    }
    dom_stored.setAttribute(name, value);
}

OutlinesItem::OutlinesItem(const FStruct::NHandle &refer)
    :fstruct_node(refer)
{
    QString err,title;
    refer.attr(err, "title", title);
    setText(title);
}

const FStruct::NHandle OutlinesItem::getRefer() const
{
    return fstruct_node;
}

FStruct::NHandle::Type OutlinesItem::getType() const{
    return getRefer().nType();
}
