#ifndef DATAACCESS_H
#define DATAACCESS_H

#include <QSqlDatabase>
#include <QVariant>
#include <QRandomGenerator>
#include <QStandardItemModel>


namespace NovelBase {
    class DBAccess : public QObject
    {
        Q_OBJECT
    public:
        DBAccess();
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
            StoryTreeNode novelStoryNode() const;

            QString titleOfStoryNode(const StoryTreeNode &node) const;
            QString descriptionOfStoryNode(const StoryTreeNode &node) const;
            void resetTitleOfStoryNode(const StoryTreeNode &node, const QString &title);
            void resetDescriptionOfStoryNode(const StoryTreeNode &node, const QString &description);

            int indexOfStoryNode(const StoryTreeNode &node) const;
            StoryTreeNode parentOfStoryNode(const StoryTreeNode &node) const;
            int childCountOfStoryNode(const StoryTreeNode &pnode, StoryTreeNode::Type type) const;
            StoryTreeNode childAtOfStoryNode(const StoryTreeNode &pnode, StoryTreeNode::Type type, int index) const;

            void removeStoryNode(const StoryTreeNode &node);
            StoryTreeNode insertChildStoryNodeBefore(const StoryTreeNode &pnode, StoryTreeNode::Type type, int index, const QString &title, const QString &description);

            StoryTreeNode getStoryNodeViaID(int id) const;

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

            BranchAttachPoint getAttachPointViaID(int id) const;
            QList<BranchAttachPoint> getAttachPointsViaDespline(const StoryTreeNode &despline) const;
            QList<BranchAttachPoint> getAttachPointsViaChapter(const StoryTreeNode &chapter) const;
            QList<BranchAttachPoint> getAttachPointsViaStoryblock(const StoryTreeNode &storyblock) const;

            BranchAttachPoint insertAttachPointBefore(const StoryTreeNode &despline, int index, const QString &title, const QString &description);
            void removeAttachPoint(BranchAttachPoint point);

            int indexOfAttachPoint(const BranchAttachPoint &node) const;
            QString titleOfAttachPoint(const BranchAttachPoint &node) const;
            QString descriptionOfAttachPoint(const BranchAttachPoint &node) const;
            StoryTreeNode desplineOfAttachPoint(const BranchAttachPoint &node) const;
            StoryTreeNode chapterOfAttachPoint(const BranchAttachPoint &node) const;
            StoryTreeNode storyblockOfAttachPoint(const BranchAttachPoint &node) const;
            void resetTitleOfAttachPoint(const BranchAttachPoint &node, const QString &title);
            void resetDescriptionOfAttachPoint(const BranchAttachPoint &node, const QString &description);
            void resetChapterOfAttachPoint(const BranchAttachPoint &node, const StoryTreeNode &chapter);
            void resetStoryblockOfAttachPoint(const BranchAttachPoint &node, const StoryTreeNode &storyblock);

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

            bool isTableDef() const;
            bool isValid() const;
            QString tableTarget() const;
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
            KeywordController(DBAccess &host);;

            KeywordField newTable(const QString &typeName);
            void removeTable(const KeywordField &tbColumn);
            KeywordField firstTable() const;
            KeywordField findTable(const QString &typeName) const;
            void fieldsAdjust(const KeywordField &target_table, const QList<QPair<KeywordField, std::tuple<QString,
                              QString, KeywordField::ValueType>>> &define);

            QString tableTargetOfFieldDefine(const KeywordField &colDef) const;
            int indexOfFieldDefine(const KeywordField &colDef) const;
            KeywordField::ValueType valueTypeOfFieldDefine(const KeywordField &colDef) const;
            QString nameOfFieldDefine(const KeywordField &colDef) const;
            void resetNameOfFieldDefine(const KeywordField &col, const QString &name);
            QString supplyValueOfFieldDefine(const KeywordField &field) const;
            void resetSupplyValueOfFieldDefine(const KeywordField &field, const QString &supply);

            KeywordField tableDefineOfField(const KeywordField &field) const;
            int fieldsCountOfTable(const KeywordField &table) const;
            KeywordField tableFieldAt(const KeywordField &table, int index) const;
            KeywordField nextSiblingField(const KeywordField &field) const;
            KeywordField previousSiblingField(const KeywordField &field) const;

            void queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KeywordField &table) const;

            void appendEmptyItem(const KeywordField &field, const QString &name);
            void removeTargetItem(const KeywordField &field, QStandardItemModel *disp_model, int index);

            QList<QPair<int, QString>> avaliableEnumsForIndex(const QModelIndex &index) const;
            QList<QPair<int, QString>> avaliableItemsForIndex(const QModelIndex &index) const;

        private:
            DBAccess &host;
        };

        QSqlQuery getStatement() const;
    private:
        QSqlDatabase dbins;
        QRandomGenerator intGen;

        void disconnect_listen_connect(QStandardItemModel *model);
        void connect_listen_connect(QStandardItemModel *model);
        void listen_keywordsmodel_itemchanged(QStandardItem *item);

        void init_tables(QSqlDatabase &db);
    };
}


#endif // DATAACCESS_H
