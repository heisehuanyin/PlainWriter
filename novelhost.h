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
        int save(QString &errOut, const QString &newFilepath);

        QString novelTitle() const;
        void resetNovelTitle(const QString &title);

        QString novelDescription() const;
        void resetNovelDescription(const QString &desp);


        int volumeCount() const;
        int volumeAt(QString err, int index, NHandle &node) const;
        int insertVolume(QString &err, const NHandle &before, const QString &title,
                         const QString &description, NHandle &node);

        int keystoryCount(QString &err, const NHandle &vmNode, int &num) const;
        int keystoryAt(QString &err, const NHandle &vmNode, int index, NHandle &node) const;
        int insertKeystory(QString &err, NHandle &vmNode, int before, const QString &title,
                           const QString &description, NHandle &node);

        int pointCount(QString &err, const NHandle &knode, int &num) const;
        int pointAt(QString &err, const NHandle &knode, int index, NHandle &node) const;
        int insertPoint(QString &err, NHandle &knode, int before, const QString &title,
                        const QString &description, NHandle &node);

        int foreshadowCount(QString &err, const NHandle &knode, int &num) const;
        int foreshadowAt(QString &err, const NHandle &knode, int index, NHandle &node) const;
        int appendForeshadow(QString &err, NHandle &knode, const QString &title,
                             const QString &desp, const QString &desp_next, NHandle &node);

        int chapterCount(QString &err, const NHandle &vmNode, int &num) const;
        int chapterAt(QString &err, const NHandle &vmNode, int index, NHandle &node) const;
        int insertChapter(QString &err, NHandle &vmNode, int before, const QString &title,
                          const QString &description, NHandle &node);
        int chapterCanonicalFilePath(QString &err, const NHandle &chapter, QString &filePath) const;
        int chapterTextEncoding(QString &err, const NHandle &chapter, QString &encoding) const;

        int shadowstartCount(QString &err, const NHandle &chpNode, int &num) const;
        int shadowstartAt(QString &err, const NHandle &chpNode, int index, NHandle &node) const;
        int appendShadowstart(QString &err, NHandle &chpNode, const QString &keystory,
                              const QString &foreshadow, NHandle &node);


        int shadowstopCount(QString &err, const NHandle &chpNode, int &num) const;
        int shadowstopAt(QString &err, const NHandle &chpNode, int index, NHandle &node) const;
        int appendShadowstop(QString &err, NHandle &chpNode, const QString &volume,
                             const QString &keystory, const QString &foreshadow, NHandle &node);


        int parentHandle(QString &err, const NHandle &base, NHandle &parent) const;
        int handleIndex(QString &err, const NHandle &node, int &index) const;
        int removeNodeHandle(QString &err, const NHandle &node);

        int checkNValid(QString &err, const NHandle &node, NHandle::Type type) const;
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
        ChaptersItem(NovelHost&host, const FStruct::NHandle &refer, bool isGroup=false);
        virtual ~ChaptersItem() override = default;

        const FStruct::NHandle getRefer() const;
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

        const FStruct::NHandle getRefer() const;
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
    int appendForeshadow(QString &err, const QModelIndex &kIndex, const QString &fName,
                         const QString &desp, const QString &desp_next);
    /**
     * @brief 在指定关键剧情下添加伏笔驻点
     * @param err
     * @param kIndex 关键剧情节点
     * @param vKey 卷宗键名
     * @param kKey 关键剧情键名
     * @param fKey 伏笔键名
     * @return
     */
    int appendShadowstop(QString &err, const QModelIndex &kIndex, const QString &vKey,
                         const QString &kKey, const QString &fKey);
    /**
     * @brief 删除任何大纲节点
     * @param errOut
     * @param nodeIndex 大纲节点
     * @return
     */
    int removeOutlineNode(QString &err, const QModelIndex &outlineNode);
    /**
     * @brief 设置指定大纲节点为当前节点，引起相应视图变化
     * @param err
     * @param outlineNode 大纲节点
     * @return
     */
    int setCurrentOutlineNode(QString &err, const QModelIndex &outlineNode);

    int get_foreshadows_under_keystory(QString &err, const NovelBase::FStruct::NHandle keystoryNode,
                                       QList<NovelBase::FStruct::NHandle> &resultSum) const;
    int get_foreshadows_until_this(QString &err, const NovelBase::FStruct::NHandle keystoryOrVolumeNode,
                                   QList<NovelBase::FStruct::NHandle> &resultSum) const;
    int get_shadowstops_under_keystory(QString &err, const NovelBase::FStruct::NHandle keystoryNode,
                                       QList<NovelBase::FStruct::NHandle> &resultSum) const;
    int get_shadowstops_until_this(QString &err, const NovelBase::FStruct::NHandle keystoryOrVolumeNode,
                                   QList<NovelBase::FStruct::NHandle> &resultSum) const;

    /**
     * @brief 获取当前大纲树节点
     * @return
     */
    NovelBase::OutlinesItem *currentOutlineNode() const;

    QStandardItemModel* foreshadowsPresent() const;

    // 章卷节点
    QStandardItemModel *navigateTree() const;
    /**
     * @brief 在指定关键剧情下添加章节
     * @param err
     * @param kIndex 关键剧情节点
     * @param aName
     * @return
     */
    int appendChapter(QString &err, const QModelIndex &outlineKeystoryIndex, const QString& aName);
    int removeChaptersNode(QString &err, const QModelIndex &chaptersNode);
    void refreshWordsCount();

    // 搜索功能
    QStandardItemModel *searchResultPresent() const;
    void searchText(const QString& text);

    // 关键剧情分解点
    QStandardItemModel *keystoryPointsPresent() const;

    // 剧情梗概*3
    QTextDocument *novelDescriptionPresent() const;
    QTextDocument *volumeDescriptionPresent() const;
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


signals:
    void documentOpened(QTextDocument *doc, const QString &title);
    void documentActived(QTextDocument *doc, const QString &title);
    void documentAboutToBeClosed(QTextDocument *doc);


private:
    ConfigHost &config_host;
    NovelBase::FStruct *desp_node;
    NovelBase::OutlinesItem *current_outline_node;

    QStandardItemModel *const outline_tree_model;
    QStandardItemModel *const foreshadows_present;

    QStandardItemModel *const result_enter_model;
    QStandardItemModel *const chapters_navigate_model;
    QStandardItemModel *const keystory_points_model;

    QTextDocument *const novel_description_present;
    QTextDocument *const volume_description_present;
    QTextDocument *const node_description_present;

    QHash<NovelBase::ChaptersItem*,QPair<QTextDocument*, NovelBase::KeywordsRender*>> opening_documents;

    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *> insert_volume(const NovelBase::FStruct::NHandle &item, int index);

    void chapters_navigate_title_midify(QStandardItem *item);
    int set_current_outline_node(QString &err, NovelBase::OutlinesItem *node);
    /**
     * @brief 通过指定节点设置当前卷节点大纲文档
     * @param err
     * @param node
     * @return
     */
    int set_current_volume_node(QString &err, const NovelBase::OutlinesItem *node);
    int get_current_volume_node(QString &err, const NovelBase::OutlinesItem **node) const ;

    void resetNovelDescription();
    void resetCurrentVolumeDescription()
    {
        auto text = volume_description_present->toPlainText();
        const OutlinesItem *node;
        QString err;
        get_current_volume_node(err, &node);

        auto nn = node->getRefer();
        nn.setAttr("desp", text);
    }
    void resetCurrentOutlineNodeDescription()
    {
        auto text = novel_description_present->toPlainText();
        const OutlinesItem *node;
        QString err;
        get_current_volume_node(err, &node);

        if(currentOutlineNode() == node){
            volume_description_present->setPlainText(text);
        }
        else {
            auto nn = current_outline_node->getRefer();
            nn.setAttr("desp", text);
        }
    }

};

#endif // NOVELHOST_H
