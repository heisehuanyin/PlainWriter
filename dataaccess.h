#ifndef DATAACCESS_H
#define DATAACCESS_H

#include <QSqlDatabase>


namespace NovelBase {
    class DataAccess
    {
    public:
        class TreeNode
        {
            friend DataAccess;
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

            QString title() const;
            void titleReset(const QString &str);
            QString description() const;
            void descriptionReset(const QString &str);
            Type type() const;
            int uniqueID() const;
            bool isValid() const;

            TreeNode& operator=(const TreeNode &other);
            bool operator==(const TreeNode &other) const;

        private:
            bool valid_state;
            int id_store;
            Type node_type;
            const DataAccess *host;

            TreeNode(const DataAccess *host, int uid, Type type);
        };

        DataAccess();
        void loadFile(const QString &filePath);
        void createEmptyDB(const QString &dest);

        // keys-tree operate
        TreeNode novelRoot() const;

        TreeNode parentNode(const TreeNode &node) const;
        int nodeIndex(const TreeNode &node) const;

        int childNodeCount(const TreeNode &pnode, TreeNode::Type type) const;
        TreeNode childNodeAt(const TreeNode &pnode, TreeNode::Type type, int index) const;
        TreeNode insertChildBefore(const TreeNode &pnode, TreeNode::Type type, int index, const QString &title, const QString &description);
        void removeNode(const TreeNode &node);

        TreeNode getTreenodeViaID(int id) const;

        TreeNode firstChapterOfFStruct() const;
        TreeNode lastChapterOfStruct() const;
        TreeNode nextChapterOfFStruct(const TreeNode &chapterIns) const;
        TreeNode previousChapterOfFStruct(const TreeNode &chapterIns) const;

        // contents_collect
        QString chapterText(const TreeNode &chapter) const;
        void chapterTextReset(const TreeNode &chapter, const QString &text);


        // points_collect operate
        class LineStop
        {
            friend DataAccess;
        public:
            LineStop(const LineStop &other);

            int uniqueID() const;
            TreeNode desplineReference() const;
            TreeNode chapterAttached() const;
            TreeNode storyblockAttached() const;
            void chapterAttachedReset(const TreeNode &chapter);
            void storyblockAttachedReset(const TreeNode &story);
            int index() const;
            bool closed() const;
            void colseReset(bool state);
            QString title() const;
            void titleReset(const QString &title);
            QString description() const;
            void descriptionReset(const QString &description);

            LineStop& operator=(const LineStop &other);

        private:
            int id_store;
            const DataAccess *host;

            LineStop(const DataAccess *host, int id);
        };

        QList<LineStop> getAttachedPointsViaDespline(const TreeNode &despline) const;
        QList<LineStop> getAttachedPointsViaChapter(const TreeNode &chapter) const;
        QList<LineStop> getAttachedPointsViaStoryblock(const TreeNode &storyblock) const;

        LineStop insertAttachpointBefore(const TreeNode &despline, int index, bool close, const QString &title, const QString &description);
        void removePoint(LineStop point);

        QSqlQuery getStatement() const;
    private:
        QSqlDatabase dbins;

        void init_tables(QSqlDatabase &db);
    };
}


#endif // DATAACCESS_H
