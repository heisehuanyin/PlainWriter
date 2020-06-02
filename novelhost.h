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


    class FStruct
    {
    public:
        class NodeHandle
        {
            friend FStruct;
        public:
            enum class Type{
                VOLUME,
                CHAPTER,
                KEYSTORY,
                POINT,
                FORESHADOW,
                SHADOWSTOP,
            };


            NodeHandle();
            NodeHandle(QDomElement domNode, Type nodeType);
            NodeHandle(const NodeHandle &other);

            NodeHandle& operator=(const NodeHandle &other);
            bool operator==(const NodeHandle &other) const;

            Type nodeType() const;
            int attr(QString &err, const QString &name, QString &out) const;
            int setAttr(QString &err, const QString &name, const QString &value);

        private:
            QDomElement dom_stored;
            Type type_stored;
        };

        FStruct();
        virtual ~FStruct();

        void newEmptyFile();
        int openFile(QString &errOut, const QString &filePath);

        QString novelDescribeFilePath() const;
        int save(QString &errOut, const QString &newFilepath);

        QString novelTitle() const;
        void resetNovelTitle(const QString &title);

        QString novelDescription() const;
        void resetNovelDescription(const QString &desp);


        int volumeCount() const;
        int volumeAt(QString err, int index, NodeHandle &node) const;
        int insertVolume(QString &err, const NodeHandle &before, const QString &title,
                         const QString &description, NodeHandle &node);

        int keystoryCount(QString &err, const NodeHandle &vmNode, int &num) const;
        int keystoryAt(QString &err, const NodeHandle &vmNode, int index, NodeHandle &node) const;
        int insertKeystory(QString &err, NodeHandle &vmNode, int before, const QString &title,
                           const QString &description, NodeHandle &node);

        int pointCount(QString &err, const NodeHandle &knode, int &num) const;
        int pointAt(QString &err, const NodeHandle &knode, int index, NodeHandle &node) const;
        int insertPoint(QString &err, NodeHandle &knode, int before, const QString &title,
                        const QString &description, NodeHandle &node);

        int foreshadowCount(QString &err, const NodeHandle &knode, int &num) const;
        int foreshadowAt(QString &err, const NodeHandle &knode, int index, NodeHandle &node) const;
        int insertForeshadow(QString &err, NodeHandle &knode, int before, const QString &title,
                             const QString &desp, const QString &desp_next, NodeHandle &node);

        int shadowstopCount(QString &err, const NodeHandle &knode, int &num) const;
        int shadowstopAt(QString &err, const NodeHandle &knode, int index, NodeHandle &node) const;
        int insertShadowstop(QString &err, NodeHandle &knode, int before, const QString &vfrom,
                             const QString &kfrom, const QString &connect_shadow, NodeHandle &node);

        int chapterCount(QString &err, const NodeHandle &knode, int &num) const;
        int chapterAt(QString &err, const NodeHandle &knode, int index, NodeHandle &node) const;
        int insertChapter(QString &err, NodeHandle &knode, int before, const QString &title,
                          const QString &description, NodeHandle &node);
        int chapterCanonicalFilePath(QString &err, const NodeHandle &chapter, QString &filePath) const;
        int chapterTextEncoding(QString &err, const NodeHandle &chapter, QString &encoding) const;

        int parentNodeHandle(QString &err, const NodeHandle &base, NodeHandle &parent) const;
        int nodeHandleIndex(QString &err, const NodeHandle &node, int &index) const;
        int removeNodeHandle(QString &err, const NodeHandle &node);

        int checkNodeValid(QString &err, const NodeHandle &node, NodeHandle::Type type) const;
    private:
        QDomDocument struct_dom_store;
        QString filepath_stored;
        QRandomGenerator gen;


        int find_direct_subdom_at_index(QString &err, const QDomElement &pnode, const QString &tagName,
                                        int index, QDomElement &node) const;
    };


    class ChaptersItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        ChaptersItem(NovelHost&host, const FStruct::NodeHandle &refer, bool isGroup=false);
        virtual ~ChaptersItem() override = default;

        const FStruct::NodeHandle getRefer() const;

    public slots:
        void calcWordsCount();

    private:
        NovelHost &host;
        const FStruct::NodeHandle &fstruct_node;
    };

    class OutlinesItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        OutlinesItem(const FStruct::NodeHandle &refer);

        const FStruct::NodeHandle getRefer() const;
    private:
        const FStruct::NodeHandle &fstruct_node;
    };
}

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    int loadDescription(QString &err, NovelBase::FStruct *desp);
    int save(QString &errorOut, const QString &filePath=QString());

    QString novelTitle() const;
    void resetNovelTitle(const QString &title);


    // 大纲节点管理
    /**
     * @brief 获取大纲树形图
     * @return
     */
    QStandardItemModel *outlineTree() const;
    /**
     * @brief 添加卷宗节点
     * @param err
     * @param gName
     * @return
     */
    int appendVolume(QString &err, const QString& gName);
    /**
     * @brief 在指定大纲节点下添加关键剧情节点
     * @param err
     * @param vmIndex 卷宗节点
     * @param kName
     * @return
     */
    int appendKeystory(QString &err, const QModelIndex &vmIndex, const QString &kName);
    /**
     * @brief 在指定关键剧情下添加剧情分解点
     * @param err
     * @param kIndex 关键剧情节点
     * @param pName
     * @return
     */
    int appendPoint(QString &err, const QModelIndex &kIndex, const QString &pName);
    /**
     * @brief 在指定关键剧情下添加伏笔
     * @param err
     * @param kIndex 关键剧情节点
     * @param fName
     * @return
     */
    int appendForeshadow(QString &err, const QModelIndex &kIndex, const QString &fName, const QString &desp, const QString &desp_next);
    /**
     * @brief 在指定关键剧情下添加伏笔驻点
     * @param err
     * @param kIndex 关键剧情节点
     * @param vKey 卷宗键名
     * @param kKey 关键剧情键名
     * @param fKey 伏笔键名
     * @return
     */
    int appendShadowstop(QString &err, const QModelIndex &kIndex, const QString &vKey, const QString &kKey, const QString &fKey);
    /**
     * @brief 在指定关键剧情下添加章节
     * @param err
     * @param kIndex 关键剧情节点
     * @param aName
     * @return
     */
    int appendChapter(QString &err, const QModelIndex &kIndex, const QString& aName);
    /**
     * @brief 删除任何大纲节点
     * @param errOut
     * @param nodeIndex 大纲节点
     * @return
     */
    int removeOutlineNode(QString &err, const QModelIndex &outlineNode);
    int removeChaptersNode(QString &err, const QModelIndex &chaptersNode);
    /**
     * @brief 获取大纲树节点标题
     * @param err
     * @param nodeIndex 卷宗节点、关键剧情节点、剧情分解点、伏笔节点，章节节点
     * @param title
     * @return
     */
    int outlineNodeTitle(QString &err, const QModelIndex &nodeIndex, QString &title) const;
    /**
     * @brief 获取大纲树节点描述
     * @param err
     * @param nodeIndex 卷宗节点、关键剧情节点、剧情分解点、伏笔节点(能够获取第一个描述)，章节节点
     * @param desp
     * @return
     */
    int outlineNodeDescription(QString &err, const QModelIndex &nodeIndex, QString &desp) const;
    int outlineForshadowNextDescription(QString &err, const QModelIndex &nodeIndex, QString &desp) const;
    /**
     * @brief 重置大纲树节点标题
     * @param err
     * @param nodeIndex 卷宗节点、关键剧情节点、剧情分解点、伏笔节点，章节节点
     * @param title
     * @return
     */
    int resetOutlineNodeTitle(QString &err, const QModelIndex &outlineNode, const QString &title);
    /**
     * @brief 重置大纲树节点描述
     * @param err
     * @param outlineNode 卷宗节点、关键剧情节点、剧情分解点、伏笔节点(能够获取第一个描述)，章节节点
     * @param title
     * @return
     */
    int resetOutlineNodeDescription(QString &err, const QModelIndex &outlineNode, const QString &desp);
    int resetOutlineForshadowNextDescription(QString &err, const QModelIndex &nodeIndex, const QString &desp) const;

    /**
     * @brief 设置指定大纲节点为当前节点
     * @param err
     * @param outlineNode 大纲节点
     * @return
     */
    int setCurrentOutlineNode(QString &err, const QModelIndex &outlineNode);
    /**
     * @brief 获取当前大纲树节点
     * @return
     */
    NovelBase::ChaptersItem *currentOutlineNode() const;

    QStandardItemModel* foreshadowsPresent() const;

    // 章卷节点
    QStandardItemModel *navigateTree() const
    {
        return node_navigate_model;
    }
    void refreshWordsCount();

    // 搜索功能
    QStandardItemModel *searchResultPresent() const;
    void searchText(const QString& text);

    // 关键剧情分解点
    QStandardItemModel *keynodePresent() const;

    // 剧情梗概*3
    QTextDocument *novelDescription() const;
    QTextDocument *volumeDescription() const;
    QTextDocument *currentNodeDescriptionPresent() const;

    /**
     * @brief 获取指定章节节点
     * @param err 错误文本
     * @param index 合法章节树index
     * @param strOut 内容输出
     * @return 状态码0成功
     */
    int chapterTextContent(QString &err, const QModelIndex& index, QString &strOut);
    int calcValidWordsCount(const QString &content);

    /**
     * @brief 打开指定章卷树节点文档
     * @param index 对应navagateTree
     */
    int openDocument(QString &err, const QModelIndex &index);

    // 显示文档相关
    int closeDocument(QString &err, QTextDocument *doc);
    /**
     * @brief 重新渲染关键词
     * @param doc 指定文档
     */
    void rehighlightDocument(QTextDocument *doc);


signals:
    void documentOpened(QTextDocument *doc, const QString &title);
    void documentActived(QTextDocument *doc, const QString &title);
    void documentAboutToBeClosed(QTextDocument *doc);

private:
    ConfigHost &config_host;
    NovelBase::FStruct *desp_node;

    QStandardItemModel *const outline_tree_model;
    QStandardItemModel *const foreshadows_present;

    QStandardItemModel *const result_enter_model;
    QStandardItemModel *const node_navigate_model;
    QStandardItemModel *const keynode_points_model;

    QTextDocument *const novel_description_present;
    QTextDocument *const volume_description_present;
    QTextDocument *const node_description_present;

    QHash<NovelBase::ChaptersItem*,QPair<QTextDocument*, NovelBase::KeywordsRender*>> opening_documents;

    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *> insert_volume(const NovelBase::FStruct::NodeHandle &item, int index);
    NovelBase::ChaptersItem* append_chapter(NovelBase::ChaptersItem* volumeNode, const QString &title);

    void navigate_title_midify(QStandardItem *item);
    int remove_node_recursive(QString &errOut, const NovelBase::FStruct::NodeHandle &one);

};

#endif // NOVELHOST_H
