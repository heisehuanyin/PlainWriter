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
      outline_tree_model(new QStandardItemModel(this)),
      foreshadows_present(new QStandardItemModel(this)),
      result_enter_model(new QStandardItemModel(this)),
      node_navigate_model(new QStandardItemModel(this)),
      keynode_points_model(new QStandardItemModel(this)),
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
        FStruct::NodeHandle volume_node;
        if((code = desp->volumeAt(err, volume_index, volume_node)))
            return code;

        auto pair = insert_volume(volume_node, volume_index);
        auto outline_volume_node = pair.first;
        auto node_navigate_volume_node = pair.second;

        int keystory_count = 0;
        if((code = desp->keystoryCount(err, volume_node, keystory_count)))
            return keystory_count;
        for (int keystory_index = 0; keystory_index < keystory_count; ++keystory_index) {
            FStruct::NodeHandle keystory_node;
            if((code = desp->keystoryAt(err, volume_node, keystory_index, keystory_node)))
                return code;

            auto ol_keystory_item = new OutlinesItem(keystory_node);
            outline_volume_node->appendRow(ol_keystory_item);

            int points_count=0;
            if((code = desp->pointCount(err, keystory_node, points_count)))
                return code;
            for (int points_index = 0; points_index < points_count; ++points_index) {
                FStruct::NodeHandle point_node;
                if((code = desp->pointAt(err, keystory_node, points_index, point_node)))
                    return code;

                auto outline_point_node = new OutlinesItem(point_node);
                ol_keystory_item->appendRow(outline_point_node);
            }

            int chapter_count =0;
            if((code = desp->chapterCount(err, keystory_node, chapter_count)))
                return code;
            for (int chapter_index = 0; chapter_index < chapter_count; ++chapter_index) {
                FStruct::NodeHandle chapter_node;
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

    return 0;
}

int NovelHost::save(QString &err, const QString &filePath)
{
    int xret;
    if((xret = desp_node->save(err, filePath)))
        return xret;

    for (auto vm_index=0; vm_index<node_navigate_model->rowCount(); ++vm_index) {
        auto item = node_navigate_model->item(vm_index);
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
    FStruct::NodeHandle volume_new;
    if((code = desp_node->insertVolume(err, FStruct::NodeHandle(), gName, "", volume_new)))
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
    if((code = desp_node->checkNodeValid(err, volume_struct_node, FStruct::NodeHandle::Type::VOLUME)))
        return code;

    int knode_count=0;
    if((code = desp_node->keystoryCount(err, volume_tree_node->getRefer(), knode_count)))
        return code;

    FStruct::NodeHandle keystory_node;
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
    auto keystory_tree_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystory_tree_node->getRefer();
    int code;
    if((code = desp_node->checkNodeValid(err, keystory_struct_node, FStruct::NodeHandle::Type::KEYSTORY)))
        return code;
    int points_count;
    if((code = desp_node->pointCount(err, keystory_struct_node, points_count)))
        return code;
    FStruct::NodeHandle point_node;
    if((code = desp_node->insertPoint(err, keystory_struct_node, points_count, pName, "", point_node)))
        return code;

    keystory_tree_node->appendRow(new OutlinesItem(point_node));
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
    auto keystory_tree_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystory_tree_node->getRefer();
    int code;
    if((code = desp_node->checkNodeValid(err, keystory_struct_node, FStruct::NodeHandle::Type::KEYSTORY)))
        return code;
    int foreshadows_count;
    if((code = desp_node->foreshadowCount(err, keystory_struct_node, foreshadows_count)))
        return code;
    FStruct::NodeHandle foreshadow_node;
    if((code = desp_node->insertForeshadow(err, keystory_struct_node, foreshadows_count,
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
    auto keystory_tree_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystory_tree_node->getRefer();
    int code;
    if((code = desp_node->checkNodeValid(err, keystory_struct_node, FStruct::NodeHandle::Type::KEYSTORY)))
        return code;
    int shadowstop_count;
    if((code = desp_node->shadowstopCount(err, keystory_struct_node, shadowstop_count)))
        return code;
    FStruct::NodeHandle shadowstop_node;
    if((code = desp_node->insertShadowstop(err, keystory_struct_node, shadowstop_count,
                                           vKey, kKey, fKey, shadowstop_node)))
        return code;
    return 0;
}

int NovelHost::appendChapter(QString &err, const QModelIndex &kIndex, const QString &aName)
{
    if(!kIndex.isValid()){
        err = "输入modelindex无效";
        return -1;
    }

    auto node = outline_tree_model->itemFromIndex(kIndex);
    auto keystory_tree_node = static_cast<OutlinesItem*>(node);
    auto keystory_struct_node = keystory_tree_node->getRefer();
    int code;
    if((code = desp_node->checkNodeValid(err, keystory_struct_node, FStruct::NodeHandle::Type::KEYSTORY)))
        return code;
    int chapter_count;
    if((code = desp_node->chapterCount(err, keystory_struct_node, chapter_count)))
        return code;
    FStruct::NodeHandle chapter_node;
    if((code = desp_node->insertChapter(err, keystory_struct_node, chapter_count, aName, "", chapter_node)))
        return code;

    auto volume_tree_node = keystory_tree_node->QStandardItem::parent();
    volume_tree_node->appendRow(new ChaptersItem(*this, chapter_node));

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
        node_navigate_model->removeRow(outline_struct_node->row());
    }
    else {
        pnode->removeRow(outline_struct_node->row());
        if(outline_struct_node->getRefer().nodeType() == FStruct::NodeHandle::Type::KEYSTORY){
            FStruct::NodeHandle volume_node;
            if((code = desp_node->parentNodeHandle(err, outline_struct_node->getRefer(), volume_node)))
                return code;
            int chapter_count_will_be_remove;
            if((code = desp_node->chapterCount(err, outline_struct_node->getRefer(), chapter_count_will_be_remove)))
                return code;

            int volume_index;
            if((code = desp_node->nodeHandleIndex(err, volume_node, volume_index)))
                return code;
            auto volume_entry = node_navigate_model->item(volume_index);
            for (auto chapter_index=0; chapter_index<chapter_count_will_be_remove; ++chapter_index) {
                FStruct::NodeHandle chapter_node;
                if((code = desp_node->chapterAt(err, outline_struct_node->getRefer(), chapter_index, chapter_node)))
                    return code;

                for (auto view_chapter_index=0; view_chapter_index<volume_entry->rowCount(); ++view_chapter_index) {
                    auto chapter_entry2 = static_cast<ChaptersItem*>(volume_entry->child(view_chapter_index));

                    if(chapter_entry2->getRefer() == chapter_node){
                        volume_entry->removeRow(view_chapter_index);
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

int NovelHost::removeChaptersNode(QString &err, const QModelIndex &chaptersNode)
{
    if(!chaptersNode.isValid()){
        err = "chaptersNodeIndex无效";
        return -1;
    }


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

            QString text_content,err;
            if(chapterTextContent(err, chapter_title_node->index(), text_content))
                return;

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
    result_enter_model->setHorizontalHeaderLabels(QStringList() << "搜索文本" << "卷宗节点" << "章节节点");

    for (int vm_index=0; vm_index<node_navigate_model->rowCount(); ++vm_index) {
        auto volume_node = node_navigate_model->item(vm_index);

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

int NovelHost::chapterTextContent(QString &err, const QModelIndex &index0, QString &strOut)
{
    QModelIndex index = index0;
    if(!index.isValid()){
        err = "输入index无效";
        return -1;
    }

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = node_navigate_model->itemFromIndex(index);
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

    auto item = node_navigate_model->itemFromIndex(index);
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

void NovelHost::rehighlightDocument(QTextDocument *doc)
{
    for (auto px : opening_documents) {
        if(px.first == doc){
            if(px.first == doc){
                px.second->rehighlight();
                break;
            }
        }
    }
}

QPair<OutlinesItem *, ChaptersItem *> NovelHost::insert_volume(const FStruct::NodeHandle &item, int index)
{
    auto outline_volume_node = new OutlinesItem(item);

    QList<QStandardItem*> navigate_valume_row;
    auto node_navigate_volume_node = new ChaptersItem(*this, item, true);
    navigate_valume_row << node_navigate_volume_node;
    navigate_valume_row << new QStandardItem("-");


    if(index >= outline_tree_model->rowCount()){
        outline_tree_model->appendRow(outline_volume_node);
        node_navigate_model->appendRow(navigate_valume_row);
    }
    else {
        outline_tree_model->insertRow(index, outline_volume_node);
        node_navigate_model->insertRow(index, navigate_valume_row);
    }


    return qMakePair(outline_volume_node, node_navigate_volume_node);
}

ChaptersItem *NovelHost::append_chapter(ChaptersItem *volumeNode, const QString &title)
{
    QList<QStandardItem*> row;
    auto one = new ChaptersItem(*this, title);
    row << one;
    auto two = new QStandardItem("-");
    row << two;

    volumeNode->appendRow(row);
    return one;
}

void NovelHost::navigate_title_midify(QStandardItem *item)
{
    if(item->column())
        return;

    auto xitem = static_cast<ChaptersItem*>(item);

    if(xitem->getRefer().first < 0){
        QString err;
        desp_node->resetVolumeTitle(err, item->row(), item->text());
    }
    else {
        QString err;
        auto flow = xitem->getRefer();
        desp_node->resetChapterTitle(err, flow.first, flow.second, item->text());

        if(opening_documents.contains(xitem))
            emit documentActived(opening_documents.value(xitem).first, item->text());
    }
}

int NovelHost::remove_node_recursive(QString &errOut, const FStruct::NodeHandle &one2)
{
    QModelIndex index = one2;
    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = node_navigate_model->itemFromIndex(index);
    auto xitem = static_cast<ChaptersItem*>(item);

    int retc;
    auto bind = xitem->getRefer();
    if(bind.first < 0){
        for (auto num_index = 0; num_index<item->rowCount(); ) {
            auto child_mindex = item->child(0)->index();
            if((retc = remove_node_recursive(errOut, child_mindex)))
                return retc;
        }

        if((retc = desp_node->removeVolume(errOut, bind.second)))
            return retc;

        node_navigate_model->removeRow(bind.second);
    }
    else {
        QString file_path;
        if((retc = desp_node->chapterCanonicalFilepath(errOut, bind.first, bind.second, file_path)))
            return retc;

        if(!QFile(file_path).remove()){
            errOut = "指定文件移除失败："+file_path;
            return -1;
        }

        if((retc = desp_node->removeChapter(errOut, bind.first, bind.second)))
            return retc;

        node_navigate_model->removeRow(xitem->row(), xitem->index().parent());
    }

    return 0;
}





ChaptersItem::ChaptersItem(NovelHost &host, const FStruct::NodeHandle &refer, bool isGroup)
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

const FStruct::NodeHandle ChaptersItem::getRefer() const
{
    return fstruct_node;
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
    struct_dom_store.appendChild(struct_dom_store.createProcessingInstruction("xml", "version='1.0' encoding='utf-8'"));
    auto root = struct_dom_store.createElement("root");
    root.setAttribute("version", "2.0");
    struct_dom_store.appendChild(root);

    auto config = struct_dom_store.createElement("config");
    root.appendChild(config);

    auto structnode = struct_dom_store.createElement("story-tree");
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
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    return story_tree.attribute("title");
}

void FStruct::resetNovelTitle(const QString &title)
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    story_tree.setAttribute("title", title);
}

QString FStruct::novelDescription() const
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    return story_tree.attribute("desp");
}

void FStruct::resetNovelDescription(const QString &desp)
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    story_tree.setAttribute("desp", desp);
}

int FStruct::volumeCount() const
{
    return struct_dom_store.elementsByTagName("volume").size();
}

int FStruct::volumeAt(QString err, int index, FStruct::NodeHandle &node) const
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();

    int code;
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, story_tree, "volume", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::VOLUME);
    return 0;
}

int FStruct::insertVolume(QString &err, const FStruct::NodeHandle &before, const QString &title,
                             const QString &description, FStruct::NodeHandle &node)
{
    if(before.nodeType() != NodeHandle::Type::VOLUME){
        err = "传入节点类型错误";
        return -1;
    }

    auto newdom = struct_dom_store.createElement("volume");
    NodeHandle aone(newdom, NodeHandle::Type::VOLUME);

    int code;
    if((code = aone.setAttr(err, "title", title)))
        return code;

    if((code = aone.setAttr(err, "desp", description)))
        return code;

    if(before.dom_stored.isNull()){
        auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
        story_tree.appendChild(newdom);
    }
    else {

        before.dom_stored.parentNode().insertBefore(newdom, before.dom_stored);
    }

    node = aone;
    return 0;
}

int FStruct::keystoryCount(QString &err, const FStruct::NodeHandle &vmNode, int &num) const
{
    int code;
    if((code = checkNodeValid(err, vmNode, NodeHandle::Type::VOLUME)))
        return code;

    auto list = vmNode.dom_stored.elementsByTagName("keynode");
    num = list.size();
    return 0;
}

int FStruct::keystoryAt(QString &err, const FStruct::NodeHandle &vmNode, int index, FStruct::NodeHandle &node) const
{
    int code;
    if((code = checkNodeValid(err, vmNode, NodeHandle::Type::VOLUME)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, vmNode.dom_stored, "keynode", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::KEYSTORY);
    return 0;
}

int FStruct::insertKeystory(QString &err, FStruct::NodeHandle &vmNode, int before, const QString &title,
                               const QString &description, FStruct::NodeHandle &node)
{
    int code;
    if((code = checkNodeValid(err, vmNode, NodeHandle::Type::VOLUME)))
        return code;

    int num;
    QList<QString> kkeys;
    if((code = keystoryCount(err, vmNode, num)))
        return code;
    for (int var = 0; var < num; ++var) {
        NodeHandle one;
        keystoryAt(err, vmNode, var, one);
        QString key;
        one.attr(err, "key", key);
        kkeys << key;
    }
    QString unique_key="keynode-0";
    while (kkeys.contains(unique_key)) {
        unique_key = QString("keynode-%1").arg(gen.generate64());
    }

    auto ndom = struct_dom_store.createElement("keynode");
    NodeHandle one(ndom, NodeHandle::Type::KEYSTORY);
    if((code = one.setAttr(err, "key", unique_key)))
        return code;
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "desp", description)))
        return code;

    if(before >= num){
        vmNode.dom_stored.appendChild(ndom);
    }
    else {
        NodeHandle _before;
        if((code = keystoryAt(err, vmNode, before, _before)))
            return code;
        vmNode.dom_stored.insertBefore(ndom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::pointCount(QString &err, const FStruct::NodeHandle &knode, int &num) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto list = knode.dom_stored.elementsByTagName("points").at(0).childNodes();
    num = list.size();
    return 0;
}

int FStruct::pointAt(QString &err, const FStruct::NodeHandle &knode, int index, FStruct::NodeHandle &node) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    QDomElement elm;
    auto points_elm = knode.dom_stored.firstChildElement("points");
    if((code = find_direct_subdom_at_index(err, points_elm, "simply", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::POINT);
    return 0;
}

int FStruct::insertPoint(QString &err, FStruct::NodeHandle &knode, int before, const QString &title,
                            const QString &description, FStruct::NodeHandle &node)
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto dom = struct_dom_store.createElement("simply");
    NodeHandle one(dom, NodeHandle::Type::POINT);
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "desp", description)))
        return code;

    int num;
    if((code = pointCount(err, knode, num)))
        return code;
    if(before >= num){
        knode.dom_stored.firstChildElement("points").appendChild(dom);
    }
    else {
        NodeHandle _before;
        if((code = pointAt(err, knode, before, _before)))
            return code;
        knode.dom_stored.firstChildElement("points").insertBefore(dom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::foreshadowCount(QString &err, const FStruct::NodeHandle &knode, int &num) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    num = foreshodows_node.elementsByTagName("foreshadow").size();
    return 0;
}

int FStruct::foreshadowAt(QString &err, const FStruct::NodeHandle &knode, int index, FStruct::NodeHandle &node) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, foreshadows_node, "foreshadow", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::FORESHADOW);
    return 0;
}

int FStruct::insertForeshadow(QString &err, FStruct::NodeHandle &knode, int before, const QString &title,
                                 const QString &desp, const QString &desp_next, FStruct::NodeHandle &node)
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    int num;
    QList<QString> fkeys;
    if((code = foreshadowCount(err, knode, num)))
        return code;
    for (auto index = 0; index<num; ++index) {
        NodeHandle one;
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
    NodeHandle one(elm, NodeHandle::Type::FORESHADOW);
    if((code = one.setAttr(err, "key", unique_key)))
        return code;
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "desp", desp)))
        return code;
    if((code = one.setAttr(err, "desp_next", desp_next)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    if(before>=num){
        foreshadows_node.appendChild(elm);
    }
    else {
        NodeHandle _before;
        if((code = foreshadowAt(err, knode, before, _before)))
            return code;
        foreshadows_node.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::shadowstopCount(QString &err, const FStruct::NodeHandle &knode, int &num) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    num = foreshodows_node.elementsByTagName("shadow-stop").size();
    return 0;
}

int FStruct::shadowstopAt(QString &err, const FStruct::NodeHandle &knode, int index, FStruct::NodeHandle &node) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, foreshadows_node, "shadow-stop", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::SHADOWSTOP);
    return 0;
}

int FStruct::insertShadowstop(QString &err, FStruct::NodeHandle &knode, int before, const QString &vfrom,
                                 const QString &kfrom, const QString &connect_shadow, FStruct::NodeHandle &node)
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    auto elm = struct_dom_store.createElement("shadow-stop");
    NodeHandle one(elm, NodeHandle::Type::SHADOWSTOP);
    if((code = one.setAttr(err, "vfrom", vfrom)))
        return code;
    if((code = one.setAttr(err, "kfrom", kfrom)))
        return code;
    if((code = one.setAttr(err, "connect", connect_shadow)))
        return code;

    int num;
    if((code = shadowstopCount(err, knode, num)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    if(before >= num){
        foreshadows_node.appendChild(elm);
    }
    else {
        NodeHandle _before;
        if((code = shadowstopAt(err, knode, before, _before)))
            return code;
        foreshadows_node.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::chapterCount(QString &err, const FStruct::NodeHandle &knode, int &num) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    num = knode.dom_stored.elementsByTagName("chapter").size();
    return 0;
}

int FStruct::chapterAt(QString &err, const FStruct::NodeHandle &knode, int index, FStruct::NodeHandle &node) const
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, knode.dom_stored, "chapter", index, elm)))
        return code;

    node = NodeHandle(elm, NodeHandle::Type::KEYSTORY);
    return 0;
}

int FStruct::insertChapter(QString &err, FStruct::NodeHandle &knode, int before, const QString &title,
                              const QString &description, FStruct::NodeHandle &node)
{
    int code;
    if((code = checkNodeValid(err, knode, NodeHandle::Type::KEYSTORY)))
        return code;

    int num;
    if((code = chapterCount(err, knode, num)))
        return code;
    QList<QString> ckeys;
    for (auto var=0; var<num; ++var) {
        NodeHandle one;
        chapterAt(err, knode, var, one);
        QString key;
        ckeys << key;
    }
    QString unique_key="chapter-0";
    while (ckeys.contains(unique_key)) {
        unique_key = QString("chapter-%1").arg(gen.generate64());
    }

    auto elm = struct_dom_store.createElement("chapter");
    NodeHandle one(elm, NodeHandle::Type::CHAPTER);
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "key", unique_key)))
        return code;
    if((code = one.setAttr(err, "encoding", "utf-8")))
        return code;
    if((code = one.setAttr(err, "relative", unique_key+".txt")))
        return code;
    if((code = one.setAttr(err, "desp", description)))
        return code;

    if(before>=num){
        knode.dom_stored.appendChild(elm);
    }
    else {
        NodeHandle _before;
        if((code = chapterAt(err, knode, before, _before)))
            return code;
        knode.dom_stored.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStruct::chapterCanonicalFilePath(QString &err, const FStruct::NodeHandle &chapter, QString &filePath) const
{
    int code;
    if((code = checkNodeValid(err, chapter, FStruct::NodeHandle::Type::CHAPTER)))
        return code;

    QString relative_path;
    if((code = chapter.attr(err, "relative", relative_path)))
        return code;

    filePath = QDir(QFileInfo(this->filepath_stored).canonicalPath()).filePath(relative_path);
    return 0;
}

int FStruct::chapterTextEncoding(QString &err, const FStruct::NodeHandle &chapter, QString &encoding) const
{
    int code;
    if((code = checkNodeValid(err, chapter, FStruct::NodeHandle::Type::CHAPTER)))
        return code;

    if((code = chapter.attr(err, "encoding", encoding)))
        return code;

    return 0;
}

int FStruct::parentNodeHandle(QString &err, const FStruct::NodeHandle &base, FStruct::NodeHandle &parent) const
{
    auto dom = base.dom_stored;
    auto pnode = dom.parentNode().toElement();

    switch (base.nodeType()) {
        case NodeHandle::Type::POINT:
        case NodeHandle::Type::FORESHADOW:
        case NodeHandle::Type::SHADOWSTOP:
        case NodeHandle::Type::CHAPTER:
            parent = NodeHandle(pnode, NodeHandle::Type::KEYSTORY);
            return 0;
        case NodeHandle::Type::KEYSTORY:
            parent = NodeHandle(pnode, NodeHandle::Type::VOLUME);
            return 0;
        default:
            parent = NodeHandle();
            err = "无有效父节点";
            return -1;
    }
}

int FStruct::nodeHandleIndex(QString &err, const FStruct::NodeHandle &node, int &index) const
{
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

int FStruct::removeNodeHandle(QString &err, const FStruct::NodeHandle &node)
{
    if(node.dom_stored.isNull()){
        err = "指定节点失效";
        return -1;
    }

    int code;
    if(node.nodeType() == NodeHandle::Type::CHAPTER){
        QString filepath;
        if((code = chapterCanonicalFilePath(err, node, filepath)))
            return code;

        QFile file(filepath);
        if(!file.remove()){
            err = "文件系统异常，移除文件失败："+filepath;
            return -1;
        }
    }

    if(node.nodeType() == NodeHandle::Type::VOLUME){
        int count;
        if((code = keystoryCount(err, node, count)))
            return code;

        for(int var=0; var < count ; ++var){
            NodeHandle _node;
            if((code = keystoryAt(err, node, var, _node)))
                return code;

            if((code = removeNodeHandle(err, _node)))
                return code;
        }
    }
    else if(node.nodeType() == NodeHandle::Type::KEYSTORY) {
        int count;
        if((code = pointCount(err, node, count)))
            return code;
        for (int var = 0; var < count; ++var) {
            NodeHandle _node;
            if((code = pointAt(err, node, var, _node)))
                return code;

            if((code = removeNodeHandle(err, _node)))
                return code;
        }

        if((code = foreshadowCount(err, node, count)))
            return code;
        for (int var = 0; var < count; ++var) {
            NodeHandle _node;
            if((code = foreshadowAt(err, node, var, _node)))
                return code;

            if((code = removeNodeHandle(err, _node)))
                return code;
        }

        if((code = chapterCount(err, node, count)))
            return code;
        for (int var = 0; var < count; ++var) {
            NodeHandle _node;
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

int FStruct::checkNodeValid(QString &err, const FStruct::NodeHandle &node, FStruct::NodeHandle::Type type) const
{
    if(node.nodeType() != type){
        err = "传入节点类型错误";
        return -1;
    }

    if(node.dom_stored.isNull()){
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

FStruct::NodeHandle::NodeHandle()
    :type_stored(Type::VOLUME){}

FStruct::NodeHandle::NodeHandle(QDomElement domNode, FStruct::NodeHandle::Type type)
    :dom_stored(domNode),
      type_stored(type){}

FStruct::NodeHandle::NodeHandle(const FStruct::NodeHandle &other)
    :dom_stored(other.dom_stored),
      type_stored(other.type_stored){}

FStruct::NodeHandle &FStruct::NodeHandle::operator=(const FStruct::NodeHandle &other){
    dom_stored = other.dom_stored;
    type_stored = other.type_stored;
    return *this;
}

bool FStruct::NodeHandle::operator==(const FStruct::NodeHandle &other) const{
    return type_stored == other.type_stored &&
            dom_stored == other.dom_stored;
}

FStruct::NodeHandle::Type FStruct::NodeHandle::nodeType() const
{
    return type_stored;
}

int FStruct::NodeHandle::attr(QString &err, const QString &name, QString &out) const
{
    if(dom_stored.isNull()){
        err = "节点已失效";
        return -1;
    }

    out = dom_stored.attribute(name);
    return 0;
}

int FStruct::NodeHandle::setAttr(QString &err, const QString &name, const QString &value)
{
    if(dom_stored.isNull()){
        err = "节点已失效";
        return -1;
    }
    dom_stored.setAttribute(name, value);
    return 0;
}

OutlinesItem::OutlinesItem(const FStruct::NodeHandle &refer)
    :fstruct_node(refer)
{
    QString err,title;
    refer.attr(err, "title", title);
    setText(title);
}

const FStruct::NodeHandle OutlinesItem::getRefer() const
{
    return fstruct_node;
}
