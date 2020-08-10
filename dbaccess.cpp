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

void DBAccess::createEmptyFile(const QString &dest)
{
    if(QFile(dest).exists())
        throw new WsException("指定文件已存在，无法完成创建!"+dest);

    loadFile(dest);
    init_tables(dbins);
}

#define ExSqlQuery(sql) \
    if(!(sql).exec()) {\
    throw new WsException(sql.lastError().text());}


QString DBAccess::chapterText(const DBAccess::StoryTreeNode &chapter) const
{
    if(chapter.type() != StoryTreeNode::Type::CHAPTER)
        throw new WsException("指定节点非章节节点");

    auto sql = getStatement();
    sql.prepare("select content from contents_collect where chapter_ref = :cid");
    sql.bindValue(":cid", chapter.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toString();
}

void DBAccess::resetChapterText(const DBAccess::StoryTreeNode &chapter, const QString &text)
{
    if(chapter.type() != StoryTreeNode::Type::CHAPTER)
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


QSqlQuery DBAccess::getStatement() const
{
    return QSqlQuery(dbins);
}

void DBAccess::disconnect_listen_connect(QStandardItemModel *model)
{
    disconnect(model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_keywordsmodel_itemchanged);
}

void DBAccess::connect_listen_connect(QStandardItemModel *model)
{
    connect(model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_keywordsmodel_itemchanged);
}

void DBAccess::listen_keywordsmodel_itemchanged(QStandardItem *item)
{
    auto sql = getStatement();
    KeywordController kwdl(*this);

    switch (item->column()) {
        case 0:{
                auto id = item->data(Qt::UserRole+1).toInt();
                auto table_ref = item->data(Qt::UserRole+2).toString();
                sql.prepare("update "+table_ref+" set name=:nm where id=:id");
                sql.bindValue(":nm", item->text());
                sql.bindValue(":id", id);
                ExSqlQuery(sql);
            }break;
        case 1:{
                auto table_root = item->index().parent();
                auto id = table_root.data(Qt::UserRole+1).toInt();         // id-number
                auto t_name = table_root.data(Qt::UserRole+2).toString();  // table-name
                auto t_type = table_root.data(Qt::UserRole+3).toString();  // table-type

                sql.prepare(QString("update "+t_name+" set field_%1=:v where id=:id").arg(item->row()));
                sql.bindValue(":id", id);
                sql.bindValue(":v", item->data());
                ExSqlQuery(sql);

                auto column_define = kwdl.findTable(t_type).childAt(item->row());
                switch (column_define.vType()) {
                    case KeywordField::ValueType::NUMBER:
                    case KeywordField::ValueType::STRING:
                        item->setText(item->data().toString());
                        break;
                    case KeywordField::ValueType::ENUM:{
                            auto values = kwdl.avaliableEnumsForIndex(item->index());
                            auto item_index = item->data().toInt();
                            if(item_index <0 || item_index>=values.size())
                                throw new WsException("存储值超界");
                            item->setText(values[item_index].second);
                        }break;
                    case KeywordField::ValueType::TABLEREF:{
                            if(item->data().isNull()){
                                item->setText("悬空");
                            }
                            else{
                                auto qex = getStatement();
                                qex.prepare("select name from "+column_define.supplyValue()+" where id=:id");
                                qex.bindValue(":id", item->data());
                                ExSqlQuery(qex);
                                if(!qex.next())
                                    throw new WsException("绑定空值");
                                item->setText(qex.value(0).toString());
                            }
                        }break;
                }
            }
    }
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
        "title text,"
        "desp text,"
        "constraint fkout0 foreign key(despline_ref) references keys_tree(id) on delete cascade,"
        "constraint fkout1 foreign key(chapter_attached) references keys_tree(id) on delete cascade,"
        "constraint fkout2 foreign key(story_attached) references keys_tree(id) on delete cascade)",

        "insert into keys_tree "
        "(type, title, desp, nindex) values(-1, '新建书籍', '无简介', 0)",

        "create table if not exists tables_define ("
        "id integer primary key autoincrement,"
        "type integer not null,"
        "parent integer,"
        "nindex integer not null,"
        "name text,"
        "vtype integer not null,"
        "supply text,"
        "constraint fkp foreign key (parent) references tables_define(id) on delete cascade)"
    };

    for (int index = 0; index < 6; ++index) {
        auto statement = statements[index];
        QSqlQuery q(db);
        if(!q.exec(statement)){
            throw new WsException(QString("执行第%1语句错误：%2").arg(index).arg(q.lastError().text()));
        }
    }
}



DBAccess::StoryTreeNode::StoryTreeNode():valid_state(false),host(nullptr){}

DBAccess::StoryTreeNode::StoryTreeNode(const DBAccess::StoryTreeNode &other)
    :valid_state(other.valid_state),
      id_store(other.id_store),
      node_type(other.node_type),
      host(other.host){}

QString DBAccess::StoryTreeNode::title() const{
    StoryTreeController handle(*host);
    return handle.titleOf(*this);
}

QString DBAccess::StoryTreeNode::description() const{
    StoryTreeController handle(*host);
    return handle.descriptionOf(*this);
}

DBAccess::StoryTreeNode::Type DBAccess::StoryTreeNode::type() const{return node_type;}

int DBAccess::StoryTreeNode::uniqueID() const{return id_store;}

bool DBAccess::StoryTreeNode::isValid() const{return valid_state;}

DBAccess::StoryTreeNode DBAccess::StoryTreeNode::parent() const{
    StoryTreeController handle(*host);
    return handle.parentOf(*this);
}

int DBAccess::StoryTreeNode::index() const
{
    StoryTreeController handle(*host);
    return handle.indexOf(*this);
}

int DBAccess::StoryTreeNode::childCount(DBAccess::StoryTreeNode::Type type) const{
    StoryTreeController handle(*host);
    return handle.childCountOf(*this, type);
}

DBAccess::StoryTreeNode DBAccess::StoryTreeNode::childAt(DBAccess::StoryTreeNode::Type type, int index) const
{
    StoryTreeController handle(*host);
    return handle.childAtOf(*this, type, index);
}

DBAccess::StoryTreeNode &DBAccess::StoryTreeNode::operator=(const DBAccess::StoryTreeNode &other)
{
    valid_state = other.valid_state;
    id_store = other.id_store;
    node_type = other.node_type;
    host = other.host;

    return *this;
}

bool DBAccess::StoryTreeNode::operator==(const DBAccess::StoryTreeNode &other) const
{
    return host==other.host && valid_state==other.valid_state &&
            id_store == other.id_store && node_type==other.node_type;
}

bool DBAccess::StoryTreeNode::operator!=(const DBAccess::StoryTreeNode &other) const{return !(*this == other);}

DBAccess::StoryTreeNode::StoryTreeNode(DBAccess *host, int uid, DBAccess::StoryTreeNode::Type type)
    :valid_state(true),id_store(uid),node_type(type),host(host){}



DBAccess::BranchAttachPoint::BranchAttachPoint(const DBAccess::BranchAttachPoint &other)
    :id_store(other.id_store),host(other.host){}

int DBAccess::BranchAttachPoint::uniqueID() const {return id_store;}

DBAccess::StoryTreeNode DBAccess::BranchAttachPoint::attachedDespline() const
{
    BranchAttachController hdl(*host);
    return hdl.desplineOf(*this);
}

DBAccess::StoryTreeNode DBAccess::BranchAttachPoint::attachedChapter() const
{
    BranchAttachController hdl(*host);
    return hdl.chapterOf(*this);
}

DBAccess::StoryTreeNode DBAccess::BranchAttachPoint::attachedStoryblock() const
{
    BranchAttachController hdl(*host);
    return hdl.storyblockOf(*this);
}

int DBAccess::BranchAttachPoint::index() const
{
    BranchAttachController hdl(*host);
    return hdl.indexOf(*this);
}

QString DBAccess::BranchAttachPoint::title() const
{
    BranchAttachController hdl(*host);
    return hdl.titleOf(*this);
}

QString DBAccess::BranchAttachPoint::description() const
{
    BranchAttachController hdl(*host);
    return hdl.descriptionOf(*this);
}

DBAccess::BranchAttachPoint &DBAccess::BranchAttachPoint::operator=(const DBAccess::BranchAttachPoint &other)
{
    id_store = other.id_store;
    host = other.host;

    return *this;
}

bool DBAccess::BranchAttachPoint::operator==(const DBAccess::BranchAttachPoint &other) const
{
    return id_store==other.id_store && host == other.host;
}

bool DBAccess::BranchAttachPoint::operator!=(const DBAccess::BranchAttachPoint &other) const
{
    return !(*this == other);
}

DBAccess::BranchAttachPoint::BranchAttachPoint(DBAccess *host, int id)
    :id_store(id), host(host){}

DBAccess::KeywordField::KeywordField():field_id_store(INT_MAX), valid_state(false), host(nullptr){}

bool DBAccess::KeywordField::isTableDef() const{return valid_state && !parent().isValid();}

bool DBAccess::KeywordField::isValid() const{return valid_state;}

QString DBAccess::KeywordField::tableTarget() const {
    KeywordController kwdl(*host);
    return kwdl.tableNameOf(*this);}

int DBAccess::KeywordField::registID() const{return field_id_store;}

int DBAccess::KeywordField::index() const{
    KeywordController kwdl(*host);
    return kwdl.indexOf(*this);
}

QString DBAccess::KeywordField::name() const{
    KeywordController kwdl(*host);
    return kwdl.nameOf(*this);
}

DBAccess::KeywordField::ValueType DBAccess::KeywordField::vType() const{
    KeywordController kwdl(*host);
    return kwdl.valueTypeOf(*this);
}

QString DBAccess::KeywordField::supplyValue() const{
    KeywordController kwdl(*host);
    return kwdl.supplyValueOf(*this);
}

DBAccess::KeywordField DBAccess::KeywordField::parent() const{
    KeywordController kwdl(*host);
    return kwdl.tableOf(*this);
}

int DBAccess::KeywordField::childCount() const {
    KeywordController kwdl(*host);
    return kwdl.fieldsCountOf(*this);}

DBAccess::KeywordField DBAccess::KeywordField::childAt(int index) const{
    KeywordController kwdl(*host);
    return kwdl.fieldAt(*this, index);}

DBAccess::KeywordField &DBAccess::KeywordField::operator=(const DBAccess::KeywordField &other)
{
    field_id_store = other.field_id_store;
    valid_state = other.valid_state;
    host = other.host;
    return *this;
}

DBAccess::KeywordField::KeywordField(DBAccess *host, int fieldID):field_id_store(fieldID),valid_state(true), host(host){}

DBAccess::KeywordField DBAccess::KeywordField::nextSibling() const{
    KeywordController kwdl(*host);
    return kwdl.nextSiblingOf(*this);
}

DBAccess::KeywordField DBAccess::KeywordField::previousSibling() const{
    KeywordController kwdl(*host);
    return kwdl.previousSiblingOf(*this);
}

DBAccess::StoryTreeController::StoryTreeController(DBAccess &host):host(host){}

DBAccess::StoryTreeNode DBAccess::StoryTreeController::novelNode() const
{
    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where type=-1");
    ExSqlQuery(sql);

    if(sql.next())
        return StoryTreeNode(&host, sql.value(0).toInt(), StoryTreeNode::Type::NOVEL);

    return StoryTreeNode();
}

QString DBAccess::StoryTreeController::titleOf(const DBAccess::StoryTreeNode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select title from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

QString DBAccess::StoryTreeController::descriptionOf(const DBAccess::StoryTreeNode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select desp from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::StoryTreeController::resetTitleOf(const DBAccess::StoryTreeNode &node, const QString &title)
{
    auto sql = host.getStatement();
    sql.prepare("update keys_tree set title=:title where id=:id");
    sql.bindValue(":title", title);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

void DBAccess::StoryTreeController::resetDescriptionOf(const DBAccess::StoryTreeNode &node, const QString &description)
{
    auto sql = host.getStatement();
    sql.prepare("update keys_tree set desp=:title where id=:id");
    sql.bindValue(":title", description);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

int DBAccess::StoryTreeController::indexOf(const DBAccess::StoryTreeNode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select nindex from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toInt();
}

DBAccess::StoryTreeNode DBAccess::StoryTreeController::parentOf(const DBAccess::StoryTreeNode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select parent from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    switch (node.type()) {
        case StoryTreeNode::Type::NOVEL:
            return StoryTreeNode();
        case StoryTreeNode::Type::VOLUME:
            return StoryTreeNode(&host, sql.value(0).toInt(), StoryTreeNode::Type::NOVEL);
        case StoryTreeNode::Type::CHAPTER:
        case StoryTreeNode::Type::DESPLINE:
        case StoryTreeNode::Type::STORYBLOCK:
            return StoryTreeNode(&host, sql.value(0).toInt(), StoryTreeNode::Type::VOLUME);
        case StoryTreeNode::Type::KEYPOINT:
            return StoryTreeNode(&host, sql.value(0).toInt(), StoryTreeNode::Type::STORYBLOCK);
        default:
            throw new WsException("意外的节点类型！");
    }
}

int DBAccess::StoryTreeController::childCountOf(const DBAccess::StoryTreeNode &pnode, StoryTreeNode::Type type) const
{
    auto sql = host.getStatement();
    sql.prepare("select count(*), type from keys_tree where parent=:pnode group by type");
    sql.bindValue(":pnode", pnode.uniqueID());
    ExSqlQuery(sql);

    while (sql.next()) {
        if(sql.value(1).toInt() == static_cast<int>(type))
            return sql.value(0).toInt();
    }

    return 0;
}

DBAccess::StoryTreeNode DBAccess::StoryTreeController::childAtOf(const DBAccess::StoryTreeNode &pnode, StoryTreeNode::Type type, int index) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where parent=:pid and nindex=:ind and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":ind", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("指定节点指定索引无子节点");
    return StoryTreeNode(&host, sql.value(0).toInt(), type);
}

void DBAccess::StoryTreeController::removeNode(const DBAccess::StoryTreeNode &node)
{
    auto pnode = parentOf(node);
    auto index = indexOf(node);
    auto type = node.type();

    auto sql = host.getStatement();
    sql.prepare("update keys_tree set nindex=nindex-1 where parent=:pid and nindex>=:index and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":index", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    sql.prepare("delete from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

DBAccess::StoryTreeNode DBAccess::StoryTreeController::insertChildNodeBefore(const DBAccess::StoryTreeNode &pnode, DBAccess::StoryTreeNode::Type type,
                                                                              int index, const QString &title, const QString &description)
{
    switch (pnode.type()) {
        case StoryTreeNode::Type::NOVEL:
            if(type != StoryTreeNode::Type::VOLUME)
                throw new WsException("插入错误节点类型");
            break;
        case StoryTreeNode::Type::VOLUME:
            if(type != StoryTreeNode::Type::CHAPTER &&
               type != StoryTreeNode::Type::STORYBLOCK &&
               type != StoryTreeNode::Type::DESPLINE)
                throw new WsException("插入错误节点类型");
            break;
        case StoryTreeNode::Type::STORYBLOCK:
            if(type != StoryTreeNode::Type::KEYPOINT)
                throw new WsException("插入错误节点类型");
            break;
        default:
            throw new WsException("插入错误节点类型");
    }


    auto sql = host.getStatement();
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
    return StoryTreeNode(&host, sql.value(0).toInt(), type);
}

DBAccess::StoryTreeNode DBAccess::StoryTreeController::getNodeViaID(int id) const
{
    auto sql = host.getStatement();
    sql.prepare("select type from keys_tree where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("传入了无效id");

    return StoryTreeNode(&host, id, static_cast<StoryTreeNode::Type>(sql.value(0).toInt()));
}

DBAccess::BranchAttachController::BranchAttachController(DBAccess &host):host(host){}

DBAccess::BranchAttachPoint DBAccess::BranchAttachController::getPointViaID(int id) const
{
    auto q = host.getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", id);
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("传入无效id");
    return BranchAttachPoint(&host, id);
}

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachController::getPointsViaDespline(const DBAccess::StoryTreeNode &despline) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from points_collect where despline_ref=:ref order by nindex");
    sql.bindValue(":ref", despline.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(&host, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachController::getPointsViaChapter(const DBAccess::StoryTreeNode &chapter) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from points_collect where chapter_attached=:ref");
    sql.bindValue(":ref", chapter.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(&host, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachController::getPointsViaStoryblock(const DBAccess::StoryTreeNode &storyblock) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from points_collect where story_attached=:ref");
    sql.bindValue(":ref", storyblock.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(&host, sql.value(0).toInt());
    }

    return ret;
}

DBAccess::BranchAttachPoint DBAccess::BranchAttachController::insertPointBefore(const DBAccess::StoryTreeNode &despline, int index,
                                                                                           const QString &title, const QString &description)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set nindex=nindex+1 where despline_ref=:ref and nindex >=:idx");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    ExSqlQuery(q);

    q.prepare("insert into points_collect (despline_ref, nindex, title, desp) values(:ref, :idx, :t, :desp)");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    q.bindValue(":t", title);
    q.bindValue(":desp", description);
    ExSqlQuery(q);

    q.prepare("select id from points_collect where despline_ref=:ref and nindex=:idx");
    q.bindValue(":ref", despline.uniqueID());
    q.bindValue(":idx", index);
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("驻点插入失败");
    return BranchAttachPoint(&host, q.value(0).toInt());
}

void DBAccess::BranchAttachController::removePoint(DBAccess::BranchAttachPoint point)
{
    auto attached = point.attachedDespline();
    auto q = host.getStatement();
    q.prepare("update points_collect set nindex=nindex-1 where despline_ref=:ref and nindex>=:idx");
    q.bindValue(":ref", attached.uniqueID());
    q.bindValue(":idx", point.index());
    ExSqlQuery(q);

    q.prepare("delete from points_collect where id=:id");
    q.bindValue(":id", point.uniqueID());
    ExSqlQuery(q);
}

int DBAccess::BranchAttachController::indexOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

QString DBAccess::BranchAttachController::titleOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select title from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

QString DBAccess::BranchAttachController::descriptionOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select desp from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

DBAccess::StoryTreeNode DBAccess::BranchAttachController::desplineOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select despline_ref from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return StoryTreeNode(&host, q.value(0).toInt(), StoryTreeNode::Type::DESPLINE);
}

DBAccess::StoryTreeNode DBAccess::BranchAttachController::chapterOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select chapter_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return StoryTreeNode();

    return StoryTreeNode(&host, q.value(0).toInt(), StoryTreeNode::Type::CHAPTER);
}

DBAccess::StoryTreeNode DBAccess::BranchAttachController::storyblockOf(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select story_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return StoryTreeNode();

    return StoryTreeNode(&host, q.value(0).toInt(), StoryTreeNode::Type::STORYBLOCK);
}

void DBAccess::BranchAttachController::resetTitleOf(const DBAccess::BranchAttachPoint &node, const QString &title)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set title=:t where id = :id");
    q.bindValue(":t", title);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachController::resetDescriptionOf(const DBAccess::BranchAttachPoint &node, const QString &description)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set desp=:t where id = :id");
    q.bindValue(":t", description);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachController::resetChapterOf(const DBAccess::BranchAttachPoint &node, const DBAccess::StoryTreeNode &chapter)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set chapter_attached = :cid where id=:id");
    q.bindValue(":cid", chapter.isValid()?chapter.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachController::resetStoryblockOf(const DBAccess::BranchAttachPoint &node, const DBAccess::StoryTreeNode &storyblock)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set story_attached = :cid where id=:id");
    q.bindValue(":cid", storyblock.isValid()?storyblock.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::KeywordController::KeywordController(DBAccess &host):host(host){}

DBAccess::KeywordField DBAccess::KeywordController::newTable(const QString &typeName)
{
    // 查重
    auto target = findTable(typeName);
    if(target.isValid())
        throw new WsException("重复定义指定类型表格");

    auto tables = host.dbins.tables();
    auto new_table_name = QString("keywords_%1").arg(host.intGen.generate64());
    while (tables.contains(new_table_name)) {
        new_table_name = QString("keywords_%1").arg(host.intGen.generate64());
    }

    auto sql = host.getStatement();
    sql.prepare("select count(*) from tables_define where type=-1 group by type");
    ExSqlQuery(sql);

    int table_count = 0;
    if(sql.next()) table_count = sql.value(0).toInt();
    // 添新
    sql.prepare("insert into tables_define (type, nindex, name, vtype, supply)"
                "values(-1, :idx, :name, 1, :spy);");
    sql.bindValue(":idx", table_count);
    sql.bindValue(":name", typeName);
    sql.bindValue(":spy", new_table_name);
    ExSqlQuery(sql);

    // 数据返回
    auto tdef = findTable(typeName);

    sql.prepare("create table " + new_table_name + "(id integer primary key autoincrement, name text)");
    ExSqlQuery(sql);

    return tdef;
}

void DBAccess::KeywordController::removeTable(const KeywordField &tbColumn)
{
    if(!tbColumn.isValid())
        return;

    auto tableDefineRow = tbColumn;
    if(!tableDefineRow.isTableDef())           // 由字段定义转为表格定义
        tableDefineRow = tableDefineRow.parent();

    int index = tableDefineRow.index();
    QString detail_table_ref = tableDefineRow.supplyValue();

    auto sql = host.getStatement();
    sql.prepare("drop table if exists "+ detail_table_ref);
    ExSqlQuery(sql);

    sql.prepare("update tables_define set nindex = nindex-1 where nindex>=:idx and type=-1");
    sql.bindValue(":idx", index);
    ExSqlQuery(sql);

    sql.prepare("delete from tables_define where id=:idx");
    sql.bindValue(":idx", tableDefineRow.registID());
    ExSqlQuery(sql);
}

DBAccess::KeywordField DBAccess::KeywordController::firstTable() const
{
    auto sql = host.getStatement();
    sql.prepare("select id from tables_define where type = -1 and nindex = 0");
    ExSqlQuery(sql);
    if(sql.next())
        return KeywordField(&host, sql.value(0).toInt());
    return KeywordField();
}

DBAccess::KeywordField DBAccess::KeywordController::findTable(const QString &typeName) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from tables_define where type = -1 and name=:nm");
    sql.bindValue(":nm", typeName);
    ExSqlQuery(sql);
    if(sql.next())
        return KeywordField(&host, sql.value(0).toInt());
    return KeywordField();
}

void DBAccess::KeywordController::tablefieldsAdjust(const KeywordField &target_table,
                                                const QList<QPair<DBAccess::KeywordField,
                                                std::tuple<QString, QString, DBAccess::KeywordField::ValueType>>> &_define)
{
    if(!target_table.isTableDef())
        throw new WsException("传入字段定义非表定义");

    auto sql = host.getStatement();
    // 关闭外键校验
    sql.prepare("PRAGMA foreign_keys = OFF");
    ExSqlQuery(sql);

    // 数据转移
    auto temp_values_table_name = target_table.tableTarget()+"____ws_transfer_table_delate_soon";
    sql.prepare("create table "+temp_values_table_name+" as select * from "+target_table.tableTarget());
    ExSqlQuery(sql);


    // 组装最后的数据中转语句
    QString select_data = "select name,";
    QString insert_data = "insert into %1 (name,";
    for (auto index=0; index<_define.size(); ++index) {
        auto base_one = _define.at(index).first;
        if(!base_one.isValid())
            continue;

        select_data += QString("field_%1,").arg(base_one.index());
        insert_data += QString("field_%1,").arg(index);
    }
    insert_data = insert_data.mid(0, insert_data.length()-1) + ")";
    insert_data += select_data.mid(0, select_data.length()-1) + " from " + temp_values_table_name;

    // 删除已建立关键词表格
    sql.prepare("drop table if exists "+ target_table.tableTarget());
    ExSqlQuery(sql);

    // 清空字段记录
    sql.prepare("delete from tables_define where type=0 and parent=:pnode");
    sql.bindValue(":pnode", target_table.registID());
    ExSqlQuery(sql);


    //=========================
    // target_table_name : table_name
    // table_define : table_instance


    // 插入自定义字段
    sql.prepare("insert into tables_define (type, parent, nindex, name, vtype, supply) values(?, ?, ?, ?, ?, ?)");
    QVariantList typelist, parentlist, indexlist, namelist, valuetypelist, supplyvaluelist;
    for (auto index=0; index<_define.size(); ++index) {
        auto custom_one = _define.at(index).second;
        typelist << 0;
        parentlist << target_table.registID();
        indexlist << index;
        namelist << std::get<0>(custom_one);
        valuetypelist << static_cast<int>(std::get<2>(custom_one));
        supplyvaluelist << std::get<1>(custom_one);
    }
    sql.addBindValue(typelist);
    sql.addBindValue(parentlist);
    sql.addBindValue(indexlist);
    sql.addBindValue(namelist);
    sql.addBindValue(valuetypelist);
    sql.addBindValue(supplyvaluelist);
    if(!sql.execBatch())
        throw new WsException(sql.lastError().text());


    // 重建关键词表格
    QString create_table = "create table "+ target_table.tableTarget() +" (" +
                           "id integer primary key autoincrement, name text,",
            constraint = "";

    for (auto index=0; index<_define.size(); ++index) {
        auto custom_one = _define.at(index).second;
        switch (std::get<2>(custom_one)) {
            case KeywordField::ValueType::NUMBER:{
                    create_table += QString("field_%1 real,").arg(index);
                }break;
            case KeywordField::ValueType::STRING:{
                    create_table += QString("field_%1 text,").arg(index);
                }break;
            case KeywordField::ValueType::ENUM:{
                    create_table += QString("field_%1 integer,").arg(index);
                }break;
            case KeywordField::ValueType::TABLEREF:{
                    create_table += QString("field_%1 integer,").arg(index);
                    constraint += QString("constraint fk_%1 foreign key(field_%1) references %2(id) on delete set null,")
                                  .arg(index).arg(std::get<1>(custom_one));
                }break;
        }
    }

    create_table += constraint;
    create_table = create_table.mid(0, create_table.length()-1);
    create_table += ")";

    sql.prepare(create_table);
    ExSqlQuery(sql);


    // 数据转移
    insert_data = insert_data.arg(target_table.tableTarget());
    sql.prepare(insert_data);
    ExSqlQuery(sql);

    sql.prepare("drop table "+temp_values_table_name);
    ExSqlQuery(sql);

    sql.exec("PRAGMA foreign_keys = ON");
}

QString DBAccess::KeywordController::tableNameOf(const DBAccess::KeywordField &colDef) const
{
    auto tableDef = colDef;
    if(!colDef.isTableDef())
        tableDef = tableDef.parent();

    return tableDef.supplyValue();
}

int DBAccess::KeywordController::indexOf(const DBAccess::KeywordField &colDef) const
{
    if(!colDef.isValid())
        throw new WsException("传入的节点无效");

    auto sql = host.getStatement();
    sql.prepare("select nindex from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toInt();

    throw new WsException("传入的节点无效");
}

DBAccess::KeywordField::ValueType DBAccess::KeywordController::valueTypeOf(const DBAccess::KeywordField &colDef) const
{
    auto sql = host.getStatement();
    sql.prepare("select vtype from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return static_cast<DBAccess::KeywordField::ValueType>(sql.value(0).toInt());
    throw new WsException("传入的节点无效");
}

QString DBAccess::KeywordController::nameOf(const DBAccess::KeywordField &colDef) const
{
    auto sql = host.getStatement();
    sql.prepare("select name from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

void DBAccess::KeywordController::resetNameOf(const DBAccess::KeywordField &col, const QString &name)
{
    if(col.isTableDef()){
        if(findTable(name).isValid())
            throw new WsException("该名称重复+无效");
    }

    auto sql = host.getStatement();
    sql.prepare("update tables_define set name=:nm where id=:id");
    sql.bindValue(":nm", name);
    sql.bindValue(":id", col.registID());
    ExSqlQuery(sql);
}

QString DBAccess::KeywordController::supplyValueOf(const DBAccess::KeywordField &field) const
{
    auto sql = host.getStatement();
    sql.prepare("select supply from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

void DBAccess::KeywordController::resetSupplyValueOf(const DBAccess::KeywordField &field, const QString &supply)
{
    auto sql = host.getStatement();
    sql.prepare("update tables_define set supply=:syp where id=:id");
    sql.bindValue(":id", field.registID());
    sql.bindValue(":spy", supply);
    ExSqlQuery(sql);
}

DBAccess::KeywordField DBAccess::KeywordController::tableOf(const DBAccess::KeywordField &field) const
{
    auto sql = host.getStatement();
    sql.prepare("select type, parent from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next()){
        if(sql.value(0).toInt() == 0)
            return KeywordField(&host, sql.value(1).toInt());
        return KeywordField();
    }
    throw new WsException("指定传入节点无效");
}

int DBAccess::KeywordController::fieldsCountOf(const DBAccess::KeywordField &table) const
{
    if(!table.isTableDef())
        throw new WsException("传入节点不是表定义节点");

    auto sql = host.getStatement();
    sql.prepare("select count(*) from tables_define where parent=:pid and type=0 group by parent");
    sql.bindValue(":pid", table.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toInt();
    return 0;
}

DBAccess::KeywordField DBAccess::KeywordController::fieldAt(const DBAccess::KeywordField &table, int index) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from tables_define where type=0 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", table.registID());
    sql.bindValue(":idx", index);
    ExSqlQuery(sql);

    if(sql.next())
        return KeywordField(&host, sql.value(0).toInt());
    return KeywordField();
}

DBAccess::KeywordField DBAccess::KeywordController::nextSiblingOf(const DBAccess::KeywordField &field) const
{
    auto sql = host.getStatement();
    if(field.parent().isValid()){
        sql.prepare("select id from tables_define where type=:type and parent=:pid and nindex=:idx");
        sql.bindValue(":type", 0);
        sql.bindValue(":pid", field.parent().registID());
    }
    else {
        sql.prepare("select id from tables_define where type=:type and nindex=:idx");
        sql.bindValue(":type", -1);
    }
    sql.bindValue(":idx", field.index()+1);

    ExSqlQuery(sql);
    if(sql.next())
        return KeywordField(&host, sql.value(0).toInt());

    return KeywordField();
}

DBAccess::KeywordField DBAccess::KeywordController::previousSiblingOf(const DBAccess::KeywordField &field) const
{
    auto sql = host.getStatement();
    if(field.parent().isValid()){
        sql.prepare("select id from tables_define where type=:type and parent=:pid and nindex=:idx");
        sql.bindValue(":type", 0);
        sql.bindValue(":pid", field.parent().registID());
    }
    else {
        sql.prepare("select id from tables_define where type=:type and nindex=:idx");
        sql.bindValue(":type", -1);
    }
    sql.bindValue(":idx", field.index()-1);
    ExSqlQuery(sql);
    if(sql.next())
        return KeywordField(&host, sql.value(0).toInt());
    return KeywordField();
}



void DBAccess::KeywordController::queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KeywordField &table) const
{
    host.disconnect_listen_connect(disp_model);
    disp_model->clear();
    if(name.isEmpty()) return;

    auto sql = host.getStatement();
    auto table_define = table;
    if(!table_define.isTableDef())
        table_define = table_define.parent();

    disp_model->setHorizontalHeaderLabels(QStringList()<<"名称"<<"数据");



    QList<DBAccess::KeywordField> cols;
    QString exstr = "select id, name,";
    auto cols_count = table_define.childCount();
    for (auto index=0; index<cols_count; ++index){
        exstr += QString("field_%1,").arg(index);
        auto cell = table_define.childAt(index);
        cols << cell;
    }

    exstr = exstr.mid(0, exstr.length()-1) + " from " + table_define.tableTarget();
    if(name != "*")
        exstr += " where name like '%"+name+"%'";

    sql.prepare(exstr);
    ExSqlQuery(sql);

    while (sql.next()) {
        QList<QStandardItem*> table_itemroot;

        table_itemroot << new QStandardItem(sql.value(1).toString());
        table_itemroot.last()->setData(sql.value(0), Qt::UserRole+1);
        table_itemroot.last()->setData(table_define.tableTarget(), Qt::UserRole+2);
        table_itemroot.last()->setData(table_define.name(), Qt::UserRole+3);

        table_itemroot << new QStandardItem("-----------------");
        table_itemroot.last()->setEditable(false);


        int size = cols.size() + 2;
        for (int index=2; index < size; ++index) {
            auto colDef = cols.at(index-2);

            QList<QStandardItem*> field_row;

            field_row << new QStandardItem(colDef.name());
            field_row.last()->setEditable(false);

            switch (colDef.vType()) {
                case KeywordField::ValueType::NUMBER:
                case KeywordField::ValueType::STRING:
                    field_row << new QStandardItem(sql.value(index).toString());
                    field_row.last()->setData(sql.value(index), Qt::UserRole+1);
                    break;
                case KeywordField::ValueType::ENUM:{
                        auto values = colDef.supplyValue().split(";");
                        auto item_index = sql.value(index).toInt();
                        if(item_index <0 || item_index>=values.size())
                            throw new WsException("存储值超界");

                        field_row << new QStandardItem(values[item_index]);
                        field_row.last()->setData(sql.value(index));
                    }break;
                case KeywordField::ValueType::TABLEREF:{
                        field_row << new QStandardItem("悬空");
                        field_row.last()->setData(sql.value(index));

                        if(!sql.value(index).isNull())
                        {
                            auto qex = host.getStatement();
                            qex.prepare("select name from "+colDef.supplyValue()+" where id=:id");
                            qex.bindValue(":id", sql.value(index));
                            ExSqlQuery(qex);
                            if(!qex.next())
                                throw new WsException("绑定空值");
                            field_row.last()->setText(qex.value(0).toString());
                        }
                    }break;
            }
            field_row.last()->setData(static_cast<int>(colDef.vType()), Qt::UserRole+2);
            table_itemroot[0]->appendRow(field_row);
        }

        disp_model->appendRow(table_itemroot);
    }
    host.connect_listen_connect(disp_model);
}



void DBAccess::KeywordController::appendEmptyItemAt(const DBAccess::KeywordField &table, const QString &name)
{
    auto sql = host.getStatement();
    sql.prepare("insert into "+table.tableTarget()+" (name) values (:nm)");
    sql.bindValue(":nm", name);
    ExSqlQuery(sql);
}

void DBAccess::KeywordController::removeTargetItemAt(const DBAccess::KeywordField &table, QStandardItemModel *disp_model, int index)
{
    auto id = disp_model->item(index)->data().toInt();

    auto sql = host.getStatement();
    sql.prepare("delete from "+table.tableTarget()+" where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);
}

QList<QPair<int, QString>> DBAccess::KeywordController::avaliableEnumsForIndex(const QModelIndex &index) const
{
    auto type = static_cast<KeywordField::ValueType>(index.data(Qt::UserRole+2).toInt());
    if(type != KeywordField::ValueType::ENUM)
        throw new WsException("目标数据类型不为ENUM");

    if(!index.column())
        return QList<QPair<int, QString>>();

    auto kw_type = index.parent().data(Qt::UserRole+3).toString();
    auto column_def = findTable(kw_type).childAt(index.row());

    auto list = column_def.supplyValue().split(";");
    QList<QPair<int, QString>> ret_list;
    for (auto item : list) {
        if(item.isEmpty())
            continue;

        ret_list << qMakePair(ret_list.size(), item);
    }
    return ret_list;
}

QList<QPair<int, QString> > DBAccess::KeywordController::avaliableItemsForIndex(const QModelIndex &index) const
{
    auto type = static_cast<KeywordField::ValueType>(index.data(Qt::UserRole+2).toInt());
    if(type != KeywordField::ValueType::TABLEREF)
        throw new WsException("目标数据类型不为TABLEREF");

    if(!index.column())
        return QList<QPair<int, QString>>();

    auto kw_type = index.parent().data(Qt::UserRole+3).toString();
    auto column_def = findTable(kw_type).childAt(index.row());

    auto reftable_reference = column_def.supplyValue();
    auto sql = host.getStatement();
    sql.prepare("select id, name from " + reftable_reference + " order by id");
    ExSqlQuery(sql);

    QList<QPair<int, QString>> retlist;
    while (sql.next()) {
        retlist << qMakePair(sql.value(0).toInt(), sql.value(1).toString());
    }

    return retlist;
}


