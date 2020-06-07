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
      main_function_tab(new QTabWidget(this)),
      outlines_tree_view(new QTreeView(this)),
      novel_description_view1(new QTextEdit(this)),
      volume_outlines_view1(new QTextEdit(this)),
      foreshadows_under_volume_view1(new QTableView(this)),
      foreshadows_remains_until_volume_view(new QTableView(this)),
      novel_description_view2(new QTextEdit(this)),             // 全书大纲
      volume_outlines_description_view_present(new QTextEdit(this)),               // 卷宗细纲显示
      navigate_between_volume(new QTreeView(this)),             // 卷宗细纲导航
      chapters_navigate_view(new QTreeView(this)),              // 章卷导航与打开与切换
      search_result_view(new QTableView(this)),                 // 搜索结果视图显示
      search_text_enter(new QLineEdit(this)),                   // 搜索内容键入框
      search(new QPushButton("检索")),
      clear(new QPushButton("检索")),
      chapter_text_edit_view(new QTextEdit(this)),              // 章节内容编辑
      chapter_outline_edit_view1(new QTextEdit(this)),          // 章节细纲视图1
      chapter_outline_edit_view2(new QTextEdit(this)),          // 章节细纲视图2
      empty_document(chapter_text_edit_view->document()),       // 空白占位
      foreshadows_under_volume_view2(new QTableView(this)),     // 卷内伏笔汇集
      foreshadows_remains_until_chapter_view1(new QTableView(this)),
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

    setCentralWidget(main_function_tab);
    main_function_tab->setTabPosition(QTabWidget::South);
    // 添加大纲视图
    {
        auto outlines_base = new QSplitter(Qt::Horizontal, this);
        main_function_tab->addTab(outlines_base, "大纲视图");
        outlines_base->addWidget(outlines_tree_view);
        outlines_tree_view->setModel(novel_core->outlineTree());
        auto funcsplit = new QTabWidget(this);
        funcsplit->setTabPosition(QTabWidget::East);
        outlines_base->addWidget(funcsplit);
        // 定制大纲视图
        auto toolbox = new QToolBox(this);
        toolbox->layout()->setSpacing(0);
        toolbox->setStyleSheet("QToolBox::tab{background-color: rgb(220, 220, 220);border-width: 1px;border-style: solid;"
                               "border-color: lightgray;}QToolBox::tab:selected{background-color: rgb(250, 250, 250);}");
        funcsplit->addTab(toolbox, "大纲视图");
        toolbox->addItem(novel_description_view1, "作品大纲");
        novel_description_view1->setDocument(novel_core->novelDescriptions());
        toolbox->addItem(volume_outlines_view1, "卷章大纲");
        volume_outlines_view1->setDocument(novel_core->volumeDescriptions());
        // 定制伏笔视图
        auto split = new QToolBox(this);
        split->layout()->setSpacing(0);
        split->setStyleSheet("QToolBox::tab{background-color: rgb(220, 220, 220);border-width: 1px;border-style: solid;"
                             "border-color: lightgray;}QToolBox::tab:selected{background-color: rgb(250, 250, 250);}");
        funcsplit->addTab(split, "伏笔视图");
        split->addItem(foreshadows_under_volume_view1, "卷内伏笔状态统计");
        split->addItem(foreshadows_remains_until_volume_view, "伏笔闭合状态统计");
    }

    // 添加章节编辑区域
    {
        // 定制编辑视图
        auto navigate_editarea_split = new QSplitter(Qt::Horizontal, this);
        main_function_tab->addTab(navigate_editarea_split, "章节内容");

        // 左方导航区域
        auto search_pane = new QWidget(this);
        auto layout = new QGridLayout(search_pane);
        layout->setMargin(0);
        layout->setSpacing(2);
        search_result_view->setModel(novel_core->findResultsPresent());
        layout->addWidget(search_result_view, 0, 0, 5, 3);
        layout->addWidget(search_text_enter, 5, 0, 1, 3);
        layout->addWidget(search, 6, 0, 1, 1);
        layout->addWidget(clear, 6, 1, 1, 1);
        connect(search, &QPushButton::clicked,  this,   &MainFrame::search_text);
        connect(clear,  &QPushButton::clicked,  this,   &MainFrame::clear_search_result);
        auto chapters_navigate_tab = new QTabWidget(this);
        chapters_navigate_tab->setTabPosition(QTabWidget::West);
        chapters_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
        chapters_navigate_view->setModel(novel_core->chaptersNavigateTree());
        chapters_navigate_tab->addTab(chapters_navigate_view, "小说结构");
        chapters_navigate_tab->addTab(search_pane, "搜索结果");
        navigate_editarea_split->addWidget(chapters_navigate_tab);

        // 定制右部区域1
        // 添加章节编辑区域和章节细纲编辑区域
        auto foreshadows_split = new QSplitter(Qt::Vertical, this);
        navigate_editarea_split->addWidget(foreshadows_split);

        // 添加伏笔视图
        auto foreshadows_tab = new QTabWidget(this);
        foreshadows_tab->addTab(foreshadows_under_volume_view2, "卷内伏笔");
        foreshadows_under_volume_view2->setModel(novel_core->foreshadowsUnderVolume());
        foreshadows_tab->addTab(foreshadows_remains_until_chapter_view1, "至章节未闭合伏笔");
        foreshadows_remains_until_chapter_view1->setModel(novel_core->foreshadowsUntilRemains());
        foreshadows_tab->addTab(novel_description_view2, "作品大纲");
        novel_description_view2->setDocument(novel_core->novelDescriptions());
        foreshadows_split->addWidget(foreshadows_tab);

        // 添加细纲编辑界面和正文编辑界面
        auto edit_stack_box = new QTabWidget(this);
        // 添加正文编辑界面
        auto outline_split = new QSplitter(Qt::Horizontal, this);
        outline_split->addWidget(chapter_outline_edit_view1);
        chapter_outline_edit_view1->setDocument(novel_core->chapterOutlinePresent());
        outline_split->addWidget(chapter_text_edit_view);
        // TODO 正文编辑区域切换功能
        edit_stack_box->addTab(outline_split, "细纲编辑区域与正文编辑区域");

        // 细纲编辑界面
        auto outline_split2 = new QSplitter(Qt::Horizontal, this);
        edit_stack_box->addTab(outline_split2, "卷宗大纲编辑与细纲编辑");
        outline_split2->addWidget(navigate_between_volume);
        navigate_between_volume->setModel(novel_core->subtreeUnderVolume());
        outline_split2->addWidget(volume_outlines_description_view_present);
        volume_outlines_description_view_present->setDocument(novel_core->volumeDescriptions());
        outline_split2->addWidget(chapter_outline_edit_view2);
        chapter_outline_edit_view2->setDocument(novel_core->chapterOutlinePresent());

        foreshadows_split->addWidget(edit_stack_box);
    }

    connect(chapters_navigate_view,     &QTreeView::clicked,        this,   &MainFrame::navigate_jump);
    connect(chapters_navigate_view,     &QTreeView::customContextMenuRequested, this,   &MainFrame::show_manipulation);
    connect(search_result_view,     &QTableView::clicked,       this,   &MainFrame::search_jump);
    connect(novel_core,             &NovelHost::documentOpened, this,   &MainFrame::documentOpened);
    connect(novel_core,     &NovelHost::documentActived,        this,   &MainFrame::documentActived);
    connect(novel_core,     &NovelHost::documentAboutToBeClosed,this,   &MainFrame::documentClosed);
    connect(timer_autosave,         &QTimer::timeout,           this,   &MainFrame::saveOp);
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

    novel_core->setCurrentChaptersNode(index);
}

void MainFrame::show_manipulation(const QPoint &point)
{
    auto index = chapters_navigate_view->indexAt(point);
    if(!index.isValid())
        return;

    auto xmenu = new QMenu("节点操控", this);
    xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
    xmenu->addSeparator();
    xmenu->addAction("增加卷宗", this, &MainFrame::append_volume);
    xmenu->addAction("增加章节", this, &MainFrame::append_chapter);
    xmenu->addSeparator();
    xmenu->addAction("删除当前", this, &MainFrame::remove_selected);
    xmenu->addSeparator();
    xmenu->addAction("输出到剪切板", this, &MainFrame::content_output);

    xmenu->exec(mapToGlobal(point));
    delete xmenu;
}

void MainFrame::append_volume()
{
    bool ok;
    auto title = QInputDialog::getText(this, "新建卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    novel_core->insertVolume(novel_core->chaptersNavigateTree()->rowCount(), title);
}

void MainFrame::append_chapter()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "新建章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

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
    search_result_view->resizeColumnsToContents();
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

void MainFrame::documentOpened(QTextDocument *doc, const QString &title){}

void MainFrame::documentClosed(QTextDocument *)
{
}

void MainFrame::documentActived(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

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
