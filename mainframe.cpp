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

using namespace NovelBase;

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
      clear(new QPushButton("清除")),
      chapter_textedit_present(new CQTextEdit(config, this)),               // 章节内容编辑
      chapter_outlines_present(new CQTextEdit(config, this)),               // 章节细纲视图1
      empty_document(chapter_textedit_present->document()),                 // 空白占位
      foreshadows_under_volume_view(new QTableView(this)),                  // 卷内伏笔汇集
      foreshadows_remains_until_volume_view(new QTableView(this)),
      foreshadows_remains_until_chapter_view(new QTableView(this)),
      novel_outlines_present(new CQTextEdit(config, this)),                  // 全书大纲
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
    outlines_navigate_treeview->setContextMenuPolicy(Qt::CustomContextMenu);
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
    connect(novel_core,                     &NovelHost::documentPrepared,            this,   &MainFrame::documentPresent);
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
    foreshadows_tab->addTab(foreshadows_under_volume_view, "卷宗内建伏笔汇总");
    foreshadows_under_volume_view->setModel(novel_core->foreshadowsUnderVolume());
    foreshadows_tab->addTab(foreshadows_remains_until_volume_view, "卷宗可见伏笔汇总");
    foreshadows_remains_until_volume_view->setModel(novel_core->foreshadowsUntilVolumeRemain());
    foreshadows_tab->addTab(foreshadows_remains_until_chapter_view, "章节可见伏笔汇总");
    foreshadows_remains_until_chapter_view->setModel(novel_core->foreshadowsUntilChapterRemain());
    foreshadows_tab->addTab(novel_outlines_present, "作品大纲");
    novel_outlines_present->setDocument(novel_core->novelOutlinesPresent());
    edit_split_base->addWidget(foreshadows_tab);


    connect(chapters_navigate_view,         &QTreeView::clicked,                    this,   &MainFrame::chapters_navigate_jump);
    connect(chapters_navigate_view,         &QTreeView::customContextMenuRequested, this,   &MainFrame::chapters_manipulation);
    connect(outlines_navigate_treeview,     &QTreeView::clicked,                    this,   &MainFrame::outlines_navigate_jump);
    connect(outlines_navigate_treeview,     &QTreeView::customContextMenuRequested, this,   &MainFrame::outlines_manipulation);
    connect(search_result_navigate_view,    &QTableView::clicked,                   this,   &MainFrame::search_jump);
    connect(timer_autosave,                 &QTimer::timeout,                       this,   &MainFrame::saveOp);
    connect(novel_core,         &NovelHost::currentVolumeActived,   this,   &MainFrame::currentVolumeOutlinesPresent);
    connect(novel_core,         &NovelHost::currentChaptersActived, this,   &MainFrame::currentChaptersAboutPresent);
    timer_autosave->start(5000*60);

    {
        chapter_outlines_present->setEnabled(false);
        chapter_textedit_present->setEnabled(false);
        volume_outlines_present->setEnabled(false);
    }
}

MainFrame::~MainFrame(){}


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

void MainFrame::resize_foreshadows_tableitem_width()
{
    foreshadows_under_volume_view->resizeColumnToContents(0);
    foreshadows_under_volume_view->resizeColumnToContents(1);
    foreshadows_under_volume_view->resizeColumnToContents(4);
    foreshadows_under_volume_view->resizeColumnToContents(5);

    foreshadows_remains_until_volume_view->resizeColumnToContents(0);
    foreshadows_remains_until_volume_view->resizeColumnToContents(1);
    foreshadows_remains_until_volume_view->resizeColumnToContents(4);
    foreshadows_remains_until_volume_view->resizeColumnToContents(5);
    foreshadows_remains_until_volume_view->resizeColumnToContents(6);

    foreshadows_remains_until_chapter_view->resizeColumnToContents(0);
    foreshadows_remains_until_chapter_view->resizeColumnToContents(1);
    foreshadows_remains_until_chapter_view->resizeColumnToContents(4);
    foreshadows_remains_until_chapter_view->resizeColumnToContents(5);
    foreshadows_remains_until_chapter_view->resizeColumnToContents(6);
}

void MainFrame::chapters_navigate_jump(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        novel_core->setCurrentChaptersNode(index);
        resize_foreshadows_tableitem_width();
    } catch (WsException *e) {
        QMessageBox::critical(this, "切换当前章节", e->reason());
    }
}

void MainFrame::chapters_manipulation(const QPoint &point)
{
    auto index = chapters_navigate_view->indexAt(point);

    auto xmenu = new QMenu("节点操控", this);
    switch (novel_core->treeNodeLevel(index)) {
        case 0:
            xmenu->addAction(QIcon(":/outlines/icon/卷.png"), "添加卷宗", this, &MainFrame::append_volume);
            break;
        case 1:
            xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
            xmenu->addSeparator();
            xmenu->addAction(QIcon(":/outlines/icon/卷.png"), "增加卷宗", this,  &MainFrame::append_volume);
            xmenu->addAction(QIcon(":/outlines/icon/卷.png"), "插入卷宗", this,  &MainFrame::insert_volume);
            xmenu->addSeparator();
            xmenu->addAction(QIcon(":/outlines/icon/章.png"), "增加章节", this,  &MainFrame::append_chapter);
            xmenu->addSeparator();
            xmenu->addAction(QIcon(":/outlines/icon/伏.png"), "新建伏笔", this,  &MainFrame::append_foreshadow_from_chapters);
            xmenu->addSeparator();
            xmenu->addAction("删除", this, &MainFrame::remove_selected_chapters);
            break;
        case 2:
            xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
            xmenu->addSeparator();
            xmenu->addAction(QIcon(":/outlines/icon/章.png"), "增加章节", this,  &MainFrame::append_chapter);
            xmenu->addAction(QIcon(":/outlines/icon/章.png"), "插入章节", this,  &MainFrame::insert_chapter);
            xmenu->addSeparator();
            xmenu->addAction(QIcon(":/outlines/icon/伏.png"), "新建伏笔", this,  &MainFrame::append_foreshadow_from_chapters);
            xmenu->addSeparator();
            auto foreshadow_absorb = xmenu->addMenu("吸附伏笔");
            connect(foreshadow_absorb,  &QMenu::triggered,  this,   &MainFrame::append_shadowstart_from_chapter);
            QList<QPair<QString, QString>> foreshadows;
            novel_core->sumForeshadowsUnderVolumeHanging(index, foreshadows);
            for (auto item : foreshadows)
                foreshadow_absorb->addAction(item.first)->setData(item.second);

            auto foreshadows_remove = xmenu->addMenu("移除吸附");
            connect(foreshadows_remove,  &QMenu::triggered,  this,   &MainFrame::remove_shadowstart_from_chapter);
            foreshadows.clear();
            novel_core->sumForeshadowsAbsorbed(index, foreshadows);
            for (auto item : foreshadows)
                foreshadows_remove->addAction(item.first)->setData(item.second);

            auto foreshadow_close = xmenu->addMenu("闭合伏笔");
            connect(foreshadow_close,   &QMenu::triggered,  this,   &MainFrame::append_shadowstop_from_chapter);
            foreshadows.clear();
            novel_core->sumForeshadowsOpening(index, foreshadows);
            for (auto item : foreshadows)
                foreshadow_close->addAction(item.first)->setData(item.second);

            auto foreshadow_open = xmenu->addMenu("移除闭合");
            connect(foreshadow_open,    &QMenu::triggered,  this,   &MainFrame::remove_shadowstop_from_chapter);
            foreshadows.clear();
            novel_core->sumForeshadowsClosed(index, foreshadows);
            for (auto item : foreshadows)
                foreshadow_open->addAction(item.first)->setData(item.second);

            xmenu->addSeparator();
            xmenu->addAction("删除", this, &MainFrame::remove_selected_chapters);
            xmenu->addSeparator();
            xmenu->addAction("输出到剪切板", this, &MainFrame::content_output);
            break;
    }

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
        else {  // chapter-node::target_node
            novel_core->insertChapter(index.parent(), target_node->parent()->rowCount(), title);
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

void MainFrame::append_foreshadow_from_chapters()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    auto list = novel_core->chaptersKeystorySum(index);
    QString name, desp0, desp1;
    QModelIndex pindex;
    ForeshadowConfig dialog(list, this);
    auto result = dialog.getForeshadowDescription(pindex, name, desp0, desp1);
    if(result == QDialog::Rejected)
        return;

    novel_core->appendForeshadow(pindex, name, desp0, desp1);
}

void MainFrame::append_shadowstart_from_chapter(QAction *item)
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    auto full_path = item->data().toString();
    auto keys = full_path.split("@");
    novel_core->appendShadowstart(index, keys.at(1), keys.at(2));
}

void MainFrame::remove_shadowstart_from_chapter(QAction *item)
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    auto target_path = item->data().toString();
    novel_core->removeShadowstart(index, target_path);
}

void MainFrame::append_shadowstop_from_chapter(QAction *item)
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    auto target_path = item->data().toString();
    auto keys = target_path.split("@");
    novel_core->appendShadowstop(index, keys.at(0), keys.at(1), keys.at(2));
}

void MainFrame::remove_shadowstop_from_chapter(QAction *item)
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    auto target_path = item->data().toString();
    novel_core->removeShadowstop(index, target_path);
}

void MainFrame::remove_selected_chapters()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QMessageBox msgBox;
    msgBox.setText("选中节点极其子节点（包含磁盘文件）将被删除！");
    msgBox.setInformativeText("是否确定执行?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();
    if(ret == QMessageBox::No)
        return;

    try {
        QList<QString> msgList;
        novel_core->checkChaptersRemoveEffect(index, msgList);
        if(msgList.size()){
            QString msg;
            for (auto line:msgList) {
                msg += line+"\n";
            }
            auto res = QMessageBox::warning(this, "移除卷章节点,影响如下：", msg, QMessageBox::Ok|QMessageBox::No, QMessageBox::No);
            if(res == QMessageBox::No)
                return;
        }

        novel_core->removeChaptersNode(index);
    } catch (WsException *e) {
        QMessageBox::critical(this, "删除文件", e->reason());
    }
}

void MainFrame::content_output()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    QClipboard *x = QApplication::clipboard();
    auto content = novel_core->chapterActiveText(index);
    x->setText(content);
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
    novel_core->findResultsPresent()->clear();
}

void MainFrame::search_jump(const QModelIndex &xindex)
{
    QModelIndex index = xindex;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto target_item = novel_core->findResultsPresent()->itemFromIndex(index);
    auto chapters_index = target_item->data().toModelIndex();
    auto select_start = target_item->data(Qt::UserRole+2).toInt();
    auto select_len = target_item->data(Qt::UserRole+3).toInt();

    novel_core->setCurrentChaptersNode(chapters_index);
    auto text_cursor = chapter_textedit_present->textCursor();
    text_cursor.setPosition(select_start);
    text_cursor.setPosition(select_start+select_len, QTextCursor::KeepAnchor);
    chapter_textedit_present->setTextCursor(text_cursor);
}

void MainFrame::outlines_navigate_jump(const QModelIndex &_index)
{
    auto index = _index;
    if(!index.isValid())
        return;

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
        resize_foreshadows_tableitem_width();
    } catch (WsException *e) {
        QMessageBox::critical(this, "大纲跳转", e->reason());
    }
}

void MainFrame::outlines_manipulation(const QPoint &point)
{
    auto index = outlines_navigate_treeview->indexAt(point);
    QList<QModelIndex> level;
    while (index.isValid()) {
        level << index;
        index = index.parent();
    }

    auto menu = new QMenu("操作大纲");
    switch (level.size()) {
        case 0:
            menu->addAction(QIcon(":/outlines/icon/卷.png"), "添加分卷", this,   &MainFrame::append_volume2);
            break;
        case 1:
            menu->addAction(QIcon(":/outlines/icon/卷.png"), "添加分卷", this,   &MainFrame::append_volume2);
            menu->addAction(QIcon(":/outlines/icon/卷.png"), "插入分卷", this,   &MainFrame::insert_volume2);
            menu->addAction(QIcon(":/outlines/icon/情.png"), "添加剧情", this,   &MainFrame::append_keystory);
            break;
        case 2:
            menu->addAction(QIcon(":/outlines/icon/情.png"), "添加剧情", this,   &MainFrame::append_keystory);
            menu->addAction(QIcon(":/outlines/icon/情.png"), "插入剧情", this,   &MainFrame::insert_keystory);
            menu->addAction(QIcon(":/outlines/icon/点.png"), "添加分解点",    this,   &MainFrame::append_point);
            break;
        case 3:
            menu->addAction(QIcon(":/outlines/icon/点.png"), "添加分解点",    this,   &MainFrame::append_point);
            menu->addAction(QIcon(":/outlines/icon/点.png"), "插入分解点",    this,   &MainFrame::insert_point);
            break;
    }
    if(level.size()){
        menu->addAction(QIcon(":/outlines/icon/伏.png"), "添加伏笔", this,   &MainFrame::append_foreshadow_from_outlines);
        menu->addSeparator();
        menu->addAction("删除",   this,   &MainFrame::remove_selected_outlines);
    }

    menu->exec(mapToGlobal(point));
    delete menu;
}

void MainFrame::append_volume2()
{
    bool ok;
    auto title = QInputDialog::getText(this, "添加分卷", "分卷名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size())
        return;

    try {
        novel_core->insertVolume(novel_core->outlineNavigateTree()->rowCount(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加分卷", e->reason());
    }
}

void MainFrame::insert_volume2()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "插入分卷", "分卷名称", QLineEdit::Normal, QString(), &ok);
    if(!ok && !title.size())
        return;

    try {
        novel_core->insertVolume(index.row(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分卷", e->reason());
    }
}

void MainFrame::append_keystory()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "添加剧情", "剧情名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size())
        return;

    try {
        if(!index.parent().isValid()) {                         // 分卷节点
            auto volume_node = novel_core->outlineNavigateTree()->itemFromIndex(index);
            novel_core->insertKeystory(index, volume_node->rowCount(), title);
        }
        else if (!index.parent().parent().isValid()) {          // 剧情节点
            auto keystory_node = novel_core->outlineNavigateTree()->itemFromIndex(index);
            auto volume_node = keystory_node->parent();
            novel_core->insertKeystory(volume_node->index(), volume_node->rowCount(), title);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加剧情", e->reason());
    }
}

void MainFrame::insert_keystory()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "插入剧情", "剧情名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size())
        return;

    try {
        novel_core->insertKeystory(index.parent(), index.row(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入剧情", e->reason());
    }
}

void MainFrame::append_point()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "添加分解点", "分解点名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size())
        return;

    try {
        if(!index.parent().parent().isValid()){
            auto keystory_node = novel_core->outlineNavigateTree()->itemFromIndex(index);
            novel_core->insertPoint(index, keystory_node->rowCount(), title);
        }
        else {
            auto point_node = novel_core->outlineNavigateTree()->itemFromIndex(index);
            auto keystory_node = point_node->parent();
            novel_core->insertPoint(keystory_node->index(), keystory_node->rowCount(), title);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加分解点", e->reason());
    }
}

void MainFrame::insert_point()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "插入分解点", "分解点名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size())
        return;

    try {
        novel_core->insertPoint(index.parent(), index.row(), title);
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分解点", e->reason());
    }
}

void MainFrame::append_foreshadow_from_outlines()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto list = novel_core->outlinesKeystorySum(index);
    QString name, desp0, desp1;
    QModelIndex pindex;
    ForeshadowConfig dialog(list, this);
    auto result = dialog.getForeshadowDescription(pindex, name, desp0, desp1);
    if(result == QDialog::Rejected)
        return;

    novel_core->appendForeshadow(pindex, name, desp0, desp1);
}

void MainFrame::remove_selected_outlines()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        QList<QString> result;
        novel_core->checkOutlinesRemoveEffect(index, result);
        if(result.size()){
            QString msg;
            for (auto line:result) {
                msg += line+"\n";
            }
            auto res = QMessageBox::warning(this, "移除大纲节点,影响如下：", msg, QMessageBox::Ok|QMessageBox::No, QMessageBox::No);
            if(res == QMessageBox::No)
                return;
        }

        novel_core->removeOutlineNode(index);
    } catch (WsException *e) {
        QMessageBox::critical(this, "移除卷章", e->reason());
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

void MainFrame::documentClosed(QTextDocument *)
{
    setWindowTitle(novel_core->novelTitle());
    chapter_textedit_present->setDocument(empty_document);
}

void MainFrame::documentPresent(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

    this->chapter_textedit_present->setDocument(doc);
}

void MainFrame::currentChaptersAboutPresent()
{
    chapter_outlines_present->setEnabled(true);
    chapter_textedit_present->setEnabled(true);
}

void MainFrame::currentVolumeOutlinesPresent()
{
    volume_outlines_present->setEnabled(true);
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
