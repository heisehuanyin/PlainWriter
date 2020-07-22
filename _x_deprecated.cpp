#include "_x_deprecated.h"
#include "common.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

using namespace NovelBase;

// novel-struct describe ===================================================================================
_X_FStruct::_X_FStruct(){}

_X_FStruct::~_X_FStruct(){}

void _X_FStruct::newEmptyFile()
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

void _X_FStruct::openFile(const QString &filePath)
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


void _X_FStruct::setAttr(_X_FStruct::NHandle &handle, const QString &name, const QString &value)
{
    handle.setAttr(name, value);
}

QString _X_FStruct::novelDescribeFilePath() const
{
    return filepath_stored;
}

void _X_FStruct::save(const QString &newFilepath)
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

QString _X_FStruct::novelTitle() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.attribute("title");
}

void _X_FStruct::resetNovelTitle(const QString &title)
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    struct_node.setAttribute("title", title);
}

QString _X_FStruct::novelDescription() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.attribute("desp");
}

void _X_FStruct::resetNovelDescription(const QString &desp)
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    struct_node.setAttribute("desp", desp);
}

int _X_FStruct::volumeCount() const
{
    auto struct_node = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    return struct_node.elementsByTagName("volume").size();
}

_X_FStruct::NHandle _X_FStruct::volumeAt(int index) const
{
    auto struct_dom = struct_dom_store.elementsByTagName("struct").at(0).toElement();
    auto elm = find_subelm_at_index(struct_dom, "volume", index);
    return NHandle(elm, NHandle::Type::VOLUME);
}

_X_FStruct::NHandle _X_FStruct::insertVolume(const _X_FStruct::NHandle &before, const QString &title, const QString &description)
{
    if(before.nType() != NHandle::Type::VOLUME)
        throw new WsException("传入节点类型错误");

    QList<QString> keys;
    for (auto var=0; var<volumeCount(); ++var) {
        _X_FStruct::NHandle volume_one = volumeAt(var);
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

int _X_FStruct::keystoryCount(const _X_FStruct::NHandle &vmNode) const
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);

    auto list = vmNode.elm_stored.elementsByTagName("keystory");
    return list.size();
}

_X_FStruct::NHandle _X_FStruct::keystoryAt(const _X_FStruct::NHandle &vmNode, int index) const
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);

    QDomElement elm = find_subelm_at_index(vmNode.elm_stored, "keystory", index);
    return NHandle(elm, NHandle::Type::KEYSTORY);
}

_X_FStruct::NHandle _X_FStruct::insertKeystory(_X_FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);

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

int _X_FStruct::pointCount(const _X_FStruct::NHandle &knode) const
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

    auto list = knode.elm_stored.elementsByTagName("points").at(0).childNodes();
    return list.size();
}

_X_FStruct::NHandle _X_FStruct::pointAt(const _X_FStruct::NHandle &knode, int index) const
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

    auto points_elm = knode.elm_stored.firstChildElement("points");
    QDomElement elm = find_subelm_at_index(points_elm, "simply", index);
    return NHandle(elm, NHandle::Type::POINT);
}

_X_FStruct::NHandle _X_FStruct::insertPoint(_X_FStruct::NHandle &knode, int before, const QString &title, const QString &description)
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

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

int _X_FStruct::foreshadowCount(const _X_FStruct::NHandle &knode) const
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshodows_node = knode.elm_stored.firstChildElement("foreshadows");
    return foreshodows_node.elementsByTagName("foreshadow").size();
}

_X_FStruct::NHandle _X_FStruct::foreshadowAt(const _X_FStruct::NHandle &knode, int index) const
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

    auto foreshadows_node = knode.elm_stored.firstChildElement("foreshadows");
    QDomElement elm = find_subelm_at_index(foreshadows_node, "foreshadow", index);
    return NHandle(elm, NHandle::Type::FORESHADOW);
}

_X_FStruct::NHandle _X_FStruct::findForeshadow(const QString &keysPath) const
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

_X_FStruct::NHandle _X_FStruct::appendForeshadow(_X_FStruct::NHandle &knode, const QString &title, const QString &desp, const QString &desp_next)
{
    checkHandleValid(knode, NHandle::Type::KEYSTORY);

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

QString _X_FStruct::foreshadowKeysPath(const _X_FStruct::NHandle &foreshadow) const
{
    checkHandleValid(foreshadow, NHandle::Type::FORESHADOW);

    auto keystory_ins = parentHandle(foreshadow);
    auto volume_ins = parentHandle(keystory_ins);
    return volume_ins.attr("key")+"@"+keystory_ins.attr("key")+"@"+foreshadow.attr("key");
}

int _X_FStruct::chapterCount(const _X_FStruct::NHandle &vmNode) const
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);

    return vmNode.elm_stored.elementsByTagName("chapter").size();
}

_X_FStruct::NHandle _X_FStruct::chapterAt(const _X_FStruct::NHandle &vmNode, int index) const
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);
    QDomElement elm = find_subelm_at_index(vmNode.elm_stored, "chapter", index);
    return NHandle(elm, NHandle::Type::CHAPTER);
}

_X_FStruct::NHandle _X_FStruct::insertChapter(_X_FStruct::NHandle &vmNode, int before, const QString &title, const QString &description)
{
    checkHandleValid(vmNode, NHandle::Type::VOLUME);

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

QString _X_FStruct::chapterKeysPath(const _X_FStruct::NHandle &chapter) const
{
    checkHandleValid(chapter, NHandle::Type::CHAPTER);

    auto volume = parentHandle(chapter);
    auto vkey = volume.attr("key");
    return vkey+"@"+chapter.attr("key");
}

QString _X_FStruct::chapterCanonicalFilePath(const _X_FStruct::NHandle &chapter) const
{
    checkHandleValid(chapter, _X_FStruct::NHandle::Type::CHAPTER);

    QString relative_path;
    relative_path = chapter.attr("relative");

    return QDir(QFileInfo(this->filepath_stored).canonicalPath()).filePath(relative_path);
}

QString _X_FStruct::chapterTextEncoding(const _X_FStruct::NHandle &chapter) const
{
    checkHandleValid(chapter, _X_FStruct::NHandle::Type::CHAPTER);

    return chapter.attr("encoding");
}

int _X_FStruct::shadowstartCount(const _X_FStruct::NHandle &chpNode) const
{
    checkHandleValid(chpNode, _X_FStruct::NHandle::Type::CHAPTER);
    return chpNode.elm_stored.elementsByTagName("shadow-start").size();
}

_X_FStruct::NHandle _X_FStruct::shadowstartAt(const _X_FStruct::NHandle &chpNode, int index) const
{
    checkHandleValid(chpNode, _X_FStruct::NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.elm_stored, "shadow-start", index);
    return NHandle(elm, NHandle::Type::SHADOWSTART);
}

_X_FStruct::NHandle _X_FStruct::findShadowstart(const _X_FStruct::NHandle &chpNode, const QString &target) const
{
    checkHandleValid(chpNode, NHandle::Type::CHAPTER);

    int count = shadowstartCount(chpNode);
    for (int var = 0; var < count; ++var) {
        auto start_ins = shadowstartAt(chpNode, var);
        if(start_ins.attr("target") == target)
            return start_ins;
    }

    return NHandle(QDomElement(), NHandle::Type::SHADOWSTART);
}

_X_FStruct::NHandle _X_FStruct::appendShadowstart(_X_FStruct::NHandle &chpNode, const QString &keystory, const QString &foreshadow)
{
    checkHandleValid(chpNode, _X_FStruct::NHandle::Type::CHAPTER);

    auto volume = parentHandle(chpNode);
    auto volume_key = volume.attr("key");

    auto elm = struct_dom_store.createElement("shadow-start");
    NHandle one(elm, NHandle::Type::SHADOWSTART);
    one.setAttr("target", volume_key+"@"+keystory+"@"+foreshadow);

    chpNode.elm_stored.appendChild(elm);
    return one;
}

int _X_FStruct::shadowstopCount(const _X_FStruct::NHandle &chpNode) const
{
    checkHandleValid(chpNode, NHandle::Type::CHAPTER);
    return chpNode.elm_stored.elementsByTagName("shadow-stop").size();
}

_X_FStruct::NHandle _X_FStruct::shadowstopAt(const _X_FStruct::NHandle &chpNode, int index) const
{
    checkHandleValid(chpNode, NHandle::Type::CHAPTER);

    QDomElement elm = find_subelm_at_index(chpNode.elm_stored, "shadow-stop", index);
    return NHandle(elm, NHandle::Type::SHADOWSTOP);
}

_X_FStruct::NHandle _X_FStruct::findShadowstop(const _X_FStruct::NHandle &chpNode, const QString &stopTarget) const
{
    checkHandleValid(chpNode, NHandle::Type::CHAPTER);

    int count = shadowstopCount(chpNode);
    for (int var=0; var < count; var++) {
        auto stop_ins = shadowstopAt(chpNode, var);
        if(stop_ins.attr("target") == stopTarget)
            return stop_ins;
    }

    return NHandle(QDomElement(), NHandle::Type::SHADOWSTOP);
}

_X_FStruct::NHandle _X_FStruct::appendShadowstop(_X_FStruct::NHandle &chpNode, const QString &volume,
                                           const QString &keystory, const QString &foreshadow)
{
    checkHandleValid(chpNode, NHandle::Type::CHAPTER);

    auto elm = struct_dom_store.createElement("shadow-stop");
    NHandle one(elm, NHandle::Type::SHADOWSTOP);
    one.setAttr("target", volume+"@"+keystory+"@"+foreshadow);

    chpNode.elm_stored.appendChild(elm);
    return one;
}

_X_FStruct::NHandle _X_FStruct::parentHandle(const _X_FStruct::NHandle &base) const
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

int _X_FStruct::handleIndex(const _X_FStruct::NHandle &node) const
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

void _X_FStruct::removeHandle(const _X_FStruct::NHandle &node)
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

_X_FStruct::NHandle _X_FStruct::firstChapterOfFStruct() const
{
    int volume_count = volumeCount();
    for (int v_index=0; v_index<volume_count; ++v_index) {
        auto volume_one = volumeAt(v_index);
        if(chapterCount(volume_one))    // 第一个chapter不为零的volume节点
            return chapterAt(volume_one, 0);
    }

    return NHandle();
}

_X_FStruct::NHandle _X_FStruct::lastChapterOfStruct() const
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

_X_FStruct::NHandle _X_FStruct::nextChapterOfFStruct(const _X_FStruct::NHandle &chapterIns) const
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

_X_FStruct::NHandle _X_FStruct::previousChapterOfFStruct(const _X_FStruct::NHandle &chapterIns) const
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

void _X_FStruct::checkHandleValid(const _X_FStruct::NHandle &node, _X_FStruct::NHandle::Type type) const
{
    if(node.nType() != type)
        throw new WsException("传入节点类型错误");

    if(!node.isValid())
        throw new WsException("传入节点已失效");
}

QDomElement _X_FStruct::find_subelm_at_index(const QDomElement &pnode, const QString &tagName, int index) const
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



_X_FStruct::NHandle::NHandle()
    :type_stored(Type::VOLUME){}

_X_FStruct::NHandle::NHandle(const _X_FStruct::NHandle &other)
    :elm_stored(other.elm_stored), type_stored(other.type_stored){}


_X_FStruct::NHandle &_X_FStruct::NHandle::operator=(const _X_FStruct::NHandle &other)
{
    elm_stored = other.elm_stored;
    type_stored = other.type_stored;
    return *this;
}

bool _X_FStruct::NHandle::operator==(const _X_FStruct::NHandle &other) const
{
    return elm_stored == other.elm_stored && type_stored == other.type_stored;
}

_X_FStruct::NHandle::Type _X_FStruct::NHandle::nType() const
{
    return type_stored;
}

bool _X_FStruct::NHandle::isValid() const
{
    return !elm_stored.isNull();
}

QString _X_FStruct::NHandle::attr(const QString &name) const{
    return elm_stored.attribute(name);
}

void _X_FStruct::NHandle::setAttr(const QString &name, const QString &value)
{
    elm_stored.setAttribute(name, value);
}


_X_FStruct::NHandle::NHandle(QDomElement elm, _X_FStruct::NHandle::Type type)
    :elm_stored(elm), type_stored(type){}
