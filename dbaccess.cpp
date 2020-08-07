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
    if(!(sql).exec()) {\
    throw new WsException(sql.lastError().text());}

DBAccess::StoryNode DBAccess::novelStoryNode() const
{
    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=-1");
    ExSqlQuery(sql);

    if(sql.next())
        return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::NOVEL);

    return StoryNode();
}

QString DBAccess::titleOfStoryNode(const DBAccess::StoryNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select title from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::resetTitleOfStoryNode(const DBAccess::StoryNode &node, const QString &title)
{
    auto sql = getStatement();
    sql.prepare("update keys_tree set title=:title where id=:id");
    sql.bindValue(":title", title);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

QString DBAccess::descriptionOfStoryNode(const DBAccess::StoryNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select desp from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::resetDescriptionOfStoryNode(const DBAccess::StoryNode &node, const QString &description)
{
    auto sql = getStatement();
    sql.prepare("update keys_tree set desp=:title where id=:id");
    sql.bindValue(":title", description);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

DBAccess::StoryNode DBAccess::parentOfStoryNode(const DBAccess::StoryNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select parent from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    switch (node.type()) {
        case StoryNode::Type::NOVEL:
            return StoryNode();
        case StoryNode::Type::VOLUME:
            return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::NOVEL);
        case StoryNode::Type::CHAPTER:
        case StoryNode::Type::DESPLINE:
        case StoryNode::Type::STORYBLOCK:
            return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::VOLUME);
        case StoryNode::Type::KEYPOINT:
            return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::STORYBLOCK);
        default:
            throw new WsException("意外的节点类型！");
    }
}

int DBAccess::indexOfStoryNode(const DBAccess::StoryNode &node) const
{
    auto sql = getStatement();
    sql.prepare("select nindex from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toInt();
}

int DBAccess::childCountOfStoryNode(const DBAccess::StoryNode &pnode, StoryNode::Type type) const
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

DBAccess::StoryNode DBAccess::childAtOfStoryNode(const DBAccess::StoryNode &pnode, StoryNode::Type type, int index) const
{
    auto sql = getStatement();
    sql.prepare("select id from keys_tree where parent=:pid and nindex=:ind and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":ind", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("指定节点指定索引无子节点");
    return StoryNode(this, sql.value(0).toInt(), type);
}

DBAccess::StoryNode DBAccess::insertChildStoryNodeBefore(const DBAccess::StoryNode &pnode, DBAccess::StoryNode::Type type,
                                   int index, const QString &title, const QString &description)
{
    switch (pnode.type()) {
        case StoryNode::Type::NOVEL:
            if(type != StoryNode::Type::VOLUME)
                throw new WsException("插入错误节点类型");
            break;
        case StoryNode::Type::VOLUME:
            if(type != StoryNode::Type::CHAPTER &&
               type != StoryNode::Type::STORYBLOCK &&
               type != StoryNode::Type::DESPLINE)
                throw new WsException("插入错误节点类型");
            break;
        case StoryNode::Type::STORYBLOCK:
            if(type != StoryNode::Type::KEYPOINT)
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
    return StoryNode(this, sql.value(0).toInt(), type);
}

void DBAccess::removeStoryNode(const DBAccess::StoryNode &node)
{
    auto pnode = parentOfStoryNode(node);
    auto index = indexOfStoryNode(node);
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

DBAccess::StoryNode DBAccess::getStoryNodeViaID(int id) const
{
    auto sql = getStatement();
    sql.prepare("select type from keys_tree where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("传入了无效id");

    return StoryNode(this, id, static_cast<StoryNode::Type>(sql.value(0).toInt()));
}

DBAccess::StoryNode DBAccess::firstChapterStoryNode() const
{
    auto sql =  getStatement();
    sql.prepare("select id from keys_tree where type=0 order by nindex");
    ExSqlQuery(sql);
    if(!sql.next())
        return StoryNode();

    auto fcid = sql.value(0).toInt();
    sql.prepare("select id from keys_tree where type=1 and parent=:pnode order by nindex");
    sql.bindValue(":pnode", fcid);
    ExSqlQuery(sql);
    if(!sql.next())
        return StoryNode();

    return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::CHAPTER);
}

DBAccess::StoryNode DBAccess::lastChapterStoryNode() const
{
    auto sql =  getStatement();
    sql.prepare("select id from keys_tree where type=0 order by nindex desc");
    ExSqlQuery(sql);
    if(!sql.next())
        return StoryNode();

    auto fcid = sql.value(0).toInt();
    sql.prepare("select id from keys_tree where type=1 and parent=:pnode order by nindex desc");
    sql.bindValue(":pnode", fcid);
    ExSqlQuery(sql);
    if(!sql.next())
        return StoryNode();

    return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::CHAPTER);
}

DBAccess::StoryNode DBAccess::nextChapterStoryNode(const DBAccess::StoryNode &chapterIns) const
{
    auto pnode = parentOfStoryNode(chapterIns);
    auto index = indexOfStoryNode(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index+1);
    ExSqlQuery(sql);

    if(!sql.next())
        return StoryNode();
    return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::CHAPTER);
}

DBAccess::StoryNode DBAccess::previousChapterStoryNode(const DBAccess::StoryNode &chapterIns) const
{
    auto pnode = parentOfStoryNode(chapterIns);
    auto index = indexOfStoryNode(chapterIns);

    auto sql = getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index-1);
    ExSqlQuery(sql);

    if(!sql.next())
        return StoryNode();
    return StoryNode(this, sql.value(0).toInt(), StoryNode::Type::CHAPTER);
}

QString DBAccess::chapterText(const DBAccess::StoryNode &chapter) const
{
    if(chapter.type() != StoryNode::Type::CHAPTER)
        throw new WsException("指定节点非章节节点");

    auto sql = getStatement();
    sql.prepare("select content from contents_collect where chapter_ref = :cid");
    sql.bindValue(":cid", chapter.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toString();
}

void DBAccess::resetChapterText(const DBAccess::StoryNode &chapter, const QString &text)
{
    if(chapter.type() != StoryNode::Type::CHAPTER)
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

DBAccess::BranchAttachPoint DBAccess::getAttachPointViaID(int id) const
{
    auto q = getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", id);
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("传入无效id");
    return BranchAttachPoint(this, id);
}

int DBAccess::indexOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

QString DBAccess::titleOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select title from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

void DBAccess::resetTitleOfAttachPoint(const DBAccess::BranchAttachPoint &node, const QString &title)
{
    auto q = getStatement();
    q.prepare("update points_collect set title=:t where id = :id");
    q.bindValue(":t", title);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

QString DBAccess::descriptionOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select desp from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

void DBAccess::resetDescriptionOfAttachPoint(const DBAccess::BranchAttachPoint &node, const QString &description)
{
    auto q = getStatement();
    q.prepare("update points_collect set desp=:t where id = :id");
    q.bindValue(":t", description);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::StoryNode DBAccess::desplineOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select despline_ref from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return StoryNode(this, q.value(0).toInt(), StoryNode::Type::DESPLINE);
}

DBAccess::StoryNode DBAccess::chapterOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select chapter_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return StoryNode();

    return StoryNode(this, q.value(0).toInt(), StoryNode::Type::CHAPTER);
}

void DBAccess::resetChapterOfAttachPoint(const DBAccess::BranchAttachPoint &node, const DBAccess::StoryNode &chapter)
{
    auto q = getStatement();
    q.prepare("update points_collect set chapter_attached = :cid where id=:id");
    q.bindValue(":cid", chapter.isValid()?chapter.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::StoryNode DBAccess::storyblockOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = getStatement();
    q.prepare("select story_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return StoryNode();

    return StoryNode(this, q.value(0).toInt(), StoryNode::Type::STORYBLOCK);
}

void DBAccess::resetStoryblockOfAttachPoint(const DBAccess::BranchAttachPoint &node, const DBAccess::StoryNode &storyblock)
{
    auto q = getStatement();
    q.prepare("update points_collect set story_attached = :cid where id=:id");
    q.bindValue(":cid", storyblock.isValid()?storyblock.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

DBAccess::KeywordsField DBAccess::newTable(const QString &typeName)
{
    // 查重
    auto target = findTable(typeName);
    if(target.isValid())
        throw new WsException("重复定义指定类型表格");

    auto tables = dbins.tables();
    auto new_table_name = QString("%1_%2").arg(typeName).arg(intGen.generate64());
    while (tables.contains(new_table_name)) {
        new_table_name = QString("%1_%2").arg(typeName).arg(intGen.generate64());
    }

    auto sql = getStatement();
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

void DBAccess::removeTable(const KeywordsField &tbColumn)
{
    if(!tbColumn.isValid())
        return;

    auto tableDefineRow = tbColumn;
    if(!tableDefineRow.isTableDef())           // 由字段定义转为表格定义
        tableDefineRow = tableDefineRow.parent();

    int index = tableDefineRow.index();
    QString detail_table_ref = tableDefineRow.supplyValue();

    auto sql = getStatement();
    sql.prepare("drop table if exists "+ detail_table_ref);
    ExSqlQuery(sql);

    sql.prepare("update tables_define set nindex = nindex-1 where nindex>=:idx and type=-1");
    sql.bindValue(":idx", index);
    ExSqlQuery(sql);

    sql.prepare("delete from tables_define where id=:idx");
    sql.bindValue(":idx", tableDefineRow.registID());
    ExSqlQuery(sql);
}

DBAccess::KeywordsField DBAccess::firstTable() const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type = -1 and nindex = 0");
    ExSqlQuery(sql);
    if(sql.next())
        return KeywordsField(this, sql.value(0).toInt());
    return KeywordsField();
}

DBAccess::KeywordsField DBAccess::findTable(const QString &typeName) const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type = -1 and name=:nm");
    sql.bindValue(":nm", typeName);
    ExSqlQuery(sql);
    if(sql.next())
        return KeywordsField(this, sql.value(0).toInt());

    return KeywordsField();
}

void DBAccess::fieldsAdjust(const KeywordsField &target,
                            QList<QPair<DBAccess::KeywordsField,
                            std::tuple<QString, QString, DBAccess::KeywordsField::ValueType>>> &_define)
{
    auto sql = getStatement();
    // 关闭外键校验
    sql.prepare("PRAGMA foreign_keys = OFF");
    ExSqlQuery(sql);

    // 数据转移
    auto target_table_name = target.isTableDef()?target.supplyValue():target.parent().supplyValue();
    sql.prepare("create table "+target_table_name+"____ws_transfer_table_delate_soon as select * from "+target_table_name);
    ExSqlQuery(sql);
    qDebug() << sql.lastQuery();

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
    insert_data += select_data.mid(0, select_data.length()-1) + " from " + target_table_name + "____ws_transfer_table_delate_soon";


    // 重建表格和字段记录
    auto table_type_store = target.isTableDef()?target.name():target.parent().name();
    removeTable(target);
    auto table_define = newTable(table_type_store);

    //=========================
    // target_table_name : table_name
    // table_define : table_instance


    // 插入自定义字段
    sql.prepare("insert into tables_define (type, parent, nindex, name, vtype, supply) values(?, ?, ?, ?, ?, ?)");
    QVariantList typelist, parentlist, indexlist, namelist, valuetypelist, supplyvaluelist;
    for (auto index=0; index<_define.size(); ++index) {
        auto custom_one = _define.at(index).second;
        typelist << 0;
        parentlist << table_define.registID();
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

    // 删除已建立默认关键词表格
    sql.prepare("drop table if exists "+ table_define.supplyValue());
    ExSqlQuery(sql);

    // 重建关键词表格
    QString create_table = "create table "+ table_define.supplyValue() +" (" +
                           "id integer primary key autoincrement, name text,",
            constraint = "";

    for (auto index=0; index<_define.size(); ++index) {
        auto custom_one = _define.at(index).second;
        switch (std::get<2>(custom_one)) {
            case KeywordsField::ValueType::INTEGER:{
                    create_table += QString("field_%1 integer,").arg(index);
                }break;
            case KeywordsField::ValueType::STRING:{
                    create_table += QString("field_%1 text,").arg(index);
                }break;
            case KeywordsField::ValueType::ENUM:{
                    create_table += QString("field_%1 integer,").arg(index);
                }break;
            case KeywordsField::ValueType::TABLEREF:{
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
    insert_data = insert_data.arg(table_define.supplyValue());
    sql.prepare(insert_data);
    ExSqlQuery(sql);

    sql.prepare("drop table "+target_table_name + "____ws_transfer_table_delate_soon");
    ExSqlQuery(sql);

    sql.exec("PRAGMA foreign_keys = ON");
}

QString DBAccess::tableTargetOfFieldDefine(const DBAccess::KeywordsField &colDef) const
{
    auto tableDef = colDef;
    if(!colDef.isTableDef())
        tableDef = tableDef.parent();

    return tableDef.supplyValue();
}

int DBAccess::indexOfFieldDefine(const DBAccess::KeywordsField &colDef) const
{
    if(!colDef.isValid())
        throw new WsException("传入的节点无效");

    auto sql = getStatement();
    sql.prepare("select nindex from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toInt();

    throw new WsException("传入的节点无效");
}

DBAccess::KeywordsField::ValueType DBAccess::valueTypeOfFieldDefine(const DBAccess::KeywordsField &colDef) const
{
    auto sql = getStatement();
    sql.prepare("select vtype from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return static_cast<DBAccess::KeywordsField::ValueType>(sql.value(0).toInt());
    throw new WsException("传入的节点无效");
}

QString DBAccess::nameOfFieldDefine(const DBAccess::KeywordsField &colDef) const
{
    auto sql = getStatement();
    sql.prepare("select name from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

void DBAccess::resetNameOfFieldDefine(const DBAccess::KeywordsField &col, const QString &name)
{
    auto sql = getStatement();
    sql.prepare("update tables_define set name=:nm where id=:id");
    sql.bindValue(":nm", name);
    sql.bindValue(":id", col.registID());
    ExSqlQuery(sql);
}

QString DBAccess::supplyValueOfFieldDefine(const DBAccess::KeywordsField &field) const
{
    auto sql = getStatement();
    sql.prepare("select supply from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

DBAccess::KeywordsField DBAccess::tableDefineOfField(const DBAccess::KeywordsField &field) const
{
    auto sql = getStatement();
    sql.prepare("select type, parent from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next()){
        if(sql.value(0).toInt() == 0)
            return KeywordsField(this, sql.value(1).toInt());
        return KeywordsField();
    }
    throw new WsException("指定传入节点无效");
}

int DBAccess::fieldsCountOfTable(const DBAccess::KeywordsField &table) const
{
    if(!table.isTableDef())
        throw new WsException("传入节点不是表定义节点");

    auto sql = getStatement();
    sql.prepare("select count(*) from tables_define where parent=:pid and type=0 group by parent");
    sql.bindValue(":pid", table.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toInt();
    return 0;
}

DBAccess::KeywordsField DBAccess::tableFieldAt(const DBAccess::KeywordsField &table, int index) const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type=0 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", table.registID());
    sql.bindValue(":idx", index);
    ExSqlQuery(sql);

    if(sql.next())
        return KeywordsField(this, sql.value(0).toInt());
    return KeywordsField();
}

DBAccess::KeywordsField DBAccess::nextSiblingField(const DBAccess::KeywordsField &field) const
{
    auto sql = getStatement();
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
        return KeywordsField(this, sql.value(0).toInt());

    return KeywordsField();
}

DBAccess::KeywordsField DBAccess::previousSiblingField(const DBAccess::KeywordsField &field) const
{
    auto sql = getStatement();
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
        return KeywordsField(this, sql.value(0).toInt());
    return KeywordsField();
}

void DBAccess::queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KeywordsField &table) const
{
    disconnect(disp_model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_2_keywords_model_changed);
    disp_model->clear();

    auto sql = getStatement();
    auto table_define = table;
    if(!table_define.isTableDef())
        table_define = table_define.parent();

    auto cols_count = table_define.childCount();
    QList<DBAccess::KeywordsField> cols;
    QString exstr = "select id, name,";
    for (auto index=0; index<cols_count; ++index){
        exstr += QString("field_%1,").arg(index);
        cols << table_define.childAt(index);
    }

    exstr += " from " + table_define.tableTarget();
    if(name != "*")
        exstr += " where name like '%"+name+"%'";

    sql.prepare(exstr);
    ExSqlQuery(sql);




    QStringList header;
    header << "名称";
    for (auto def : cols)
        header << def.name();
    disp_model->setHorizontalHeaderLabels(header);




    while (sql.next()) {
        QList<QStandardItem*> row;

        row << new QStandardItem(sql.value(1).toString());
        row.last()->setData(sql.value(0), Qt::UserRole+1);                      // id-number
        row.last()->setData(table_define.tableTarget(), Qt::UserRole+2);        // table-name
        row.last()->setData(table_define.name(), Qt::UserRole+3);      // table-type

        int size = cols.size() + 2;
        for (int index=2; index < size; ++index) {
            auto colDef = cols.at(index-2);

            switch (colDef.vType()) {
                case KeywordsField::ValueType::INTEGER:
                case KeywordsField::ValueType::STRING:
                    row << new QStandardItem(sql.value(index).toString());
                    row.last()->setData(sql.value(index));
                    row.last()->setData(colDef.index(), Qt::UserRole+2);
                    break;
                case KeywordsField::ValueType::ENUM:{
                        auto values = colDef.supplyValue().split(";");
                        auto item_index = sql.value(index).toInt();
                        if(item_index <0 || item_index>=values.size())
                            throw new WsException("存储值超界");
                        row << new QStandardItem(values[item_index]);
                        row.last()->setData(sql.value(index));
                        row.last()->setData(colDef.index(), Qt::UserRole+2);
                    }break;
                case KeywordsField::ValueType::TABLEREF:{
                        auto qex = getStatement();
                        qex.prepare("select name from "+colDef.supplyValue()+" where id=:id");
                        qex.bindValue(":id", sql.value(index));
                        ExSqlQuery(qex);
                        if(qex.next())
                            throw new WsException("绑定空值");
                        row << new QStandardItem(qex.value(0).toString());
                        row.last()->setData(sql.value(index));
                        row.last()->setData(colDef.index(), Qt::UserRole+2);
                    }break;
            }
        }

        disp_model->appendRow(row);
    }

    connect(disp_model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_2_keywords_model_changed);
}



QList<DBAccess::BranchAttachPoint> DBAccess::getAttachPointsViaDespline(const DBAccess::StoryNode &despline) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where despline_ref=:ref order by nindex");
    sql.bindValue(":ref", despline.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::BranchAttachPoint> DBAccess::getAttachPointsViaChapter(const DBAccess::StoryNode &chapter) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where chapter_attached=:ref");
    sql.bindValue(":ref", chapter.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

QList<DBAccess::BranchAttachPoint> DBAccess::getAttachPointsViaStoryblock(const DBAccess::StoryNode &storyblock) const
{
    auto sql = getStatement();
    sql.prepare("select id from points_collect where story_attached=:ref");
    sql.bindValue(":ref", storyblock.uniqueID());
    ExSqlQuery(sql);

    QList<BranchAttachPoint> ret;
    while (sql.next()) {
        ret << BranchAttachPoint(this, sql.value(0).toInt());
    }

    return ret;
}

DBAccess::BranchAttachPoint DBAccess::insertAttachPointBefore(const DBAccess::StoryNode &despline, int index,
                                                              const QString &title, const QString &description)
{
    auto q = getStatement();
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
    return BranchAttachPoint(this, q.value(0).toInt());
}

void DBAccess::removeAttachPoint(DBAccess::BranchAttachPoint point)
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

void DBAccess::listen_2_keywords_model_changed(QStandardItem *item)
{
    auto sql = getStatement();

    switch (item->column()) {
        case 0:{
                auto id = item->data(Qt::UserRole+1).toInt();
                auto t_name = item->data(Qt::UserRole+2).toString();
                sql.prepare("update "+t_name+" set name=:nm where id=:id");
                sql.bindValue(":nm", item->text());
                sql.bindValue(":id", id);
                ExSqlQuery(sql);
            }break;
        default:{
                auto col_index = item->data(Qt::UserRole+2).toInt();                      // column-index

                auto index = item->index();
                auto first_index = index.sibling(index.row(), 0);
                auto id = first_index.data(Qt::UserRole+1).toInt();         // id-number
                auto t_name = first_index.data(Qt::UserRole+2).toString();  // table-name
                auto t_type = first_index.data(Qt::UserRole+3).toString();  // table-type

                sql.prepare(QString("update "+t_name+" set field_%1=:v where id=:id").arg(col_index));
                sql.bindValue(":id", id);
                sql.bindValue(":v", item->data());
                ExSqlQuery(sql);
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














DBAccess::StoryNode::StoryNode():valid_state(false),host(nullptr){}

DBAccess::StoryNode::StoryNode(const DBAccess::StoryNode &other)
    :valid_state(other.valid_state),
      id_store(other.id_store),
      node_type(other.node_type),
      host(other.host){}

QString DBAccess::StoryNode::title() const{return host->titleOfStoryNode(*this);}

QString DBAccess::StoryNode::description() const{return host->descriptionOfStoryNode(*this);}

DBAccess::StoryNode::Type DBAccess::StoryNode::type() const{return node_type;}

int DBAccess::StoryNode::uniqueID() const{return id_store;}

bool DBAccess::StoryNode::isValid() const{return valid_state;}

DBAccess::StoryNode DBAccess::StoryNode::parent() const{return host->parentOfStoryNode(*this);}

int DBAccess::StoryNode::index() const
{
    return host->indexOfStoryNode(*this);
}

int DBAccess::StoryNode::childCount(DBAccess::StoryNode::Type type) const{return host->childCountOfStoryNode(*this, type);}

DBAccess::StoryNode DBAccess::StoryNode::childAt(DBAccess::StoryNode::Type type, int index) const
{return host->childAtOfStoryNode(*this, type, index);}

DBAccess::StoryNode &DBAccess::StoryNode::operator=(const DBAccess::StoryNode &other)
{
    valid_state = other.valid_state;
    id_store = other.id_store;
    node_type = other.node_type;
    host = other.host;

    return *this;
}

bool DBAccess::StoryNode::operator==(const DBAccess::StoryNode &other) const
{
    return host==other.host && valid_state==other.valid_state &&
            id_store == other.id_store && node_type==other.node_type;
}

bool DBAccess::StoryNode::operator!=(const DBAccess::StoryNode &other) const{return !(*this == other);}

DBAccess::StoryNode::StoryNode(const DBAccess *host, int uid, DBAccess::StoryNode::Type type)
    :valid_state(true),id_store(uid),node_type(type),host(host){}












DBAccess::BranchAttachPoint::BranchAttachPoint(const DBAccess::BranchAttachPoint &other)
    :id_store(other.id_store),host(other.host){}

int DBAccess::BranchAttachPoint::uniqueID() const {return id_store;}

DBAccess::StoryNode DBAccess::BranchAttachPoint::attachedDespline() const
{
    return host->desplineOfAttachPoint(*this);
}

DBAccess::StoryNode DBAccess::BranchAttachPoint::attachedChapter() const
{
    return host->chapterOfAttachPoint(*this);
}

DBAccess::StoryNode DBAccess::BranchAttachPoint::attachedStoryblock() const
{
    return host->storyblockOfAttachPoint(*this);
}

int DBAccess::BranchAttachPoint::index() const
{
    return host->indexOfAttachPoint(*this);
}

QString DBAccess::BranchAttachPoint::title() const
{
    return host->titleOfAttachPoint(*this);
}

QString DBAccess::BranchAttachPoint::description() const
{
    return host->descriptionOfAttachPoint(*this);
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

DBAccess::BranchAttachPoint::BranchAttachPoint(const DBAccess *host, int id)
    :id_store(id), host(host){}

DBAccess::KeywordsField::KeywordsField():field_id_store(INT_MAX), valid_state(false), host(nullptr){}

bool DBAccess::KeywordsField::isTableDef() const{return valid_state && !parent().isValid();}

bool DBAccess::KeywordsField::isValid() const{return valid_state;}

QString DBAccess::KeywordsField::tableTarget() const {return host->tableTargetOfFieldDefine(*this);}

int DBAccess::KeywordsField::registID() const{return field_id_store;}

int DBAccess::KeywordsField::index() const{return host->indexOfFieldDefine(*this);}

QString DBAccess::KeywordsField::name() const{return host->nameOfFieldDefine(*this);}

DBAccess::KeywordsField::ValueType DBAccess::KeywordsField::vType() const{return host->valueTypeOfFieldDefine(*this);}

QString DBAccess::KeywordsField::supplyValue() const{return host->supplyValueOfFieldDefine(*this);}

DBAccess::KeywordsField DBAccess::KeywordsField::parent() const{return host->tableDefineOfField(*this);}

int DBAccess::KeywordsField::childCount() const {return host->fieldsCountOfTable(*this);}

DBAccess::KeywordsField DBAccess::KeywordsField::childAt(int index) const{return host->tableFieldAt(*this, index);}

DBAccess::KeywordsField &DBAccess::KeywordsField::operator=(const DBAccess::KeywordsField &other)
{
    field_id_store = other.field_id_store;
    valid_state = other.valid_state;
    host = other.host;
    return *this;
}

DBAccess::KeywordsField::KeywordsField(const DBAccess *host, int fieldID):field_id_store(fieldID),valid_state(true), host(host){}

DBAccess::KeywordsField DBAccess::KeywordsField::nextSibling() const{
    return host->nextSiblingField(*this);
}

DBAccess::KeywordsField DBAccess::KeywordsField::previousSibling() const{
    return host->previousSiblingField(*this);
}
