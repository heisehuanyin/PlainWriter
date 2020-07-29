#ifndef DATAACCESS_H
#define DATAACCESS_H

#include <QSqlDatabase>
#include <QVariant>
#include <QRandomGenerator>
#include <QStandardItemModel>


namespace NovelBase {
    class DBAccess : QObject
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
        StoryNode insertChildStoryNodeBefore(const StoryNode &pnode,
                                           StoryNode::Type type, int index,
                                           const QString &title, const QString &description);

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


        class KWFieldDefine
        {
            friend DBAccess;
        public:
            enum class VType{
                INTEGER = 0,
                STRING = 1,
                ENUM = 2,
                TABLEREF = 3,
            };

            KWFieldDefine();

            bool isTableDef() const;
            bool isValid() const;
            QString tableTarget() const;
            int registID() const;

            int index() const;
            QString name() const;
            VType vType() const;
            QString supplyValue() const; // split with “;”

            KWFieldDefine parent() const;
            int childCount() const;
            KWFieldDefine childAt(int index) const;

            KWFieldDefine &operator=(const KWFieldDefine &other);

        private:
            int field_id_store;
            bool valid_state;
            const DBAccess *host;

            // typestring用于命名分类结构定义表
            // type_sum + type_detail_xxxx
            KWFieldDefine(const DBAccess *host, int fieldID);
        };

        KWFieldDefine newTable(const QString &typeName);
        void removeTable(const KWFieldDefine &tbColumn);
        KWFieldDefine firstTable() const;
        KWFieldDefine findTable(const QString &typeName) const;
        void fieldsAdjust(const KWFieldDefine &target,
                          QList<QPair<KWFieldDefine, std::tuple<QString, QString, KWFieldDefine::VType>>> &define);

        QString tableTargetOfFieldDefine(const KWFieldDefine &colDef) const;
        int indexOfFieldDefine(const KWFieldDefine &colDef) const;
        KWFieldDefine::VType valueTypeOfFieldDefine(const KWFieldDefine &colDef) const;
        QString nameOfFieldDefine(const KWFieldDefine &colDef) const;
        void resetNameOfFieldDefine(const KWFieldDefine &col, const QString &name);
        QString supplyValueOfFieldDefine(const KWFieldDefine &field) const;
        void resetSupplyValueOfFieldDefine(const KWFieldDefine &field, const QString &supply);

        KWFieldDefine tableDefineOfField(const KWFieldDefine &field) const;
        int fieldsCountOfTable(const KWFieldDefine &table) const;
        KWFieldDefine tableFieldAt(const KWFieldDefine &table, int index) const;
        KWFieldDefine nextSiblingField(const KWFieldDefine &field) const;
        KWFieldDefine previousSiblingField(const KWFieldDefine &field) const;

        void queryKeywordsLike(QStandardItemModel *disp_model, const QString &name, QList<KWFieldDefine> cols) const;

        QSqlQuery getStatement() const;
    private:
        QSqlDatabase dbins;
        QRandomGenerator intGen;

        void listen_2_keywords_model_changed(QStandardItem *item);

        void init_tables(QSqlDatabase &db);
    };
}


#endif // DATAACCESS_H
