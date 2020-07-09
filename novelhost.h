#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QDomDocument>
#include <QRandomGenerator>
#include <QRunnable>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>


class NovelHost;

namespace NovelBase {
    class FStruct
    {
    public:
        class NHandle
        {
            friend FStruct;
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
            NHandle(const FStruct::NHandle &other);

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

        FStruct();
        virtual ~FStruct();

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

        void checkNandleValid(const NHandle &node, NHandle::Type type) const;


    private:
        QDomDocument struct_dom_store;
        QString filepath_stored;
        QRandomGenerator random_gen;

        QDomElement find_subelm_at_index(const QDomElement &pnode, const QString &tagName, int index) const;
    };
    class ChaptersItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        ChaptersItem(NovelHost&host, const FStruct::NHandle &refer, bool isGroup=false);
        virtual ~ChaptersItem() override = default;

    public slots:
        void calcWordsCount();

    private:
        NovelHost &host;
    };
    class OutlinesItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        OutlinesItem(const FStruct::NHandle &refer);
    };
    class OutlinesRender : public QSyntaxHighlighter
    {
    public:
        OutlinesRender(QTextDocument *doc, ConfigHost &config);

        // QSyntaxHighlighter interface
    protected:
        virtual void highlightBlock(const QString &text) override;

    private:
        ConfigHost &config;
    };

    class WordsRender : public QSyntaxHighlighter
    {
        Q_OBJECT
    public:
        WordsRender(QTextDocument *target, ConfigHost &config);
        virtual ~WordsRender() override;

        ConfigHost &configBase() const;

        void acceptRenderResult(const QString &content, const QList<std::tuple<QTextCharFormat, QString, int, int>> &rst);
        // QSyntaxHighlighter interface
    protected:
        virtual void highlightBlock(const QString &text) override;

    private:
        QMutex mutex;
        ConfigHost &config;
        QHash<QString, QList<std::tuple<QTextCharFormat, QString, int, int>>> _result_store;

        void extract_render_result(const QString &text, QList<std::tuple<QTextCharFormat, QString, int, int>> &rst);
    };

    class WordsRenderWorker : public QObject, public QRunnable
    {
        Q_OBJECT

    public:
        WordsRenderWorker(WordsRender *poster, const QTextBlock pholder, const QString &content);

        // QRunnable interface
    public:
        virtual void run() override;

    signals:
        void renderFinished(const QTextBlock blk);

    private:
        WordsRender *const poster_stored;
        ConfigHost &config_symbo;
        const QTextBlock placeholder;
        const QString content_stored;

        void _words_render(const QString &text, QList<QString> words, const QTextCharFormat &format,
                              QList<std::tuple<QTextCharFormat, QString, int, int>> &rst) const;
    };

    class WsBlockData : public QTextBlockUserData
    {
    public:
        using Type = FStruct::NHandle::Type;
        WsBlockData(const QModelIndex &target, FStruct::NHandle::Type blockType);
        virtual ~WsBlockData() = default;

        bool operator==(const WsBlockData &other) const;

        QModelIndex outlineTarget() const;
        Type blockType() const;

    private:
        const QModelIndex outline_index;   // 指向大纲树节点
        Type block_type;
    };
}

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    void loadDescription(NovelBase::FStruct *desp);
    void save(const QString &filePath = QString());

    QString novelTitle() const;
    void resetNovelTitle(const QString &title);
    int treeNodeLevel(const QModelIndex &node) const;

    // 大纲节点管理
    /**
     * @brief 获取大纲树形图，包含分卷、剧情、分解点
     * @return
     */
    QStandardItemModel *outlineNavigateTree() const;
    /**
     * @brief 获取全书大纲编辑文档
     * @return
     */
    QTextDocument *novelOutlinesPresent() const;
    /**
     * @brief 获取当前卷所有细纲
     * @return
     */
    QTextDocument *volumeOutlinesPresent() const;
    /**
     * @brief 获取本卷下所有伏笔汇总
     * @return
     */
    QStandardItemModel *foreshadowsUnderVolume() const;
    /**
     * @brief 获取至此卷宗未闭合伏笔
     * @return
     */
    QStandardItemModel *foreshadowsUntilVolumeRemain() const;
    /**
     * @brief 获取至此章节未闭合伏笔
     * @return
     */
    QStandardItemModel *foreshadowsUntilChapterRemain() const;
    /**
     * @brief 传入outlines-node-index获取可用于建立伏笔的outlines-keystory的名称和index
     * @param outlinesNode
     * @return
     */
    QList<QPair<QString, QModelIndex>> outlinesKeystorySum(const QModelIndex &outlinesNode) const;
    /**
     * @brief 添加卷宗节点
     * @param err
     * @param gName
     * @return
     */
    void insertVolume(int before, const QString& gName);
    /**
     * @brief 在指定大纲节点下添加关键剧情节点
     * @param err
     * @param vmIndex 卷宗节点
     * @param kName
     * @return
     */
    void insertKeystory(const QModelIndex &vmIndex, int before, const QString &kName);
    /**
     * @brief 在指定关键剧情下添加剧情分解点
     * @param err
     * @param kIndex 关键剧情节点
     * @param pName
     * @return
     */
    void insertPoint(const QModelIndex &kIndex, int before, const QString &pName);
    /**
     * @brief 在指定关键剧情下添加伏笔
     * @param err
     * @param kIndex 关键剧情节点
     * @param fName
     * @return
     */
    void appendForeshadow(const QModelIndex &kIndex, const QString &fName, const QString &desp, const QString &desp_next);
    void checkOutlinesRemoveEffect(const QModelIndex &outlinesIndex, QList<QString> &msgList) const;
    /**
     * @brief 删除任何大纲节点
     * @param errOut
     * @param nodeIndex 大纲节点
     * @return
     */
    void removeOutlineNode(const QModelIndex &outlineNode);
    /**
     * @brief 设置指定大纲节点为当前节点，引起相应视图变化
     * @param err
     * @param outlineNode 大纲节点
     * @return
     */
    void setCurrentOutlineNode(const QModelIndex &outlineNode);



    // 章卷节点
    /**
     * @brief 章节导航模型
     * @return
     */
    QStandardItemModel *chaptersNavigateTree() const;
    /**
     * @brief 查找结果模型
     * @return
     */
    QStandardItemModel *findResultsPresent() const;
    /**
     * @brief 章节细纲呈现
     * @return
     */
    QTextDocument *chapterOutlinePresent() const;
    /**
     * @brief 传入chapters-node-index获取可用于建立伏笔的outlines-keystory的名称和index
     * @param chaptersNode
     * @return
     */
    QList<QPair<QString, QModelIndex>> chaptersKeystorySum(const QModelIndex &chaptersNode) const;
    /**
     * @brief 在指定卷宗下添加章节
     * @param err
     * @param kIndex 关键剧情节点
     * @param aName
     * @return
     */
    void insertChapter(const QModelIndex &chpsVmIndex, int before, const QString &chpName);
    /**
     * @brief 在指定章节下添加伏笔起点（吸附伏笔）
     * @param chpIndex
     * @param keystory
     * @param foreshadow
     */
    void appendShadowstart(const QModelIndex &chpIndex, const QString &keystory, const QString &foreshadow);
    void removeShadowstart(const QModelIndex &chpIndex, const QString &targetPath);
    /**
     * @brief 在指定关键剧情下添加伏笔驻点
     * @param err
     * @param kIndex 关键剧情节点
     * @param vKey 卷宗键名
     * @param kKey 关键剧情键名
     * @param fKey 伏笔键名
     * @return
     */
    void appendShadowstop(const QModelIndex &chpIndex, const QString &volume,const QString &keystory, const QString &foreshadow);
    void removeShadowstop(const QModelIndex &chpIndex, const QString &targetPath);
    void checkChaptersRemoveEffect(const QModelIndex &chpsIndex, QList<QString> &msgList) const;
    void removeChaptersNode(const QModelIndex &chaptersNode);
    void setCurrentChaptersNode(const QModelIndex &chaptersNode);
    void refreshWordsCount();
    /**
     * @brief 汇聚所有本卷下未吸附伏笔
     * @param foreshadowsList   title,fullpath
     */
    void sumForeshadowsUnderVolumeHanging(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
    /**
     * @brief 汇聚本章下所有吸附伏笔
     * @param chpsNode
     * @param foreshadows
     */
    void sumForeshadowsAbsorbed(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
    void sumForeshadowsOpening(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
    void sumForeshadowsClosed(const QModelIndex &chpsNode, QList<QPair<QString,QString>> &foreshadows) const;


    // 搜索功能
    void searchText(const QString& text);
    QString chapterActiveText(const QModelIndex& index);
    int calcValidWordsCount(const QString &content);


signals:
    void documentPrepared(QTextDocument *doc, const QString &title);
    void messagePopup(const QString &title, const QString &message);
    void warningPopup(const QString &title, const QString &message);
    void errorPopup(const QString &title, const QString &message);

    void currentChaptersActived();
    void currentVolumeActived();

private:
    ConfigHost &config_host;
    NovelBase::FStruct *desp_tree;

    QStandardItemModel *const outline_navigate_treemodel;
    QTextDocument *const novel_outlines_present;
    QTextDocument *const volume_outlines_present;
    QStandardItemModel *const foreshadows_under_volume_present;
    QStandardItemModel *const foreshadows_until_volume_remain_present;
    QStandardItemModel *const foreshadows_until_chapter_remain_present;

    QStandardItemModel *const find_results_model;
    QStandardItemModel *const chapters_navigate_treemodel;
    QTextDocument *const chapter_outlines_present;

    // 所有活动文档存储容器anchor:<doc*,randerer*[nullable]>
    QHash<NovelBase::ChaptersItem*,QPair<QTextDocument*, NovelBase::WordsRender*>> all_documents;
    NovelBase::FStruct::NHandle current_volume_node;
    NovelBase::FStruct::NHandle current_chapter_node;

    /**
     * @brief 向chapters-tree和outline-tree上插入卷宗节点
     * @param item
     * @param index
     * @return
     */
    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *>
    insert_volume(const NovelBase::FStruct::NHandle &volume_handle, int index);

    void listen_novel_description_change();
    void listen_volume_outlines_description_change(int pos, int removed, int added);
    bool check_volume_structure_diff(const NovelBase::OutlinesItem *base_node, QTextBlock &blk) const;
    void listen_volume_outlines_structure_changed();
    void listen_chapter_outlines_description_change();
    void outlines_node_title_changed(QStandardItem *item);
    void chapters_node_title_changed(QStandardItem *item);

    void set_current_volume_outlines(const NovelBase::FStruct::NHandle &node_under_volume);
    void insert_content_at_document(QTextCursor cursor, NovelBase::OutlinesItem *outline_node);

    void sum_foreshadows_under_volume(const NovelBase::FStruct::NHandle &volume_node);
    void sum_foreshadows_until_volume_remains(const NovelBase::FStruct::NHandle &volume_node);
    void sum_foreshadows_until_chapter_remains(const NovelBase::FStruct::NHandle &chapter_node);
    void listen_foreshadows_volume_changed(QStandardItem *item);
    void listen_foreshadows_until_volume_changed(QStandardItem *item);
    void listen_foreshadows_until_chapter_changed(QStandardItem *item);

    NovelBase::FStruct::NHandle _locate_outline_handle_via(QStandardItem *outline_item) const;
    void _check_remove_effect(const NovelBase::FStruct::NHandle &target, QList<QString> &msgList) const;

    QTextDocument* _load_chapter_text_content(QStandardItem* chpAnchor);
};

#endif // NOVELHOST_H
