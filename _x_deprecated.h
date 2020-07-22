#ifndef _X_DEPRECATED_H
#define _X_DEPRECATED_H

#include <QDomDocument>
#include <QRandomGenerator>
#include <QString>


namespace NovelBase {

    class _X_FStruct
    {
    public:
        class NHandle
        {
            friend _X_FStruct;
        public:
            enum class Type{
                VOLUME,
                CHAPTER,
                KEYSTORY,
                POINT,
                FORESHADOW,
                SHADOWSTART,
                SHADOWSTOP,
            };

            NHandle();
            NHandle(const _X_FStruct::NHandle &other);

            NHandle& operator=(const NHandle &other);
            bool operator==(const NHandle &other) const;

            Type nType() const;
            bool isValid() const;

            QString attr(const QString &name) const;

        private:
            QDomElement elm_stored;
            Type type_stored;

            NHandle(QDomElement elm, Type type);
            void setAttr(const QString &name, const QString &value);
        };

        _X_FStruct();
        virtual ~_X_FStruct();

        void newEmptyFile();
        void openFile(const QString &filePath);

        void setAttr(NHandle &handle, const QString &name, const QString &value);

        QString novelDescribeFilePath() const;
        void save(const QString &newFilepath);

        QString novelTitle() const;
        void resetNovelTitle(const QString &title);

        QString novelDescription() const;
        void resetNovelDescription(const QString &desp);


        int volumeCount() const;
        NHandle volumeAt(int index) const;
        NHandle insertVolume(const NHandle &before, const QString &title, const QString &description);

        int keystoryCount(const NHandle &vmNode) const;
        NHandle keystoryAt(const NHandle &vmNode, int index) const;
        NHandle insertKeystory(NHandle &vmNode, int before, const QString &title, const QString &description);

        int pointCount(const NHandle &knode) const;
        NHandle pointAt(const NHandle &knode, int index) const;
        NHandle insertPoint(NHandle &knode, int before, const QString &title, const QString &description);

        int foreshadowCount(const NHandle &knode) const;
        NHandle foreshadowAt(const NHandle &knode, int index) const;
        /**
         * @brief 通过keysPath获取唯一伏笔
         * @param keysPath volume@keystory@foreshadow
         * @return
         */
        NHandle findForeshadow(const QString &keysPath) const;
        QString foreshadowKeysPath(const NHandle &foreshadow) const;
        NHandle appendForeshadow(NHandle &knode, const QString &title, const QString &desp, const QString &desp_next);

        int chapterCount(const NHandle &vmNode) const;
        NHandle chapterAt(const NHandle &vmNode, int index) const;
        QString chapterKeysPath(const NHandle &chapter) const;
        QString chapterCanonicalFilePath(const NHandle &chapter) const;
        QString chapterTextEncoding(const NHandle &chapter) const;
        NHandle insertChapter(NHandle &vmNode, int before, const QString &title, const QString &description);

        int shadowstartCount(const NHandle &chpNode) const;
        NHandle shadowstartAt(const NHandle &chpNode, int index) const;
        /**
         * @brief 在章节中查找指定shadowstart节点
         * @param chpNode
         * @param target shadowstart节点的target属性值 volume@keystory@foreshadow
         * @return
         */
        NHandle findShadowstart(const NHandle &chpNode, const QString &target) const;
        NHandle appendShadowstart(NHandle &chpNode, const QString &keystory, const QString &foreshadow);


        int shadowstopCount(const NHandle &chpNode) const;
        NHandle shadowstopAt(const NHandle &chpNode, int index) const;
        NHandle findShadowstop(const NHandle &chpNode, const QString &stopTarget) const;
        NHandle appendShadowstop(NHandle &chpNode, const QString &volume, const QString &keystory, const QString &foreshadow);

        // 全局操作
        NHandle parentHandle(const NHandle &base) const;
        int handleIndex(const NHandle &node) const;
        void removeHandle(const NHandle &node);

        // 全局范围内迭代chapter-node
        NHandle firstChapterOfFStruct() const;
        NHandle lastChapterOfStruct() const;
        NHandle nextChapterOfFStruct(const NHandle &chapterIns) const;
        NHandle previousChapterOfFStruct(const NHandle &chapterIns) const;

        void checkHandleValid(const NHandle &node, NHandle::Type type) const;


    private:
        QDomDocument struct_dom_store;
        QString filepath_stored;
        QRandomGenerator random_gen;

        QDomElement find_subelm_at_index(const QDomElement &pnode, const QString &tagName, int index) const;
    };
}

#endif // _X_DEPRECATED_H
