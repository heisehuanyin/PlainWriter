#ifndef DATAACCESS_H
#define DATAACCESS_H

#include <QSqlDatabase>
#include <QVariant>
#include <QRandomGenerator>
#include <QStandardItemModel>

#include "confighost.h"


namespace NovelBase {
    class DBAccess : public QObject
    {
        Q_OBJECT
    public:
        DBAccess(ConfigHost &configPort);
        virtual ~DBAccess() = default;
        void loadFile(const QString &filePath);
        void createEmptyFile(const QString &dest);

        class StoryTreeController;
        class StoryTreeNode
        {
            friend StoryTreeController;
            friend DBAccess;
        public:
            enum class Type{
                NOVEL = -1,
                VOLUME = 0,
                CHAPTER = 1,
                STORYBLOCK = 2,
                KEYPOINT = 3,
                DESPLINE = 4
            };

            StoryTreeNode();
            StoryTreeNode(const StoryTreeNode &other);

            Type type() const;
            int uniqueID() const;
            bool isValid() const;
            QString title() const;
            QString description() const;

            StoryTreeNode parent() const;
            int index() const;
            int childCount(Type type) const;
            StoryTreeNode childAt(Type type, int index) const;

            StoryTreeNode& operator=(const StoryTreeNode &other);
            bool operator==(const StoryTreeNode &other) const;
            bool operator!=(const StoryTreeNode &other) const;

        private:
            bool valid_state;
            int id_store;
            Type node_type;
            DBAccess *host;

            StoryTreeNode(DBAccess *host, int uid, Type type);
        };
        class StoryTreeController {
        public:
            StoryTreeController(DBAccess &host);

            // keys-tree operate
            StoryTreeNode novelNode() const;

            QString titleOf(const StoryTreeNode &node) const;
            QString descriptionOf(const StoryTreeNode &node) const;
            void resetTitleOf(const StoryTreeNode &node, const QString &title);
            void resetDescriptionOf(const StoryTreeNode &node, const QString &description);

            int indexOf(const StoryTreeNode &node) const;
            StoryTreeNode parentOf(const StoryTreeNode &node) const;
            int childCountOf(const StoryTreeNode &pnode, StoryTreeNode::Type type) const;
            StoryTreeNode childAtOf(const StoryTreeNode &pnode, StoryTreeNode::Type type, int index) const;

            void removeNode(const StoryTreeNode &node);
            StoryTreeNode insertChildNodeBefore(const StoryTreeNode &pnode, StoryTreeNode::Type type,
                                            int index, const QString &title, const QString &description);

            StoryTreeNode getNodeViaID(int id) const;

        private:
            DBAccess &host;
        };


        // contents_collect
        QString chapterText(const StoryTreeNode &chapter) const;
        void resetChapterText(const StoryTreeNode &chapter, const QString &text);


        // points_collect operate
        class BranchAttachController;
        class BranchAttachPoint
        {
            friend DBAccess;
            friend BranchAttachController;
        public:
            BranchAttachPoint(const BranchAttachPoint &other);

            int uniqueID() const;
            int index() const;
            QString title() const;
            QString description() const;
            StoryTreeNode attachedDespline() const;
            StoryTreeNode attachedChapter() const;
            StoryTreeNode attachedStoryblock() const;

            BranchAttachPoint &operator=(const BranchAttachPoint &other);
            bool operator==(const BranchAttachPoint &other) const;
            bool operator!=(const BranchAttachPoint &other) const;

        private:
            int id_store;
            DBAccess *host;

            BranchAttachPoint(DBAccess *host, int id);
        };
        class BranchAttachController{
        public:
            BranchAttachController(DBAccess &host);

            BranchAttachPoint getPointViaID(int id) const;
            QList<BranchAttachPoint> getPointsViaDespline(const StoryTreeNode &despline) const;
            QList<BranchAttachPoint> getPointsViaChapter(const StoryTreeNode &chapter) const;
            QList<BranchAttachPoint> getPointsViaStoryblock(const StoryTreeNode &storyblock) const;

            BranchAttachPoint insertPointBefore(const StoryTreeNode &despline, int index, const QString &title, const QString &description);
            void removePoint(BranchAttachPoint point);

            int indexOf(const BranchAttachPoint &node) const;
            QString titleOf(const BranchAttachPoint &node) const;
            QString descriptionOf(const BranchAttachPoint &node) const;
            StoryTreeNode desplineOf(const BranchAttachPoint &node) const;
            StoryTreeNode chapterOf(const BranchAttachPoint &node) const;
            StoryTreeNode storyblockOf(const BranchAttachPoint &node) const;
            void resetTitleOf(const BranchAttachPoint &node, const QString &title);
            void resetDescriptionOf(const BranchAttachPoint &node, const QString &description);
            void resetChapterOf(const BranchAttachPoint &node, const StoryTreeNode &chapter);
            void resetStoryblockOf(const BranchAttachPoint &node, const StoryTreeNode &storyblock);

            bool moveUpOf(const BranchAttachPoint &point);
            bool moveDownOf(const BranchAttachPoint &point);
        private:
            DBAccess &host;
        };


        class KeywordController;
        class KeywordField
        {
            friend DBAccess;
            friend KeywordController;
        public:
            enum class ValueType{
                NUMBER = 0,
                STRING = 1,
                ENUM = 2,
                TABLEREF = 3,
            };

            KeywordField();

            bool isTableDefine() const;
            bool isValid() const;
            QString tableName() const;
            int registID() const;

            int index() const;
            QString name() const;
            ValueType vType() const;
            QString supplyValue() const; // split with “;”

            KeywordField parent() const;
            int childCount() const;
            KeywordField childAt(int index) const;
            KeywordField nextSibling() const;
            KeywordField previousSibling() const;

            KeywordField &operator=(const KeywordField &other);

        private:
            int field_id_store;
            bool valid_state;
            DBAccess *host;

            // typestring用于命名分类结构定义表
            // type_sum + type_detail_xxxx
            KeywordField(DBAccess *host, int fieldID);
        };
        class KeywordController
        {
        public:
            KeywordController(DBAccess &host);

            KeywordField defRoot() const;

            int childCountOf(const KeywordField &pnode) const;
            KeywordField childFieldOf(const KeywordField &pnode, int index) const;

            // table 操作
            KeywordField newTable(const QString &typeName);
            void tableForward(const KeywordField &node);
            void tableBackward(const KeywordField &node);
            void removeTable(const KeywordField &tbColumn);
            KeywordField firstTable() const;
            KeywordField findTableViaTypeName(const QString &typeName) const;
            KeywordField findTableViaTableName(const QString &tableName) const;
            void tablefieldsAdjust(const KeywordField &target_table, const QList<QPair<KeywordField, std::tuple<QString,
                                   QString, KeywordField::ValueType>>> &define);


            // column-define 操作
            QString tableNameOf(const KeywordField &colDef) const;
            KeywordField::ValueType valueTypeOf(const KeywordField &colDef) const;
            int indexOf(const KeywordField &colDef) const;
            QString nameOf(const KeywordField &colDef) const;
            QString supplyValueOf(const KeywordField &field) const;
            void resetNameOf(const KeywordField &col, const QString &name);
            void resetSupplyValueOf(const KeywordField &field, const QString &supply);

            KeywordField parentOf(const KeywordField &field) const;
            int fieldsCountOf(const KeywordField &table) const;
            KeywordField fieldAt(const KeywordField &table, int index) const;
            KeywordField nextSiblingOf(const KeywordField &field) const;
            KeywordField previousSiblingOf(const KeywordField &field) const;


            // items 操作
            void queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KeywordField &table) const;

            void appendEmptyItemAt(const KeywordField &table, const QString &name);
            void removeTargetItemAt(const KeywordField &table, const QModelIndex &index);

            QList<QPair<int, QString>> avaliableEnumsForIndex(const QModelIndex &index) const;
            QList<QPair<int, QString>> avaliableItemsForIndex(const QModelIndex &index) const;

            // pick-mixture-itemslist
            void queryKeywordsViaMixtureList(const QList<QPair<QString, int>> &mixttureList, QStandardItemModel *disp_model) const;

        private:
            DBAccess &host;
        };

        QSqlQuery getStatement() const;
    private:
        ConfigHost &config_host;

        QSqlDatabase dbins;
        QRandomGenerator intGen;

        void disconnect_listen_connect(QStandardItemModel *model);
        void connect_listen_connect(QStandardItemModel *model);
        void listen_keywordsmodel_itemchanged(QStandardItem *item);

        void init_tables(QSqlDatabase &db);

        void _push_all_keywords_to_confighost();
    };
}


#endif // DATAACCESS_H
