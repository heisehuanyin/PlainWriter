#include "confighost.h"

#include <QDir>
#include <QFileInfo>
#include <QTextFrame>
#include <QTextStream>

using namespace NovelBase;

#define ExSqlQuery(q) \
    if(!q.exec()) \
    throw new NovelBase::WsException(q.lastError().text());

ConfigHost::ConfigHost(const QString &wfPath)
    :warrings_filepath(wfPath)
{
    qRegisterMetaType<QTextBlock>("QTextBlock");

    // load warring-words
    QFile warrings(wfPath);
    if(!warrings.exists()){
        if(!warrings.open(QIODevice::Text|QIODevice::WriteOnly)){
            throw new WsException("指定文件不存在，新建过程中指定文件无法打开："+wfPath);
        }
        QTextStream tout(&warrings);
        tout.setCodec("UTF-8");
        tout << QObject::tr("# 敏感字配置文件") << endl;
        tout << QObject::tr("# 每一行视为一个敏感字，请注意暂不支持正则表达式") << endl;
        tout << QObject::tr("# 每一行以“#”开头的字符意味着注释，不会被软件收录") << endl;
        tout << QObject::tr("# 删除本文件，将重建本文件并附带示例敏感字，不代表本软件观点") << endl;
        tout << QObject::tr("八九") << endl;
        tout << QObject::tr("乳交") << endl;
        tout << QObject::tr("鸡鸡") << endl;
        tout.flush();
        warrings.close();
    }

    warring_words.clear();

    if(!warrings.open(QIODevice::ReadOnly|QIODevice::Text)){
        throw new WsException("指定文件无法打开："+wfPath);
    }
    QTextStream wtin(&warrings);
    wtin.setCodec("UTF-8");
    while (!wtin.atEnd()) {
        auto line = wtin.readLine();
        line = line.trimmed();
        if (line.startsWith("#"))
            continue;

        if(!warring_words.contains(std::make_tuple("", INT_MAX, line)))
            warring_words.append(std::make_tuple("", INT_MAX, line));
    }
    warrings.close();

    // load view-config.db
    auto config_file_path = QDir(QFileInfo(wfPath).canonicalPath()).filePath("uiconfig.db");
    this->dbins = QSqlDatabase::addDatabase("QSQLITE", "config-access");
    dbins.setDatabaseName(config_file_path);
    if(!dbins.open())
        throw new WsException("数据库无法打开->"+config_file_path);

    QSqlQuery x(dbins);
    x.exec("PRAGMA foreign_keys = ON;");
    x.prepare("create table if not exists view_config ("
              "id integer primary key autoincrement not null,"
              "type integer not null,"
              "parent integer, "
              "nindex integer not null, "
              "supply text,"
              "constraint pkey foreign key (parent) references view_config(id) on delete cascade)");
    ExSqlQuery(x);
}


void ConfigHost::volumeTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(3);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(40);
    cFormat.setFontWeight(200);
}

void ConfigHost::storyblockTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(30);
    cFormat.setFontWeight(200);
}

void ConfigHost::keypointTitleFormat(QTextBlockFormat &bFormat, QTextCharFormat &cFormat) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    bFormat.setTextIndent(10);
    bFormat.setTopMargin(2);
    bFormat.setBottomMargin(2);
    bFormat.setBackground(Qt::lightGray);

    cFormat.setFontPointSize(20);
    cFormat.setFontWeight(200);
}


void ConfigHost::textFrameFormat(QTextFrameFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setTopMargin(10);
    formatOut.setBottomMargin(10);
    formatOut.setBackground(QColor(250,250,250));
}


void ConfigHost::textFormat(QTextBlockFormat &pFormatOut, QTextCharFormat &wFormatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    wFormatOut.setFontPointSize(15);

    pFormatOut.setTopMargin(1);
    pFormatOut.setBottomMargin(1);
    pFormatOut.setLeftMargin(9);
    pFormatOut.setRightMargin(9);
    pFormatOut.setTextIndent(wFormatOut.fontPointSize()*2);
    pFormatOut.setLineHeight(110, QTextBlockFormat::ProportionalHeight);
    pFormatOut.setBackground(QColor(250,250,250));
}

void ConfigHost::warringFormat(QTextCharFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setFontUnderline(true);
    formatOut.setForeground(Qt::red);
}

void ConfigHost::keywordsFormat(QTextCharFormat &formatOut) const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    formatOut.setFontItalic(true);
    formatOut.setForeground(Qt::blue);
}

QList<std::tuple<QString, int, QString>> ConfigHost::warringWords() const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    return warring_words;
}

QList<std::tuple<QString, int, QString>> ConfigHost::getKeywordsWithMSG() const
{
    QMutexLocker locker(const_cast<QMutex*>(&mutex));

    return keywords_list;
}

QString ConfigHost::warringsFilePath() const{
    return warrings_filepath;
}

void ConfigHost::appendKeyword(QString tableRealname, int uniqueID, const QString &words)
{
    QMutexLocker locker((&mutex));

    for (int index=0; index < keywords_list.size(); ++index) {
        auto tuple = keywords_list.at(index);
        auto ref_tablename = std::get<0>(tuple);
        auto unique_id = std::get<1>(tuple);

        if(ref_tablename == tableRealname && uniqueID == unique_id){
            keywords_list.insert(index, std::make_tuple(tableRealname, uniqueID, words));
            keywords_list.removeAt(index+1);
            return;
        }
    }

    keywords_list.append(std::make_tuple(tableRealname, uniqueID, words));
}

void ConfigHost::removeKeyword(QString tableRealname, int uniqueID)
{
    QMutexLocker locker(&mutex);

    for (int index=0; index < keywords_list.size(); ++index) {
        auto tuple = keywords_list.at(index);
        auto ref_tablename = std::get<0>(tuple);
        auto unique_id = std::get<1>(tuple);

        if(ref_tablename == tableRealname && uniqueID == unique_id){
            keywords_list.removeAt(index+1);
        }
    }
}

ConfigHost::ViewConfigController::ViewConfigController(ConfigHost &config):host(config){}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::firstModeConfig() const
{
    return modeConfigAt(0);
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::modeConfigAt(int index) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select count(*) from view_config where type=0 group by type");
    ExSqlQuery(q);
    q.next();
    auto count = q.value(0).toInt();
    if(!count) return ViewConfig();

    if(index < 0 || index > count)
        index = count -1;

    q.prepare("select id from view_config where type=0 and nindex=:idx");
    q.bindValue(":idx", index);
    ExSqlQuery(q);

    if(!q.next())
        throw new WsException("modeconfigat出错");
    return ViewConfig(q.value(0).toInt(), &host);
}

ConfigHost::ViewConfig::Type ConfigHost::ViewConfigController::typeOf(const ConfigHost::ViewConfig &node) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select type from view_config where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("非法节点");
    return static_cast<ViewConfig::Type>(q.value(0).toInt());
}

int ConfigHost::ViewConfigController::indexOf(const ConfigHost::ViewConfig &node) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select nindex from view_config where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("非法节点");
    return q.value(0).toInt();
}

QString ConfigHost::ViewConfigController::supplyOf(const ConfigHost::ViewConfig &node) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select supply from view_config where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("非法节点");
    return q.value(0).toString();
}

void ConfigHost::ViewConfigController::resetSupplyOf(const ConfigHost::ViewConfig &node, const QString &value)
{
    QSqlQuery q(host.dbins);
    q.prepare("update view_config set supply=:spy where id=:id");
    q.bindValue(":spy", value);
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::parentOf(const ConfigHost::ViewConfig &node) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select parent from view_config where id=:id");
    q.bindValue(":id", node.uniqueID());
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("非法节点");

    if(q.value(0).isNull())
        return ViewConfig();

    return ViewConfig(q.value(0).toInt(), &host);
}

int ConfigHost::ViewConfigController::childCountOf(const ConfigHost::ViewConfig &pnode) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select count(*) from view_config where parent=:pid");
    q.bindValue(":pid", pnode.uniqueID());
    ExSqlQuery(q);
    if(!q.next())
        throw new WsException("非法节点");
    return q.value(0).toInt();
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::childAtOf(const ConfigHost::ViewConfig &pnode, int index) const
{
    QSqlQuery q(host.dbins);
    q.prepare("select id from view_config where parent=:pid and nindex=:idx");
    q.bindValue(":pid", pnode.uniqueID());
    q.bindValue(":idx", index);
    ExSqlQuery(q);
    if(!q.next())
        return ViewConfig();
    return ViewConfig(q.value(0).toInt(), &host);
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::nextSibling(const ConfigHost::ViewConfig &item) const
{
    QSqlQuery q(host.dbins);

    switch (item.configType()) {
        case ViewConfig::Type::UICONFIG:
        case ViewConfig::Type::MODEINDICATOR:
            q.prepare("select id from view_config where type=:type and nindex=:idx");
            q.bindValue(":type", item.configType());
            q.bindValue(":idx", item.index()+1);
            ExSqlQuery(q);
            if(!q.next())
                return ViewConfig();
            return ViewConfig(q.value(0).toInt(), &host);

        default:
            auto pnode = item.parent();
            return pnode.childAt(item.index()+1);
    }
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::previousSibling(const ConfigHost::ViewConfig &item) const
{
    QSqlQuery q(host.dbins);

    switch (item.configType()) {
        case ViewConfig::Type::UICONFIG:
        case ViewConfig::Type::MODEINDICATOR:
            q.prepare("select id from view_config where type=:type and nindex=:idx");
            q.bindValue(":type", item.configType());
            q.bindValue(":idx", item.index()-1);
            ExSqlQuery(q);
            if(!q.next())
                return ViewConfig();
            return ViewConfig(q.value(0).toInt(), &host);

        default:
            auto pnode = item.parent();
            return pnode.childAt(item.index()-1);
    }
}

void ConfigHost::ViewConfigController::configMove(const ConfigHost::ViewConfig &destParent, int destIndex, const ConfigHost::ViewConfig &targetConfig)
{
    if(!destParent.isValid() || !targetConfig.isValid())
        throw new WsException("传入的节点非法");
    if(destIndex < 0 || destIndex > destParent.childCount())
        destIndex = destParent.childCount()-1;

    auto temp_node = destParent;
    while (temp_node.isValid()) {
        if(temp_node == targetConfig)
            throw new WsException("不允许父节点添加到子节点下");
        temp_node = temp_node.parent();
    }


    QSqlQuery q(host.dbins);
    q.prepare("update view_config set nindex=nindex-1 where nindex>:idx and parent=:pnode");
    q.bindValue(":idx", targetConfig.index());
    q.bindValue(":pnode", targetConfig.parent().uniqueID());
    ExSqlQuery(q);

    q.prepare("update view_config set nindex=nindex+1 where nindex>=:idx and parent=:pnode");
    q.bindValue(":idx", destIndex);
    q.bindValue(":pnode", destParent.uniqueID());
    ExSqlQuery(q);

    q.prepare("update view_config set parent=:pnode , nindex=:idx where id=:id");
    q.bindValue(":pnode", destParent.uniqueID());
    q.bindValue(":idx", destIndex);
    q.bindValue(":id", targetConfig.uniqueID());
    ExSqlQuery(q);
}

ConfigHost::ViewConfig ConfigHost::ViewConfigController::insertBefore(const ConfigHost::ViewConfig &pnode,
                                                                      ConfigHost::ViewConfig::Type type,
                                                                      int index, QString supply)
{
    QSqlQuery q(host.dbins);

    if(pnode.isValid()){
        switch (pnode.configType()) {
            case ViewConfig::Type::UICONFIG:
                throw new WsException("UICONFIG下不允许插入子项");
            default:break;
                if(type == ViewConfig::Type::VIEWSELECTOR || type == ViewConfig::Type::VIEWSPLITTER)
                    break;
                throw new WsException("插入非法节点");
        }
    }


    switch (type) {
        case ViewConfig::Type::UICONFIG:
        case ViewConfig::Type::MODEINDICATOR:
            q.prepare("update view_config set nindex=nindex+1 where nindex>=:idx and type=:type");
            q.bindValue(":type", type);
            q.bindValue(":idx", index);
            ExSqlQuery(q);

            q.prepare("insert into view_config (type, nindex, supply) values(:t,:i,:s)");
            q.bindValue(":t", type);
            q.bindValue(":i", index);
            q.bindValue(":s", supply);
            ExSqlQuery(q);

            q.prepare("select id from view_config where nindex=:idx and type=:type");
            q.bindValue(":type", type);
            q.bindValue(":idx", index);
            ExSqlQuery(q);
            if(!q.next())
                throw new WsException("项目插入错误");

            return ViewConfig(q.value(0).toInt(), &host);
        default:
            q.prepare("update view_config set nindex=nindex+1 where nindex>=:idx and parent=:pnode");
            q.bindValue(":pnode", pnode.uniqueID());
            q.bindValue(":idx", index);
            ExSqlQuery(q);

            q.prepare("insert into view_config (type, nindex, parent, supply) values(:t,:i,:p, :s)");
            q.bindValue(":t", type);
            q.bindValue(":i", index);
            q.bindValue(":p", pnode.uniqueID());
            q.bindValue(":s", supply);
            ExSqlQuery(q);

            q.prepare("select id from view_config where nindex=:idx and parent=:pnode");
            q.bindValue(":pnode", pnode.uniqueID());
            q.bindValue(":idx", index);
            ExSqlQuery(q);
            if(!q.next())
                throw new WsException("节点插入失败");

            return ViewConfig(q.value(0).toInt(), &host);
    }
}

void ConfigHost::ViewConfigController::remove(const ConfigHost::ViewConfig &item)
{
    int index = item.index();
    auto parent = item.parent();

    QSqlQuery q(host.dbins);
    if(!parent.isValid()){
        q.prepare("update view_config set nindex=nindex-1 where nindex>=:idx and type=:type");
        q.bindValue(":type", item.configType());
    }
    else {
        q.prepare("update view_config set nindex=nindex-1 where nindex>=:idx and parent=:pnode");
        q.bindValue(":pnode", parent.uniqueID());
    }
    q.bindValue(":idx", index);
    ExSqlQuery(q);

    q.prepare("delete from view_config where id=:id");
    q.bindValue(":id", item.uniqueID());
    ExSqlQuery(q);
}


ConfigHost::ViewConfig::ViewConfig():valid_state(false), config(nullptr){}

ConfigHost::ViewConfig::ViewConfig(const ConfigHost::ViewConfig &other)
    :id_store(other.id_store),
      valid_state(other.valid_state),
      config(other.config){}

int ConfigHost::ViewConfig::uniqueID() const {return  id_store;}

bool ConfigHost::ViewConfig::isValid() const {return valid_state;}

ConfigHost::ViewConfig::Type ConfigHost::ViewConfig::configType() const {
    ViewConfigController hdl(*config);
    return hdl.typeOf(*this);
}

int ConfigHost::ViewConfig::index() const
{
    ViewConfigController hdl(*config);
    return hdl.indexOf(*this);
}

QString ConfigHost::ViewConfig::supply() const
{
    ViewConfigController hdl(*config);
    return hdl.supplyOf(*this);
}

ConfigHost::ViewConfig ConfigHost::ViewConfig::parent() const
{
    ViewConfigController hdl(*config);
    return hdl.parentOf(*this);
}

int ConfigHost::ViewConfig::childCount() const
{
    ViewConfigController hdl(*config);
    return hdl.childCountOf(*this);
}

ConfigHost::ViewConfig ConfigHost::ViewConfig::childAt(int index) const
{
    ViewConfigController hdl(*config);
    return hdl.childAtOf(*this, index);
}

ConfigHost::ViewConfig ConfigHost::ViewConfig::nextSibling() const
{
    ViewConfigController hdl(*config);
    return hdl.nextSibling(*this);
}

ConfigHost::ViewConfig ConfigHost::ViewConfig::previousSibling() const
{
    ViewConfigController hdl(*config);
    return hdl.previousSibling(*this);
}

bool ConfigHost::ViewConfig::operator==(const ConfigHost::ViewConfig &other) const
{
    return (id_store == other.id_store) &&
            (valid_state == other.valid_state) &&
            (config == other.config);
}

ConfigHost::ViewConfig::ViewConfig(int id, ConfigHost *config)
    :id_store(id), valid_state(true), config(config){}


