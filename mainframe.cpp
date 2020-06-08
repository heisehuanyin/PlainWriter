#include "common.h"
#include "mainframe.h"

#include <QtDebug>
#include <QScrollBar>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QToolBox>

MainFrame::MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent)
    : QMainWindow(parent),
      timer_autosave(new QTimer(this)),
      novel_core(core),
      config(host),
      functions_split_base(new QSplitter(Qt::Horizontal, this)),
      outlines_navigate_treeview(new QTreeView(this)),
      volume_outlines_present(new QTextEdit(this)),                 // 卷宗细纲显示
      chapters_navigate_view(new QTreeView(this)),                  // 章卷导航与打开与切换
      search_result_navigate_view(new QTableView(this)),            // 搜索结果视图显示
      search_text_enter(new QLineEdit(this)),                       // 搜索内容键入框
      search(new QPushButton("检索")),
      clear(new QPushButton("检索")),
      chapter_textedit_present(new QTextEdit(this)),                // 章节内容编辑
      chapter_outlines_present(new QTextEdit(this)),                // 章节细纲视图1
      empty_document(chapter_textedit_present->document()),         // 空白占位
      foreshadows_under_volume_view(new QTableView(this)),          // 卷内伏笔汇集
      foreshadows_remains_until_volume_view(new QTableView(this)),
      foreshadows_remains_until_chapter_view(new QTableView(this)),
      novel_outlines_present(new QTextEdit(this)),                  // 全书大纲
      file(new QMenu("文件", this)),
      func(new QMenu("功能", this))
{
    setWindowTitle(novel_core->novelTitle());

    connect(novel_core, &NovelHost::messagePopup,   this,   &MainFrame::acceptMessage);
    connect(novel_core, &NovelHost::warningPopup,   this,   &MainFrame::acceptWarning);
    connect(novel_core, &NovelHost::errorPopup,   this,   &MainFrame::acceptError);

    // 定制菜单
    {
        menuBar()->addMenu(file);
        file->addAction("新建卷宗",    this, &MainFrame::append_volume);
        file->addSeparator();
        file->addAction("保存",       this, &MainFrame::saveOp);
        file->addSeparator();
        file->addAction("重命名小说",  this, &MainFrame::rename_novel_title);
        menuBar()->addMenu(func);
        func->addAction("自动保存间隔", this, &MainFrame::autosave_timespan_reset);
    }

    setCentralWidget(functions_split_base);

    // 定制导航区域视图=======================================================================
    auto click_navigate_cube = new QTabWidget(this);
    click_navigate_cube->addTab(chapters_navigate_view, "卷章结构树");
    chapters_navigate_view->setModel(novel_core->chaptersNavigateTree());
    click_navigate_cube->addTab(outlines_navigate_treeview, "故事结构树");
    outlines_navigate_treeview->setModel(novel_core->outlineNavigateTree());
    auto search_pane = new QWidget(this);
    auto layout = new QGridLayout(search_pane);
    layout->setMargin(0);
    layout->setSpacing(2);
    search_result_navigate_view->setModel(novel_core->findResultsPresent());
    layout->addWidget(search_result_navigate_view, 0, 0, 5, 3);
    layout->addWidget(search_text_enter, 5, 0, 1, 3);
    layout->addWidget(search, 6, 0, 1, 1);
    layout->addWidget(clear, 6, 1, 1, 1);
    connect(search, &QPushButton::clicked,  this,   &MainFrame::search_text);
    connect(clear,  &QPushButton::clicked,  this,   &MainFrame::clear_search_result);
    click_navigate_cube->setTabPosition(QTabWidget::West);
    chapters_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
    chapters_navigate_view->setModel(novel_core->chaptersNavigateTree());
    click_navigate_cube->addTab(search_pane, "搜索结果");
    functions_split_base->addWidget(click_navigate_cube);

    //定制右方内容编辑区域========================================================================
    auto edit_split_base = new QSplitter(Qt::Vertical, this);
    functions_split_base->addWidget(edit_split_base);

    // 添加主编辑区域
    auto edit_main_cube = new QSplitter(Qt::Horizontal, this);
    auto content_stack_tab = new QTabWidget(this);
    edit_main_cube->addWidget(content_stack_tab);
    // 添加正文编辑界面
    content_stack_tab->addTab(chapter_textedit_present, "正文编辑");
    content_stack_tab->addTab(volume_outlines_present, "卷宗细纲");
    volume_outlines_present->setDocument(novel_core->volumeOutlinesPresent());
    connect(novel_core, &NovelHost::documentActived,    this,   &MainFrame::documentActived);
    connect(novel_core, &NovelHost::documentOpened,     this,   &MainFrame::documentOpened);
    connect(novel_core, &NovelHost::documentAboutToBeClosed,    this,   &MainFrame::documentClosed);
    // 右方小区域
    {
        auto toolbox = new QToolBox(this);
        edit_main_cube->addWidget(toolbox);
        toolbox->layout()->setSpacing(0);
        toolbox->setStyleSheet("QToolBox::tab{background-color: rgb(220, 220, 220);border-width: 1px;border-style: solid;"
                               "border-color: lightgray;}QToolBox::tab:selected{background-color: rgb(250, 250, 250);}");


        toolbox->addItem(chapter_outlines_present, "章节细纲");
        chapter_outlines_present->setDocument(novel_core->chapterOutlinePresent());
    }
    edit_split_base->addWidget(edit_main_cube);



    // 添加伏笔视图
    auto foreshadows_tab = new QTabWidget(this);
    foreshadows_tab->addTab(foreshadows_under_volume_view, "卷内伏笔");
    foreshadows_under_volume_view->setModel(novel_core->foreshadowsUnderVolume());
    foreshadows_tab->addTab(foreshadows_remains_until_volume_view, "卷宗前未闭合伏笔");
    foreshadows_remains_until_volume_view->setModel(novel_core->foreshadowsUntilVolumeRemain());
    foreshadows_tab->addTab(foreshadows_remains_until_chapter_view, "章节前未闭合伏笔");
    foreshadows_remains_until_chapter_view->setModel(novel_core->foreshadowsUntilChapterRemain());
    foreshadows_tab->addTab(novel_outlines_present, "作品大纲");
    novel_outlines_present->setDocument(novel_core->novelOutlinesPresent());
    edit_split_base->addWidget(foreshadows_tab);


    connect(chapters_navigate_view,         &QTreeView::clicked,                    this,   &MainFrame::navigate_jump);
    connect(chapters_navigate_view,         &QTreeView::customContextMenuRequested, this,   &MainFrame::show_manipulation);
    connect(search_result_navigate_view,    &QTableView::clicked,                   this,   &MainFrame::search_jump);
    connect(outlines_navigate_treeview,     &QTreeView::clicked,                    this,   &MainFrame::outlines_jump);
    connect(timer_autosave,                 &QTimer::timeout,                       this,   &MainFrame::saveOp);
    connect(novel_core,                     &NovelHost::documentOpened,             this,   &MainFrame::documentOpened);
    connect(novel_core,                     &NovelHost::documentActived,            this,   &MainFrame::documentActived);
    connect(novel_core,                     &NovelHost::documentAboutToBeClosed,    this,   &MainFrame::documentClosed);
    timer_autosave->start(5000*60);
}

MainFrame::~MainFrame()
{

}


void MainFrame::acceptMessage(const QString &title, const QString &message){
    QMessageBox::information(this, title, message);
}

void MainFrame::acceptWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void MainFrame::acceptError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainFrame::rename_novel_title()
{
start:
    bool ok;
    auto name = QInputDialog::getText(this, "重命名小说", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok) return;

    if(name == ""){
        QMessageBox::information(this, "重命名小说", "未检测到新名");
        goto start;
    }

    novel_core->resetNovelTitle(name);
    setWindowTitle(name);
}

void MainFrame::navigate_jump(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        novel_core->setCurrentChaptersNode(index);
    } catch (WsException *e) {
        QMessageBox::critical(this, "切换当前章节", e->reason());
    }
}

void MainFrame::show_manipulation(const QPoint &point)
{
    auto index = chapters_navigate_view->indexAt(point);
    if(!index.isValid())
        return;

    auto xmenu = new QMenu("节点操控", this);
    xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
    xmenu->addSeparator();
    xmenu->addAction("增加卷宗", this,  &MainFrame::append_volume);
    xmenu->addAction("插入卷宗", this,  &MainFrame::insert_volume);
    xmenu->addSeparator();
    xmenu->addAction("增加章节", this,  &MainFrame::append_chapter);
    xmenu->addAction("插入章节", this,  &MainFrame::insert_chapter);
    xmenu->addSeparator();
    xmenu->addAction("删除", this, &MainFrame::remove_selected);
    xmenu->addSeparator();
    xmenu->addAction("输出到剪切板", this, &MainFrame::content_output);

    xmenu->exec(mapToGlobal(point));
    delete xmenu;
}

void MainFrame::append_volume()
{
    bool ok;
    auto title = QInputDialog::getText(this, "新增卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    try {
        novel_core->insertVolume(novel_core->chaptersNavigateTree()->rowCount(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "增加卷宗", e->reason());
    }
}

void MainFrame::insert_volume()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    bool ok;
    auto title = QInputDialog::getText(this, "插入新卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    auto node = novel_core->chaptersNavigateTree()->itemFromIndex(index);
    if(node->parent())  // 目标为章节
        node = node->parent();

    try {
        novel_core->insertVolume(node->row(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入卷宗", e->reason());
    }
}

void MainFrame::append_chapter()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    bool ok;
    auto title = QInputDialog::getText(this, "新增章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    auto target_node = novel_core->chaptersNavigateTree()->itemFromIndex(index);
    try {
        if(!target_node->parent()){  // volume-node
            novel_core->insertChapter(index, target_node->rowCount(), title);
        }
        else {
            novel_core->insertChapter(target_node->parent()->index(), target_node->row(), title);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加章节", e->reason());
    }
}

void MainFrame::insert_chapter()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    bool ok;
    auto title = QInputDialog::getText(this, "插入新章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    auto node = novel_core->chaptersNavigateTree()->itemFromIndex(index);
    try {
        if(node->parent()){ // 选中章节节点
            novel_core->insertChapter(node->parent()->index(), node->row(), title);
        }
        else {
            novel_core->insertChapter(index, node->rowCount(), title);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入章节", e->reason());
    }
}

void MainFrame::remove_selected()
{
    auto index = chapters_navigate_view->currentIndex();
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


}

void MainFrame::content_output()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    QClipboard *x = QApplication::clipboard();
    QString text, err;
}

void MainFrame::search_text()
{
    auto text = search_text_enter->text();
    if(!text.length())
        return;

    novel_core->searchText(text);
    search_result_navigate_view->resizeColumnsToContents();
}

void MainFrame::clear_search_result()
{
}

void MainFrame::search_jump(const QModelIndex &xindex)
{
    QModelIndex index = xindex;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);
}

void MainFrame::outlines_jump(const QModelIndex &_index)
{
    auto index = _index;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        novel_core->setCurrentOutlineNode(index);
        auto blk = novel_core->volumeOutlinesPresent()->firstBlock();
        while (blk.isValid()) {
            if(blk.userData()){
                auto ndata = static_cast<NovelBase::WsBlockData*>(blk.userData());
                if(ndata->outlineTarget() == index){
                    QTextCursor cursor(blk);
                    volume_outlines_present->setTextCursor(cursor);

                    auto at_value = volume_outlines_present->verticalScrollBar()->value();
                    volume_outlines_present->verticalScrollBar()->setValue(
                                at_value + volume_outlines_present->cursorRect().y());
                    break;
                }
            }
            blk = blk.next();
        }

    } catch (WsException *e) {
        QMessageBox::critical(this, "大纲跳转", e->reason());
    }
}

void MainFrame::saveOp()
{
    try {
        novel_core->save();
    } catch (WsException *e) {
        QMessageBox::critical(this, "保存过程出错", e->reason(), QMessageBox::Ok);
    }
}

void MainFrame::autosave_timespan_reset()
{
    bool ok;
    auto timespan = QInputDialog::getInt(this, "重设自动保存间隔", "输入时间间隔(分钟)", 5, 1, 30, 1, &ok);
    if(!ok) return;

    timer_autosave->start(timespan*1000*60);
}

void MainFrame::documentOpened(QTextDocument *doc, const QString &title)
{

}

void MainFrame::documentClosed(QTextDocument *)
{
    setWindowTitle(novel_core->novelTitle());
    chapter_textedit_present->setDocument(empty_document);
}

void MainFrame::documentActived(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

    this->chapter_textedit_present->setDocument(doc);
}



CQTextEdit::CQTextEdit(ConfigHost &config, QWidget *parent)
    :QTextEdit(parent),host(config){}

void CQTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasText()) {
        QTextBlockFormat format0;
        QTextCharFormat format1;
        host.textFormat(format0, format1);

        QString context = source->text();
        QTextCursor cursor = this->textCursor();
        cursor.setBlockFormat(format0);
        cursor.setBlockCharFormat(format1);
        cursor.insertText(context);
    }
}
