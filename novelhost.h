#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"

#include <QDomDocument>
#include <QRandomGenerator>
#include <QRunnable>
#include <QSemaphore>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTextFrame>
#include <QThread>
#include <type_traits>

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
            NHandle(QDomElement domNode, Type nType);
            NHandle(const NHandle &other);

            NHandle& operator=(const NHandle &other);
            bool operator==(const NHandle &other) const;

            Type nType() const;
            bool isValid() const;
            QString attr(const QString &name) const;
            void setAttr(const QString &name, const QString &value);

        private:
            QDomElement dom_stored;
            Type type_stored;
        };

        FStruct();
        virtual ~FStruct();

        void newEmptyFile();
        int openFile(QString &errOut, const QString &filePath);

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
         * @param target shadowstart节点的target属性值
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

        const FStruct::NHandle getHandleRef() const;
        FStruct::NHandle::Type getType() const;

    public slots:
        void calcWordsCount();

    private:
        NovelHost &host;
        const FStruct::NHandle &fstruct_node;
    };
    class OutlinesItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        OutlinesItem(const FStruct::NHandle &refer);

        const FStruct::NHandle getHandleRef() const;
        FStruct::NHandle::Type getType() const;
    private:
        const FStruct::NHandle &fstruct_node;
    };
    class KeywordsRender : public QSyntaxHighlighter
    {
        Q_OBJECT
    public:
        KeywordsRender(QTextDocument *target, ConfigHost &config);
        virtual ~KeywordsRender() override;

        // QSyntaxHighlighter interface
    protected:
        virtual void highlightBlock(const QString &text) override;

    private:
        ConfigHost &config;
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

    /**
     * @brief 获取大纲树形图
     * @return
     */
    QStandardItemModel *outlineTree() const;
    /**
     * @brief 获取全书大纲
     * @return
     */
    QTextDocument *novelDescriptions() const;
    /**
     * @brief 获取当前卷所有细纲
     * @return
     */
    QTextDocument *volumeDescriptions() const;
    /**
     * @brief 获取本卷下所有伏笔
     * @return
     */
    QStandardItemModel *foreshadowsUnderVolume() const;
    /**
     * @brief 获取至今未闭合伏笔
     * @return
     */
    QStandardItemModel *foreshadowsUntilRemain() const;

    // 大纲节点管理
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
    int setCurrentOutlineNode(QString &err, const QModelIndex &outlineNode);

    /**
     * @brief checkEffect
     * @param target
     * @param msgList : [type](target)<keys-to-target>msg-body
     */
    void checkRemoveEffect(const NovelBase::FStruct::NHandle &target, QList<QString> &msgList) const;

    // 章卷节点
    QStandardItemModel *chaptersNavigateTree() const;
    QStandardItemModel *findResultsPresent() const;
    QStandardItemModel *outlinesUnderVolume() const;
    /**
     * @brief 在指定关键剧情下添加章节
     * @param err
     * @param kIndex 关键剧情节点
     * @param aName
     * @return
     */
    void insertChapter(const QModelIndex &chpVmIndex, int before, const QString &chpName);/**
     * @brief 在指定章节下添加伏笔起点（吸附伏笔）
     * @param chpIndex
     * @param keystory
     * @param foreshadow
     */
    void appendShadowstart(const QModelIndex &chpIndex, const QString &keystory, const QString &foreshadow);
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
    void removeChaptersNode(const QModelIndex &chaptersNode);
    void refreshWordsCount();

    // 搜索功能
    void searchText(const QString& text);

    /**
     * @brief 获取指定章节节点
     * @param err 错误文本
     * @param index 合法章节树index
     * @param strOut 内容输出
     * @return 状态码0成功
     */
    QString chapterTextContent(const QModelIndex& index);
    int calcValidWordsCount(const QString &content);

    /**
     * @brief 打开指定章卷树节点文档
     * @param index 对应navagateTree
     */
    void openDocument(const QModelIndex &index);

    // 显示文档相关
    void closeDocument(QTextDocument *doc);


signals:
    void documentOpened(QTextDocument *doc, const QString &title);
    void documentActived(QTextDocument *doc, const QString &title);
    void documentAboutToBeClosed(QTextDocument *doc);


private:
    ConfigHost &config_host;
    NovelBase::FStruct *desp_tree;

    QStandardItemModel *const outline_tree_model;
    QTextDocument *const novel_description_present;
    QTextDocument *const volume_description_present;
    QStandardItemModel *const foreshadows_under_volume_present;
    QStandardItemModel *const foreshadows_until_remain_present;

    QStandardItemModel *const find_results_model;
    QStandardItemModel *const chapters_navigate_model;
    QStandardItemModel *const outline_under_volume_prsent;

    // 所有活动文档存储容器
    QHash<NovelBase::ChaptersItem*,QPair<QTextDocument*, NovelBase::KeywordsRender*>> opening_documents;

    /**
     * @brief 向chapters-tree和outline-tree上插入卷宗节点
     * @param item
     * @param index
     * @return
     */
    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *> insert_volume(const NovelBase::FStruct::NHandle &item, int index);

    void resetNovelDescription();

    /**
     * @brief 监听树标题修改
     * @param item
     */
    void chapters_navigate_title_midify(QStandardItem *item)
    {
        if(item->column())
            return;

        auto xitem = static_cast<ChaptersItem*>(item);
        auto struct_node = xitem->getRefer();
        QString err;
        struct_node.setAttr("title", item->text());
    }

};

#endif // NOVELHOST_H
