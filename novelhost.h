#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"
#include "dataaccess.h"

#include <QDomDocument>
#include <QItemDelegate>
#include <QRandomGenerator>
#include <QRunnable>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>


class NovelHost;

namespace NovelBase {
    class ChaptersItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        ChaptersItem(NovelHost&host, const DataAccess::TreeNode &refer, bool isGroup=false);
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
        OutlinesItem(const DataAccess::TreeNode &refer);
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

        bool _check_extract_render_result(const QString &text, QList<std::tuple<QTextCharFormat, QString, int, int>> &rst);
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

        void _highlighter_render(const QString &text, QList<QString> words, const QTextCharFormat &format,
                              QList<std::tuple<QTextCharFormat, QString, int, int>> &rst) const;
    };

    class WsBlockData : public QTextBlockUserData
    {
    public:
        using Type = _X_FStruct::NHandle::Type;
        WsBlockData(const QModelIndex &target, _X_FStruct::NHandle::Type blockType);
        virtual ~WsBlockData() = default;

        bool operator==(const WsBlockData &other) const;

        QModelIndex outlineTarget() const;
        Type blockType() const;

    private:
        const QModelIndex outline_index;   // 指向大纲树节点
        Type block_type;
    };

    class ForeshadowRedirectDelegate : public QItemDelegate
    {
    public:
        ForeshadowRedirectDelegate(NovelHost *const host);

        // QAbstractItemDelegate interface
    public:
        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;

    private:
        NovelHost *const host;
    };
}

class NovelHost : public QObject
{
    Q_OBJECT

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    void convert20_21(const QString &validPath);

    void loadDescription(NovelBase::DataAccess *desp);
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
    void allKeystoriesUnderCurrentVolume(QList<QPair<QString, int> > &keystories) const;



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
    void checkForeshadowRemoveEffect(const QString &pathString, QList<QString> &msgList) const;
    void removeChaptersNode(const QModelIndex &chaptersNode);
    void removeForeshadowNode(const QString &keysPath);
    void setCurrentChaptersNode(const QModelIndex &chaptersNode);
    void refreshWordsCount();
    /**
     * @brief 汇聚本卷下所走伏笔[故事线]
     * @param chpsNode
     * @param foreshadows
     */
    NovelBase::DataAccess::TreeNode sumForeshadowsUnderVolumeAll(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
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
    void sumForeshadowsAbsorbedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
    void sumForeshadowsOpeningUntilChapter(const QModelIndex &chpsNode, QList<QPair<QString, QString>> &foreshadows) const;
    void sumForeshadowsClosedAtChapter(const QModelIndex &chpsNode, QList<QPair<QString,QString>> &foreshadows) const;


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
    NovelBase::DataAccess *desp_ins;

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
    NovelBase::DataAccess::TreeNode current_volume_node;
    NovelBase::DataAccess::TreeNode current_chapter_node;

    /**
     * @brief 向chapters-tree和outline-tree上插入卷宗节点
     * @param item
     * @param index
     * @return
     */
    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *>
    insert_volume(const NovelBase::DataAccess::TreeNode &volume_handle, int index);

    void listen_novel_description_change();
    void listen_volume_outlines_description_change(int pos, int removed, int added);
    bool check_volume_structure_diff(const NovelBase::OutlinesItem *base_node, QTextBlock &blk) const;
    void listen_volume_outlines_structure_changed();
    void listen_chapter_outlines_description_change();
    void outlines_node_title_changed(QStandardItem *item);
    void chapters_node_title_changed(QStandardItem *item);

    void set_current_volume_outlines(const NovelBase::DataAccess::TreeNode &node_under_volume);
    void insert_content_at_document(QTextCursor cursor, NovelBase::OutlinesItem *outline_node);

    void sum_foreshadows_under_volume(const NovelBase::DataAccess::TreeNode &volume_node);
    void sum_foreshadows_until_volume_remains(const NovelBase::DataAccess::TreeNode &volume_node);
    void sum_foreshadows_until_chapter_remains(const NovelBase::DataAccess::TreeNode &chapter_node);
    void listen_foreshadows_volume_changed(QStandardItem *item);
    void listen_foreshadows_until_volume_changed(QStandardItem *item);
    void listen_foreshadows_until_chapter_changed(QStandardItem *item);

    NovelBase::DataAccess::TreeNode _locate_outline_handle_via(QStandardItem *outline_item) const;
    void _check_remove_effect(const NovelBase::DataAccess::TreeNode &target, QList<QString> &msgList) const;

    QTextDocument* _load_chapter_text_content(QStandardItem* chpAnchor);
};

#endif // NOVELHOST_H
