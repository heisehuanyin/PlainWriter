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

int NovelHost::loadDescription(QString &err, StructDescription *desp)
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

int NovelHost::appendVolume(QString &errOut, const QString &gName)
{
    append_volume(node_navigate_model, gName);

    int code;
    if((code = desp_node->insertVolume(errOut, desp_node->volumeCount()+1, gName)))
        return code;

    return 0;
}

int NovelHost::appendChapter(QString &errOut, const QString &aName, const QModelIndex &_navigate_index)
{
    QModelIndex navigate_index = _navigate_index;
    if(!navigate_index.isValid()){
        errOut = "appendChapter: 非法modelindex";
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

        if((code = desp_node->insertChapter(errOut, xitem->row(), xitem->rowCount()+1, aName)))
            return code;

        int ccount;
        if((code = desp_node->chapterCount(errOut, xitem->row(), ccount)))
            return code;

        if((code = desp_node->chapterCanonicalFilepath(errOut, xitem->row(), ccount-1, new_file_path)))
            return code;
    }
    else {
        // 选中了章节节点
        auto pitem = item->parent();
        append_chapter(static_cast<ReferenceItem*>(pitem), aName);
        if((code = desp_node->insertChapter(errOut, pitem->row(), pitem->rowCount()+1, aName)))
            return code;

        int ccount;
        if((code = desp_node->chapterCount(errOut, pitem->row(), ccount)))
            return code;

        if((code = desp_node->chapterCanonicalFilepath(errOut, pitem->row(), ccount-1, new_file_path)))
            return code;
    }

    QFile target(new_file_path);
    if(target.exists()){
        errOut = "软件错误，出现重复文件名："+new_file_path;
        return -1;
    }

    if(!target.open(QIODevice::WriteOnly|QIODevice::Text)){
        errOut = "软件错误，指定路径文件无法打开："+new_file_path;
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
StructDescription::StructDescription(){}

StructDescription::~StructDescription(){}

void StructDescription::newDescription()
{
    struct_dom_store.appendChild(struct_dom_store.createProcessingInstruction("xml", "version='1.0' encoding='utf-8'"));
    auto root = struct_dom_store.createElement("root");
    root.setAttribute("version", "1.0");
    root.setAttribute("title", "新建小说");
    struct_dom_store.appendChild(root);

    auto config = struct_dom_store.createElement("config");
    root.appendChild(config);

    auto structnode = struct_dom_store.createElement("struct");
    root.appendChild(structnode);
}

int StructDescription::openDescription(QString &errOut, const QString &filePath)
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

QString StructDescription::novelDescribeFilePath() const
{
    return filepath_stored;
}

int StructDescription::save(QString &errOut, const QString &newFilepath)
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

QString StructDescription::novelTitle() const
{
    auto root = struct_dom_store.documentElement();
    return root.attribute("title");
}

void StructDescription::resetNovelTitle(const QString &title)
{
    struct_dom_store.documentElement().setAttribute("title", title);
}

int StructDescription::volumeCount() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
    return struct_node.childNodes().size();
}

int StructDescription::volumeTitle(QString &errOut, int volumeIndex, QString &titleOut) const
{
    QDomElement volume_node;
    auto x = find_volume_domnode_by_index(errOut, volumeIndex, volume_node);
    if(x) return x;

    titleOut = volume_node.attribute("title");
    return 0;
}

int StructDescription::insertVolume(QString &errOut, int volumeIndexBefore, const QString &volumeTitle)
{
    auto newv = struct_dom_store.createElement("volume");
    newv.setAttribute("title", volumeTitle);

    int ret;
    QDomElement volume_node;
    if((ret = find_volume_domnode_by_index(errOut, volumeIndexBefore, volume_node))){
        auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
        struct_node.appendChild(newv);
    }
    else {
        struct_dom_store.insertBefore(newv, volume_node);
    }

    return 0;
}

int StructDescription::removeVolume(QString &errOut, int volumeIndex)
{
    int ret;
    QDomElement volume_node;

    if((ret = find_volume_domnode_by_index(errOut, volumeIndex, volume_node)))
        return ret;

    volume_node.parentNode().removeChild(volume_node);
    return 0;
}

int StructDescription::resetVolumeTitle(QString &errOut, int volumeIndex, const QString &volumeTitle)
{
    int ret_code;
    QDomElement volume_node;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndex, volume_node)))
        return ret_code;

    volume_node.setAttribute("title", volumeTitle);
    return 0;
}

int StructDescription::chapterCount(QString &errOut, int volumeIndex, int &numOut) const
{
    int ret_code;
    QDomElement volume_dom;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return ret_code;

    numOut = volume_dom.childNodes().size();
    return 0;
}

int StructDescription::insertChapter(QString &errOut, int volumeIndexAt, int chapterIndexBefore,
                                     const QString &chapterTitle, const QString &encoding)
{
    int ret_code;
    QDomElement volume_dom;

    if((ret_code = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return ret_code;

    QList<QString> paths;
    // 新建章节节点
    auto all_chapter = struct_dom_store.elementsByTagName("chapter");
    for (int index=0; index<all_chapter.size(); ++index) {
        auto dom_item = all_chapter.at(index).toElement();

        auto relative_path = dom_item.attribute("relative");
        paths.append(relative_path);
    }

    QString new_relative_path = "chapter_0000000000.txt";
    while (paths.contains(new_relative_path)) {
        new_relative_path = QString("chapter_%1.txt").arg(gen.generate64());
    }

    auto newdom = struct_dom_store.createElement("chapter");
    newdom.setAttribute("title", chapterTitle);
    newdom.setAttribute("relative", new_relative_path);
    newdom.setAttribute("encoding", encoding);


    QDomElement chapter_dom;
    if((ret_code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndexBefore, chapter_dom))){
        volume_dom.appendChild(newdom);
    }
    else {
        volume_dom.insertBefore(newdom, chapter_dom);
    }

    return 0;
}

int StructDescription::removeChapter(QString &errOut, int volumeIndexAt, int chapterIndex)
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    volume_dom.removeChild(chapter_dom);
    return 0;
}

int StructDescription::resetChapterTitle(QString &errOut, int volumeIndexAt, int chapterIndex, const QString &title)
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndexAt, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    chapter_dom.setAttribute("title", title);
    return 0;
}

int StructDescription::chapterTitle(QString &errOut, int volumeIndex, int chapterIndex, QString &titleOut) const
{
    int rcode;
    QDomElement volume_dom, chapter_dom;

    if((rcode = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return rcode;

    if((rcode = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return rcode;

    titleOut =  chapter_dom.attribute("title");
    return 0;
}

int StructDescription::chapterCanonicalFilepath(QString &errOut, int volumeIndex, int chapterIndex, QString &pathOut) const
{
    int code;
    QDomElement volume_dom, chapter_dom;

    if((code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return code;

    if((code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return code;

    auto relative_path = chapter_dom.attribute("relative");
    auto dir_path = QFileInfo(filepath_stored).canonicalPath();

    pathOut = QDir(dir_path).filePath(relative_path);
    return 0;
}

int StructDescription::chapterTextEncoding(QString &errOut, int volumeIndex, int chapterIndex, QString &encodingOut) const
{
    int code;
    QDomElement volume_dom, chapter_dom;

    if((code = find_volume_domnode_by_index(errOut, volumeIndex, volume_dom)))
        return code;

    if((code = find_chapter_domnode_ty_index(errOut, volume_dom, chapterIndex, chapter_dom)))
        return code;

    encodingOut = chapter_dom.attribute("encoding");
    return 0;
}

int StructDescription::find_volume_domnode_by_index(QString &errO, int index, QDomElement &domOut) const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0);
    auto node = struct_node.firstChildElement("volume");

    while (!node.isNull()) {
        if(!index){
            domOut = node.toElement();
            return 0;
        }

        node = node.nextSiblingElement("volume");
        index--;
    }

    errO = QString("volumeIndex超界：%1").arg(index);
    return -1;
}

int StructDescription::find_chapter_domnode_ty_index(QString &errO, const QDomElement &volumeNode,
                                                     int index, QDomElement &domOut) const
{
    auto node = volumeNode.firstChildElement("chapter");

    while (!node.isNull()) {
        if(!index){
            domOut = node.toElement();
            return 0;
        }

        node = node.nextSiblingElement("chapter");
        index--;
    }

    errO = QString("chapterIndex超界：%1").arg(index);
    return -1;
}
