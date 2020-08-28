#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "common.h"
#include "novelhost.h"

#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>

class MainFrame;

namespace WidgetBase {
    class CQTextEdit : public QTextEdit
    {
    public:
        CQTextEdit(ConfigHost &config, QWidget *parent=nullptr);

        // QTextEdit interface
    protected:
        virtual void insertFromMimeData(const QMimeData *source) override;

    private:
        ConfigHost &host;
    };

    class StoryblockRedirect : public QStyledItemDelegate
    {
    public:
        StoryblockRedirect(NovelHost *const host);

        // QAbstractItemDelegate interface
    public:
        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;

    private:
        NovelHost *const host;
    };

    class FieldsAdjustDialog : public QDialog
    {
        using KfvType = NovelBase::DBAccess::KeywordField::ValueType;
    public:
        // vtype/field-name/supply-string/peer-field
        FieldsAdjustDialog(const QList<QPair<int, std::tuple<QString, QString, KfvType>>> &base, const NovelHost *host);
        virtual ~FieldsAdjustDialog() = default;

        void extractFieldsDefine(QList<QPair<int, std::tuple<QString, QString, KfvType>>> &result) const;

    private:
        const NovelHost *const host;
        const QList<QPair<int, std::tuple<QString, QString, KfvType>>> base;
        QTableView *const view;
        QStandardItemModel *const model;
        QPushButton *const appendItem, *const removeItem, *const itemMoveUp,
        *const itemMoveDown, *const accept_action, *const reject_action;

        void append_field();
        void remove_field();
        void item_moveup();
        void item_movedown();
    };

    class ValueTypeDelegate : public QStyledItemDelegate
    {
    public:
        ValueTypeDelegate(const NovelHost *host, QObject *object);
        virtual ~ValueTypeDelegate() = default;

        // QAbstractItemDelegate interface
    public:
        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;

    private:
        const NovelHost *const host;
    };

    class ValueAssignDelegate : public QStyledItemDelegate
    {
    public:
        ValueAssignDelegate(const NovelHost *host, QObject *object);
        virtual ~ValueAssignDelegate() = default;

        // QAbstractItemDelegate interface
    public:
        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const override;
        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    private:
        const NovelHost *const host;
    };

    class ViewFrame
    {
    public:
        enum class FrameType{
            VIEWSPLITTER,
            VIEWSELECTOR
        };

        virtual FrameType viewType() const = 0;
    };
    class ViewSplitter : public QSplitter, public ViewFrame
    {
        Q_OBJECT
    public:
        ViewSplitter(Qt::Orientation ori, QWidget *parent);
        virtual ~ViewSplitter() = default;

        FrameType viewType() const;

    signals:
        void splitterPositionMoved(ViewSplitter*poster, int index, int pos);
    };
    class ViewSelector : public QWidget, public ViewFrame
    {
        Q_OBJECT

    public:
        ViewSelector(MainFrame *parent = 0);
        ~ViewSelector();

        void setCurrentView(const QString &name, QWidget *view);
        QString currentViewName() const;
        void clearAllViews(bool titleClear=true);

        FrameType viewType() const;

    public slots:
        void acceptItemInsert(const QString &name);
        void acceptItemRemove(const QString &name);

    signals:
        void splitLeft(ViewSelector *poster);
        void splitRight(ViewSelector *poster);
        void splitTop(ViewSelector *poster);
        void splitBottom(ViewSelector *poster);
        void splitCancel(ViewSelector *poster);

        void currentViewItemChanged(ViewSelector *poster, const QString &name);

    private:
        const MainFrame *const view_controller;
        QComboBox *const view_select;
        QPushButton *const split_view;
        QPushButton *const close_action;
        QWidget *const place_holder;

        void view_split_operate();
        void view_close_operate();
    };
}


class MainFrame : public QMainWindow
{
    Q_OBJECT

public:
    MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent = nullptr);
    ~MainFrame();

signals:
    void itemNameAvaliable(const QString &name);
    void itemNameUnavaliable(const QString &name);

private:
    // name : content-view : select-view
    QList<std::tuple<QString, QWidget*, WidgetBase::ViewSelector*>> views_group;
    QTimer *const timer_autosave;
    NovelHost *const novel_core;
    ConfigHost &config;
    QTabWidget *const mode_uibase;

    // 可用视图组合视图
    QWidget *get_view_according_name(const QString &name) const;
    void load_all_predefine_views();
    QWidget *group_chapters_navigate_view(QAbstractItemModel *model);
    QWidget *group_storyblks_navigate_view(QAbstractItemModel *model);
    QWidget *group_desplines_manage_view(QAbstractItemModel *model);
    QWidget *group_keywords_manager_view(NovelHost *novel_core);
    QWidget *group_keywords_quicklook_view(QAbstractItemModel *model);
    QWidget *group_search_result_summary_panel();
    WidgetBase::CQTextEdit *group_textedit_view(QTextDocument *doc);


    // 基本通知机制
    void acceptMessage(const QString &title, const QString &message);
    void acceptWarning(const QString &title, const QString &message);
    void acceptError(const QString &title, const QString &message);

    // 接收splitter调整信息
    void acceptSplitterPostionChanged(WidgetBase::ViewSplitter*poster, int, int);
    // 切分操作
    void acceptSplitLeftRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitRightRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitTopRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitBottomRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitCancelRequest(WidgetBase::ViewSelector *poster);

    // 接收视图切换
    void acceptViewmItemChanged(WidgetBase::ViewSelector *poster, const QString &name);


    void disconnect_signals_and_clear_views_present(int page_index, WidgetBase::ViewFrame *root_start) const;
    void connect_signals_and_set_views_present(int page_index, WidgetBase::ViewFrame *root_start) const;

    WidgetBase::ViewSplitter *find_parent_splitter(WidgetBase::ViewSplitter *parent, WidgetBase::ViewFrame *view) const;
    WidgetBase::ViewFrame *findRootFrameWithinModeSwitch(int index) const;

    // 模式管理和模式视图配置载入
    void build_new_mode_page();
    void close_target_mode_page(int index);
    void frames_views_config_load(int index);

    // 迭代构建模式全部框架
    void create_all_frames_order_by_config();
    void _create_element_frame_recursive(WidgetBase::ViewSplitter *psplitter, ConfigHost::ViewConfig &pconfig);

    // 迭代获取对应模式配置项
    ConfigHost::ViewConfig find_frame_opposite_config(int mode_index, WidgetBase::ViewFrame *target_view) const;
    ConfigHost::ViewConfig _find_element_opposite_config(WidgetBase::ViewSplitter *p_splitter, ConfigHost::ViewConfig &p_config,
                                                         WidgetBase::ViewFrame *target_view) const;

    // 全局范围功能函数
    void rename_novel_title();
    void resize_foreshadows_tableitem_width();

    // 正文编辑界面
    void chapters_navigate_jump(const QModelIndex &index);
    void show_chapters_operate(const QPoint &point);
    void append_volume();
    void insert_volume();
    void append_chapter();
    void insert_chapter();
    void append_despline_from_chapters();
    void pointattach_from_chapter(QAction *item);
    void pointclear_from_chapter(QAction *item);
    void remove_selected_chapters();
    void content_output();

    // 大纲编辑界面
    void outlines_navigate_jump(const QModelIndex &index);
    void outlines_manipulation(const QPoint &point);
    void insert_volume2();
    void append_storyblock();
    void insert_storyblock();
    void append_keypoint();
    void insert_keypoint();
    void append_despline_from_outlines();
    void pointattach_from_storyblock(QAction *item);
    void pointclear_from_storyblock(QAction *item);
    void remove_selected_outlines();

    void saveOp();
    void autosave_timespan_reset();

    void documentClosed(QTextDocument *);
    void documentPresent(QTextDocument *doc, const QString &title);
    void currentChaptersAboutPresent();
    void currentVolumeOutlinesPresent();

    // 支线剧情管理与显示界面
    void append_despline_from_desplineview();
    void remove_despline_from_desplineview(QTreeView *view);
    void append_attachpoint_from_desplineview(QTreeView *view);
    void insert_attachpoint_from_desplineview(QTreeView *widget);
    void remove_attachpoint_from_desplineview(QTreeView *widget);
    void attachpoint_moveup(QTreeView *view);
    void attachpoint_movedown(QTreeView *widget);
    void refresh_desplineview(QTreeView *view = nullptr);

    int getDescription(const QString &title, QString &nameOut, QString &descriptionOut);
    QList<QPair<int, int> > extractPositionData(const QModelIndex &index) const;
    void scrollToSamePosition(QAbstractItemView *view, const QList<QPair<int, int> > &poslist) const;

};

#endif // MAINFRAME_H















