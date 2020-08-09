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
        class StoryNode
        {
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

            StoryNode();
            StoryNode(const StoryNode &other);

            Type type() const;
            int uniqueID() const;
            bool isValid() const;
            QString title() const;
            QString description() const;

            StoryNode parent() const;
            int index() const;
            int childCount(Type type) const;
            StoryNode childAt(Type type, int index) const;

            StoryNode& operator=(const StoryNode &other);
            bool operator==(const StoryNode &other) const;
            bool operator!=(const StoryNode &other) const;

        private:
            bool valid_state;
            int id_store;
            Type node_type;
            const DBAccess *host;

            StoryNode(const DBAccess *host, int uid, Type type);
        };

        DBAccess();
        virtual ~DBAccess() = default;
        void loadFile(const QString &filePath);
        void createEmptyDB(const QString &dest);

        // keys-tree operate
        StoryNode novelStoryNode() const;

        QString titleOfStoryNode(const StoryNode &node) const;
        QString descriptionOfStoryNode(const StoryNode &node) const;
        void resetTitleOfStoryNode(const StoryNode &node, const QString &title);
        void resetDescriptionOfStoryNode(const StoryNode &node, const QString &description);

        int indexOfStoryNode(const StoryNode &node) const;
        StoryNode parentOfStoryNode(const StoryNode &node) const;
        int childCountOfStoryNode(const StoryNode &pnode, StoryNode::Type type) const;
        StoryNode childAtOfStoryNode(const StoryNode &pnode, StoryNode::Type type, int index) const;

        void removeStoryNode(const StoryNode &node);
        StoryNode insertChildStoryNodeBefore(const StoryNode &pnode, StoryNode::Type type, int index, const QString &title, const QString &description);

        StoryNode getStoryNodeViaID(int id) const;
        StoryNode firstChapterStoryNode() const;
        StoryNode lastChapterStoryNode() const;
        StoryNode nextChapterStoryNode(const StoryNode &chapterIns) const;
        StoryNode previousChapterStoryNode(const StoryNode &chapterIns) const;

        // contents_collect
        QString chapterText(const StoryNode &chapter) const;
        void resetChapterText(const StoryNode &chapter, const QString &text);


        // points_collect operate
        class BranchAttachPoint
        {
            friend DBAccess;
        public:
            BranchAttachPoint(const BranchAttachPoint &other);

            int uniqueID() const;
            int index() const;
            QString title() const;
            QString description() const;
            StoryNode attachedDespline() const;
            StoryNode attachedChapter() const;
            StoryNode attachedStoryblock() const;

            BranchAttachPoint &operator=(const BranchAttachPoint &other);
            bool operator==(const BranchAttachPoint &other) const;
            bool operator!=(const BranchAttachPoint &other) const;

        private:
            int id_store;
            const DBAccess *host;

            BranchAttachPoint(const DBAccess *host, int id);
        };

        BranchAttachPoint getAttachPointViaID(int id) const;
        QList<BranchAttachPoint> getAttachPointsViaDespline(const StoryNode &despline) const;
        QList<BranchAttachPoint> getAttachPointsViaChapter(const StoryNode &chapter) const;
        QList<BranchAttachPoint> getAttachPointsViaStoryblock(const StoryNode &storyblock) const;

        BranchAttachPoint insertAttachPointBefore(const StoryNode &despline, int index, const QString &title, const QString &description);
        void removeAttachPoint(BranchAttachPoint point);

        int indexOfAttachPoint(const BranchAttachPoint &node) const;
        QString titleOfAttachPoint(const BranchAttachPoint &node) const;
        QString descriptionOfAttachPoint(const BranchAttachPoint &node) const;
        StoryNode desplineOfAttachPoint(const BranchAttachPoint &node) const;
        StoryNode chapterOfAttachPoint(const BranchAttachPoint &node) const;
        StoryNode storyblockOfAttachPoint(const BranchAttachPoint &node) const;
        void resetTitleOfAttachPoint(const BranchAttachPoint &node, const QString &title);
        void resetDescriptionOfAttachPoint(const BranchAttachPoint &node, const QString &description);
        void resetChapterOfAttachPoint(const BranchAttachPoint &node, const StoryNode &chapter);
        void resetStoryblockOfAttachPoint(const BranchAttachPoint &node, const StoryNode &storyblock);


        class KWsField
        {
            friend DBAccess;
        public:
            enum class ValueType{
                INTEGER = 0,
                STRING = 1,
                ENUM = 2,
                TABLEREF = 3,
            };

            KWsField();

            bool isTableDef() const;
            bool isValid() const;
            QString tableTarget() const;
            int registID() const;

            int index() const;
            QString name() const;
            ValueType vType() const;
            QString supplyValue() const; // split with “;”

            KWsField parent() const;
            int childCount() const;
            KWsField childAt(int index) const;
            KWsField nextSibling() const;
            KWsField previousSibling() const;

            KWsField &operator=(const KWsField &other);

        private:
            int field_id_store;
            bool valid_state;
            const DBAccess *host;

            // typestring用于命名分类结构定义表
            // type_sum + type_detail_xxxx
            KWsField(const DBAccess *host, int fieldID);
        };

        KWsField newTable(const QString &typeName);
        void removeTable(const KWsField &tbColumn);
        KWsField firstTable() const;
        KWsField findTable(const QString &typeName) const;
        void fieldsAdjust(const KWsField &target_table,
                          const QList<QPair<KWsField, std::tuple<QString, QString, KWsField::ValueType> > > &define);

        QString tableTargetOfFieldDefine(const KWsField &colDef) const;
        int indexOfFieldDefine(const KWsField &colDef) const;
        KWsField::ValueType valueTypeOfFieldDefine(const KWsField &colDef) const;
        QString nameOfFieldDefine(const KWsField &colDef) const;
        void resetNameOfFieldDefine(const KWsField &col, const QString &name);
        QString supplyValueOfFieldDefine(const KWsField &field) const;
        void resetSupplyValueOfFieldDefine(const KWsField &field, const QString &supply);

        KWsField tableDefineOfField(const KWsField &field) const;
        int fieldsCountOfTable(const KWsField &table) const;
        KWsField tableFieldAt(const KWsField &table, int index) const;
        KWsField nextSiblingField(const KWsField &field) const;
        KWsField previousSiblingField(const KWsField &field) const;

        void appendEmptyItem(const KWsField &field, const QString &name);
        void removeTargetItem(const KWsField &field, QStandardItemModel *disp_model, int index);
        void queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, const DBAccess::KWsField &table) const;

        QList<QPair<int, QString>> avaliableEnumsForIndex(const QModelIndex &index) const;
        QList<QPair<int, QString>> avaliableItemsForIndex(const QModelIndex &index) const;

        QSqlQuery getStatement() const;
    private:
        QSqlDatabase dbins;
        QRandomGenerator intGen;

        void listen_2_keywords_model_changed(QStandardItem *item);

        void init_tables(QSqlDatabase &db);
    };
}


#endif // DATAACCESS_H
