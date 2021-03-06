#ifndef NOVELHOST_H
#define NOVELHOST_H

#include "confighost.h"
#include "dbaccess.h"

#include <QItemDelegate>
#include <QRandomGenerator>
#include <QRunnable>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>


class NovelHost;

namespace NovelBase {

    class ChaptersItem : public QObject, public QStandardItem
    {
        Q_OBJECT

    public:
        ChaptersItem(NovelHost&host, const DBAccess::StoryTreeNode &refer, bool isGroup=false);
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
        OutlinesItem(const DBAccess::StoryTreeNode &refer);
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
        WordsRender(QTextDocument *target, NovelHost &novel_host);
        virtual ~WordsRender() override;

        ConfigHost &configBase() const;

        void acceptRenderResult(const QString &content, const QList<std::tuple<QString, int, QTextCharFormat, int, int> > &rst);
        // QSyntaxHighlighter interface
    protected:
        virtual void highlightBlock(const QString &text) override;

    private:
        QMutex mutex;
        NovelHost &novel_host;
        //                      format  :   keyword-id  : start : length
        QHash<QString, QList<std::tuple<QString, int, QTextCharFormat, int, int>>> _result_store;

        bool _check_extract_render_result(const QString &text, QList<std::tuple<QString, int, QTextCharFormat, int, int> > &rst);
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

        void keywords_highlighter_render(const QString &text, QList<std::tuple<QString, int, QString> > words, const QTextCharFormat &format,
                              QList<std::tuple<QString, int, QTextCharFormat, int, int> > &rst) const;

    };
    class WsBlockData : public QTextBlockUserData
    {
    public:
        using Type = DBAccess::StoryTreeNode::Type;
        WsBlockData(const QModelIndex &target, Type blockType);
        virtual ~WsBlockData() = default;

        bool operator==(const WsBlockData &other) const;

        QModelIndex navigateIndex() const;
        Type blockType() const;

    private:
        const QModelIndex outline_index;   // ?????????????????????
        Type block_type;
    };

    class DesplineFilterModel : public QSortFilterProxyModel
    {
    public:
        enum Type{
            UNDERVOLUME,
            UNTILWITHVOLUME,
            UNTILWITHCHAPTER
        };

        explicit DesplineFilterModel(Type operateType, QObject*parent=nullptr);

        void setFilterBase(const DBAccess::StoryTreeNode &volume_node, const DBAccess::StoryTreeNode
                           &chapter_node = DBAccess::StoryTreeNode());

        // QSortFilterProxyModel interface
    protected:
        virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

    private:
        Type operate_type_store;
        int volume_filter_index;
        QVariant chapter_filter_id;
    };
}

class NovelHost : public QObject
{
    Q_OBJECT
    using KfvType = NovelBase::DBAccess::KeywordField::ValueType;

public:
    explicit NovelHost(ConfigHost &config);
    virtual ~NovelHost() override;

    /**
     * @brief ????????????????????????
     * @param desp ??????????????????
     */
    void loadBase(NovelBase::DBAccess *desp);
    void save();

    QString novelTitle() const;
    void resetNovelTitle(const QString &title);




    /**
     * @brief ???????????????????????????
     * @return
     */
    QAbstractItemModel *keywordsTypeslistModel() const;
    /**
     * @brief ?????????????????????????????????
     * @param name
     * @return
     */
    QAbstractItemModel *appendKeywordsModelToTheList(const QString &name);
    /**
     * @brief ??????????????????ModelIndex????????????????????????????????????
     * @param mindex ???????????????ModelIndex
     * @return
     */
    QAbstractItemModel *keywordsModelViaTheList(const QModelIndex &mindex) const;
    /**
     * @brief ?????????????????????ModelIndex????????????????????????
     * @param mindex
     */
    void removeKeywordsModelViaTheList(const QModelIndex &mindex);

    void keywordsTypeForward(const QModelIndex &mindex);
    void keywordsTypeBackward(const QModelIndex &mindex);

    void getAllKeywordsTableRefs(QList<QPair<QString, QString>> &name_ref_list) const;

    QList<QPair<int,std::tuple<QString, QString, KfvType>>>
    customedFieldsListViaTheList(const QModelIndex &mindex) const;
    void renameKeywordsTypenameViaTheList(const QModelIndex &mindex, const QString &newName);
    void adjustKeywordsFieldsViaTheList(const QModelIndex &mindex, const QList<QPair<int, std::tuple<QString,
                                         QString, KfvType>>> fields_defines);

    void appendNewItemViaTheList(const QModelIndex &mindex, const QString &name);
    void removeTargetItemViaTheList(const QModelIndex &mindex, const QModelIndex &tIndex);

    void queryKeywordsViaTheList(const QModelIndex &mindex, const QString &itemName) const;

    QList<QPair<int, QString>> avaliableEnumsForIndex(const QModelIndex &index) const;
    QList<QPair<int, QString>> avaliableItemsForIndex(const QModelIndex &index) const;


    QAbstractItemModel *quicklookItemsModel() const;



    // ??????????????????
    /**
     * @brief ?????????????????????????????????????????????????????????
     * @return
     */
    QStandardItemModel *outlineNavigateTree() const;
    /**
     * @brief ??????????????????????????????
     * @return
     */
    QTextDocument *novelOutlinesPresent() const;
    /**
     * @brief ???????????????????????????
     * @return
     */
    QTextDocument *volumeOutlinesPresent() const;
    /**
     * @brief ?????????????????????????????????
     * @return
     */
    QAbstractItemModel *desplinesUnderVolume() const;
    /**
     * @brief ?????????????????????????????????
     * @return
     */
    QAbstractItemModel *desplinesUntilVolumeRemain() const;
    /**
     * @brief ?????????????????????????????????
     * @return
     */
    QAbstractItemModel *desplinesUntilChapterRemain() const;
    // ????????????
    /**
     * @brief ??????????????????
     * @return
     */
    QAbstractItemModel *chaptersNavigateTree() const;
    /**
     * @brief ??????????????????
     * @return
     */
    QTextDocument *chapterOutlinePresent() const;
    /**
     * @brief ??????????????????
     * @return
     */
    QStandardItemModel *findResultTable() const;









    /**
     * @brief ??????????????????
     * @param name ????????????
     * @param description ????????????
     * @param index ???????????????-1????????????
     * @return
     */
    void insertVolume(const QString& name, const QString &description, int index=-1);
    /**
     * @brief ????????????????????????????????????????????????
     * @param vmIndex
     * @param kName
     * @param description
     * @param index ???????????????-1????????????
     */
    void insertStoryblock(const QModelIndex &pIndex, const QString &name, const QString &description, int index=-1);
    /**
     * @brief ?????????????????????????????????????????????
     * @param pIndex
     * @param name
     * @param description
     * @param index ???????????????-1????????????
     */
    void insertKeypoint(const QModelIndex &pIndex, const QString &name, const QString description, int index=-1);

    /**
     * @brief ????????????????????????
     * @param nodeIndex ????????????
     * @return
     */
    void removeOutlinesNode(const QModelIndex &outlineNode);

    /**
     * @brief ??????????????????????????????????????????????????????????????????
     * @param err
     * @param outlineNode ????????????
     * @return
     */
    void setCurrentOutlineNode(const QModelIndex &outlineNode);

    void checkOutlinesRemoveEffect(const QModelIndex &outlinesIndex, QList<QString> &msgList) const;





    /**
     * @brief ??????????????????????????????
     * @param pIndex
     * @param name
     * @param description
     * @param index ???????????????-1????????????
     */
    void insertChapter(const QModelIndex &pIndex, const QString &name, const QString &description, int index=-1);

    void removeChaptersNode(const QModelIndex &chaptersNode);

    void setCurrentChaptersNode(const QModelIndex &chaptersNode);

    void checkChaptersRemoveEffect(const QModelIndex &chpsIndex, QList<QString> &msgList) const;





    void appendDesplineUnder(const QModelIndex &anyVolumeIndex, const QString &name, const QString &description);
    void appendDesplineUnderCurrentVolume(const QString &name, const QString &description);
    /**
     * @brief ???????????????????????????????????????index=-1????????????
     * @param desplineID
     * @param title
     * @param desp
     * @param index ???????????????-1????????????
     */
    void insertAttachpoint(int desplineID, const QString &title, const QString &desp, int index=-1);
    void removeDespline(int desplineID);
    void removeAttachpoint(int attachpointID);

    void attachPointMoveup(const QModelIndex &desplineIndex);
    void attachPointMovedown(const QModelIndex &desplineIndex);

    void allStoryblocksWithIDUnderCurrentVolume(QList<QPair<QString, int> > &storyblocks) const;
    /**
     * @brief ?????????????????????????????????????????????
     * @param node
     * @param desplines
     * @return
     */
    void sumDesplinesUntilVolume(const QModelIndex &node, QList<QPair<QString, int>> &desplines) const;
    /**
     * @brief ????????????????????????????????????
     * @param foreshadowsList   title,fullpath
     */
    void sumPointWithChapterSuspend(int desplineID, QList<QPair<QString, int>> &suspendPoints) const;
    void sumPointWithChapterAttached(const QModelIndex &chapterIndex, int desplineID, QList<QPair<QString, int>> &attachedPoints) const;
    void chapterAttachSet(const QModelIndex &chapterIndex, int pointID);
    void chapterAttachClear(int pointID);

    void sumPointWithStoryblcokSuspend(int desplineID, QList<QPair<QString, int>> &suspendPoints) const;
    void sumPointWithStoryblockAttached(const QModelIndex &outlinesIndex, int desplineID, QList<QPair<QString, int>> &attachedPoints) const;
    void storyblockAttachSet(const QModelIndex &outlinesIndex, int pointID);
    void storyblockAttachClear(int pointID);


    void checkDesplineRemoveEffect(int fsid, QList<QString> &msgList) const;




    void searchText(const QString& text);




    void pushToQuickLook(const QTextBlock &block, const QList<QPair<QString,int>> &mixtureList);

    int indexDepth(const QModelIndex &node) const;
    void refreshWordsCount();
    QString chapterActiveText(const QModelIndex& index);
    int calcValidWordsCount(const QString &content);

    void refreshDesplinesSummary();
    ConfigHost &getConfigHost() const;

    void testMethod();

    void appendActiveTask(const QString &taskMask, int number=1);
    void finishActiveTask(const QString &taskMask, const QString &finalTip, int number=1);

signals:
    void documentAboutToBoClosed(QTextDocument *doc);
    void documentPrepared(QTextDocument *doc, const QString &title);

    void messagePopup(const QString &title, const QString &message);
    void warningPopup(const QString &title, const QString &message);
    void errorPopup(const QString &title, const QString &message);

    void taskAppended(const QString &taskType, int number);
    void taskFinished(const QString &taskType, const QString &finalTip, int number);

    void currentChaptersActived();
    void currentVolumeActived();

private:
    ConfigHost &config_host;
    NovelBase::DBAccess *desp_ins;

    QStandardItemModel *const outline_navigate_treemodel;
    QTextDocument *const novel_outlines_present;
    QTextDocument *const volume_outlines_present;

    QStandardItemModel *const chapters_navigate_treemodel;
    QTextDocument *const chapter_outlines_present;

    QStandardItemModel *const desplines_fuse_source_model;
    NovelBase::DesplineFilterModel *const desplines_filter_under_volume;
    NovelBase::DesplineFilterModel *const desplines_filter_until_volume_remain;
    NovelBase::DesplineFilterModel *const desplines_filter_until_chapter_remain;

    QStandardItemModel *const find_results_model;

    // ??????????????????????????????anchor:<doc*,randerer*[nullable]>
    QHash<NovelBase::ChaptersItem*,QPair<QTextDocument*, NovelBase::WordsRender*>> all_documents;
    NovelBase::DBAccess::StoryTreeNode current_volume_node;
    NovelBase::DBAccess::StoryTreeNode current_chapter_node;
    QTextBlock  current_editing_textblock;
    void acceptEditingTextblock(const QTextCursor &cursor);


    QStandardItemModel *const keywords_types_configmodel;
    QList<QPair<NovelBase::DBAccess::KeywordField, QStandardItemModel*>> keywords_manager_group;
    void _load_all_keywords_types_only_once();

    QStandardItemModel *const quicklook_backend_model;

    /**
     * @brief ???chapters-tree???outline-tree?????????????????????
     * @param volume_handle
     * @param index
     * @return ???????????????????????????????????????????????????
     */
    QPair<NovelBase::OutlinesItem *, NovelBase::ChaptersItem *>
    insert_volume(const NovelBase::DBAccess::StoryTreeNode &volume_handle, int index);

    void listen_novel_description_change();
    void listen_volume_outlines_description_change(int pos, int removed, int added);
    bool check_volume_structure_diff(const NovelBase::OutlinesItem *base_node, QTextBlock &blk) const;
    void listen_volume_outlines_structure_changed();
    void listen_chapter_outlines_description_change();
    void outlines_node_title_changed(QStandardItem *item);
    void chapters_node_title_changed(QStandardItem *item);
    void _listen_basic_datamodel_changed(QStandardItem *item);

    void set_current_volume_outlines(const NovelBase::DBAccess::StoryTreeNode &node_under_volume);
    void set_current_chapter_content(const QModelIndex &chaptersNode, const NovelBase::DBAccess::StoryTreeNode &node);
    void insert_description_at_volume_outlines_doc(QTextCursor cursor, NovelBase::OutlinesItem *outline_node);

    NovelBase::DBAccess::StoryTreeNode _locate_outline_handle_via(QStandardItem *outline_item) const;
    void _check_remove_effect(const NovelBase::DBAccess::StoryTreeNode &target, QList<QString> &msgList) const;

    QTextDocument* load_chapter_text_content(QStandardItem* chpAnchor);

    QModelIndex get_table_presentindex_via_typelist_model(const QModelIndex &mindex) const;
    int extract_tableid_from_the_typelist_model(const QModelIndex &mindex) const;
};

#endif // NOVELHOST_H
