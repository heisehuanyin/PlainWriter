#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "novelhost.h"

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
    MainFrame(NovelHost *core, QWidget *parent = nullptr);
    ~MainFrame();

private:
    QTimer *const timer_autosave;
    NovelHost *const novel_core;
    QSplitter *const split_panel;
    QTreeView *const node_navigate_view;
    QTableView *const search_result_view;
    QLineEdit *const search_text_enter;
    QPushButton *const search, *const clear;
    QTabWidget *const edit_blocks_stack;

    QMenu *const file;
    QMenu *const func;

    void navigate_jump(const QModelIndex &index);
    void show_manipulation(const QPoint &point);
    void append_volume();
    void append_chapter();
    void remove_selected();
    void content_output();

    void search_text();
    void clear_search_result();
    void search_jump(const QModelIndex &index);

    void saveOp();
    void autosave_timespan_reset();

    void tabCloseRequest(int index)
    {
        auto widget = static_cast<QTextEdit*>(edit_blocks_stack->widget(index));
        QString err;
        novel_core->closeDocument(err, widget->document());
    }
    void tabCurrentChanged(int index)
    {
        auto widget = static_cast<QTextEdit*>(edit_blocks_stack->widget(index));
        novel_core->rehighlightDocument(widget->document());
    }
    void documentOpened(QTextDocument *doc, const QString &title)
    {
        auto view = new QTextEdit(this);
        view->setDocument(doc);
        edit_blocks_stack->addTab(view, title);
    }
    void documentClosed(QTextDocument *doc)
    {
        for (auto index = 0; index<edit_blocks_stack->count(); ++index) {
            auto widget = edit_blocks_stack->widget(index);
            if(static_cast<QTextEdit*>(widget)->document() == doc){
                edit_blocks_stack->removeTab(index);
                delete widget;
                break;
            }
        }
    }
    void documentActived(QTextDocument *doc, const QString &title)
    {
        for (auto index = 0; index<edit_blocks_stack->count(); ++index) {
            auto widget = edit_blocks_stack->widget(index);
            if(static_cast<QTextEdit*>(widget)->document() == doc){
                edit_blocks_stack->setCurrentIndex(index);
                edit_blocks_stack->setTabText(index, title);
                novel_core->rehighlightDocument(doc);
                break;
            }
        }
    }
};

#endif // MAINFRAME_H
