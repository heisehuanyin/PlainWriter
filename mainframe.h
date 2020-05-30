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
    QTextEdit *const text_edit_view_comp;
    QTableView *const search_result_view;
    QLineEdit *const search_text_enter;
    QPushButton *const search, *const clear;

    QMenu *const file;
    QMenu *const func;

    void navigate_jump(const QModelIndex &index);
    void selection_verify();
    void text_change_listener();
    void cursor_position_verify();

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
};

#endif // MAINFRAME_H
