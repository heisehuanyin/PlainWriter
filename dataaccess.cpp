#include "dataaccess.h"
#include "common.h"

#include <QFile>
#include <QSqlQuery>
#include <QSqlError>

using namespace NovelBase;

DataAccess::DataAccess(){}

void DataAccess::loadFile(const QString &filePath)
{
    this->dbins = QSqlDatabase::addDatabase("QSQLITE");
    dbins.setDatabaseName(filePath);
    if(!dbins.open())
        throw new WsException("数据库无法打开！"+filePath);

    QSqlQuery x(dbins);
    x.exec("PRAGMA foreign_keys = ON;");
}

void DataAccess::createEmptyDB(const QString &dest)
{
    if(QFile(dest).exists())
        throw new WsException("指定文件已存在，无法完成创建!"+dest);

    loadFile(dest);
    init_tables(dbins);
}

#define ExSqlQuery(sql) \
    if(!(sql).exec()) \
    throw new WsException(sql.lastError().text());

DataAccess::TreeNode DataAccess::novelRoot() const
{
    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=-1");
    ExSqlQuery(sql);

    if(sql.next())
        return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::NOVEL);

    return TreeNode();
}

DataAccess::TreeNode DataAccess::parent(const DataAccess::TreeNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select parent from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    switch (node.type()) {
        case TreeNode::Type::NOVEL:
            return TreeNode();
        case TreeNode::Type::VOLUME:
            return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::NOVEL);
        case TreeNode::Type::CHAPTER:
        case TreeNode::Type::DESPLINE:
        case TreeNode::Type::STORYBLOCK:
            return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::VOLUME);
        case TreeNode::Type::KEYPOINT:
            return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::STORYBLOCK);
        default:
            throw new WsException("意外的节点类型！");
    }
}

int DataAccess::nodeIndex(const DataAccess::TreeNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select nindex from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toInt();
}

int DataAccess::childNodeCount(const DataAccess::TreeNode &pnode, TreeNode::Type type) const
{
    auto sql = getStatement();
    sql.prepare("select count(*), type from keys_tree where parent=:pnode group by type");
    sql.bindValue(":pnode", pnode.uniqueID());
    ExSqlQuery(sql);

    while (sql.next()) {
        if(sql.value(1).toInt() == static_cast<int>(type))
            return sql.value(0).toInt();
    }

    return 0;
}

DataAccess::TreeNode DataAccess::childNodeAt(const DataAccess::TreeNode &pnode, TreeNode::Type type, int index) const
{
    auto sql = getStatement();
    sql.prepare("select id from keys_tree where parent=:pid and nindex=:ind and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":ind", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("指定节点指定索引无子节点");
    return TreeNode(this, sql.value(0).toInt(), type);
}

DataAccess::TreeNode DataAccess::insertChildBefore(DataAccess::TreeNode &pnode, DataAccess::TreeNode::Type type,
                                   int index, const QString &title, const QString &description)
{
    switch (pnode.type()) {
        case TreeNode::Type::NOVEL:
            if(type != TreeNode::Type::VOLUME)
                throw new WsException("插入错误节点类型");
            break;
        case TreeNode::Type::VOLUME:
            if(type != TreeNode::Type::CHAPTER &&
               type != TreeNode::Type::STORYBLOCK &&
               type != TreeNode::Type::DESPLINE)
                throw new WsException("插入错误节点类型");
            break;
        case TreeNode::Type::STORYBLOCK:
            if(type != TreeNode::Type::KEYPOINT)
                throw new WsException("插入错误节点类型");
            break;
        default:
            throw new WsException("插入错误节点类型");
    }


    auto sql = getStatement();
    sql.prepare("update keys_tree set nindex=nindex+1 where parent=:pid and nindex>=:index and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":index", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);


    sql.prepare("insert into keys_tree (type, parent, nindex, title, desp) "
                "values(:type, :pnode, :idx, :title, :desp)");
    sql.bindValue(":type", static_cast<int>(type));
    sql.bindValue(":pnode", pnode.uniqueID());
    sql.bindValue(":idx", index);
    sql.bindValue(":title", title);
    sql.bindValue(":desp", description);
    ExSqlQuery(sql);

    sql.prepare("select id from keys_tree where type=:type and nindex=:idx and parent=:pnode");
    sql.bindValue(":type", static_cast<int>(type));
    sql.bindValue(":idx", index);
    sql.bindValue(":pnode", pnode.uniqueID());
    ExSqlQuery(sql);
    if(!sql.next())
        throw new WsException("插入失败！");
    return TreeNode(this, sql.value(0).toInt(), type);
}

void DataAccess::removeNode(const DataAccess::TreeNode &node)
{
    auto pnode = parent(node);
    auto index = nodeIndex(node);
    auto type = node.type();

    auto sql = getStatement();
    sql.prepare("update keys_tree set nindex=nindex-1 where parent=:pid and nindex>=:index and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":index", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    sql.prepare("delete from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

DataAccess::TreeNode DataAccess::firstChapterOfFStruct() const
{
    auto sql =  getStatement();
    sql.prepare("select id from keys_tree where type=0 order by nindex");
    ExSqlQuery(sql);
    if(!sql.next())
        return TreeNode();

    auto fcid = sql.value(0).toInt();
    sql.prepare("select id from keys_tree where type=1 and parent=:pnode order by nindex");
    sql.bindValue(":pnode", fcid);
    ExSqlQuery(sql);
    if(!sql.next())
        return TreeNode();

    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

DataAccess::TreeNode DataAccess::lastChapterOfStruct() const
{
    auto sql =  getStatement();
    sql.prepare("select id from keys_tree where type=0 order by nindex desc");
    ExSqlQuery(sql);
    if(!sql.next())
        return TreeNode();

    auto fcid = sql.value(0).toInt();
    sql.prepare("select id from keys_tree where type=1 and parent=:pnode order by nindex desc");
    sql.bindValue(":pnode", fcid);
    ExSqlQuery(sql);
    if(!sql.next())
        return TreeNode();

    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

DataAccess::TreeNode DataAccess::nextChapterOfFStruct(const DataAccess::TreeNode &chapterIns) const
{
    auto pnode = parent(chapterIns);
    auto index = nodeIndex(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index+1);
    ExSqlQuery(sql);

    if(!sql.next())
        return TreeNode();
    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

DataAccess::TreeNode DataAccess::previousChapterOfFStruct(const DataAccess::TreeNode &chapterIns) const
{
    auto pnode = parent(chapterIns);
    auto index = nodeIndex(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index-1);
    ExSqlQuery(sql);

    if(!sql.next())
        return TreeNode();
    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

QString DataAccess::chapterText(const DataAccess::TreeNode &chapter) const
{
    if(chapter.type() != TreeNode::Type::CHAPTER)
        throw new WsException("指定节点非章节节点");

    auto sql = getStatement();
    sql.prepare("select content from contents_collect where chapter_ref = :cid");
    sql.bindValue(":cid", chapter.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toString();
}

void DataAccess::resetChapterText(const DataAccess::TreeNode &chapter, const QString &text)
{
    if(chapter.type() != TreeNode::Type::CHAPTER)
        throw new WsException("指定节点非章节节点");

    auto sql = getStatement();
    sql.prepare("select id from contents_collect where chapter_ref = :cid");
    sql.bindValue(":cid", chapter.uniqueID());
    ExSqlQuery(sql);

    if(!sql.next()){
        sql.prepare("insert into contents_collect "
                 "(chapter_ref, content) values(:cid, :text)");
        sql.bindValue(":cid", chapter.uniqueID());
        sql.bindValue(":text", text);
        ExSqlQuery(sql);
    }
    else {
        auto idint = sql.value(0).toInt();
        sql.prepare("update contents_collect set "
                    "content = :text where id = :id");
        sql.bindValue(":text", text);
        sql.bindValue(":id", idint);
        ExSqlQuery(sql);
    }
}

QList<DataAccess::LineStop> DataAccess::allPoints(const DataAccess::TreeNode &despline) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where despline_ref=:ref order by nindex");
    sql.bindValue(":ref", despline.uniqueID());
    ExSqlQuery(sql);

    QList<LineStop> ret;
    while (sql.next()) {
        ret << LineStop(this, sql.value(0).toInt());
    }

    return ret;
}

DataAccess::LineStop DataAccess::insertPointBefore(const DataAccess::TreeNode &despline, int index, bool close,
                                   const QString &title, const QString &description)
{
    auto q = getStatement();
    q.prepare("update points_collect set nindex=nindex+1 where despline_ref=:ref and nindex >=:idx");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    ExSqlQuery(q);

    q.prepare("insert into points_collect (despline_ref, nindex, close, title, desp) values(:ref, :idx, :cls, :t, :desp)");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    q.bindValue(":cls", close);
    q.bindValue(":t", title);
    q.bindValue(":desp", description);
    ExSqlQuery(q);

    q.prepare("select id from points_collect where despline_ref=:ref and nindex=:idx");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("驻点插入失败");
    return LineStop(this, q.value(0).toInt());
}

void DataAccess::removePoint(DataAccess::LineStop point)
{
    auto attached = point.desplineReference();
    auto q = getStatement();
    q.prepare("update points_collect set nindex=nindex-1 where despline_ref=:ref and nindex>=:idx");
    q.bindValue(":ref", attached.uniqueID());
    q.bindValue(":idx", point.index());
    ExSqlQuery(q);

    q.prepare("delete from points_collect where id=:id");
    q.bindValue(":id", point.uniqueID());
    ExSqlQuery(q);
}

QSqlQuery DataAccess::getStatement() const
{
    return QSqlQuery(dbins);
}

void DataAccess::init_tables(QSqlDatabase &db)
{
    QString statements[] = {
        "create table if not exists novel_basic("
        "id integer primary key autoincrement,"
        "title text not null,"
        "content text,"
        "comment text)",

        "create table if not exists keys_tree ("
        "id integer primary key autoincrement,"
        "type integer not null,"
        "parent integer,"
        "nindex integer,"
        "title text,"
        "desp text,"
        "constraint fkself foreign key (parent) references keys_tree(id) on delete cascade)",

        "create table if not exists contents_collect("
        "id integer primary key autoincrement,"
        "chapter_ref integer,"
        "content text,"
        "constraint fkout foreign key(chapter_ref) references keys_tree(id) on delete cascade)",

        "create table if not exists points_collect("
        "id integer primary key autoincrement,"
        "despline_ref integer not null,"
        "chapter_attached integer,"
        "story_attached integer,"
        "nindex integer,"
        "close integer default(0),"
        "title text,"
        "desp text,"
        "constraint fkout0 foreign key(despline_ref) references keys_tree(id) on delete cascade,"
        "constraint fkout1 foreign key(chapter_attached) references keys_tree(id) on delete cascade,"
        "constraint fkout2 foreign key(story_attached) references keys_tree(id) on delete cascade)",

        "insert into keys_tree "
        "(type, title, desp, nindex) values(-1, '新建书籍', '无简介', 0)"
    };

    for (int index = 0; index < 5; ++index) {
        auto statement = statements[index];
        QSqlQuery q(db);
        if(!q.exec(statement)){
            throw new WsException(QString("执行第%1语句错误：%2").arg(index).arg(q.lastError().text()));
        }
    }
}














DataAccess::TreeNode::TreeNode():valid_state(false),host(nullptr){}

DataAccess::TreeNode::TreeNode(const DataAccess::TreeNode &other)
    :valid_state(other.valid_state),
      id_store(other.id_store),
      node_type(other.node_type),
      host(nullptr){}

QString DataAccess::TreeNode::title() const
{
    auto sql = host->getStatement();
    sql.prepare("select title from keys_tree where id = :id");
    sql.bindValue(":id", id_store);
    if(!sql.exec()){
        throw new WsException(sql.lastError().text());
    }
    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DataAccess::TreeNode::titleReset(const QString &str)
{
    auto sql = host->getStatement();
    sql.prepare("update keys_tree set title=:title where id=:id");
    sql.bindValue(":title", str);
    sql.bindValue(":id", id_store);
    if(!sql.exec())
        throw new WsException(sql.lastError().text());
}

QString DataAccess::TreeNode::description() const
{
    auto sql = host->getStatement();
    sql.prepare("select desp from keys_tree where id = :id");
    sql.bindValue(":id", id_store);
    if(!sql.exec()){
        throw new WsException(sql.lastError().text());
    }
    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DataAccess::TreeNode::descriptionReset(const QString &str)
{
    auto sql = host->getStatement();
    sql.prepare("update keys_tree set desp=:title where id=:id");
    sql.bindValue(":title", str);
    sql.bindValue(":id", id_store);
    if(!sql.exec())
        throw new WsException(sql.lastError().text());
}

DataAccess::TreeNode::Type DataAccess::TreeNode::type() const{return node_type;}

int DataAccess::TreeNode::uniqueID() const{return id_store;}

bool DataAccess::TreeNode::isValid() const{return valid_state;}

DataAccess::TreeNode &DataAccess::TreeNode::operator=(const DataAccess::TreeNode &other)
{
    valid_state = other.valid_state;
    id_store = other.id_store;
    node_type = other.node_type;
    host = other.host;

    return *this;
}

bool DataAccess::TreeNode::operator==(const DataAccess::TreeNode &other) const
{
    return host==other.host && valid_state==other.valid_state &&
            id_store == other.id_store && node_type==other.node_type;
}

DataAccess::TreeNode::TreeNode(const DataAccess *host, int uid, DataAccess::TreeNode::Type type)
    :valid_state(true),id_store(uid),node_type(type),host(host){}

DataAccess::LineStop::LineStop(const DataAccess::LineStop &other)
    :id_store(other.id_store),host(other.host){}

int DataAccess::LineStop::uniqueID() const {return id_store;}

DataAccess::TreeNode DataAccess::LineStop::desplineReference() const
{
    auto q = host->getStatement();
    q.prepare("select despline_ref from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);

    q.next();
    return TreeNode(host, q.value(0).toInt(), TreeNode::Type::DESPLINE);
}

DataAccess::TreeNode DataAccess::LineStop::chapterAttached() const
{
    auto q = host->getStatement();
    q.prepare("select chapter_attached from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return TreeNode();

    return TreeNode(host, q.value(0).toInt(), TreeNode::Type::CHAPTER);
}

DataAccess::TreeNode DataAccess::LineStop::storyAttached() const
{
    auto q = host->getStatement();
    q.prepare("select story_attached from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return TreeNode();

    return TreeNode(host, q.value(0).toInt(), TreeNode::Type::STORYBLOCK);
}

void DataAccess::LineStop::chapterAttachedReset(const DataAccess::TreeNode &chapter)
{
    auto q = host->getStatement();
    q.prepare("update points_collect set chapter_attached = :cid where id=:id");
    q.bindValue(":cid", chapter.uniqueID());
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
}

void DataAccess::LineStop::storyAttachedReset(const DataAccess::TreeNode &story)
{
    auto q = host->getStatement();
    q.prepare("update points_collect set story_attached = :cid where id=:id");
    q.bindValue(":cid", story.uniqueID());
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
}

int DataAccess::LineStop::index() const
{
    auto q = host->getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

bool DataAccess::LineStop::closed() const
{
    auto q = host->getStatement();
    q.prepare("select close from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

void DataAccess::LineStop::colseReset(bool state)
{
    auto q = host->getStatement();
    q.prepare("update points_collect set close = :close where id=:id");
    q.bindValue(":close", static_cast<int>(state));
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
}

QString DataAccess::LineStop::title() const
{
    auto q = host->getStatement();
    q.prepare("select title from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);

    return q.value(0).toString();
}

void DataAccess::LineStop::titleReset(const QString &title)
{
    auto q = host->getStatement();
    q.prepare("update points_collect set title=:t where id = :id");
    q.bindValue(":t", title);
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
}

QString DataAccess::LineStop::description() const
{
    auto q = host->getStatement();
    q.prepare("select desp from points_collect where id=:id");
    q.bindValue(":id", id_store);
    ExSqlQuery(q);

    return q.value(0).toString();
}

void DataAccess::LineStop::descriptionReset(const QString &description)
{
    auto q = host->getStatement();
    q.prepare("update points_collect set desp=:t where id = :id");
    q.bindValue(":t", description);
    q.bindValue(":id", id_store);
    ExSqlQuery(q);
}

DataAccess::LineStop &DataAccess::LineStop::operator=(const DataAccess::LineStop &other){
    id_store = other.id_store;
    host = other.host;

    return *this;
}

DataAccess::LineStop::LineStop(const DataAccess *host, int id)
    :id_store(id), host(host){}
