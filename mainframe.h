#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "novelhost.h"

#include <QMainWindow>
#include <QSplitter>
#include <QTextEdit>
#include <QTreeView>

class MainFrame : public QMainWindow
{
    Q_OBJECT

public:
    MainFrame(NovelHost &core, QWidget *parent = nullptr);
    ~MainFrame();

private:
    NovelHost &novel_core;
    QSplitter *const split_panel;
    QTreeView *const node_navigate_view;
    QTextEdit *const text_edit_block;

    void navigate_jump(const QModelIndex &index);
    void selection_verify();
    void titles_listener();
    void record_text_changed();

    void show_manipulation(const QPoint &point);
    void append_volume();
    void append_chapter();
    void remove_selected();
};

#endif // MAINFRAME_H
