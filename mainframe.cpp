#include "mainframe.h"

MainFrame::MainFrame(NovelHost &core, QWidget *parent)
    : QMainWindow(parent),
      novel_core(core),
      split_panel(new QSplitter(this)),
      node_navigate_view(new QTreeView(this)),
      text_edit_block(new QTextEdit(this))
{
    setCentralWidget(split_panel);
    split_panel->addWidget(node_navigate_view);
    node_navigate_view->setModel(novel_core.navigateModel());
    split_panel->addWidget(text_edit_block);
    text_edit_block->setDocument(novel_core.presentModel());
}

MainFrame::~MainFrame()
{

}
