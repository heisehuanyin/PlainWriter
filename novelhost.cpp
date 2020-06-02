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
      node_navigate_model(new QStandardItemModel(this)),
      result_enter_model(new QStandardItemModel(this))
{
    connect(node_navigate_model,   &QStandardItemModel::itemChanged,  this,  &NovelHost::navigate_title_midify);
    node_navigate_model->setHorizontalHeaderLabels(QStringList() << "章节标题" << "字数统计");
}

NovelHost::~NovelHost(){}

int NovelHost::loadDescription(QString &err, FStructure *desp)
{
    // save description
    this->desp_node = desp;

    for (int vm_index=0; vm_index<desp_node->volumeCount(); ++vm_index) {
        int code, chpr_count;
        QString vm_title;

        if((code = desp_node->volumeTitle(err, vm_index, vm_title)))
            return code;

        // append volume
        auto volume = append_volume(node_navigate_model, vm_title);
        if((code = desp_node->chapterCount(err, vm_index, chpr_count)))
            return code;

        for (int chpr_index=0; chpr_index<chpr_count; ++chpr_index) {
            QString chpr_title;
            if((code = desp_node->chapterTitle(err, vm_index, chpr_index, chpr_title)))
                return code;

            // append chapter
            append_chapter(volume, chpr_title);
        }
    }
    return 0;
}

int NovelHost::save(QString &errorOut, const QString &filePath)
{
    auto xret = desp_node->save(errorOut, filePath);
    if(xret) return xret;

    for (auto vm_index=0; vm_index<node_navigate_model->rowCount(); ++vm_index) {
        auto item = node_navigate_model->item(vm_index);
        auto xitem = static_cast<ReferenceItem*>(item);

        for (auto chp_index=0; chp_index<xitem->rowCount(); ++chp_index) {
            auto chapter = xitem->child(chp_index);
            auto xchapter = static_cast<ReferenceItem*>(chapter);
            // 检测文件是否打开
            if(!opening_documents.contains(xchapter))
                continue;
            auto pak = opening_documents.value(xchapter);

            // 检测文件是否修改
            if(pak.first->isModified()){
                QString file_canonical_path;
                if((xret = desp_node->chapterCanonicalFilepath(errorOut, vm_index, chp_index, file_canonical_path)))
                    return xret;

                QFile file(file_canonical_path);
                if(!file.open(QIODevice::Text|QIODevice::WriteOnly)){
                    errorOut = "保存内容过程，目标无法打开："+ file_canonical_path;
                    return -1;
                }

                QTextStream txt_out(&file);
                QString file_encoding;
                if((xret = desp_node->chapterTextEncoding(errorOut, vm_index, chp_index, file_encoding)))
                    return xret;

                txt_out.setCodec(file_encoding.toLocal8Bit());

                QString content;
                if((xret = chapterTextContent(errorOut, xchapter->index(), content)))
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

QStandardItemModel *NovelHost::navigateTree() const
{
    return node_navigate_model;
}

int NovelHost::appendVolume(QString &err, const QString &gName)
{
    append_volume(node_navigate_model, gName);

    int code;
    if((code = desp_node->insertVolume(err, desp_node->volumeCount()+1, gName)))
        return code;

    return 0;
}

int NovelHost::appendChapter(QString &err, const QModelIndex &kIndex, const QString &aName)
{
    QModelIndex navigate_index = kIndex;
    if(!navigate_index.isValid()){
        err = "appendChapter: 非法modelindex";
        return -1;
    }
    if(navigate_index.column())
        navigate_index = navigate_index.sibling(navigate_index.row(), 0);

    int code;
    QString new_file_path;
    // 选中了卷节点
    auto item = node_navigate_model->itemFromIndex(navigate_index);
    auto xitem = static_cast<ReferenceItem*>(item);
    if(xitem->getTargetBinding().first<0){
        append_chapter(xitem, aName);

        if((code = desp_node->insertChapter(err, xitem->row(), xitem->rowCount()+1, aName)))
            return code;

        int ccount;
        if((code = desp_node->chapterCount(err, xitem->row(), ccount)))
            return code;

        if((code = desp_node->chapterCanonicalFilepath(err, xitem->row(), ccount-1, new_file_path)))
            return code;
    }
    else {
        // 选中了章节节点
        auto pitem = item->parent();
        append_chapter(static_cast<ReferenceItem*>(pitem), aName);
        if((code = desp_node->insertChapter(err, pitem->row(), pitem->rowCount()+1, aName)))
            return code;

        int ccount;
        if((code = desp_node->chapterCount(err, pitem->row(), ccount)))
            return code;

        if((code = desp_node->chapterCanonicalFilepath(err, pitem->row(), ccount-1, new_file_path)))
            return code;
    }

    QFile target(new_file_path);
    if(target.exists()){
        err = "软件错误，出现重复文件名："+new_file_path;
        return -1;
    }

    if(!target.open(QIODevice::WriteOnly|QIODevice::Text)){
        err = "软件错误，指定路径文件无法打开："+new_file_path;
        return -1;
    }
    target.close();

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

                auto path = static_cast<ReferenceItem*>(chp_node)->getTargetBinding();
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
    auto refer_node = static_cast<ReferenceItem*>(item);
    if(refer_node->getTargetBinding().first < 0) {
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
    desp_node->chapterCanonicalFilepath(err, refer_node->getTargetBinding().first,
                                        refer_node->getTargetBinding().second, file_path);
    desp_node->chapterTextEncoding(err, refer_node->getTargetBinding().first,
                                   refer_node->getTargetBinding().second, fencoding);

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
    auto chapter_node = static_cast<ReferenceItem*>(item);
    auto bind = chapter_node->getTargetBinding();
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
    connect(ndoc, &QTextDocument::contentsChanged, chapter_node,  &ReferenceItem::calcWordsCount);

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

ReferenceItem *NovelHost::append_volume(QStandardItemModel *model, const QString &title)
{
    QList<QStandardItem*> row;
    auto one = new ReferenceItem(*this, title, true);
    row << one;
    auto two = new QStandardItem("-");
    row << two;
    model->appendRow(row);

    return one;
}

ReferenceItem *NovelHost::append_chapter(ReferenceItem *volumeNode, const QString &title)
{
    QList<QStandardItem*> row;
    auto one = new ReferenceItem(*this, title);
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

    auto xitem = static_cast<ReferenceItem*>(item);

    if(xitem->getTargetBinding().first < 0){
        QString err;
        desp_node->resetVolumeTitle(err, item->row(), item->text());
    }
    else {
        QString err;
        auto flow = xitem->getTargetBinding();
        desp_node->resetChapterTitle(err, flow.first, flow.second, item->text());

        if(opening_documents.contains(xitem))
            emit documentActived(opening_documents.value(xitem).first, item->text());
    }
}

int NovelHost::remove_node_recursive(QString &errOut, const QModelIndex &one2)
{
    QModelIndex index = one2;
    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = node_navigate_model->itemFromIndex(index);
    auto xitem = static_cast<ReferenceItem*>(item);

    int retc;
    auto bind = xitem->getTargetBinding();
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



ReferenceItem::ReferenceItem(NovelHost &host, const QString &disp, bool isGroup)
    :QStandardItem (disp),host(host)
{
    if(isGroup){
        setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    }
    else {
        setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    }
}

QPair<int, int> ReferenceItem::getTargetBinding()
{
    auto pnode = QStandardItem::parent();
    if(pnode)
        return qMakePair(pnode->row(), row());
    else
        return qMakePair(-1, row());
}

void ReferenceItem::calcWordsCount()
{
    auto bind = getTargetBinding();
    if(bind.first < 0){
        for (auto index = 0; index<rowCount(); ++index) {
            static_cast<ReferenceItem*>(child(index))->calcWordsCount();
        }
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
FStructure::FStructure(){}

FStructure::~FStructure(){}

void FStructure::newEmptyFile()
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

int FStructure::openFile(QString &errOut, const QString &filePath)
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

QString FStructure::novelDescribeFilePath() const
{
    return filepath_stored;
}

int FStructure::save(QString &errOut, const QString &newFilepath)
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

QString FStructure::novelTitle() const
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    return story_tree.attribute("title");
}

void FStructure::resetNovelTitle(const QString &title)
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    story_tree.setAttribute("title", title);
}

QString FStructure::novelDescription() const
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    return story_tree.attribute("desp");
}

void FStructure::resetNovelDescription(const QString &desp)
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();
    story_tree.setAttribute("desp", desp);
}

int FStructure::volumeCount() const
{
    return struct_dom_store.elementsByTagName("volume").size();
}

int FStructure::volumeAt(QString err, int index, FStructure::NodeSymbo &node) const
{
    auto story_tree = struct_dom_store.elementsByTagName("story-tree").at(0).toElement();

    int code;
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, story_tree, "volume", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::VOLUME);
    return 0;
}

int FStructure::insertVolume(QString &err, const FStructure::NodeSymbo &before, const QString &title, const QString &description, FStructure::NodeSymbo &node)
{
    if(before.type_stored != NodeSymbo::Type::VOLUME){
        err = "传入节点类型错误";
        return -1;
    }

    auto newdom = struct_dom_store.createElement("volume");
    NodeSymbo aone(newdom, NodeSymbo::Type::VOLUME);

    int code;
    if((code = aone.setAttr(err, "name", title)))
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

int FStructure::knodeCount(QString &err, const FStructure::NodeSymbo &vmNode, int &num) const
{
    int code;
    if((code = check_node_valid(err, vmNode, NodeSymbo::Type::VOLUME)))
        return code;

    auto list = vmNode.dom_stored.elementsByTagName("keynode");
    num = list.size();
    return 0;
}

int FStructure::knodeAt(QString &err, const FStructure::NodeSymbo &vmNode, int index, FStructure::NodeSymbo &node) const
{
    int code;
    if((code = check_node_valid(err, vmNode, NodeSymbo::Type::VOLUME)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, vmNode.dom_stored, "keynode", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::KEYNODE);
    return 0;
}

int FStructure::insertKnode(QString &err, FStructure::NodeSymbo &vmNode, int before, const QString &title, const QString &description, FStructure::NodeSymbo &node)
{
    int code;
    if((code = check_node_valid(err, vmNode, NodeSymbo::Type::VOLUME)))
        return code;

    int num;
    QList<QString> kkeys;
    if((code = knodeCount(err, vmNode, num)))
        return code;
    for (int var = 0; var < num; ++var) {
        NodeSymbo one;
        knodeAt(err, vmNode, var, one);
        QString key;
        one.attr(err, "key", key);
        kkeys << key;
    }
    QString unique_key="keynode-0";
    while (kkeys.contains(unique_key)) {
        unique_key = QString("keynode-%1").arg(gen.generate64());
    }

    auto ndom = struct_dom_store.createElement("keynode");
    NodeSymbo one(ndom, NodeSymbo::Type::KEYNODE);
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
        NodeSymbo _before;
        if((code = knodeAt(err, vmNode, before, _before)))
            return code;
        vmNode.dom_stored.insertBefore(ndom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStructure::pnodeCount(QString &err, const FStructure::NodeSymbo &knode, int &num) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto list = knode.dom_stored.elementsByTagName("points").at(0).childNodes();
    num = list.size();
    return 0;
}

int FStructure::pnodeAt(QString &err, const FStructure::NodeSymbo &knode, int index, FStructure::NodeSymbo &node) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    QDomElement elm;
    auto points_elm = knode.dom_stored.firstChildElement("points");
    if((code = find_direct_subdom_at_index(err, points_elm, "simply", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::POINT);
    return 0;
}

int FStructure::insertPnode(QString &err, FStructure::NodeSymbo &knode, int before, const QString &title, const QString &description, FStructure::NodeSymbo &node)
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto dom = struct_dom_store.createElement("simply");
    NodeSymbo one(dom, NodeSymbo::Type::POINT);
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "desp", description)))
        return code;

    int num;
    if((code = pnodeCount(err, knode, num)))
        return code;
    if(before >= num){
        knode.dom_stored.firstChildElement("points").appendChild(dom);
    }
    else {
        NodeSymbo _before;
        if((code = pnodeAt(err, knode, before, _before)))
            return code;
        knode.dom_stored.firstChildElement("points").insertBefore(dom, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStructure::foreshadowCount(QString &err, const FStructure::NodeSymbo &knode, int &num) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    num = foreshodows_node.elementsByTagName("foreshadow").size();
    return 0;
}

int FStructure::foreshadowAt(QString &err, const FStructure::NodeSymbo &knode, int index, FStructure::NodeSymbo &node) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, foreshadows_node, "foreshadow", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::FORESHADOW);
    return 0;
}

int FStructure::insertForeshadow(QString &err, FStructure::NodeSymbo &knode, int before, const QString &title,
                                 const QString &desp0, const QString &desp1, FStructure::NodeSymbo &node)
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    int num;
    QList<QString> fkeys;
    if((code = foreshadowCount(err, knode, num)))
        return code;
    for (auto index = 0; index<num; ++index) {
        NodeSymbo one;
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
    NodeSymbo one(elm, NodeSymbo::Type::FORESHADOW);
    if((code = one.setAttr(err, "key", unique_key)))
        return code;
    if((code = one.setAttr(err, "title", title)))
        return code;
    if((code = one.setAttr(err, "desp", desp0)))
        return code;
    if((code = one.setAttr(err, "desp_next", desp1)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    if(before>=num){
        foreshadows_node.appendChild(elm);
    }
    else {
        NodeSymbo _before;
        if((code = foreshadowAt(err, knode, before, _before)))
            return code;
        foreshadows_node.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStructure::shadowstopCount(QString &err, const FStructure::NodeSymbo &knode, int &num) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto foreshodows_node = knode.dom_stored.firstChildElement("foreshadows");
    num = foreshodows_node.elementsByTagName("shadow-stop").size();
    return 0;
}

int FStructure::shadowstopAt(QString &err, const FStructure::NodeSymbo &knode, int index, FStructure::NodeSymbo &node) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto foreshadows_node = knode.dom_stored.firstChildElement("foreshadows");
    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, foreshadows_node, "shadow-stop", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::SHADOWSTOP);
    return 0;
}

int FStructure::insertShadowstop(QString &err, FStructure::NodeSymbo &knode, int before, const QString &vfrom,
                                 const QString &kfrom, const QString &connect_shadow, FStructure::NodeSymbo &node)
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    auto elm = struct_dom_store.createElement("shadow-stop");
    NodeSymbo one(elm, NodeSymbo::Type::SHADOWSTOP);
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
        NodeSymbo _before;
        if((code = shadowstopAt(err, knode, before, _before)))
            return code;
        foreshadows_node.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStructure::chapterCount(QString &err, const FStructure::NodeSymbo &knode, int &num) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    num = knode.dom_stored.elementsByTagName("chapter").size();
    return 0;
}

int FStructure::chapterAt(QString &err, const FStructure::NodeSymbo &knode, int index, FStructure::NodeSymbo &node) const
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    QDomElement elm;
    if((code = find_direct_subdom_at_index(err, knode.dom_stored, "chapter", index, elm)))
        return code;

    node = NodeSymbo(elm, NodeSymbo::Type::KEYNODE);
    return 0;
}

int FStructure::insertChapter(QString &err, FStructure::NodeSymbo &knode, int before, const QString &title, const QString &description, FStructure::NodeSymbo &node)
{
    int code;
    if((code = check_node_valid(err, knode, NodeSymbo::Type::KEYNODE)))
        return code;

    int num;
    if((code = chapterCount(err, knode, num)))
        return code;
    QList<QString> ckeys;
    for (auto var=0; var<num; ++var) {
        NodeSymbo one;
        chapterAt(err, knode, var, one);
        QString key;
        ckeys << key;
    }
    QString unique_key="chapter-0";
    while (ckeys.contains(unique_key)) {
        unique_key = QString("chapter-%1").arg(gen.generate64());
    }

    auto elm = struct_dom_store.createElement("chapter");
    NodeSymbo one(elm, NodeSymbo::Type::CHAPTER);
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
        NodeSymbo _before;
        if((code = chapterAt(err, knode, before, _before)))
            return code;
        knode.dom_stored.insertBefore(elm, _before.dom_stored);
    }

    node = one;
    return 0;
}

int FStructure::removeNodeSymbo(QString &err, const FStructure::NodeSymbo &node)
{
    if(node.dom_stored.isNull()){
        err = "指定节点失效";
        return -1;
    }

    auto parent = node.dom_stored.parentNode();
    if(parent.isNull()){
        err = "父节点非法";
        return -1;
    }

    parent.removeChild(node.dom_stored);
    return 0;
}

int FStructure::check_node_valid(QString &err, const FStructure::NodeSymbo &node, FStructure::NodeSymbo::Type type) const
{
    if(node.type_stored != type){
        err = "传入节点类型错误";
        return -1;
    }

    if(node.dom_stored.isNull()){
        err = "传入节点已失效";
        return -1;
    }

    return 0;
}

int FStructure::find_direct_subdom_at_index(QString &err, const QDomElement &pnode, const QString &tagName, int index, QDomElement &node) const
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

FStructure::NodeSymbo::NodeSymbo(){}

FStructure::NodeSymbo::NodeSymbo(QDomElement domNode, FStructure::NodeSymbo::Type type)
    :dom_stored(domNode),
      type_stored(type){}

FStructure::NodeSymbo::NodeSymbo(const FStructure::NodeSymbo &other)
    :dom_stored(other.dom_stored),
      type_stored(other.type_stored){}

FStructure::NodeSymbo &FStructure::NodeSymbo::operator=(const FStructure::NodeSymbo &other){
    dom_stored = other.dom_stored;
    type_stored = other.type_stored;
    return *this;
}

int FStructure::NodeSymbo::attr(QString &err, const QString &name, QString &out) const
{
    if(dom_stored.isNull()){
        err = "节点已失效";
        return -1;
    }

    out = dom_stored.attribute(name);
    return 0;
}

int FStructure::NodeSymbo::setAttr(QString &err, const QString &name, const QString &value)
{
    if(dom_stored.isNull()){
        err = "节点已失效";
        return -1;
    }
    dom_stored.setAttribute(name, value);
    return 0;
}
