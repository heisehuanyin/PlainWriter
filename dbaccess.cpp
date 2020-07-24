#include "dbaccess.h"
#include "common.h"

#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QtDebug>

using namespace NovelBase;

DBAccess::DBAccess(){}

void DBAccess::loadFile(const QString &filePath)
{
    this->dbins = QSqlDatabase::addDatabase("QSQLITE");
    dbins.setDatabaseName(filePath);
    if(!dbins.open())
        throw new WsException("数据库无法打开！"+filePath);

    QSqlQuery x(dbins);
    x.exec("PRAGMA foreign_keys = ON;");
}

void DBAccess::createEmptyDB(const QString &dest)
{
    if(QFile(dest).exists())
        throw new WsException("指定文件已存在，无法完成创建!"+dest);

    loadFile(dest);
    init_tables(dbins);
}

#define ExSqlQuery(sql) \
    if(!(sql).exec()) \
    throw new WsException(sql.lastError().text());

DBAccess::TreeNode DBAccess::novelTreeNode() const
{
    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=-1");
    ExSqlQuery(sql);

    if(sql.next())
        return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::NOVEL);

    return TreeNode();
}

QString DBAccess::titleOfTreeNode(const DBAccess::TreeNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select title from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::resetTitleOfTreeNode(const DBAccess::TreeNode &node, const QString &title)
{
    auto sql = getStatement();
    sql.prepare("update keys_tree set title=:title where id=:id");
    sql.bindValue(":title", title);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

QString DBAccess::descriptionOfTreeNode(const DBAccess::TreeNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select desp from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::resetDescriptionOfTreeNode(const DBAccess::TreeNode &node, const QString &description)
{
    auto sql = getStatement();
    sql.prepare("update keys_tree set desp=:title where id=:id");
    sql.bindValue(":title", description);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

DBAccess::TreeNode DBAccess::parentOfTreeNode(const DBAccess::TreeNode &node) const
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

int DBAccess::indexOfTreeNode(const DBAccess::TreeNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select nindex from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toInt();
}

int DBAccess::childCountOfTreeNode(const DBAccess::TreeNode &pnode, TreeNode::Type type) const
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

DBAccess::TreeNode DBAccess::childAtOfTreeNode(const DBAccess::TreeNode &pnode, TreeNode::Type type, int index) const
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

DBAccess::TreeNode DBAccess::insertChildTreeNodeBefore(const DBAccess::TreeNode &pnode, DBAccess::TreeNode::Type type,
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

void DBAccess::removeTreeNode(const DBAccess::TreeNode &node)
{
    auto pnode = parentOfTreeNode(node);
    auto index = indexOfTreeNode(node);
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

DBAccess::TreeNode DBAccess::getTreeNodeViaID(int id) const
{
    auto sql = getStatement();
    sql.prepare("select type from keys_tree where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("传入了无效id");

    return TreeNode(this, id, static_cast<TreeNode::Type>(sql.value(0).toInt()));
}

DBAccess::TreeNode DBAccess::firstChapterTreeNode() const
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

DBAccess::TreeNode DBAccess::lastChapterTreeNode() const
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

DBAccess::TreeNode DBAccess::nextChapterTreeNode(const DBAccess::TreeNode &chapterIns) const
{
    auto pnode = parentOfTreeNode(chapterIns);
    auto index = indexOfTreeNode(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index+1);
    ExSqlQuery(sql);

    if(!sql.next())
        return TreeNode();
    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

DBAccess::TreeNode DBAccess::previousChapterTreeNode(const DBAccess::TreeNode &chapterIns) const
{
    auto pnode = parentOfTreeNode(chapterIns);
    auto index = indexOfTreeNode(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index-1);
    ExSqlQuery(sql);

    if(!sql.next())
        return TreeNode();
    return TreeNode(this, sql.value(0).toInt(), TreeNode::Type::CHAPTER);
}

QString DBAccess::chapterText(const DBAccess::TreeNode &chapter) const
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

void DBAccess::resetChapterText(const DBAccess::TreeNode &chapter, const QString &text)
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

int DBAccess::indexOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

bool DBAccess::closeStateOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select close from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

void DBAccess::resetCloseStateOfAttachPoint(const DBAccess::LineAttachPoint &node, bool state)
{
    auto q = getStatement();
    q.prepare("update points_collect set close = :close where id=:id");
    q.bindValue(":close", static_cast<int>(state));
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

QString DBAccess::titleOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select title from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

void DBAccess::resetTitleOfAttachPoint(const DBAccess::LineAttachPoint &node, const QString &title)
{
    auto q = getStatement();
    q.prepare("update points_collect set title=:t where id = :id");
    q.bindValue(":t", title);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

QString DBAccess::descriptionOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select desp from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

void DBAccess::resetDescriptionOfAttachPoint(const DBAccess::LineAttachPoint &node, const QString &description)
{
    auto q = getStatement();
    q.prepare("update points_collect set desp=:t where id = :id");
    q.bindValue(":t", description);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::TreeNode DBAccess::desplineOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select despline_ref from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return TreeNode(this, q.value(0).toInt(), TreeNode::Type::DESPLINE);
}

DBAccess::TreeNode DBAccess::chapterOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select chapter_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return TreeNode();

    return TreeNode(this, q.value(0).toInt(), TreeNode::Type::CHAPTER);
}

void DBAccess::resetChapterOfAttachPoint(const DBAccess::LineAttachPoint &node, const DBAccess::TreeNode &chapter)
{
    auto q = getStatement();
    q.prepare("update points_collect set chapter_attached = :cid where id=:id");
    q.bindValue(":cid", chapter.isValid()?chapter.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::TreeNode DBAccess::storyblockOfAttachPoint(const DBAccess::LineAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select story_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return TreeNode();

    return TreeNode(this, q.value(0).toInt(), TreeNode::Type::STORYBLOCK);
}

void DBAccess::resetStoryblockOfAttachPoint(const DBAccess::LineAttachPoint &node, const DBAccess::TreeNode &storyblock)
{
    auto q = getStatement();
    q.prepare("update points_collect set story_attached = :cid where id=:id");
    q.bindValue(":cid", storyblock.uniqueID());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

QList<DBAccess::LineAttachPoint> DBAccess::getAttachPointsViaDespline(const DBAccess::TreeNode &despline) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where despline_ref=:ref order by nindex");
    sql.bindValue(":ref", despline.uniqueID());
    ExSqlQuery(sql);

    QList<LineAttachPoint> ret;
    while (sql.next()) {
        ret << LineAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::LineAttachPoint> DBAccess::getAttachPointsViaChapter(const DBAccess::TreeNode &chapter) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where chapter_attached=:ref");
    sql.bindValue(":ref", chapter.uniqueID());
    ExSqlQuery(sql);

    QList<LineAttachPoint> ret;
    while (sql.next()) {
        ret << LineAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::LineAttachPoint> DBAccess::getAttachPointsViaStoryblock(const DBAccess::TreeNode &storyblock) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where story_attached=:ref");
    sql.bindValue(":ref", storyblock.uniqueID());
    ExSqlQuery(sql);

    QList<LineAttachPoint> ret;
    while (sql.next()) {
        ret << LineAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

DBAccess::LineAttachPoint DBAccess::insertAttachPointBefore(const DBAccess::TreeNode &despline, int index, bool close,
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
    return LineAttachPoint(this, q.value(0).toInt());
}

void DBAccess::removeAttachPoint(DBAccess::LineAttachPoint point)
{
    auto attached = point.attachedDespline();
    auto q = getStatement();
    q.prepare("update points_collect set nindex=nindex-1 where despline_ref=:ref and nindex>=:idx");
    q.bindValue(":ref", attached.uniqueID());
    q.bindValue(":idx", point.index());
    ExSqlQuery(q);

    q.prepare("delete from points_collect where id=:id");
    q.bindValue(":id", point.uniqueID());
    ExSqlQuery(q);
}

QSqlQuery DBAccess::getStatement() const
{
    return QSqlQuery(dbins);
}

void DBAccess::init_tables(QSqlDatabase &db)
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














DBAccess::TreeNode::TreeNode():valid_state(false),host(nullptr){}

DBAccess::TreeNode::TreeNode(const DBAccess::TreeNode &other)
    :valid_state(other.valid_state),
      id_store(other.id_store),
      node_type(other.node_type),
      host(other.host){}

QString DBAccess::TreeNode::title() const{return host->titleOfTreeNode(*this);}

QString DBAccess::TreeNode::description() const{return host->descriptionOfTreeNode(*this);}

DBAccess::TreeNode::Type DBAccess::TreeNode::type() const{return node_type;}

int DBAccess::TreeNode::uniqueID() const{return id_store;}

bool DBAccess::TreeNode::isValid() const{return valid_state;}

DBAccess::TreeNode DBAccess::TreeNode::parent() const{return host->parentOfTreeNode(*this);}

int DBAccess::TreeNode::index() const
{
    return host->indexOfTreeNode(*this);
}

int DBAccess::TreeNode::childCount(DBAccess::TreeNode::Type type) const{return host->childCountOfTreeNode(*this, type);}

DBAccess::TreeNode DBAccess::TreeNode::childAt(DBAccess::TreeNode::Type type, int index) const
{return host->childAtOfTreeNode(*this, type, index);}

DBAccess::TreeNode &DBAccess::TreeNode::operator=(const DBAccess::TreeNode &other)
{
    valid_state = other.valid_state;
    id_store = other.id_store;
    node_type = other.node_type;
    host = other.host;

    return *this;
}

bool DBAccess::TreeNode::operator==(const DBAccess::TreeNode &other) const
{
    return host==other.host && valid_state==other.valid_state &&
            id_store == other.id_store && node_type==other.node_type;
}

bool DBAccess::TreeNode::operator!=(const DBAccess::TreeNode &other) const{return !(*this == other);}

DBAccess::TreeNode::TreeNode(const DBAccess *host, int uid, DBAccess::TreeNode::Type type)
    :valid_state(true),id_store(uid),node_type(type),host(host){}












DBAccess::LineAttachPoint::LineAttachPoint(const DBAccess::LineAttachPoint &other)
    :id_store(other.id_store),host(other.host){}

int DBAccess::LineAttachPoint::uniqueID() const {return id_store;}

DBAccess::TreeNode DBAccess::LineAttachPoint::attachedDespline() const
{
    return host->desplineOfAttachPoint(*this);
}

DBAccess::TreeNode DBAccess::LineAttachPoint::attachedChapter() const
{
    return host->chapterOfAttachPoint(*this);
}

DBAccess::TreeNode DBAccess::LineAttachPoint::attachedStoryblock() const
{
    return host->storyblockOfAttachPoint(*this);
}

int DBAccess::LineAttachPoint::index() const
{
    return host->indexOfAttachPoint(*this);
}

bool DBAccess::LineAttachPoint::isClosed() const
{
    return host->closeStateOfAttachPoint(*this);
}

QString DBAccess::LineAttachPoint::title() const
{
    return host->titleOfAttachPoint(*this);
}

QString DBAccess::LineAttachPoint::description() const
{
    return host->descriptionOfAttachPoint(*this);
}

DBAccess::LineAttachPoint &DBAccess::LineAttachPoint::operator=(const DBAccess::LineAttachPoint &other)
{
    id_store = other.id_store;
    host = other.host;

    return *this;
}

DBAccess::LineAttachPoint::LineAttachPoint(const DBAccess *host, int id)
    :id_store(id), host(host){}
