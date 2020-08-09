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
        void createEmptyDB(const QString &dest);

        class StorynodeController;
        class Storynode
        {
            friend StorynodeController;
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

            Storynode();
            Storynode(const Storynode &other);

            Type type() const;
            int uniqueID() const;
            bool isValid() const;
            QString title() const;
            QString description() const;

            Storynode parent() const;
            int index() const;
            int childCount(Type type) const;
            Storynode childAt(Type type, int index) const;

            Storynode& operator=(const Storynode &other);
            bool operator==(const Storynode &other) const;
            bool operator!=(const Storynode &other) const;

        private:
            bool valid_state;
            int id_store;
            Type node_type;
            DBAccess *host;

            Storynode(DBAccess *host, int uid, Type type);
        };

        class StorynodeController {
        public:
            StorynodeController(DBAccess &host);

            // keys-tree operate
            Storynode novelStoryNode() const;

            QString titleOfStoryNode(const Storynode &node) const;
            QString descriptionOfStoryNode(const Storynode &node) const;
            void resetTitleOfStoryNode(const Storynode &node, const QString &title);
            void resetDescriptionOfStoryNode(const Storynode &node, const QString &description);

            int indexOfStoryNode(const Storynode &node) const;
            Storynode parentOfStoryNode(const Storynode &node) const;
            int childCountOfStoryNode(const Storynode &pnode, Storynode::Type type) const;
            Storynode childAtOfStoryNode(const Storynode &pnode, Storynode::Type type, int index) const;

            void removeStoryNode(const Storynode &node);
            Storynode insertChildStoryNodeBefore(const Storynode &pnode, Storynode::Type type, int index, const QString &title, const QString &description);

            Storynode getStoryNodeViaID(int id) const;
            Storynode firstChapterStoryNode() const;
            Storynode lastChapterStoryNode() const;
            Storynode nextChapterStoryNode(const Storynode &chapterIns) const;
            Storynode previousChapterStoryNode(const Storynode &chapterIns) const;

        private:
            DBAccess &host;
        };


        // contents_collect
        QString chapterText(const Storynode &chapter) const;
        void resetChapterText(const Storynode &chapter, const QString &text);


        // points_collect operate
        class BranchAttachPointController;
        class BranchAttachPoint
        {
            friend DBAccess;
            friend BranchAttachPointController;
        public:
            BranchAttachPoint(const BranchAttachPoint &other);

            int uniqueID() const;
            int index() const;
            QString title() const;
            QString description() const;
            Storynode attachedDespline() const;
            Storynode attachedChapter() const;
            Storynode attachedStoryblock() const;

            BranchAttachPoint &operator=(const BranchAttachPoint &other);
            bool operator==(const BranchAttachPoint &other) const;
            bool operator!=(const BranchAttachPoint &other) const;

        private:
            int id_store;
            DBAccess *host;

            BranchAttachPoint(DBAccess *host, int id);
        };

        class BranchAttachPointController{
        public:
            BranchAttachPointController(DBAccess &host);

            BranchAttachPoint getAttachPointViaID(int id) const;
            QList<BranchAttachPoint> getAttachPointsViaDespline(const Storynode &despline) const;
            QList<BranchAttachPoint> getAttachPointsViaChapter(const Storynode &chapter) const;
            QList<BranchAttachPoint> getAttachPointsViaStoryblock(const Storynode &storyblock) const;

            BranchAttachPoint insertAttachPointBefore(const Storynode &despline, int index, const QString &title, const QString &description);
            void removeAttachPoint(BranchAttachPoint point);

            int indexOfAttachPoint(const BranchAttachPoint &node) const;
            QString titleOfAttachPoint(const BranchAttachPoint &node) const;
            QString descriptionOfAttachPoint(const BranchAttachPoint &node) const;
            Storynode desplineOfAttachPoint(const BranchAttachPoint &node) const;
            Storynode chapterOfAttachPoint(const BranchAttachPoint &node) const;
            Storynode storyblockOfAttachPoint(const BranchAttachPoint &node) const;
            void resetTitleOfAttachPoint(const BranchAttachPoint &node, const QString &title);
            void resetDescriptionOfAttachPoint(const BranchAttachPoint &node, const QString &description);
            void resetChapterOfAttachPoint(const BranchAttachPoint &node, const Storynode &chapter);
            void resetStoryblockOfAttachPoint(const BranchAttachPoint &node, const Storynode &storyblock);

        private:
            DBAccess &host;
        };



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
