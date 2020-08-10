#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "novelhost.h"

#include <QDialog>
#include <QGridLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>


class MainFrame : public QMainWindow
{
    Q_OBJECT

public:
    MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent = nullptr);
    ~MainFrame();

private:
    QTimer *const timer_autosave;
    NovelHost *const novel_core;
    ConfigHost &config;
    QSplitter *const functions_split_base;                  // 主要区域分割，左边导航区域【故事结构树，卷章结构树，搜索结果】，
    // 大纲编辑界面                                           // 右边整卷细纲详细，伏笔汇总，章节细纲编写
    QTreeView *const outlines_navigate_treeview;
    // 文本编辑界面
    QTextEdit *const volume_outlines_present;               // 卷宗详细描述大纲
    QTreeView *const chapters_navigate_view;                // 卷宗章节打开
    QTableView *const search_result_navigate_view;          // 查询结果导航
    QLineEdit *const search_text_enter;
    QPushButton *const search, *const clear;
    QTextEdit *const chapter_textedit_present;              // 章节内容编辑
    QTextEdit *const chapter_outlines_present;              // 章节细纲编辑1
    QTextDocument *const empty_document;
    QTabWidget *const desplines_stack;
    QTreeView *const desplines_under_volume_view;           // 卷内伏笔汇集
    QTreeView *const desplines_remains_until_volume_view;
    QTreeView *const desplines_remains_until_chapter_view;
    QTextEdit *const novel_outlines_present;                // 作品整体描述大纲

    QMenu *const file;
    QMenu *const func;

    // 全局范围功能函数
    void acceptMessage(const QString &title, const QString &message);
    void acceptWarning(const QString &title, const QString &message);
    void acceptError(const QString &title, const QString &message);
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

    // 全局搜索界面
    void search_text();
    void clear_search_result();
    void search_jump(const QModelIndex &index);

    void saveOp();
    void autosave_timespan_reset();

    void documentClosed(QTextDocument *);
    void documentPresent(QTextDocument *doc, const QString &title);
    void currentChaptersAboutPresent();
    void currentVolumeOutlinesPresent();

    void convert20_21();

    // 支线剧情管理与显示界面
    void show_despline_operate(const QPoint &point);
    void append_despline_from_desplineview();
    void remove_despline_from_desplineview();
    void append_attachpoint_from_desplineview();
    void insert_attachpoint_from_desplineview();
    void remove_attachpoint_from_desplineview();
    void attachpoint_moveup();
    void attachpoint_movedown();
    void refresh_desplineview();

    QList<QPair<int, int> > extractPositionData(const QModelIndex &index) const;
    void scrollToSamePosition(QAbstractItemView *view, const QList<QPair<int, int> > &poslist) const;

    QWidget *groupManagerPanel(QAbstractItemModel *model, int table_id);

    int getDescription(const QString &title, QString &nameOut, QString &descriptionOut);
};

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

    class StoryblockRedirect : public QItemDelegate
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

    class ValueTypeDelegate : public QItemDelegate
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

    class ValueAssignDelegate : public QItemDelegate
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
}
#endif // MAINFRAME_H















