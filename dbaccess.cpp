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


QString DBAccess::chapterText(const DBAccess::Storynode &chapter) const
{
    if(chapter.type() != Storynode::Type::CHAPTER)
        throw new WsException("指定节点非章节节点");

    auto sql = getStatement();
    sql.prepare("select content from contents_collect where chapter_ref = :cid");
    sql.bindValue(":cid", chapter.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toString();
}

void DBAccess::resetChapterText(const DBAccess::Storynode &chapter, const QString &text)
{
    if(chapter.type() != Storynode::Type::CHAPTER)
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


DBAccess::KWsField DBAccess::newTable(const QString &typeName)
{
    // 查重
    auto target = findTable(typeName);
    if(target.isValid())
        throw new WsException("重复定义指定类型表格");

    auto tables = dbins.tables();
    auto new_table_name = QString("keywords_%1").arg(intGen.generate64());
    while (tables.contains(new_table_name)) {
        new_table_name = QString("keywords_%1").arg(intGen.generate64());
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

void DBAccess::removeTable(const KWsField &tbColumn)
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

DBAccess::KWsField DBAccess::firstTable() const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type = -1 and nindex = 0");
    ExSqlQuery(sql);
    if(sql.next())
        return KWsField(this, sql.value(0).toInt());
    return KWsField();
}

DBAccess::KWsField DBAccess::findTable(const QString &typeName) const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type = -1 and name=:nm");
    sql.bindValue(":nm", typeName);
    ExSqlQuery(sql);
    if(sql.next())
        return KWsField(this, sql.value(0).toInt());

    return KWsField();
}

void DBAccess::fieldsAdjust(const KWsField &target_table,
                            const QList<QPair<DBAccess::KWsField,
                            std::tuple<QString, QString, DBAccess::KWsField::ValueType>>> &_define)
{
    if(!target_table.isTableDef())
        throw new WsException("传入字段定义非表定义");

    auto sql = getStatement();
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
            case KWsField::ValueType::INTEGER:{
                    create_table += QString("field_%1 integer,").arg(index);
                }break;
            case KWsField::ValueType::STRING:{
                    create_table += QString("field_%1 text,").arg(index);
                }break;
            case KWsField::ValueType::ENUM:{
                    create_table += QString("field_%1 integer,").arg(index);
                }break;
            case KWsField::ValueType::TABLEREF:{
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

QString DBAccess::tableTargetOfFieldDefine(const DBAccess::KWsField &colDef) const
{
    auto tableDef = colDef;
    if(!colDef.isTableDef())
        tableDef = tableDef.parent();

    return tableDef.supplyValue();
}

int DBAccess::indexOfFieldDefine(const DBAccess::KWsField &colDef) const
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

DBAccess::KWsField::ValueType DBAccess::valueTypeOfFieldDefine(const DBAccess::KWsField &colDef) const
{
    auto sql = getStatement();
    sql.prepare("select vtype from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return static_cast<DBAccess::KWsField::ValueType>(sql.value(0).toInt());
    throw new WsException("传入的节点无效");
}

QString DBAccess::nameOfFieldDefine(const DBAccess::KWsField &colDef) const
{
    auto sql = getStatement();
    sql.prepare("select name from tables_define where id=:id");
    sql.bindValue(":id", colDef.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

void DBAccess::resetNameOfFieldDefine(const DBAccess::KWsField &col, const QString &name)
{
    if(col.isTableDef()){
        if(findTable(name).isValid())
            throw new WsException("该名称重复+无效");
    }

    auto sql = getStatement();
    sql.prepare("update tables_define set name=:nm where id=:id");
    sql.bindValue(":nm", name);
    sql.bindValue(":id", col.registID());
    ExSqlQuery(sql);
}

QString DBAccess::supplyValueOfFieldDefine(const DBAccess::KWsField &field) const
{
    auto sql = getStatement();
    sql.prepare("select supply from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next())
        return sql.value(0).toString();
    throw new WsException("传入的节点无效");
}

DBAccess::KWsField DBAccess::tableDefineOfField(const DBAccess::KWsField &field) const
{
    auto sql = getStatement();
    sql.prepare("select type, parent from tables_define where id=:id");
    sql.bindValue(":id", field.registID());
    ExSqlQuery(sql);
    if(sql.next()){
        if(sql.value(0).toInt() == 0)
            return KWsField(this, sql.value(1).toInt());
        return KWsField();
    }
    throw new WsException("指定传入节点无效");
}

int DBAccess::fieldsCountOfTable(const DBAccess::KWsField &table) const
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

DBAccess::KWsField DBAccess::tableFieldAt(const DBAccess::KWsField &table, int index) const
{
    auto sql = getStatement();
    sql.prepare("select id from tables_define where type=0 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", table.registID());
    sql.bindValue(":idx", index);
    ExSqlQuery(sql);

    if(sql.next())
        return KWsField(this, sql.value(0).toInt());
    return KWsField();
}

DBAccess::KWsField DBAccess::nextSiblingField(const DBAccess::KWsField &field) const
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
        return KWsField(this, sql.value(0).toInt());

    return KWsField();
}

DBAccess::KWsField DBAccess::previousSiblingField(const DBAccess::KWsField &field) const
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
        return KWsField(this, sql.value(0).toInt());
    return KWsField();
}

void DBAccess::appendEmptyItem(const DBAccess::KWsField &field, const QString &name)
{
    auto sql = getStatement();
    sql.prepare("insert into "+field.tableTarget()+" (name) values (:nm)");
    sql.bindValue(":nm", name);
    ExSqlQuery(sql);
}

void DBAccess::removeTargetItem(const DBAccess::KWsField &field, QStandardItemModel *disp_model, int index)
{
    auto id = disp_model->item(index)->data().toInt();

    auto sql = getStatement();
    sql.prepare("delete from "+field.tableTarget()+" where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);
}

void DBAccess::queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KWsField &table) const
{
    disconnect(disp_model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_2_keywords_model_changed);
    disp_model->clear();

    auto sql = getStatement();
    auto table_define = table;
    if(!table_define.isTableDef())
        table_define = table_define.parent();

    auto cols_count = table_define.childCount();
    QList<DBAccess::KWsField> cols;
    QString exstr = "select id, name,";
    for (auto index=0; index<cols_count; ++index){
        exstr += QString("field_%1,").arg(index);
        cols << table_define.childAt(index);
    }

    exstr = exstr.mid(0, exstr.length()-1) + " from " + table_define.tableTarget();
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
        row.last()->setData(table_define.name(), Qt::UserRole+3);               // table-type

        int size = cols.size() + 2;
        for (int index=2; index < size; ++index) {
            auto colDef = cols.at(index-2);

            switch (colDef.vType()) {
                case KWsField::ValueType::INTEGER:
                case KWsField::ValueType::STRING:
                    row << new QStandardItem(sql.value(index).toString());
                    row.last()->setData(sql.value(index));
                    row.last()->setData(colDef.index(), Qt::UserRole+2);
                    break;
                case KWsField::ValueType::ENUM:{
                        auto values = colDef.supplyValue().split(";");
                        auto item_index = sql.value(index).toInt();
                        if(item_index <0 || item_index>=values.size())
                            throw new WsException("存储值超界");

                        row << new QStandardItem(values[item_index]);
                        row.last()->setData(sql.value(index));
                        row.last()->setData(colDef.index(), Qt::UserRole+2);
                    }break;
                case KWsField::ValueType::TABLEREF:{
                        row << new QStandardItem("悬空");
                        row.last()->setData(sql.value(index));
                        row.last()->setData(colDef.index(), Qt::UserRole+2);

                        if(!sql.value(index).isNull())
                        {
                            auto qex = getStatement();
                            qex.prepare("select name from "+colDef.supplyValue()+" where id=:id");
                            qex.bindValue(":id", sql.value(index));
                            ExSqlQuery(qex);
                            if(!qex.next())
                                throw new WsException("绑定空值");
                            row.last()->setText(qex.value(0).toString());
                        }
                    }break;
            }
            row.last()->setData(static_cast<int>(colDef.vType()), Qt::UserRole+3);
        }

        disp_model->appendRow(row);
    }

    connect(disp_model, &QStandardItemModel::itemChanged,    this,   &DBAccess::listen_2_keywords_model_changed);
}

QList<QPair<int, QString> > DBAccess::avaliableEnumsForIndex(const QModelIndex &index) const
{
    auto type = static_cast<KWsField::ValueType>(index.data(Qt::UserRole+3).toInt());
    if(type != KWsField::ValueType::ENUM)
        throw new WsException("目标数据类型不为ENUM");

    auto kw_type = index.sibling(index.row(), 0).data(Qt::UserRole+3).toString();
    auto column_def = findTable(kw_type).childAt(index.column()-1);

    auto list = column_def.supplyValue().split(";");
    QList<QPair<int, QString>> ret_list;
    for (auto item : list) {
        if(item.isEmpty())
            continue;

        ret_list << qMakePair(ret_list.size(), item);
    }
    return ret_list;
}

QList<QPair<int, QString> > DBAccess::avaliableItemsForIndex(const QModelIndex &index) const
{
    auto type = static_cast<KWsField::ValueType>(index.data(Qt::UserRole+3).toInt());
    if(type != KWsField::ValueType::TABLEREF)
        throw new WsException("目标数据类型不为TABLEREF");

    auto kw_type = index.sibling(index.row(), 0).data(Qt::UserRole+3).toString();
    auto column_def = findTable(kw_type).childAt(index.column()-1);

    auto reftable_reference = column_def.supplyValue();
    auto sql = getStatement();
    sql.prepare("select id, name from " + reftable_reference + " order by id");
    ExSqlQuery(sql);

    QList<QPair<int, QString>> retlist;
    while (sql.next()) {
        retlist << qMakePair(sql.value(0).toInt(), sql.value(1).toString());
    }

    return retlist;
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
                auto table_ref = item->data(Qt::UserRole+2).toString();
                sql.prepare("update "+table_ref+" set name=:nm where id=:id");
                sql.bindValue(":nm", item->text());
                sql.bindValue(":id", id);
                ExSqlQuery(sql);
            }break;
        default:{
                auto col_index = item->data(Qt::UserRole+2).toInt();        // column-index

                auto first_index = item->index().sibling(item->row(), 0);
                auto id = first_index.data(Qt::UserRole+1).toInt();         // id-number
                auto t_name = first_index.data(Qt::UserRole+2).toString();  // table-name
                auto t_type = first_index.data(Qt::UserRole+3).toString();  // table-type

                sql.prepare(QString("update "+t_name+" set field_%1=:v where id=:id").arg(col_index));
                sql.bindValue(":id", id);
                auto val = item->data().toInt();
                sql.bindValue(":v", val);
                ExSqlQuery(sql);

                auto column_define = findTable(t_type).childAt(col_index);
                switch (column_define.vType()) {
                    case KWsField::ValueType::INTEGER:
                    case KWsField::ValueType::STRING:
                        item->setText(item->data().toString());
                        break;
                    case KWsField::ValueType::ENUM:{
                            auto values = avaliableEnumsForIndex(item->index());
                            auto item_index = item->data().toInt();
                            if(item_index <0 || item_index>=values.size())
                                throw new WsException("存储值超界");
                            item->setText(values[item_index].second);
                        }break;
                    case KWsField::ValueType::TABLEREF:{
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



DBAccess::Storynode::Storynode():valid_state(false),host(nullptr){}

DBAccess::Storynode::Storynode(const DBAccess::Storynode &other)
    :valid_state(other.valid_state),
      id_store(other.id_store),
      node_type(other.node_type),
      host(other.host){}

QString DBAccess::Storynode::title() const{
    StorynodeController handle(*host);
    return handle.titleOfStoryNode(*this);
}

QString DBAccess::Storynode::description() const{
    StorynodeController handle(*host);
    return handle.descriptionOfStoryNode(*this);
}

DBAccess::Storynode::Type DBAccess::Storynode::type() const{return node_type;}

int DBAccess::Storynode::uniqueID() const{return id_store;}

bool DBAccess::Storynode::isValid() const{return valid_state;}

DBAccess::Storynode DBAccess::Storynode::parent() const{
    StorynodeController handle(*host);
    return handle.parentOfStoryNode(*this);
}

int DBAccess::Storynode::index() const
{
    StorynodeController handle(*host);
    return handle.indexOfStoryNode(*this);
}

int DBAccess::Storynode::childCount(DBAccess::Storynode::Type type) const{
    StorynodeController handle(*host);
    return handle.childCountOfStoryNode(*this, type);
}

DBAccess::Storynode DBAccess::Storynode::childAt(DBAccess::Storynode::Type type, int index) const
{
    StorynodeController handle(*host);
    return handle.childAtOfStoryNode(*this, type, index);
}

DBAccess::Storynode &DBAccess::Storynode::operator=(const DBAccess::Storynode &other)
{
    valid_state = other.valid_state;
    id_store = other.id_store;
    node_type = other.node_type;
    host = other.host;

    return *this;
}

bool DBAccess::Storynode::operator==(const DBAccess::Storynode &other) const
{
    return host==other.host && valid_state==other.valid_state &&
            id_store == other.id_store && node_type==other.node_type;
}

bool DBAccess::Storynode::operator!=(const DBAccess::Storynode &other) const{return !(*this == other);}

DBAccess::Storynode::Storynode(DBAccess *host, int uid, DBAccess::Storynode::Type type)
    :valid_state(true),id_store(uid),node_type(type),host(host){}



DBAccess::BranchAttachPoint::BranchAttachPoint(const DBAccess::BranchAttachPoint &other)
    :id_store(other.id_store),host(other.host){}

int DBAccess::BranchAttachPoint::uniqueID() const {return id_store;}

DBAccess::Storynode DBAccess::BranchAttachPoint::attachedDespline() const
{
    BranchAttachPointController hdl(*host);
    return hdl.desplineOfAttachPoint(*this);
}

DBAccess::Storynode DBAccess::BranchAttachPoint::attachedChapter() const
{
    BranchAttachPointController hdl(*host);
    return hdl.chapterOfAttachPoint(*this);
}

DBAccess::Storynode DBAccess::BranchAttachPoint::attachedStoryblock() const
{
    BranchAttachPointController hdl(*host);
    return hdl.storyblockOfAttachPoint(*this);
}

int DBAccess::BranchAttachPoint::index() const
{
    BranchAttachPointController hdl(*host);
    return hdl.indexOfAttachPoint(*this);
}

QString DBAccess::BranchAttachPoint::title() const
{
    BranchAttachPointController hdl(*host);
    return hdl.titleOfAttachPoint(*this);
}

QString DBAccess::BranchAttachPoint::description() const
{
    BranchAttachPointController hdl(*host);
    return hdl.descriptionOfAttachPoint(*this);
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

DBAccess::KWsField::KWsField():field_id_store(INT_MAX), valid_state(false), host(nullptr){}

bool DBAccess::KWsField::isTableDef() const{return valid_state && !parent().isValid();}

bool DBAccess::KWsField::isValid() const{return valid_state;}

QString DBAccess::KWsField::tableTarget() const {return host->tableTargetOfFieldDefine(*this);}

int DBAccess::KWsField::registID() const{return field_id_store;}

int DBAccess::KWsField::index() const{return host->indexOfFieldDefine(*this);}

QString DBAccess::KWsField::name() const{return host->nameOfFieldDefine(*this);}

DBAccess::KWsField::ValueType DBAccess::KWsField::vType() const{return host->valueTypeOfFieldDefine(*this);}

QString DBAccess::KWsField::supplyValue() const{return host->supplyValueOfFieldDefine(*this);}

DBAccess::KWsField DBAccess::KWsField::parent() const{return host->tableDefineOfField(*this);}

int DBAccess::KWsField::childCount() const {return host->fieldsCountOfTable(*this);}

DBAccess::KWsField DBAccess::KWsField::childAt(int index) const{return host->tableFieldAt(*this, index);}

DBAccess::KWsField &DBAccess::KWsField::operator=(const DBAccess::KWsField &other)
{
    field_id_store = other.field_id_store;
    valid_state = other.valid_state;
    host = other.host;
    return *this;
}

DBAccess::KWsField::KWsField(const DBAccess *host, int fieldID):field_id_store(fieldID),valid_state(true), host(host){}

DBAccess::KWsField DBAccess::KWsField::nextSibling() const{
    return host->nextSiblingField(*this);
}

DBAccess::KWsField DBAccess::KWsField::previousSibling() const{
    return host->previousSiblingField(*this);
}

DBAccess::StorynodeController::StorynodeController(DBAccess &host):host(host){}

DBAccess::Storynode DBAccess::StorynodeController::novelStoryNode() const
{
    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where type=-1");
    ExSqlQuery(sql);

    if(sql.next())
        return Storynode(&host, sql.value(0).toInt(), Storynode::Type::NOVEL);

    return Storynode();
}

QString DBAccess::StorynodeController::titleOfStoryNode(const DBAccess::Storynode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select title from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

QString DBAccess::StorynodeController::descriptionOfStoryNode(const DBAccess::Storynode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select desp from keys_tree where id = :id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    if(sql.next())
        return sql.value(0).toString();
    return "";
}

void DBAccess::StorynodeController::resetTitleOfStoryNode(const DBAccess::Storynode &node, const QString &title)
{
    auto sql = host.getStatement();
    sql.prepare("update keys_tree set title=:title where id=:id");
    sql.bindValue(":title", title);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

void DBAccess::StorynodeController::resetDescriptionOfStoryNode(const DBAccess::Storynode &node, const QString &description)
{
    auto sql = host.getStatement();
    sql.prepare("update keys_tree set desp=:title where id=:id");
    sql.bindValue(":title", description);
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);
}

int DBAccess::StorynodeController::indexOfStoryNode(const DBAccess::Storynode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select nindex from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    return sql.value(0).toInt();
}

DBAccess::Storynode DBAccess::StorynodeController::parentOfStoryNode(const DBAccess::Storynode &node) const
{
    auto sql = host.getStatement();
    sql.prepare("select parent from keys_tree where id=:id");
    sql.bindValue(":id", node.uniqueID());
    ExSqlQuery(sql);

    sql.next();
    switch (node.type()) {
        case Storynode::Type::NOVEL:
            return Storynode();
        case Storynode::Type::VOLUME:
            return Storynode(&host, sql.value(0).toInt(), Storynode::Type::NOVEL);
        case Storynode::Type::CHAPTER:
        case Storynode::Type::DESPLINE:
        case Storynode::Type::STORYBLOCK:
            return Storynode(&host, sql.value(0).toInt(), Storynode::Type::VOLUME);
        case Storynode::Type::KEYPOINT:
            return Storynode(&host, sql.value(0).toInt(), Storynode::Type::STORYBLOCK);
        default:
            throw new WsException("意外的节点类型！");
    }
}

int DBAccess::StorynodeController::childCountOfStoryNode(const DBAccess::Storynode &pnode, Storynode::Type type) const
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

DBAccess::Storynode DBAccess::StorynodeController::childAtOfStoryNode(const DBAccess::Storynode &pnode, Storynode::Type type, int index) const
{
    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where parent=:pid and nindex=:ind and type=:type");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":ind", index);
    sql.bindValue(":type", static_cast<int>(type));
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("指定节点指定索引无子节点");
    return Storynode(&host, sql.value(0).toInt(), type);
}

void DBAccess::StorynodeController::removeStoryNode(const DBAccess::Storynode &node)
{
    auto pnode = parentOfStoryNode(node);
    auto index = indexOfStoryNode(node);
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

DBAccess::Storynode DBAccess::StorynodeController::insertChildStoryNodeBefore(const DBAccess::Storynode &pnode, DBAccess::Storynode::Type type,
                                                                              int index, const QString &title, const QString &description)
{
    switch (pnode.type()) {
        case Storynode::Type::NOVEL:
            if(type != Storynode::Type::VOLUME)
                throw new WsException("插入错误节点类型");
            break;
        case Storynode::Type::VOLUME:
            if(type != Storynode::Type::CHAPTER &&
               type != Storynode::Type::STORYBLOCK &&
               type != Storynode::Type::DESPLINE)
                throw new WsException("插入错误节点类型");
            break;
        case Storynode::Type::STORYBLOCK:
            if(type != Storynode::Type::KEYPOINT)
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
    return Storynode(&host, sql.value(0).toInt(), type);
}

DBAccess::Storynode DBAccess::StorynodeController::getStoryNodeViaID(int id) const
{
    auto sql = host.getStatement();
    sql.prepare("select type from keys_tree where id=:id");
    sql.bindValue(":id", id);
    ExSqlQuery(sql);

    if(!sql.next())
        throw new WsException("传入了无效id");

    return Storynode(&host, id, static_cast<Storynode::Type>(sql.value(0).toInt()));
}

DBAccess::Storynode DBAccess::StorynodeController::firstChapterStoryNode() const
{
    auto sql =  host.getStatement();
    sql.prepare("select id from keys_tree where type=1 order by parent, nindex");
    ExSqlQuery(sql);
    if(!sql.next())
        return Storynode();

    return Storynode(&host, sql.value(0).toInt(), Storynode::Type::CHAPTER);
}

DBAccess::Storynode DBAccess::StorynodeController::lastChapterStoryNode() const
{
    auto sql =  host.getStatement();
    sql.prepare("select id from keys_tree where type=0 order by nindex desc");
    ExSqlQuery(sql);
    if(!sql.next())
        return Storynode();

    auto fcid = sql.value(0).toInt();
    sql.prepare("select id from keys_tree where type=1 and parent=:pnode order by nindex desc");
    sql.bindValue(":pnode", fcid);
    ExSqlQuery(sql);
    if(!sql.next())
        return Storynode();

    return Storynode(&host, sql.value(0).toInt(), Storynode::Type::CHAPTER);
}

DBAccess::Storynode DBAccess::StorynodeController::nextChapterStoryNode(const DBAccess::Storynode &chapterIns) const
{
    auto pnode = parentOfStoryNode(chapterIns);
    auto index = indexOfStoryNode(chapterIns);

    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index+1);
    ExSqlQuery(sql);

    if(!sql.next())
        return Storynode();
    return Storynode(&host, sql.value(0).toInt(), Storynode::Type::CHAPTER);
}

DBAccess::Storynode DBAccess::StorynodeController::previousChapterStoryNode(const DBAccess::Storynode &chapterIns) const
{
    auto pnode = parentOfStoryNode(chapterIns);
    auto index = indexOfStoryNode(chapterIns);

    auto sql = host.getStatement();
    sql.prepare("select id from keys_tree where type=1 and parent=:pid and nindex=:idx");
    sql.bindValue(":pid", pnode.uniqueID());
    sql.bindValue(":idx", index-1);
    ExSqlQuery(sql);

    if(!sql.next())
        return Storynode();
    return Storynode(&host, sql.value(0).toInt(), Storynode::Type::CHAPTER);
}

DBAccess::BranchAttachPointController::BranchAttachPointController(DBAccess &host):host(host){}

DBAccess::BranchAttachPoint DBAccess::BranchAttachPointController::getAttachPointViaID(int id) const
{
    auto q = host.getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", id);
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("传入无效id");
    return BranchAttachPoint(&host, id);
}

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachPointController::getAttachPointsViaDespline(const DBAccess::Storynode &despline) const
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

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachPointController::getAttachPointsViaChapter(const DBAccess::Storynode &chapter) const
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

QList<DBAccess::BranchAttachPoint> DBAccess::BranchAttachPointController::getAttachPointsViaStoryblock(const DBAccess::Storynode &storyblock) const
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

DBAccess::BranchAttachPoint DBAccess::BranchAttachPointController::insertAttachPointBefore(const DBAccess::Storynode &despline, int index,
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

void DBAccess::BranchAttachPointController::removeAttachPoint(DBAccess::BranchAttachPoint point)
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

int DBAccess::BranchAttachPointController::indexOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select nindex from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    q.next();

    return q.value(0).toInt();
}

QString DBAccess::BranchAttachPointController::titleOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select title from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

QString DBAccess::BranchAttachPointController::descriptionOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select desp from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return q.value(0).toString();
}

DBAccess::Storynode DBAccess::BranchAttachPointController::desplineOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select despline_ref from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    return Storynode(&host, q.value(0).toInt(), Storynode::Type::DESPLINE);
}

DBAccess::Storynode DBAccess::BranchAttachPointController::chapterOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select chapter_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return Storynode();

    return Storynode(&host, q.value(0).toInt(), Storynode::Type::CHAPTER);
}

DBAccess::Storynode DBAccess::BranchAttachPointController::storyblockOfAttachPoint(const DBAccess::BranchAttachPoint &node) const
{
    auto q = host.getStatement();
    q.prepare("select story_attached from points_collect where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);

    q.next();
    if(q.value(0).isNull())
        return Storynode();

    return Storynode(&host, q.value(0).toInt(), Storynode::Type::STORYBLOCK);
}

void DBAccess::BranchAttachPointController::resetTitleOfAttachPoint(const DBAccess::BranchAttachPoint &node, const QString &title)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set title=:t where id = :id");
    q.bindValue(":t", title);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachPointController::resetDescriptionOfAttachPoint(const DBAccess::BranchAttachPoint &node, const QString &description)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set desp=:t where id = :id");
    q.bindValue(":t", description);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachPointController::resetChapterOfAttachPoint(const DBAccess::BranchAttachPoint &node, const DBAccess::Storynode &chapter)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set chapter_attached = :cid where id=:id");
    q.bindValue(":cid", chapter.isValid()?chapter.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

void DBAccess::BranchAttachPointController::resetStoryblockOfAttachPoint(const DBAccess::BranchAttachPoint &node, const DBAccess::Storynode &storyblock)
{
    auto q = host.getStatement();
    q.prepare("update points_collect set story_attached = :cid where id=:id");
    q.bindValue(":cid", storyblock.isValid()?storyblock.uniqueID():QVariant());
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

//=================================================


