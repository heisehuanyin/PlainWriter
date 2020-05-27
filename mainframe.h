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
};

#endif // MAINFRAME_H
