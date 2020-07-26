#ifndef DATAACCESS_H
#define DATAACCESS_H

#include <QSqlDatabase>


namespace NovelBase {
    class DBAccess
    {
    public:
        class TreeNode
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

            TreeNode();
            TreeNode(const TreeNode &other);

            Type type() const;
            int uniqueID() const;
            bool isValid() const;
            QString title() const;
            QString description() const;

            TreeNode parent() const;
            int index() const;
            int childCount(Type type) const;
            TreeNode childAt(Type type, int index) const;

            TreeNode& operator=(const TreeNode &other);
            bool operator==(const TreeNode &other) const;
            bool operator!=(const TreeNode &other) const;

        private:
            bool valid_state;
            int id_store;
            Type node_type;
            const DBAccess *host;

            TreeNode(const DBAccess *host, int uid, Type type);
        };

        DBAccess();
        void loadFile(const QString &filePath);
        void createEmptyDB(const QString &dest);

        // keys-tree operate
        TreeNode novelTreeNode() const;

        QString titleOfTreeNode(const TreeNode &node) const;
        QString descriptionOfTreeNode(const TreeNode &node) const;
        void resetTitleOfTreeNode(const TreeNode &node, const QString &title);
        void resetDescriptionOfTreeNode(const TreeNode &node, const QString &description);

        int indexOfTreeNode(const TreeNode &node) const;
        TreeNode parentOfTreeNode(const TreeNode &node) const;
        int childCountOfTreeNode(const TreeNode &pnode, TreeNode::Type type) const;
        TreeNode childAtOfTreeNode(const TreeNode &pnode, TreeNode::Type type, int index) const;

        void removeTreeNode(const TreeNode &node);
        TreeNode insertChildTreeNodeBefore(const TreeNode &pnode,
                                           TreeNode::Type type, int index,
                                           const QString &title, const QString &description);

        TreeNode getTreeNodeViaID(int id) const;

        TreeNode firstChapterTreeNode() const;
        TreeNode lastChapterTreeNode() const;
        TreeNode nextChapterTreeNode(const TreeNode &chapterIns) const;
        TreeNode previousChapterTreeNode(const TreeNode &chapterIns) const;

        // contents_collect
        QString chapterText(const TreeNode &chapter) const;
        void resetChapterText(const TreeNode &chapter, const QString &text);


        // points_collect operate
        class LineAttachPoint
        {
            friend DBAccess;
        public:
            LineAttachPoint(const LineAttachPoint &other);

            int uniqueID() const;
            int index() const;
            QString title() const;
            QString description() const;
            TreeNode attachedDespline() const;
            TreeNode attachedChapter() const;
            TreeNode attachedStoryblock() const;

            LineAttachPoint &operator=(const LineAttachPoint &other);
            bool operator==(const LineAttachPoint &other) const;
            bool operator!=(const LineAttachPoint &other) const;

        private:
            int id_store;
            const DBAccess *host;

            LineAttachPoint(const DBAccess *host, int id);
        };

        LineAttachPoint getAttachPointViaID(int id) const;

        QList<LineAttachPoint> getAttachPointsViaDespline(const TreeNode &despline) const;
        QList<LineAttachPoint> getAttachPointsViaChapter(const TreeNode &chapter) const;
        QList<LineAttachPoint> getAttachPointsViaStoryblock(const TreeNode &storyblock) const;

        LineAttachPoint insertAttachPointBefore(const TreeNode &despline, int index,const QString &title, const QString &description);
        void removeAttachPoint(LineAttachPoint point);

        int indexOfAttachPoint(const LineAttachPoint &node) const;
        QString titleOfAttachPoint(const LineAttachPoint &node) const;
        QString descriptionOfAttachPoint(const LineAttachPoint &node) const;
        TreeNode desplineOfAttachPoint(const LineAttachPoint &node) const;
        TreeNode chapterOfAttachPoint(const LineAttachPoint &node) const;
        TreeNode storyblockOfAttachPoint(const LineAttachPoint &node) const;
        void resetTitleOfAttachPoint(const LineAttachPoint &node, const QString &title);
        void resetDescriptionOfAttachPoint(const LineAttachPoint &node, const QString &description);
        void resetChapterOfAttachPoint(const LineAttachPoint &node, const TreeNode &chapter);
        void resetStoryblockOfAttachPoint(const LineAttachPoint &node, const TreeNode &storyblock);


        QSqlQuery getStatement() const;
    private:
        QSqlDatabase dbins;

        void init_tables(QSqlDatabase &db);
    };
}


#endif // DATAACCESS_H
