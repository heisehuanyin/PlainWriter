#include "mainframe.h"

#include <QtDebug>
#include <QScrollBar>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

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

    connect(node_navigate_view, &QTreeView::clicked,            this,   &MainFrame::navigate_jump);
    connect(text_edit_block,    &QTextEdit::selectionChanged,   this,   &MainFrame::selection_verify);
    connect(text_edit_block,    &QTextEdit::textChanged,        this,   &MainFrame::titles_listener);
    node_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(node_navigate_view, &QTreeView::customContextMenuRequested, this,   &MainFrame::show_manipulation);
}

MainFrame::~MainFrame()
{

}

void MainFrame::navigate_jump(const QModelIndex &index)
{
    auto item = novel_core.navigateModel()->itemFromIndex(index);
    auto xitem = static_cast<ReferenceItem*>(item);

    auto anchor_item = xitem->getAnchorItem();
    QTextCursor cursor = text_edit_block->textCursor();
    cursor.setPosition(anchor_item->firstPosition()+1);
    text_edit_block->setTextCursor(cursor);

    auto x = text_edit_block->cursorRect().y();
    if(x>0){
        auto at = text_edit_block->verticalScrollBar()->value();
        text_edit_block->verticalScrollBar()->setValue(at + x);
    }
}

void MainFrame::selection_verify()
{
    auto cursor = text_edit_block->textCursor();

    if(cursor.hasSelection()){
        auto frame = cursor.currentFrame();
        if(!frame){
            qDebug() << "frame invalid";
            return;
        }

        if(frame->childFrames().size()){
            cursor.clearSelection();
            text_edit_block->setTextCursor(cursor);
        }
    }
}

void MainFrame::titles_listener()
{
    auto cursor = text_edit_block->textCursor();
    auto string = cursor.block().text();
    auto f_around = cursor.currentFrame();

    // 校验是否title-block，如果是则p_around属于跳转接口
    auto p_around = f_around->parentFrame();
    if(!p_around) return;

    for(auto it = p_around->begin(); !it.atEnd(); it++){
        if(it.currentFrame()){
            if(it.currentFrame() == f_around)
                break;
            else
                return;
        }
    }

    QList<QTextFrame*> temp;
    while (p_around && p_around != novel_core.presentModel()->rootFrame()) {
        temp.insert(0, p_around);
        p_around = p_around->parentFrame();
    }
    if(!temp.size()) return;



    QStandardItem *volume_node = nullptr;
    for (auto index=0; index < novel_core.navigateModel()->rowCount(); ++index) {
        auto volume = novel_core.navigateModel()->item(index);
        if(static_cast<ReferenceItem*>(volume)->getAnchorItem() == temp.at(0))
            volume_node = volume;
    }

    if(temp.size() == 1){
        volume_node->setText(string);
    }
    else if (temp.size() == 2) {
        for (auto index=0; index<volume_node->rowCount(); ++index) {
            auto article = volume_node->child(index);
            if(static_cast<ReferenceItem*>(article)->getAnchorItem() == temp.at(1))
                article->setText(string);
        }
    }
    else {
        qDebug() << "Error Occur： titles_listener";
    }
}

void MainFrame::record_text_changed()
{
    auto cursor = text_edit_block->textCursor();
    auto f_around = cursor.currentFrame();

    // 校验是否title-block，如果是则p_around属于跳转接口
    auto p_around = f_around->parentFrame();
    if(!p_around) return;

    for(auto it = p_around->begin(); !it.atEnd(); it++){
        if(it.currentFrame()){
            if(it.currentFrame() != f_around){
                //TODO
            }
            else
                break;
        }
    }
}

void MainFrame::show_manipulation(const QPoint &point)
{
    auto index = node_navigate_view->indexAt(point);
    if(!index.isValid())
        return;

    auto xmenu = new QMenu("节点操控", this);
    xmenu->addAction("增加卷宗", this, &MainFrame::append_volume);
    xmenu->addAction("增加章节", this, &MainFrame::append_chapter);
    xmenu->addSeparator();
    xmenu->addAction("删除当前", this, &MainFrame::remove_selected);

    xmenu->exec(mapToGlobal(point));
    delete xmenu;
}

void MainFrame::append_volume()
{
    bool ok;
    auto title = QInputDialog::getText(this, "新建卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok) return;

    novel_core.appendVolume(title);
}

void MainFrame::append_chapter()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "新建章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok) return;

    novel_core.appendChapter(title, index);

}

void MainFrame::remove_selected()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    QMessageBox msgBox;
    msgBox.setText("选中节点极其子节点（包含磁盘文件）将被删除！");
    msgBox.setInformativeText("是否确定执行?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();
    if(ret == QMessageBox::No)
        return;

    novel_core.removeNode(index);
}












