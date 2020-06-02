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


namespace NovelBase {
    class ReferenceItem;
    class FStructure;
    class KeywordsRender;
    class GlobalFormater;
}

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    int loadDescription(QString &err, NovelBase::FStructure *desp);
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
    int appendKeynode(QString &err, const QModelIndex &vmIndex, const QString &kName);
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
    int appendForeshadow(QString &err, const QModelIndex &kIndex, const QString &fName);
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
    int removeNode(QString &errOut, const QModelIndex &nodeIndex);
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
    NovelBase::ReferenceItem *currentOutlineNode() const;

    QStandardItemModel* foreshadowsPresent() const;

    // 章卷节点
    QStandardItemModel *navigateTree() const;
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
     * @param index 合法章节index
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
    NovelBase::FStructure * desp_node;
    QHash<NovelBase::ReferenceItem*,QPair<QTextDocument*, NovelBase::KeywordsRender*>> opening_documents;
    QStandardItemModel *const node_navigate_model;
    QStandardItemModel *const result_enter_model;

    NovelBase::ReferenceItem* append_volume(QStandardItemModel* model, const QString &title);
    NovelBase::ReferenceItem* append_chapter(NovelBase::ReferenceItem* volumeNode, const QString &title);

    void navigate_title_midify(QStandardItem *item);
    int remove_node_recursive(QString &errOut, const QModelIndex &one);

};

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

    class ReferenceItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        ReferenceItem(NovelHost&host, const QString &disp, bool isGroup=false);
        virtual ~ReferenceItem() override = default;

        QPair<int, int> getTargetBinding();

    public slots:
        void calcWordsCount();

    private:
        NovelHost &host;
    };


    class FStructure
    {
    public:
        class NodeSymbo
        {
            friend FStructure;
        public:
            enum class Type{
                VOLUME,
                CHAPTER,
                KEYNODE,
                POINT,
                FORESHADOW,
                SHADOWSTOP,
            };


            NodeSymbo();
            NodeSymbo(QDomElement domNode, Type type);
            NodeSymbo(const NodeSymbo &other);

            NodeSymbo& operator=(const NodeSymbo &other);

            int attr(QString &err, const QString &name, QString &out) const;
            int setAttr(QString &err, const QString &name, const QString &value);

        private:
            QDomElement dom_stored;
            Type type_stored;
        };

        FStructure();
        virtual ~FStructure();

        void newEmptyFile();
        int openFile(QString &errOut, const QString &filePath);

        QString novelDescribeFilePath() const;
        int save(QString &errOut, const QString &newFilepath);

        QString novelTitle() const;
        void resetNovelTitle(const QString &title);

        QString novelDescription() const;
        void resetNovelDescription(const QString &desp);


        int volumeCount() const;
        int volumeAt(QString err, int index, NodeSymbo &node) const;
        int insertVolume(QString &err, const NodeSymbo &before, const QString &title,
                         const QString &description, NodeSymbo &node);

        int knodeCount(QString &err, const NodeSymbo &vmNode, int &num) const;
        int knodeAt(QString &err, const NodeSymbo &vmNode, int index, NodeSymbo &node) const;
        int insertKnode(QString &err, NodeSymbo &vmNode, int before, const QString &title,
                        const QString &description, NodeSymbo &node);

        int pnodeCount(QString &err, const NodeSymbo &knode, int &num) const;
        int pnodeAt(QString &err, const NodeSymbo &knode, int index, NodeSymbo &node) const;
        int insertPnode(QString &err, NodeSymbo &knode, int before, const QString &title,
                        const QString &description, NodeSymbo &node);

        int foreshadowCount(QString &err, const NodeSymbo &knode, int &num) const;
        int foreshadowAt(QString &err, const NodeSymbo &knode, int index, NodeSymbo &node) const;
        int insertForeshadow(QString &err, NodeSymbo &knode, int before, const QString &title,
                             const QString &desp0, const QString &desp1, NodeSymbo &node);

        int shadowstopCount(QString &err, const NodeSymbo &knode, int &num) const;
        int shadowstopAt(QString &err, const NodeSymbo &knode, int index, NodeSymbo &node) const;
        int insertShadowstop(QString &err, NodeSymbo &knode, int before, const QString &vfrom,
                             const QString &kfrom, const QString &connect_shadow, NodeSymbo &node);

        int chapterCount(QString &err, const NodeSymbo &knode, int &num) const;
        int chapterAt(QString &err, const NodeSymbo &knode, int index, NodeSymbo &node) const;
        int insertChapter(QString &err, NodeSymbo &knode, int before, const QString &title,
                          const QString &description, NodeSymbo &node);

        int removeNodeSymbo(QString &err, const NodeSymbo &node);

    private:
        QDomDocument struct_dom_store;
        QString filepath_stored;
        QRandomGenerator gen;

        int check_node_valid(QString &err, const NodeSymbo &node, NodeSymbo::Type type) const;

        int find_direct_subdom_at_index(QString &err, const QDomElement &pnode, const QString &tagName,
                                        int index, QDomElement &node) const;
    };
}

#endif // NOVELHOST_H
