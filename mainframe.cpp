#include "mainframe.h"

#include <QtDebug>
#include <QScrollBar>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QMenuBar>
#include <QFileDialog>

MainFrame::MainFrame(NovelHost *core, QWidget *parent)
    : QMainWindow(parent),
      novel_core(core),
      split_panel(new QSplitter(this)),
      node_navigate_view(new QTreeView(this)),
      text_edit_view_comp(new QTextEdit(this)),
      search_result_view(new QTableView(this)),
      search_text_enter(new QLineEdit(this)),
      search(new QPushButton("搜索", this)),
      clear(new QPushButton("清空", this)),
      file(new QMenu("文件", this))
{
    menuBar()->addMenu(file);
    file->addAction("新建卷宗", this, &MainFrame::append_volume);
    file->addSeparator();
    file->addAction("保存", this, &MainFrame::saveOp);

    setCentralWidget(split_panel);

    auto search_pane = new QWidget(this);
    auto layout = new QGridLayout(search_pane);
    layout->setMargin(0);
    layout->setSpacing(2);
    layout->addWidget(search_result_view, 0, 0, 5, 3);
    layout->addWidget(search_text_enter, 5, 0, 1, 3);
    layout->addWidget(search, 6, 0, 1, 1);
    layout->addWidget(clear, 6, 1, 1, 1);
    connect(search, &QPushButton::clicked,  this,   &MainFrame::search_text);
    connect(clear,  &QPushButton::clicked,  this,   &MainFrame::clear_search_result);

    auto tab = new QTabWidget(this);
    tab->setTabPosition(QTabWidget::West);
    tab->addTab(node_navigate_view, "小说结构");
    tab->addTab(search_pane, "搜索结果");

    split_panel->addWidget(tab);
    auto w = split_panel->width();
    QList<int> ws;
    ws.append(40);ws.append(w-40);
    split_panel->setSizes(ws);
    split_panel->setStretchFactor(0,0);
    split_panel->setStretchFactor(1,1);

    node_navigate_view->setModel(novel_core->navigateTree());
    split_panel->addWidget(text_edit_view_comp);
    text_edit_view_comp->setDocument(novel_core->presentDocument());
    search_result_view->setModel(novel_core->searchResultPresent());

    connect(node_navigate_view,     &QTreeView::clicked,            this,   &MainFrame::navigate_jump);
    connect(text_edit_view_comp,    &QTextEdit::selectionChanged,   this,   &MainFrame::selection_verify);
    connect(text_edit_view_comp,    &QTextEdit::textChanged,        this,   &MainFrame::text_change_listener);
    node_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(node_navigate_view, &QTreeView::customContextMenuRequested, this,   &MainFrame::show_manipulation);
    connect(search_result_view, &QTableView::clicked,   this,   &MainFrame::search_jump);
}

MainFrame::~MainFrame()
{

}

void MainFrame::navigate_jump(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = novel_core->navigateTree()->itemFromIndex(index);
    auto xitem = static_cast<ReferenceItem*>(item);

    auto anchor_item = xitem->getAnchorItem();
    QTextCursor cursor = text_edit_view_comp->textCursor();
    cursor.setPosition(anchor_item->firstPosition()+1);
    text_edit_view_comp->setTextCursor(cursor);

    auto offset_value = text_edit_view_comp->cursorRect().y();
    if(offset_value > 0){
        auto sbar = text_edit_view_comp->verticalScrollBar();
        auto pos_at = sbar->value();
        sbar->setValue(pos_at + offset_value);
    }
}

void MainFrame::selection_verify()
{
    auto cursor = text_edit_view_comp->textCursor();

    if(cursor.hasSelection()){
        auto frame = cursor.currentFrame();
        if(!frame){
            qDebug() << "frame invalid";
            return;
        }

        if(frame->childFrames().size()){
            cursor.clearSelection();
            text_edit_view_comp->setTextCursor(cursor);
        }
    }
}

void MainFrame::text_change_listener()
{
    auto cursor = text_edit_view_comp->textCursor();
    auto f_around = cursor.currentFrame();

    auto p_around = f_around->parentFrame();
    if(!p_around) return;

    // 校验是否title-block，如果是则p_around属于跳转接口
    int kind = -1; // 0 title; 1 text;
    for(auto it = p_around->begin(); !it.atEnd(); it++){
        if(it.currentFrame()){
            // 第一个frame就是title
            if(it.currentFrame() == f_around){
                kind = 0;
            }
            // 当前处于正文区域
            else{
                kind = 1;
            }

            break;
        }
    }

    QList<QTextFrame*> temp;
    while (p_around && p_around != novel_core->presentDocument()->rootFrame()) {
        temp.insert(0, p_around);
        p_around = p_around->parentFrame();
    }
    // 当前编辑的是总标题
    if(!temp.size()) return;



    QStandardItem *volume_node = nullptr;
    for (auto num_index=0; num_index < novel_core->navigateTree()->rowCount(); ++num_index) {
        auto volume = novel_core->navigateTree()->item(num_index);
        if(static_cast<ReferenceItem*>(volume)->getAnchorItem() == temp.at(0))
            volume_node = volume;
    }

    if(temp.size() == 1){
        // 更新卷标题
        if(kind == 0)
            volume_node->setText(cursor.block().text());
    }
    else if (temp.size() == 2) {
        for (auto num_index=0; num_index<volume_node->rowCount(); ++num_index) {
            auto article = volume_node->child(num_index);
            auto count_node = volume_node->child(article->row(), 1);

            // 更新标题或字数
            if(static_cast<ReferenceItem*>(article)->getAnchorItem() == temp.at(1)){
                if(kind == 0)
                    article->setText(cursor.block().text());
                else {
                    static_cast<ReferenceItem*>(volume_node)->resetModified(true);
                    static_cast<ReferenceItem*>(article)->resetModified(true);
                    auto text_context = novel_core->chapterTextContent(article->index());
                    count_node->setText(QString("%1").arg(novel_core->calcValidWordsCount(text_context)));
                }

                break;
            }
        }
    }
    else {
        qDebug() << "Error Occur： titles_listener";
    }
}

void MainFrame::show_manipulation(const QPoint &point)
{
    auto index = node_navigate_view->indexAt(point);
    if(!index.isValid())
        return;

    auto xmenu = new QMenu("节点操控", this);
    xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
    xmenu->addSeparator();
    xmenu->addAction("增加卷宗", this, &MainFrame::append_volume);
    xmenu->addAction("增加章节", this, &MainFrame::append_chapter);
    xmenu->addSeparator();
    xmenu->addAction("删除当前", this, &MainFrame::remove_selected);
    xmenu->addAction("输出章节内容", this, &MainFrame::content_output);

    xmenu->exec(mapToGlobal(point));
    delete xmenu;
}

void MainFrame::append_volume()
{
    bool ok;
    auto title = QInputDialog::getText(this, "新建卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    QString err;
    novel_core->appendVolume(err, title);
}

void MainFrame::append_chapter()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "新建章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    QString err;
    novel_core->appendChapter(err, title, index);
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

    QString err;
    novel_core->removeNode(err, index);
}

void MainFrame::content_output()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    qDebug() << novel_core->chapterTextContent(index);
}

void MainFrame::search_text()
{
    novel_core->searchResultPresent()->clear();
    auto text = search_text_enter->text();
    if(!text.length())
        return;

    novel_core->searchText(text);
}

void MainFrame::clear_search_result()
{
    novel_core->searchResultPresent()->clear();
}

void MainFrame::search_jump(const QModelIndex &xindex)
{
    QModelIndex index = xindex;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = novel_core->searchResultPresent()->itemFromIndex(index);

    auto cursor = text_edit_view_comp->textCursor();
    cursor.setPosition(item->data(Qt::UserRole+1).toInt());
    cursor.setPosition(item->data(Qt::UserRole+1).toInt() +
                       item->data(Qt::UserRole+2).toInt(), QTextCursor::KeepAnchor);
    text_edit_view_comp->setTextCursor(cursor);
}

void MainFrame::saveOp()
{
    QString err;

    if(!novel_core->save(err))
        return;

    auto dir_path = QFileDialog::getExistingDirectory(this, "选择基准文件夹", QDir::homePath(),
                                 QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);

    auto target_path = QDir(dir_path).filePath("NovelStruct.xml");
    if(novel_core->save(err, target_path))
        QMessageBox::critical(this, "保存过程出错", err, QMessageBox::Ok, QMessageBox::Ok);
}












