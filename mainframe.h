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
    public:
        // vtype/field-name/supply-string/peer-field
        FieldsAdjustDialog(const QList<QPair<int, std::tuple<QString, QString,
                           NovelBase::DBAccess::KeywordField::ValueType>>> &base, const NovelHost *host);
        virtual ~FieldsAdjustDialog() = default;

        void extractFieldsDefine(QList<QPair<int, std::tuple<QString, QString,
                                 NovelBase::DBAccess::KeywordField::ValueType>>> &result) const;

    private:
        const NovelHost *const host;
        const QList<QPair<int, std::tuple<QString, QString, NovelBase::DBAccess::KeywordField::ValueType>>> base;
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
            VIEW_SPLITTER,
            VIEW_SELECTOR
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

        void setCurrentView(const QString &name);
        QString currentViewName() const;
        bool isFocusLocked() const;

        FrameType viewType() const;

    public slots:
        void acceptItemInsert(const QString &name);
        void acceptItemRemove(const QString &name);
        void setFocusLock(bool state);
        void activePresentView();

    signals:
        void focusLockReport(ViewSelector *poster, bool focusSet);

        void splitLeft(ViewSelector *poster);
        void splitRight(ViewSelector *poster);
        void splitTop(ViewSelector *poster);
        void splitBottom(ViewSelector *poster);

        void splitCancel(ViewSelector *poster);
        void closeViewRequest(ViewSelector *poster);

        void currentViewItemChanged(ViewSelector *poster, const QString &name);
    private:
        MainFrame *const view_controller;
        QComboBox *const view_select;
        QPushButton *const lock_focus;
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

    QWidget *locateAndViewlockChangeViaTitlename(WidgetBase::ViewSelector *poster, const QString &name);

signals:
    void itemNameAvaliable(const QString &name);
    void itemNameUnavaliable(const QString &name);

private:
    QWidget *get_view_according_name(const QString &name) const;

    void load_all_predefine_views();
    // name : content-view : select-view
    QList<std::tuple<QString, QWidget*, WidgetBase::ViewSelector*>> views_group;

    QWidget *group_chapters_navigate_view(QAbstractItemModel *model);
    QWidget *group_storyblks_navigate_view(QAbstractItemModel *model);
    QWidget *group_desplines_manage_view(QAbstractItemModel *model);
    QWidget *group_keywords_manager_view(NovelHost *novel_core);
    QWidget *group_keywords_quicklook_view(QAbstractItemModel *model);
    QWidget *group_search_result_summary_panel();

    WidgetBase::CQTextEdit *group_textedit_panel(QTextDocument *doc);

    /**
     * @brief 获取当前取得焦点的文本编辑视图
     * @return
     */
    QTextEdit *get_active_textedit_view() const;


    QTimer *const timer_autosave;
    NovelHost *const novel_core;
    ConfigHost &config;

    WidgetBase::CQTextEdit *active_text_edit_view;

    QMenu *const file;
    QMenu *const func;

    void acceptMessage(const QString &title, const QString &message);
    void acceptWarning(const QString &title, const QString &message);
    void acceptError(const QString &title, const QString &message);

    void acceptSplitterPostionChanged(WidgetBase::ViewSplitter*poster, int index, int pos);

    void acceptSplitLeftRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitRightRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitTopRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitBottomRequest(WidgetBase::ViewSelector *poster);
    void acceptSplitCancelRequest(WidgetBase::ViewSelector *poster);

    void acceptFocusLockRequest(WidgetBase::ViewSelector *poster, bool);
    void acceptViewCloseRequest(WidgetBase::ViewSelector *poster);
    void acceptViewmItemChanged(WidgetBase::ViewSelector *poster, const QString &name);

    void connect_all_frame_before_present(WidgetBase::ViewFrame *root) const;
    void disconnect_all_frame_before_hidden(WidgetBase::ViewFrame *root) const;

    WidgetBase::ViewSplitter *find_parent_splitter(WidgetBase::ViewSplitter *parent, WidgetBase::ViewFrame *view) const;

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

    void convert20_21();

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

    void _structure_output(WidgetBase::ViewFrame *base=nullptr, int deepth=0) const;
};

#endif // MAINFRAME_H















